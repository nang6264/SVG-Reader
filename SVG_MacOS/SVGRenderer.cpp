// SVGRenderer.cpp - FINAL SHARP VERSION
// Tính năng: Viền sắc nét (Miter Joins) + Tô màu thông minh + Fix lỗi trùng lặp

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
    if (len < 0.0001f) return { 0, 0 };
    return v / len;
}

// Vector pháp tuyến (vuông góc 90 độ)
sf::Vector2f getNormal(const sf::Vector2f& v) {
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

        // Clean up
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
        if (index < flatPoints.size()) {
            vertices.append(sf::Vertex(flatPoints[index], color));
        }
    }
    window.draw(vertices);
}

// =============================================================
// 3. LOGIC VẼ VIỀN SẮC NÉT (SHARP STROKE - MITER JOINT)
// =============================================================

// Vẽ tam giác đơn giản
void drawTri(sf::RenderWindow& w, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3, sf::Color c) {
    sf::VertexArray tri(sf::PrimitiveType::Triangles, 3);
    tri[0] = { p1, c }; tri[1] = { p2, c }; tri[2] = { p3, c };
    w.draw(tri);
}

// Vẽ thân đoạn thẳng (dùng 2 tam giác ghép lại để chính xác hơn RectangleShape)
void drawSegment(sf::RenderWindow& w, sf::Vector2f p1, sf::Vector2f p2, float thickness, sf::Color color) {
    sf::Vector2f dir = normalize(p2 - p1);
    sf::Vector2f normal = getNormal(dir) * (thickness / 2.f);

    sf::VertexArray quad(sf::PrimitiveType::Triangles, 6);
    // Tam giác 1
    quad[0] = { p1 + normal, color };
    quad[1] = { p1 - normal, color };
    quad[2] = { p2 - normal, color };
    // Tam giác 2
    quad[3] = { p1 + normal, color };
    quad[4] = { p2 - normal, color };
    quad[5] = { p2 + normal, color };
    w.draw(quad);
}

// Hàm quan trọng nhất: Tính toán và vẽ góc nhọn (Miter Join)
void drawLineJoin(sf::RenderWindow& w, sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2,
    float thickness, sf::Color color, std::string type, float miterLimit) {

    sf::Vector2f dir1 = normalize(p1 - p0);
    sf::Vector2f dir2 = normalize(p2 - p1);
    sf::Vector2f n1 = getNormal(dir1); // Pháp tuyến đoạn trước
    sf::Vector2f n2 = getNormal(dir2); // Pháp tuyến đoạn sau

    // Tính vector tiếp tuyến trung bình tại góc
    sf::Vector2f tangent = normalize(dir1 + dir2);
    // Hướng của đỉnh nhọn Miter
    sf::Vector2f miterDir = { -tangent.y, tangent.x };

    // Xác định hướng rẽ (trái hay phải)
    float cp = crossProduct(dir1, dir2);
    float halfWidth = thickness / 2.f;

    sf::Vector2f outer1, outer2; // Hai điểm ngoài cùng của 2 đoạn thẳng

    if (cp < 0) { // Rẽ phải
        outer1 = p1 - n1 * halfWidth;
        outer2 = p1 - n2 * halfWidth;
        miterDir = -miterDir; // Đảo chiều miter
    }
    else { // Rẽ trái
        outer1 = p1 + n1 * halfWidth;
        outer2 = p1 + n2 * halfWidth;
    }

    // Tính độ dài từ tâm góc đến đỉnh nhọn
    float dot = dotProduct(miterDir, (cp < 0 ? -n1 : n1));
    if (std::abs(dot) < 0.01f) return; // Song song, bỏ qua
    float miterLength = halfWidth / dot;

    // --- XỬ LÝ LOẠI KHỚP NỐI ---

    // 1. ROUND (Tròn)
    if (type == "round") {
        sf::CircleShape circle(halfWidth);
        circle.setOrigin({ halfWidth, halfWidth });
        circle.setPosition(p1);
        circle.setFillColor(color);
        w.draw(circle);
        return;
    }

    // 2. MITER (Nhọn) - Có kiểm tra giới hạn
    if (type == "miter") {
        // Kiểm tra Miter Limit (tránh gai quá dài)
        if ((miterLength / halfWidth) <= miterLimit) {
            sf::Vector2f miterPoint = p1 + miterDir * miterLength;
            // Vẽ 2 tam giác để lấp đầy khoảng trống tạo thành mũi nhọn
            drawTri(w, p1, outer1, miterPoint, color);
            drawTri(w, p1, miterPoint, outer2, color);
            return;
        }
        // Nếu quá nhọn -> Tự động chuyển sang Bevel
    }

    // 3. BEVEL (Vát/Tù)
    drawTri(w, p1, outer1, outer2, color);
}

// =============================================================
// 4. MAIN IMPLEMENTATION
// =============================================================

