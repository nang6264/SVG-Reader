
#include "SVGRenderer.h"
#include "SVGElement.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <array>
#include "Transform.h"
#include "SVGPath.h"
#include "earcut.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// =============================================================
// 1. CÁC HÀM TOÁN HỌC VECTOR (VECTOR MATH)
// =============================================================

float getLength(const sf::Vector2f& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

sf::Vector2f normalize(const sf::Vector2f& v) {
    float len = getLength(v);
    if (len < 0.00001f) return { 0, 0 };
    return v / len;
}

// Vector pháp tuyến 90 độ (Perpendicular)
sf::Vector2f getPerpendicular(const sf::Vector2f& v) {
    return { -v.y, v.x };
}

float dotProduct(const sf::Vector2f& a, const sf::Vector2f& b) { return a.x * b.x + a.y * b.y; }
float crossProduct(const sf::Vector2f& a, const sf::Vector2f& b) { return a.x * b.y - a.y * b.x; }

sf::Vector2f cubicBezier(float t, sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3) {
    float u = 1 - t;
    float tt = t * t; float uu = u * u; float uuu = uu * u; float ttt = tt * t;
    return (uuu * p0) + (3 * uu * t * p1) + (3 * u * tt * p2) + (ttt * p3);
}

// =============================================================
// 2. LOGIC TÔ MÀU THÔNG MINH (SMART FILL)
// =============================================================

bool isPointInPolygon(const sf::Vector2f& point, const std::vector<sf::Vector2f>& polygon) {
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        if (((polygon[i].y > point.y) != (polygon[j].y > point.y)) &&
            (point.x < (polygon[j].x - polygon[i].x) * (point.y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

bool isPolygonInside(const std::vector<sf::Vector2f>& inner, const std::vector<sf::Vector2f>& outer) {
    if (inner.empty() || outer.empty()) return false;
    return isPointInPolygon(inner[0], outer);
}

double getArea(const std::vector<sf::Vector2f>& points) {
    double area = 0.0;
    if (points.size() < 3) return 0.0;
    for (size_t i = 0; i < points.size(); ++i) {
        size_t j = (i + 1) % points.size();
        area += (double)(points[i].x * points[j].y) - (double)(points[j].x * points[i].y);
    }
    return std::abs(area) / 2.0;
}

void drawConcaveShape(sf::RenderWindow& window, const std::vector<std::vector<sf::Vector2f>>& inputShapes, sf::Color color) {
    if (inputShapes.empty()) return;

    using Point = std::array<double, 2>;
    std::vector<std::vector<Point>> polygon;
    std::vector<sf::Vector2f> flatPoints;

    for (const auto& shape : inputShapes) {
        if (shape.size() < 3) continue;
        std::vector<Point> ring;
        for (const auto& p : shape) ring.push_back({ (double)p.x, (double)p.y });

        if (ring.size() >= 3) {
            auto& f = ring.front(); auto& l = ring.back();
            if (std::abs(f[0] - l[0]) < 0.001 && std::abs(f[1] - l[1]) < 0.001) ring.pop_back();
        }
        if (ring.size() >= 3) {
            polygon.push_back(ring);
            for (const auto& p : ring) flatPoints.emplace_back((float)p[0], (float)p[1]);
        }
    }

    if (polygon.empty()) return;
    std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);

    sf::VertexArray vertices(sf::PrimitiveType::Triangles);
    for (uint32_t index : indices) {
        if (index < flatPoints.size()) vertices.append(sf::Vertex(flatPoints[index], color));
    }
    window.draw(vertices);
}

// =============================================================
// 3. LOGIC VẼ VIỀN CAO CẤP (MITER JOIN EXTRUSION)
// =============================================================

// Hàm này tạo ra một lưới tam giác (Triangle Strip) bao quanh đường path
// Để tạo ra nét vẽ có độ dày và góc nhọn chính xác.
void drawStrokeMesh(sf::RenderWindow& window, const std::vector<sf::Vector2f>& points, float thickness, sf::Color color, bool isClosed) {
    if (points.size() < 2) return;

    std::vector<sf::Vector2f> path = points;

    // Nếu đóng kín, thêm điểm đầu vào cuối và điểm cuối lên đầu (để tính góc)
    if (isClosed) {
        path.push_back(points[0]);
        path.push_back(points[1]); // Đệm thêm để tính góc kề
        path.insert(path.begin(), points.back());
    }
    else {
        // Nếu hở, thêm điểm giả bằng cách kéo dài tiếp tuyến
        sf::Vector2f startDir = normalize(points[1] - points[0]);
        path.insert(path.begin(), points[0] - startDir);
        sf::Vector2f endDir = normalize(points.back() - points[points.size() - 2]);
        path.push_back(points.back() + endDir);
    }

    float halfWidth = thickness / 2.0f;
    float miterLimit = 10.0f; // Giới hạn độ nhọn để tránh tia gai vô tận

    sf::VertexArray vertices(sf::PrimitiveType::TriangleStrip);

    // Duyệt qua các điểm chính (bỏ qua điểm đệm đầu và cuối)
    for (size_t i = 1; i < path.size() - 1; ++i) {
        sf::Vector2f curr = path[i];
        sf::Vector2f prev = path[i - 1];
        sf::Vector2f next = path[i + 1];

        sf::Vector2f dir1 = normalize(curr - prev);
        sf::Vector2f dir2 = normalize(next - curr);

        // Vector tiếp tuyến tại góc (Average tangent)
        sf::Vector2f tangent = normalize(dir1 + dir2);

        // Vector pháp tuyến miter (hướng ra ngoài góc nhọn)
        sf::Vector2f miter = { -tangent.y, tangent.x };

        // Vector pháp tuyến của cạnh đang xét
        sf::Vector2f normal1 = { -dir1.y, dir1.x };

        // Tính độ dài miter cần thiết để đạt đến đỉnh nhọn
        float dot = dotProduct(miter, normal1);

        // Tránh lỗi chia cho 0 hoặc song song
        if (std::abs(dot) < 0.001f) dot = 1.0f;

        float miterLength = halfWidth / dot;

        // Nếu góc quá gắt (miter quá dài), cắt bớt (Bevel) hoặc giới hạn
        if (std::abs(miterLength) > miterLimit * halfWidth) {
            miterLength = miterLimit * halfWidth;
        }

        // Tính 2 điểm: Ngoài (Outer) và Trong (Inner) của đường viền tại đỉnh này
        sf::Vector2f pOuter = curr + miter * miterLength;
        sf::Vector2f pInner = curr - miter * miterLength;

        vertices.append(sf::Vertex(pOuter, color));
        vertices.append(sf::Vertex(pInner, color));
    }

    // Nếu không đóng kín, cần xử lý điểm đầu và điểm cuối cho đẹp (cắt vuông)
    // Ở đây thuật toán TriangleStrip tự nối, nên với path hở ta chỉ cần render
    // đoạn giữa (đã loại bỏ điểm đệm).
    // Tuy nhiên, logic trên đã bao gồm điểm đầu/cuối thật sự.

    window.draw(vertices);
}

// =============================================================
// 4. MAIN IMPLEMENTATION
// =============================================================

SVGRenderer::SVGRenderer(unsigned int width, unsigned int height) {
    sf::ContextSettings settings; settings.antiAliasingLevel = 8;
    window.create(sf::VideoMode({ 1200, 800 }), "SVG Renderer", sf::Style::Default, sf::State::Windowed, settings);
    view = window.getDefaultView();
    if (!font.openFromFile("times.ttf")) {}
}

void SVGRenderer::addElement(std::shared_ptr<SVGElement> element) { if (element) elements.push_back(element); }
void SVGRenderer::setViewBox(float x, float y, float w, float h) { if (w > 0 && h > 0) { view = sf::View(sf::FloatRect({ x, y }, { w, h })); window.setView(view); } }
void SVGRenderer::zoomIn() { view.zoom(0.9f); }
void SVGRenderer::zoomOut() { view.zoom(1.1f); }
void SVGRenderer::rotate(float angle) { view.rotate(sf::degrees(angle)); }
sf::Transform SVGRenderer::getSFMLTransform(const TransformMatrix& m) const { return sf::Transform(m.m[0], m.m[3], m.m[2], m.m[1], m.m[4], m.m[5], 0, 0, 1); }

void SVGRenderer::render() {
    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) window.close();
            else if (auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) window.close();
                else if (key->scancode == sf::Keyboard::Scancode::L) rotate(10.f);
                else if (key->scancode == sf::Keyboard::Scancode::R) rotate(-10.f);
            }
            else if (auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                wheel->delta > 0 ? zoomIn() : zoomOut();
            }
            else if (auto* press = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (press->button == sf::Mouse::Button::Left) { isPanning = true; lastMousePos = press->position; }
            }
            else if (auto* release = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (release->button == sf::Mouse::Button::Left) isPanning = false;
            }
            else if (auto* move = event->getIf<sf::Event::MouseMoved>()) {
                if (isPanning) {
                    sf::Vector2f delta = window.mapPixelToCoords(lastMousePos) - window.mapPixelToCoords(move->position);
                    view.move(delta); window.setView(view); lastMousePos = move->position;
                }
            }
        }
        window.clear(sf::Color::White);
        window.setView(view);
        for (auto& e : elements) e->draw(*this);
        window.display();
    }
}

