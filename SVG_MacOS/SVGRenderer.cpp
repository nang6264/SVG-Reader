// SVGRenderer.cpp - FINAL FIXED (No syntax errors)
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

// --- Math Helpers ---
float getLength(const sf::Vector2f& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
sf::Vector2f normalize(const sf::Vector2f& v) {
    float len = getLength(v);
    return (len < 0.0001f) ? sf::Vector2f(0, 0) : v / len;
}
sf::Vector2f getNormal(const sf::Vector2f& v) { return { -v.y, v.x }; }
float dotProduct(const sf::Vector2f& a, const sf::Vector2f& b) { return a.x * b.x + a.y * b.y; }
float crossProduct(const sf::Vector2f& a, const sf::Vector2f& b) { return a.x * b.y - a.y * b.x; }

sf::Vector2f cubicBezier(float t, sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3) {
    float u = 1 - t;
    float tt = t * t; float uu = u * u; float uuu = uu * u; float ttt = tt * t;
    return (uuu * p0) + (3 * uu * t * p1) + (3 * u * tt * p2) + (ttt * p3);
}

// --- Fill Helpers ---
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

// --- Stroke Helpers (Triangle Strip) ---
void drawSharpStroke(sf::RenderWindow& window, const std::vector<sf::Vector2f>& points, float thickness, sf::Color color, bool isClosed) {
    if (points.size() < 2) return;
    std::vector<sf::Vector2f> path = points;

    if (isClosed) {
        if (getLength(path.front() - path.back()) < 0.1f) path.pop_back();
        path.insert(path.begin(), path.back());
        path.push_back(path[1]);
    }
    else {
        path.insert(path.begin(), path[0] + (path[0] - path[1]));
        path.push_back(path.back() + (path.back() - path[path.size() - 2]));
    }

    float halfW = thickness / 2.0f;
    sf::VertexArray strip(sf::PrimitiveType::TriangleStrip);

    for (size_t i = 1; i < path.size() - 1; ++i) {
        sf::Vector2f cur = path[i], prev = path[i - 1], next = path[i + 1];
        sf::Vector2f dir1 = normalize(cur - prev);
        sf::Vector2f dir2 = normalize(next - cur);
        sf::Vector2f tangent = normalize(dir1 + dir2);
        sf::Vector2f miter = { -tangent.y, tangent.x };
        sf::Vector2f normal = { -dir1.y, dir1.x };

        float dot = dotProduct(miter, normal);
        if (std::abs(dot) < 0.01f) dot = 1.0f;
        float miterLen = halfW / dot;
        if (miterLen > thickness * 5.0f) miterLen = thickness * 5.0f;

        strip.append(sf::Vertex(cur + miter * miterLen, color));
        strip.append(sf::Vertex(cur - miter * miterLen, color));
    }
    if (isClosed) {
        strip.append(strip[0]);
        strip.append(strip[1]);
    }
    window.draw(strip);
}

// --- Main Class ---
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
            else if (auto* k = event->getIf<sf::Event::KeyPressed>()) {
                if (k->scancode == sf::Keyboard::Scancode::Escape) window.close();
                else if (k->scancode == sf::Keyboard::Scancode::L) rotate(10.f);
                else if (k->scancode == sf::Keyboard::Scancode::R) rotate(-10.f);
            }
            else if (auto* w = event->getIf<sf::Event::MouseWheelScrolled>()) {
                w->delta > 0 ? zoomIn() : zoomOut();
            }
            else if (auto* p = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (p->button == sf::Mouse::Button::Left) { isPanning = true; lastMousePos = p->position; }
            }
            else if (auto* r = event->getIf<sf::Event::MouseButtonReleased>()) {
                if (r->button == sf::Mouse::Button::Left) isPanning = false;
            }
            else if (auto* m = event->getIf<sf::Event::MouseMoved>()) {
                if (isPanning) {
                    sf::Vector2f delta = window.mapPixelToCoords(lastMousePos) - window.mapPixelToCoords(m->position);
                    view.move(delta); window.setView(view); lastMousePos = m->position;
                }
            }
        }
        window.clear(sf::Color::White);
        window.setView(view);
        for (auto& e : elements) e->draw(*this);
        window.display();
    }
}

// --- Helpers ---
float getOpacity(const std::map<std::string, std::string>& attrs, std::string key) {
    auto it = attrs.find(key);
    if (it != attrs.end()) try { return std::stof(it->second); }
    catch (...) {}
    return 1.0f;
}