SVGRenderer::SVGRenderer(unsigned int width, unsigned int height) {
    sf::ContextSettings settings; settings.antiAliasingLevel = 8;
    window.create(sf::VideoMode({ width + 400, height + 200 }), "SVG Renderer", sf::Style::Default, sf::State::Windowed, settings);
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

// Helpers Attribute
float getOpacity(const std::map<std::string, std::string>& attrs, std::string key);
sf::Color SVGRenderer::stringToColor(std::string colorStr, std::string type);
Attributes SVGRenderer::getEffectiveAttributes(const Attributes& localAttrs) const {
    Attributes e; if (!renderStack_.empty())e = renderStack_.back().inheritedAttributes; for (auto& k : localAttrs)e[k.first] = k.second;
    if (e.count("style")) { std::stringstream ss(e["style"]); std::string s; while (std::getline(ss, s, ';')) { size_t p = s.find(':'); if (p != std::string::npos) { std::string k = s.substr(0, p), v = s.substr(p + 1); k.erase(0, k.find_first_not_of(" \t")); k.erase(k.find_last_not_of(" \t") + 1); v.erase(0, v.find_first_not_of(" \t")); v.erase(v.find_last_not_of(" \t") + 1); if (!k.empty())e[k] = v; } } }
    return e;
}

// Basic Renderers 
void SVGRenderer::renderCircle(const Circle& c) {
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
    sf::RectangleShape s({ (float)r.getWidth(),(float)r.getHeight() }); s.setPosition({ (float)r.getX(),(float)r.getY() });
    Attributes attrs = getEffectiveAttributes(r.getAttributes()); TransformMatrix tm = getCumulativeTransform(); tm.combine(r.getTransform());
    sf::Color fill = stringToColor(attrs.count("fill") ? attrs["fill"] : "none", "fill");
    if (fill != sf::Color::Transparent) { fill.a = getOpacity(attrs, "fill-opacity") * 255; s.setFillColor(fill); }
    else s.setFillColor(sf::Color::Transparent);
    sf::Color stroke = stringToColor(attrs.count("stroke") ? attrs["stroke"] : "none", "stroke");
    if (stroke != sf::Color::Transparent) { stroke.a = getOpacity(attrs, "stroke-opacity") * 255; s.setOutlineColor(stroke); }
    float w = 1.f; if (attrs.count("stroke-width")) try { w = std::stof(attrs["stroke-width"]); }
    catch (...) {};
    s.setOutlineThickness(stroke.a > 0 ? w : 0); window.draw(s, getSFMLTransform(tm));
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
    if (stroke.a > 0 && w > 0) drawSegment(window, { x1,y1 }, { x2,y2 }, w, stroke);
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
    if (stroke.a > 0 && w > 0) { for (size_t i = 0;i < pts.size() - 1;++i) { drawSegment(window, pts[i], pts[i + 1], w, stroke); if (i > 0) drawLineJoin(window, pts[i - 1], pts[i], pts[i + 1], w, stroke, "round", 4.f); } }
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
    if (stroke.a > 0 && w > 0) { for (size_t i = 0;i < pts.size();++i) { sf::Vector2f p1 = pts[i], p2 = pts[(i + 1) % pts.size()]; drawSegment(window, p1, p2, w, stroke); sf::Vector2f p0 = pts[(i == 0 ? pts.size() - 1 : i - 1)]; drawLineJoin(window, p0, p1, p2, w, stroke, "miter", 4.f); } }
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

    std::string lineJoin = "miter"; // Mặc định là nhọn
    if (attrs.count("stroke-linejoin")) lineJoin = attrs["stroke-linejoin"];

    float miterLimit = 4.0f;
    if (attrs.count("stroke-miterlimit")) try { miterLimit = std::stof(attrs["stroke-miterlimit"]); }
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

    // --- BƯỚC 3: VẼ VIỀN SẮC NÉT (SHARP STROKE) ---
    if (strokeColor.a > 0 && strokeWidth > 0) {
        for (const auto& pathPts : subPaths) {
            if (pathPts.size() < 2) continue;

            for (size_t i = 0; i < pathPts.size() - 1; ++i) {
                sf::Vector2f p1 = pathPts[i];
                sf::Vector2f p2 = pathPts[i + 1];

                // Vẽ thân (Segment)
                drawSegment(window, p1, p2, strokeWidth, strokeColor);

                // Vẽ khớp nối (Join)
                if (i > 0) {
                    sf::Vector2f p0 = pathPts[i - 1];
                    drawLineJoin(window, p0, p1, p2, strokeWidth, strokeColor, lineJoin, miterLimit);
                }
            }

            // Xử lý đóng kín
            sf::Vector2f start = pathPts.front();
            sf::Vector2f end = pathPts.back();
            if (getLength(start - end) < 0.1f && pathPts.size() > 2) {
                sf::Vector2f pLastPrev = pathPts[pathPts.size() - 2];
                sf::Vector2f pSecond = pathPts[1];
                // Nối cuối và đầu bằng khớp Miter
                drawLineJoin(window, pLastPrev, end, pSecond, strokeWidth, strokeColor, lineJoin, miterLimit);
            }
            else if (lineJoin == "round") {
                sf::CircleShape cap(strokeWidth / 2.f); cap.setOrigin({ strokeWidth / 2.f, strokeWidth / 2.f }); cap.setFillColor(strokeColor);
                cap.setPosition(start); window.draw(cap);
                cap.setPosition(end); window.draw(cap);
            }
        }
    }
}

// Các hàm beginElement, endElement giữ nguyên
void SVGRenderer::beginElement(const TransformMatrix& t, const Attributes& a) { if (renderStack_.empty()) { renderStack_.push_back({ t,a }); } else { RenderState s = renderStack_.back(); s.cumulativeTransform.combine(t); for (auto& k : a)s.inheritedAttributes[k.first] = k.second; renderStack_.push_back(s); } }
void SVGRenderer::endElement() { if (!renderStack_.empty()) renderStack_.pop_back(); }
const TransformMatrix& SVGRenderer::getCumulativeTransform() const { static TransformMatrix i; return renderStack_.empty() ? i : renderStack_.back().cumulativeTransform; }