// Helpers
float getOpacity(const std::map<std::string, std::string>& attrs, std::string key) {
    auto it = attrs.find(key);
    if (it != attrs.end()) try { return std::stof(it->second); }
    catch (...) {}
    return 1.0f;
}

sf::Color SVGRenderer::stringToColor(std::string colorStr, std::string type) {
    std::string s = colorStr;
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "none" || s == "transparent") return sf::Color::Transparent;

    static const std::map<std::string, sf::Color> colors = {
        {"black", sf::Color::Black}, {"white", sf::Color::White},
        {"red", sf::Color::Red}, {"green", sf::Color::Green},
        {"blue", sf::Color::Blue}, {"yellow", sf::Color::Yellow},
        {"magenta", sf::Color::Magenta}, {"cyan", sf::Color::Cyan},
        {"gray", sf::Color(128, 128, 128)}, {"grey", sf::Color(128, 128, 128)},
        {"orange", sf::Color(255, 165, 0)}, {"purple", sf::Color(128, 0, 128)},
        {"lime", sf::Color(0, 255, 0)}, {"navy", sf::Color(0, 0, 128)},
        {"pink", sf::Color(255, 192, 203)}, {"gold", sf::Color(255, 215, 0)}
    };
    if (colors.count(s)) return colors.at(s);

    if (!s.empty() && s[0] == '#') {
        s.erase(0, 1);
        if (s.size() == 3) { std::string t; for (char c : s) { t += c; t += c; } s = t; }
        if (s.size() >= 6) {
            unsigned int hex; std::stringstream ss; ss << std::hex << s; ss >> hex;
            if (s.size() == 8) return sf::Color((hex >> 24) & 0xFF, (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
            return sf::Color((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
        }
    }
    if (s.find("rgb(") == 0) {
        size_t end = s.find(')');
        if (end != std::string::npos) {
            std::string c = s.substr(4, end - 4);
            std::replace(c.begin(), c.end(), ',', ' ');
            std::stringstream ss(c); int r, g, b; ss >> r >> g >> b;
            return sf::Color(r, g, b);
        }
    }
    return sf::Color::Transparent;
}

Attributes SVGRenderer::getEffectiveAttributes(const Attributes& localAttrs) const {
    Attributes e; if (!renderStack_.empty())e = renderStack_.back().inheritedAttributes; for (auto& k : localAttrs)e[k.first] = k.second;
    if (e.count("style")) { std::stringstream ss(e["style"]); std::string s; while (std::getline(ss, s, ';')) { size_t p = s.find(':'); if (p != std::string::npos) { std::string k = s.substr(0, p), v = s.substr(p + 1); k.erase(0, k.find_first_not_of(" \t")); k.erase(k.find_last_not_of(" \t") + 1); v.erase(0, v.find_first_not_of(" \t")); v.erase(v.find_last_not_of(" \t") + 1); if (!k.empty())e[k] = v; } } }
    return e;
}

// Basic Renderers (Giữ nguyên logic cũ, tập trung sửa renderPath)
void SVGRenderer::renderCircle(const Circle& c) {
    // Vẽ Circle bằng Path Logic để đồng bộ stroke nhọn nếu cần, hoặc giữ nguyên CircleShape nếu muốn nhanh
    // Ở đây giữ nguyên CircleShape vì hình tròn không có góc nhọn để mà lỗi
    sf::CircleShape s(c.getR());
    Attributes attrs = getEffectiveAttributes(c.getAttributes());
    TransformMatrix tm = getCumulativeTransform(); tm.combine(c.getTransform());
    float tx, ty; tm.transformPoint(c.getCx() - c.getR(), c.getCy() - c.getR(), tx, ty); s.setPosition({ tx, ty });

    sf::Color fill = stringToColor(attrs.count("fill") ? attrs["fill"] : "none", "fill");
    if (fill != sf::Color::Transparent) { fill.a = getOpacity(attrs, "fill-opacity") * 255; s.setFillColor(fill); }
    else s.setFillColor(sf::Color::Transparent);
    sf::Color stroke = stringToColor(attrs.count("stroke") ? attrs["stroke"] : "none", "stroke");
    if (stroke != sf::Color::Transparent) { stroke.a = getOpacity(attrs, "stroke-opacity") * 255; s.setOutlineColor(stroke); }
    float w = 1.f; if (attrs.count("stroke-width")) try { w = std::stof(attrs["stroke-width"]); }
    catch (...) {};
    s.setOutlineThickness(stroke.a > 0 ? w : 0); window.draw(s);
}
void SVGRenderer::renderRect(const Rect& r) {
    // Chuyển Rect thành Path để vẽ góc nhọn (Miter) chuẩn xác
    // Nếu dùng RectangleShape, góc sẽ bị bo tròn hoặc hở khi stroke dày.
    // Ta giả lập Rect bằng polygon
    std::vector<sf::Vector2f> pts;
    float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    pts.push_back({ x, y }); pts.push_back({ x + w, y }); pts.push_back({ x + w, y + h }); pts.push_back({ x, y + h });

    // Gọi logic vẽ Polygon (đã được nâng cấp bên dưới)
    // Cần tạo Polygon giả
    Attributes attrs = getEffectiveAttributes(r.getAttributes());
    TransformMatrix tm = getCumulativeTransform(); tm.combine(r.getTransform());

    // Transform điểm
    for (auto& p : pts) { float tx, ty; tm.transformPoint(p.x, p.y, tx, ty); p.x = tx; p.y = ty; }

    sf::Color fill = stringToColor(attrs.count("fill") ? attrs["fill"] : "none", "fill");
    if (fill != sf::Color::Transparent) { fill.a = getOpacity(attrs, "fill-opacity") * 255; drawConcaveShape(window, { pts }, fill); }

    sf::Color stroke = stringToColor(attrs.count("stroke") ? attrs["stroke"] : "none", "stroke");
    if (stroke != sf::Color::Transparent) { stroke.a = getOpacity(attrs, "stroke-opacity") * 255; }
    float th = 1.f; if (attrs.count("stroke-width")) try { th = std::stof(attrs["stroke-width"]); }
    catch (...) {};

    if (stroke.a > 0 && th > 0) drawStrokeMesh(window, pts, th, stroke, true); // True = Closed
}
void SVGRenderer::renderText(const Text& t) {
    sf::Text s(font); s.setString(t.getContent()); s.setCharacterSize((unsigned)t.getFontSize()); s.setPosition({ (float)t.getX(), (float)t.getY() - s.getCharacterSize() });
    Attributes attrs = getEffectiveAttributes(t.getAttributes()); TransformMatrix tm = getCumulativeTransform(); tm.combine(t.getTransform());
    sf::Color fill = stringToColor(attrs.count("fill") ? attrs["fill"] : "black", "fill"); fill.a = getOpacity(attrs, "fill-opacity") * 255; s.setFillColor(fill);
    if (attrs.count("stroke") && attrs["stroke"] != "none") {
        sf::Color stroke = stringToColor(attrs["stroke"], "stroke"); stroke.a = getOpacity(attrs, "stroke-opacity") * 255; s.setOutlineColor(stroke);
        float w = 0.5f; if (attrs.count("stroke-width")) try { w = std::stof(attrs["stroke-width"]); }
        catch (...) {}; s.setOutlineThickness(w);
    }
    window.draw(s, getSFMLTransform(tm));
}
void SVGRenderer::renderLine(const Line& l) {
    Attributes attrs = getEffectiveAttributes(l.getAttributes()); TransformMatrix tm = getCumulativeTransform(); tm.combine(l.getTransform());
    float x1 = l.getX1(), y1 = l.getY1(), x2 = l.getX2(), y2 = l.getY2(); tm.transformPoint(x1, y1, x1, y1); tm.transformPoint(x2, y2, x2, y2);
    sf::Color stroke = stringToColor(attrs.count("stroke") ? attrs["stroke"] : "none", "stroke"); stroke.a = getOpacity(attrs, "stroke-opacity") * 255;
    float w = 1.f; if (attrs.count("stroke-width")) try { w = std::stof(attrs["stroke-width"]); }
    catch (...) {};

    if (stroke.a > 0 && w > 0) {
        std::vector<sf::Vector2f> pts = { {x1,y1}, {x2,y2} };
        drawStrokeMesh(window, pts, w, stroke, false);
    }
}
void SVGRenderer::renderPolyline(const Polyline& p) {
    std::istringstream iss(p.getPoints()); std::vector<sf::Vector2f> pts; float x, y; char c; TransformMatrix tm = getCumulativeTransform(); tm.combine(p.getTransform());
    while (iss >> x) { if (iss.peek() == ',')iss >> c; if (iss >> y) { float tx, ty; tm.transformPoint(x, y, tx, ty); pts.emplace_back(tx, ty); } if (iss.peek() == ',')iss >> c; }
    if (pts.size() < 2) return;
    Attributes attrs = getEffectiveAttributes(p.getAttributes());
    sf::Color stroke = stringToColor(attrs.count("stroke") ? attrs["stroke"] : "none", "stroke"); stroke.a = getOpacity(attrs, "stroke-opacity") * 255;
    float w = 1.f; if (attrs.count("stroke-width")) try { w = std::stof(attrs["stroke-width"]); }
    catch (...) {};
    sf::Color fill = stringToColor(attrs.count("fill") ? attrs["fill"] : "none", "fill"); if (fill != sf::Color::Transparent) { fill.a = getOpacity(attrs, "fill-opacity") * 255; drawConcaveShape(window, { pts }, fill); }

    if (stroke.a > 0 && w > 0) drawStrokeMesh(window, pts, w, stroke, false); // False = Open
}
void SVGRenderer::renderPolygon(const Polygon& p) {
    std::istringstream iss(p.getPoints()); std::vector<sf::Vector2f> pts; float x, y; char c; TransformMatrix tm = getCumulativeTransform(); tm.combine(p.getTransform());
    while (iss >> x) { if (iss.peek() == ',')iss >> c; if (iss >> y) { float tx, ty; tm.transformPoint(x, y, tx, ty); pts.emplace_back(tx, ty); } if (iss.peek() == ',')iss >> c; }
    if (pts.size() < 3) return;
    Attributes attrs = getEffectiveAttributes(p.getAttributes());
    sf::Color fill = stringToColor(attrs.count("fill") ? attrs["fill"] : "none", "fill"); if (fill != sf::Color::Transparent) { fill.a = getOpacity(attrs, "fill-opacity") * 255; drawConcaveShape(window, { pts }, fill); }
    sf::Color stroke = stringToColor(attrs.count("stroke") ? attrs["stroke"] : "none", "stroke"); stroke.a = getOpacity(attrs, "stroke-opacity") * 255;
    float w = 1.f; if (attrs.count("stroke-width")) try { w = std::stof(attrs["stroke-width"]); }
    catch (...) {};

    if (stroke.a > 0 && w > 0) drawStrokeMesh(window, pts, w, stroke, true); // True = Closed
}
void SVGRenderer::renderEllipse(const Ellipse& e) { renderCircle(Circle(e.getAttributes())); }

// =============================================================
// 5. RENDER PATH (FINAL)
// =============================================================

void SVGRenderer::renderPath(const Path& path) {
    const auto& commands = path.getCommands();
    if (commands.empty()) return;

    Attributes attrs = getEffectiveAttributes(path.getAttributes());
    TransformMatrix finalMatrix = getCumulativeTransform();
    finalMatrix.combine(path.getTransform());

    sf::Color fillColor = stringToColor(attrs.count("fill") ? attrs["fill"] : "black", "fill");
    float fillOpacity = getOpacity(attrs, "fill-opacity");
    if (fillColor != sf::Color::Transparent) fillColor.a = static_cast<uint8_t>(fillOpacity * 255);

    sf::Color strokeColor = stringToColor(attrs.count("stroke") ? attrs["stroke"] : "none", "stroke");
    float strokeOpacity = getOpacity(attrs, "stroke-opacity");
    if (strokeColor != sf::Color::Transparent) strokeColor.a = static_cast<uint8_t>(strokeOpacity * 255);

    float strokeWidth = 1.0f;
    if (attrs.count("stroke-width")) try { strokeWidth = std::stof(attrs["stroke-width"]); }
    catch (...) {}

    // --- BƯỚC 1: GIẢI MÃ PATH ---
    std::vector<std::vector<sf::Vector2f>> subPaths;
    std::vector<sf::Vector2f> currentPoints;
    sf::Vector2f currentPos(0, 0), startPathPos(0, 0);

    auto addBezier = [&](sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3) {
        int steps = 20 + (int)(getLength(p3 - p0) / 5.f);
        if (steps > 100) steps = 100;
        for (int i = 1; i <= steps; ++i) currentPoints.push_back(cubicBezier(i / (float)steps, p0, p1, p2, p3));
        };

    for (const auto& cmd : commands) {
        char type = cmd.type; const auto& args = cmd.args;
        if ((type == 'M' || type == 'm') && !currentPoints.empty()) { subPaths.push_back(currentPoints); currentPoints.clear(); }

        if (type == 'M') { currentPos = { args[0], args[1] }; startPathPos = currentPos; currentPoints.push_back(currentPos); }
        else if (type == 'm') { currentPos += {args[0], args[1]}; startPathPos = currentPos; currentPoints.push_back(currentPos); }
        else if (type == 'L') { currentPos = { args[0], args[1] }; currentPoints.push_back(currentPos); }
        else if (type == 'l') { currentPos += {args[0], args[1]}; currentPoints.push_back(currentPos); }
        else if (type == 'H') { currentPos.x = args[0]; currentPoints.push_back(currentPos); }
        else if (type == 'h') { currentPos.x += args[0]; currentPoints.push_back(currentPos); }
        else if (type == 'V') { currentPos.y = args[0]; currentPoints.push_back(currentPos); }
        else if (type == 'v') { currentPos.y += args[0]; currentPoints.push_back(currentPos); }
        else if (type == 'C') { sf::Vector2f p1(args[0], args[1]), p2(args[2], args[3]), p3(args[4], args[5]); addBezier(currentPos, p1, p2, p3); currentPos = p3; }
        else if (type == 'c') { sf::Vector2f p1 = currentPos + sf::Vector2f(args[0], args[1]), p2 = currentPos + sf::Vector2f(args[2], args[3]), p3 = currentPos + sf::Vector2f(args[4], args[5]); addBezier(currentPos, p1, p2, p3); currentPos = p3; }
        else if (type == 'Z' || type == 'z') { if (currentPos != startPathPos) { currentPoints.push_back(startPathPos); currentPos = startPathPos; } }
        else if (type == 'S' || type == 's' || type == 'Q' || type == 'q' || type == 'T' || type == 't') {
            if (args.size() >= 2) { currentPos = (type == 's' || type == 't' || type == 'q') ? currentPos + sf::Vector2f(args[args.size() - 2], args[args.size() - 1]) : sf::Vector2f(args[args.size() - 2], args[args.size() - 1]); currentPoints.push_back(currentPos); }
        }
    }
    if (!currentPoints.empty()) subPaths.push_back(currentPoints);

    // Transform
    for (auto& pathPts : subPaths) {
        for (auto& p : pathPts) { float tx, ty; finalMatrix.transformPoint(p.x, p.y, tx, ty); p.x = tx; p.y = ty; }
    }

    // --- BƯỚC 2: TÔ MÀU THÔNG MINH (HIERARCHICAL) ---
    if (fillColor.a > 0) {
        struct ShapeGroup { std::vector<sf::Vector2f> outer; std::vector<std::vector<sf::Vector2f>> holes; };
        std::vector<ShapeGroup> groups;
        std::sort(subPaths.begin(), subPaths.end(), [](const auto& a, const auto& b) { return getArea(a) > getArea(b); });

        for (const auto& pathPts : subPaths) {
            if (pathPts.size() < 3) continue;
            bool added = false;
            for (auto& g : groups) {
                if (isPolygonInside(pathPts, g.outer)) { g.holes.push_back(pathPts); added = true; break; }
            }
            if (!added) groups.push_back({ pathPts, {} });
        }

        for (const auto& g : groups) {
            std::vector<std::vector<sf::Vector2f>> input; input.push_back(g.outer);
            for (const auto& h : g.holes) input.push_back(h);
            drawConcaveShape(window, input, fillColor);
        }
    }

    // --- BƯỚC 3: VẼ VIỀN SẮC NÉT (VECTOR EXTRUSION) ---
    if (strokeColor.a > 0 && strokeWidth > 0) {
        for (const auto& pathPts : subPaths) {
            // Kiểm tra xem path này có kín không (đầu == cuối)
            bool isClosed = false;
            if (pathPts.size() > 2) {
                if (getLength(pathPts.front() - pathPts.back()) < 0.1f) isClosed = true;
            }
            drawStrokeMesh(window, pathPts, strokeWidth, strokeColor, isClosed);
        }
    }
}

// Các hàm beginElement, endElement giữ nguyên
void SVGRenderer::beginElement(const TransformMatrix& t, const Attributes& a) { if (renderStack_.empty()) { renderStack_.push_back({ t,a }); } else { RenderState s = renderStack_.back(); s.cumulativeTransform.combine(t); for (auto& k : a)s.inheritedAttributes[k.first] = k.second; renderStack_.push_back(s); } }
void SVGRenderer::endElement() { if (!renderStack_.empty()) renderStack_.pop_back(); }
const TransformMatrix& SVGRenderer::getCumulativeTransform() const { static TransformMatrix i; return renderStack_.empty() ? i : renderStack_.back().cumulativeTransform; }