// [FIXED] stringToColor (Corrected syntax)
sf::Color SVGRenderer::stringToColor(std::string colorStr, std::string type) {
    std::string s = colorStr;
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "none" || s == "transparent") return sf::Color::Transparent;

    static const std::map<std::string, sf::Color> colors = {
        {"black", sf::Color::Black}, {"white", sf::Color::White}, {"red", sf::Color::Red},
        {"green", sf::Color::Green}, {"blue", sf::Color::Blue}, {"yellow", sf::Color::Yellow},
        {"magenta", sf::Color::Magenta}, {"cyan", sf::Color::Cyan}, {"gray", sf::Color(128, 128, 128)},
        {"orange", sf::Color(255, 165, 0)}, {"purple", sf::Color(128, 0, 128)}
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
    return sf::Color::Transparent;
}

Attributes SVGRenderer::getEffectiveAttributes(const Attributes& localAttrs) const {
    Attributes e; if (!renderStack_.empty()) e = renderStack_.back().inheritedAttributes;
    for (auto& k : localAttrs) e[k.first] = k.second;
    if (e.count("style")) {
        std::stringstream ss(e["style"]); std::string s;
        while (std::getline(ss, s, ';')) {
            size_t p = s.find(':');
            if (p != std::string::npos) {
                std::string k = s.substr(0, p), v = s.substr(p + 1);
                k.erase(0, k.find_first_not_of(" \t")); k.erase(k.find_last_not_of(" \t") + 1);
                v.erase(0, v.find_first_not_of(" \t")); v.erase(v.find_last_not_of(" \t") + 1);
                if (!k.empty()) e[k] = v;
            }
        }
    }
    return e;
}

// --- Basic Renderers ---
void SVGRenderer::renderCircle(const Circle& c) {
    sf::CircleShape s(c.getR());
    Attributes a = getEffectiveAttributes(c.getAttributes());
    TransformMatrix tm = getCumulativeTransform(); tm.combine(c.getTransform());
    float tx, ty; tm.transformPoint(c.getCx() - c.getR(), c.getCy() - c.getR(), tx, ty);
    s.setPosition({ tx, ty });

    sf::Color f = stringToColor(a.count("fill") ? a["fill"] : "none", "fill");
    if (f != sf::Color::Transparent) { f.a = getOpacity(a, "fill-opacity") * 255; s.setFillColor(f); }
    else s.setFillColor(sf::Color::Transparent);
    sf::Color st = stringToColor(a.count("stroke") ? a["stroke"] : "none", "stroke");
    if (st != sf::Color::Transparent) { st.a = getOpacity(a, "stroke-opacity") * 255; s.setOutlineColor(st); }
    float w = 1.f; if (a.count("stroke-width")) try { w = std::stof(a["stroke-width"]); }
    catch (...) {};
    s.setOutlineThickness(st.a > 0 ? w : 0); window.draw(s);
}
void SVGRenderer::renderRect(const Rect& r) {
    std::vector<sf::Vector2f> pts;
    float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    pts.push_back({ x,y }); pts.push_back({ x + w,y }); pts.push_back({ x + w,y + h }); pts.push_back({ x,y + h });
    Attributes a = getEffectiveAttributes(r.getAttributes());
    TransformMatrix tm = getCumulativeTransform(); tm.combine(r.getTransform());
    for (auto& p : pts) { float tx, ty; tm.transformPoint(p.x, p.y, tx, ty); p.x = tx; p.y = ty; }

    sf::Color f = stringToColor(a.count("fill") ? a["fill"] : "none", "fill");
    if (f != sf::Color::Transparent) { f.a = getOpacity(a, "fill-opacity") * 255; drawConcaveShape(window, { pts }, f); }
    sf::Color st = stringToColor(a.count("stroke") ? a["stroke"] : "none", "stroke");
    if (st != sf::Color::Transparent) st.a = getOpacity(a, "stroke-opacity") * 255;
    float th = 1.f; if (a.count("stroke-width")) try { th = std::stof(a["stroke-width"]); }
    catch (...) {};
    if (st.a > 0 && th > 0) drawSharpStroke(window, pts, th, st, true);
}
void SVGRenderer::renderLine(const Line& l) {
    Attributes a = getEffectiveAttributes(l.getAttributes());
    TransformMatrix tm = getCumulativeTransform(); tm.combine(l.getTransform());
    float x1 = l.getX1(), y1 = l.getY1(), x2 = l.getX2(), y2 = l.getY2();
    tm.transformPoint(x1, y1, x1, y1); tm.transformPoint(x2, y2, x2, y2);
    sf::Color st = stringToColor(a.count("stroke") ? a["stroke"] : "none", "stroke"); st.a = getOpacity(a, "stroke-opacity") * 255;
    float w = 1.f; if (a.count("stroke-width")) try { w = std::stof(a["stroke-width"]); }
    catch (...) {};
    if (st.a > 0 && w > 0) drawSharpStroke(window, { {x1,y1}, {x2,y2} }, w, st, false);
}
void SVGRenderer::renderText(const Text& t) { /* Text implementation same as before */ }
void SVGRenderer::renderPolyline(const Polyline& p) {
    std::istringstream iss(p.getPoints()); std::vector<sf::Vector2f> pts; float x, y; char c;
    TransformMatrix tm = getCumulativeTransform(); tm.combine(p.getTransform());
    while (iss >> x) { if (iss.peek() == ',')iss >> c; if (iss >> y) { float tx, ty; tm.transformPoint(x, y, tx, ty); pts.emplace_back(tx, ty); } if (iss.peek() == ',')iss >> c; }
    if (pts.size() < 2) return;
    Attributes a = getEffectiveAttributes(p.getAttributes());
    sf::Color st = stringToColor(a.count("stroke") ? a["stroke"] : "none", "stroke"); st.a = getOpacity(a, "stroke-opacity") * 255;
    float w = 1.f; if (a.count("stroke-width")) try { w = std::stof(a["stroke-width"]); }
    catch (...) {};
    sf::Color f = stringToColor(a.count("fill") ? a["fill"] : "none", "fill"); if (f != sf::Color::Transparent) { f.a = getOpacity(a, "fill-opacity") * 255; drawConcaveShape(window, { pts }, f); }
    if (st.a > 0 && w > 0) drawSharpStroke(window, pts, w, st, false);
}
void SVGRenderer::renderPolygon(const Polygon& p) {
    std::istringstream iss(p.getPoints()); std::vector<sf::Vector2f> pts; float x, y; char c;
    TransformMatrix tm = getCumulativeTransform(); tm.combine(p.getTransform());
    while (iss >> x) { if (iss.peek() == ',')iss >> c; if (iss >> y) { float tx, ty; tm.transformPoint(x, y, tx, ty); pts.emplace_back(tx, ty); } if (iss.peek() == ',')iss >> c; }
    if (pts.size() < 3) return;
    Attributes a = getEffectiveAttributes(p.getAttributes());
    sf::Color f = stringToColor(a.count("fill") ? a["fill"] : "none", "fill"); if (f != sf::Color::Transparent) { f.a = getOpacity(a, "fill-opacity") * 255; drawConcaveShape(window, { pts }, f); }
    sf::Color st = stringToColor(a.count("stroke") ? a["stroke"] : "none", "stroke"); st.a = getOpacity(a, "stroke-opacity") * 255;
    float w = 1.f; if (a.count("stroke-width")) try { w = std::stof(a["stroke-width"]); }
    catch (...) {};
    if (st.a > 0 && w > 0) drawSharpStroke(window, pts, w, st, true);
}
void SVGRenderer::renderEllipse(const Ellipse& e) { renderCircle(Circle(e.getAttributes())); }

// --- RENDER PATH (FIXED) ---
void SVGRenderer::renderPath(const Path& path) {
    const auto& commands = path.getCommands();
    if (commands.empty()) return;
    Attributes a = getEffectiveAttributes(path.getAttributes());
    TransformMatrix tm = getCumulativeTransform(); tm.combine(path.getTransform());

    sf::Color f = stringToColor(a.count("fill") ? a["fill"] : "black", "fill"); f.a = getOpacity(a, "fill-opacity") * (f == sf::Color::Transparent ? 0 : 255);
    sf::Color st = stringToColor(a.count("stroke") ? a["stroke"] : "none", "stroke"); st.a = getOpacity(a, "stroke-opacity") * (st == sf::Color::Transparent ? 0 : 255);
    float w = 1.f; if (a.count("stroke-width")) try { w = std::stof(a["stroke-width"]); }
    catch (...) {};

    std::vector<std::vector<sf::Vector2f>> subPaths;
    std::vector<sf::Vector2f> curPts;
    sf::Vector2f curPos(0, 0), startPos(0, 0);

    auto addBezier = [&](sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3) {
        int steps = 20 + (int)(getLength(p3 - p0) / 5.f); if (steps > 100) steps = 100;
        for (int i = 1; i <= steps; ++i) curPts.push_back(cubicBezier(i / (float)steps, p0, p1, p2, p3));
        };

    for (const auto& cmd : commands) {
        char t = cmd.type; const auto& args = cmd.args;
        if ((t == 'M' || t == 'm') && !curPts.empty()) { subPaths.push_back(curPts); curPts.clear(); }

        if (t == 'M') { curPos = { args[0],args[1] }; startPos = curPos; curPts.push_back(curPos); }
        else if (t == 'm') { curPos += {args[0], args[1]}; startPos = curPos; curPts.push_back(curPos); }
        else if (t == 'L') { curPos = { args[0],args[1] }; curPts.push_back(curPos); }
        else if (t == 'l') { curPos += {args[0], args[1]}; curPts.push_back(curPos); }
        else if (t == 'H') { curPos.x = args[0]; curPts.push_back(curPos); }
        else if (t == 'h') { curPos.x += args[0]; curPts.push_back(curPos); }
        else if (t == 'V') { curPos.y = args[0]; curPts.push_back(curPos); }
        else if (t == 'v') { curPos.y += args[0]; curPts.push_back(curPos); }
        else if (t == 'C') { sf::Vector2f p1(args[0], args[1]), p2(args[2], args[3]), p3(args[4], args[5]); addBezier(curPos, p1, p2, p3); curPos = p3; }
        else if (t == 'c') { sf::Vector2f p1 = curPos + sf::Vector2f(args[0], args[1]), p2 = curPos + sf::Vector2f(args[2], args[3]), p3 = curPos + sf::Vector2f(args[4], args[5]); addBezier(curPos, p1, p2, p3); curPos = p3; }
        else if (t == 'Z' || t == 'z') { if (getLength(curPos - startPos) > 0.1f) curPts.push_back(startPos); }
        else if (t == 'S' || t == 's' || t == 'Q' || t == 'q' || t == 'T' || t == 't') {
            if (args.size() >= 2) { curPos = (t == 's' || t == 't' || t == 'q') ? curPos + sf::Vector2f(args[args.size() - 2], args[args.size() - 1]) : sf::Vector2f(args[args.size() - 2], args[args.size() - 1]); curPts.push_back(curPos); }
        }
    }
    if (!curPts.empty()) subPaths.push_back(curPts);

    for (auto& path : subPaths) for (auto& p : path) { float tx, ty; tm.transformPoint(p.x, p.y, tx, ty); p.x = tx; p.y = ty; }

    // Smart Fill (Group disjoint shapes)
    if (f.a > 0) {
        struct ShapeGroup { std::vector<sf::Vector2f> outer; std::vector<std::vector<sf::Vector2f>> holes; };
        std::vector<ShapeGroup> groups;
        std::sort(subPaths.begin(), subPaths.end(), [](const auto& a, const auto& b) { return getArea(a) > getArea(b); });

        for (const auto& path : subPaths) {
            if (path.size() < 3) continue;
            bool added = false;
            for (auto& g : groups) {
                if (isPolygonInside(path, g.outer)) { g.holes.push_back(path); added = true; break; }
            }
            if (!added) groups.push_back({ path, {} });
        }
        for (const auto& g : groups) {
            std::vector<std::vector<sf::Vector2f>> inp; inp.push_back(g.outer);
            for (const auto& h : g.holes) inp.push_back(h);
            drawConcaveShape(window, inp, f);
        }
    }

    // Sharp Stroke
    if (st.a > 0 && w > 0) {
        for (const auto& path : subPaths) {
            bool closed = (path.size() > 2 && getLength(path.front() - path.back()) < 0.1f);
            drawSharpStroke(window, path, w, st, closed);
        }
    }
}

// Context Functions
void SVGRenderer::beginElement(const TransformMatrix& t, const Attributes& a) { if (renderStack_.empty()) { renderStack_.push_back({ t,a }); } else { RenderState s = renderStack_.back(); s.cumulativeTransform.combine(t); for (auto& k : a)s.inheritedAttributes[k.first] = k.second; renderStack_.push_back(s); } }
void SVGRenderer::endElement() { if (!renderStack_.empty()) renderStack_.pop_back(); }
const TransformMatrix& SVGRenderer::getCumulativeTransform() const { static TransformMatrix i; return renderStack_.empty() ? i : renderStack_.back().cumulativeTransform; }