#include "SVGRenderer.h"
#include "SVGElement.h" // Cần include file này để lấy định nghĩa đầy đủ của các lớp con
#include <iostream>
#include <sstream> // Dùng cho renderPolygon và stringToColor
#include <vector>
#include <cctype>    // Dùng cho ::tolower
#include <cmath>     // Dùng cho sqrt, atan2
#include <algorithm> // Dùng cho std::max
// Dùng std::cout, std::endl, v.v.
using namespace std;

// --- Hàm Tiện Ích Chung ---

/**
 * @brief Tự định nghĩa clamp (thay thế std::clamp cho C++ cũ)
 */
template <typename T>
constexpr const T manual_clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

/**
 * @brief Hàm tiện ích chuyển đổi chuỗi màu SVG sang màu SFML (ĐÃ SỬA RGB)
 */
sf::Color SVGRenderer::stringToColor(std::string colorStr, std::string type) {
    std::transform(colorStr.begin(), colorStr.end(), colorStr.begin(), ::tolower);

    if (colorStr == "none") return sf::Color::Transparent;
    if (colorStr == "black") return sf::Color::Black;
    if (colorStr == "white") return sf::Color::White;
    if (colorStr == "red") return sf::Color::Red;
    if (colorStr == "green") return sf::Color::Green;
    if (colorStr == "blue") return sf::Color::Blue;

    // --- Xử lý định dạng RGB: rgb(R,G,B) ---
    if (colorStr.rfind("rgb(", 0) == 0) {
        // Cắt bỏ "rgb(" ở đầu và ")" ở cuối
        std::string rgb_content = colorStr.substr(4, colorStr.length() - 5);

        std::stringstream ss(rgb_content);
        std::string segment;
        std::vector<int> colors;

        while (std::getline(ss, segment, ',')) {
            try {
                colors.push_back(std::stoi(segment));
            }
            catch (...) {
                return sf::Color::Black; // Lỗi cú pháp
            }
        }

        if (colors.size() == 3) {
            // SỬA LỖI: Dùng unsigned char và std::min/std::max
            unsigned char r = (unsigned char)std::min(255, std::max(0, colors[0]));
            unsigned char g = (unsigned char)std::min(255, std::max(0, colors[1]));
            unsigned char b = (unsigned char)std::min(255, std::max(0, colors[2]));
            return sf::Color(r, g, b);
        }
    }

    // --- Xử lý mã Hex (ví dụ: #FF0000) ---
    if (colorStr.length() > 0 && colorStr[0] == '#') {
        try {
            unsigned int hexValue = std::stoul(colorStr.substr(1), nullptr, 16);
            if (colorStr.length() == 7) {
                return sf::Color(
                    (hexValue >> 16) & 0xFF,
                    (hexValue >> 8) & 0xFF,
                    (hexValue) & 0xFF
                );
            }
        }
        catch (...) {}
    }

    // Mặc định
    if (type == "fill") return sf::Color::Black;
    if (type == "stroke") return sf::Color::Transparent;

    return sf::Color::Magenta;
}

/**
 * @brief Hàm tiện ích áp dụng Opacity (Alpha) cho màu
 */
static sf::Color applyOpacity(sf::Color color, const Attributes& attributes, const std::string& opacityAttribute) {
    auto it_opacity = attributes.find(opacityAttribute);
    if (it_opacity != attributes.end()) {
        try {
            float opacity = std::stof(it_opacity->second);

            // Chuyển 0.0-1.0 sang 0-255 và dùng manual_clamp
            unsigned char alpha = (unsigned char)manual_clamp(opacity * 255.0f, 0.0f, 255.0f);

            color.a = alpha;
        }
        catch (...) {}
    }
    return color;
}


// --- Constructor ---
SVGRenderer::SVGRenderer(unsigned int width, unsigned int height) {
    window.create(sf::VideoMode({ width, height }), "SVG Renderer (SFML)");
    view = window.getDefaultView();
    cout << "SVGRenderer: Window created " << width << "x" << height << std::endl;
    cout << "Controls: Mouse Wheel = Zoom, R = Rotate, ESC = Exit" << std::endl;
}

// --- Add Element ---
void SVGRenderer::addElement(std::shared_ptr<SVGElement> element) {
    if (element) {
        elements.push_back(element);
    }
}

// --- Zoom & Rotate ---
void SVGRenderer::zoomIn() {
    view.zoom(0.9f);
    cout << "Zoomed In" << std::endl;
}

void SVGRenderer::zoomOut() {
    view.zoom(1.1f);
    cout << "Zoomed Out" << std::endl;
}

void SVGRenderer::rotate(float angle) {
    view.rotate(sf::degrees(angle));
    cout << "Rotated by " << angle << " degrees" << std::endl;
}

// --- Render Loop ---
void SVGRenderer::render() {
    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
                cout << "Window closed by user" << std::endl;
            }
            else if (auto* keyEvent = event->getIf<sf::Event::KeyPressed>()) {
                if (keyEvent->scancode == sf::Keyboard::Scancode::Escape) {
                    window.close();
                    cout << "Window closed by ESC" << std::endl;
                }
                else if (keyEvent->scancode == sf::Keyboard::Scancode::R) {
                    rotate(10.f);
                }
            }
            else if (auto* wheelEvent = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (wheelEvent->delta > 0) zoomIn();
                else zoomOut();
            }
        }

        window.clear(sf::Color::White);
        window.setView(view);

        for (auto& element : elements) {
            element->draw(*this);
        }

        window.display();
    }
    cout << "Render loop ended" << std::endl;
}

// --- Render Circle (ĐÃ THÊM OPACITY) ---
void SVGRenderer::renderCircle(const Circle& circle) {
    float r = static_cast<float>(circle.getR());
    sf::CircleShape shape(r);

    shape.setPosition(sf::Vector2f(
        static_cast<float>(circle.getCx() - r),
        static_cast<float>(circle.getCy() - r)
    ));

    const auto& attributes = circle.getAttributes();

    // 1. FILL COLOR & OPACITY
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";

    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    fillColor = applyOpacity(fillColor, attributes, "fill-opacity"); // Áp dụng opacity
    shape.setFillColor(fillColor);

    // 2. STROKE COLOR & OPACITY
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";

    sf::Color strokeColor = stringToColor(stroke_color_str, "stroke");
    strokeColor = applyOpacity(strokeColor, attributes, "stroke-opacity"); // Áp dụng opacity
    shape.setOutlineColor(strokeColor);

    // 3. STROKE WIDTH
    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f;
    if (it_width != attributes.end()) {
        try {
            thickness = std::stof(it_width->second);
        }
        catch (...) { /* giữ giá trị mặc định */ }
    }

    // Nếu màu là none hoặc alpha=0, set thickness = 0
    shape.setOutlineThickness((stroke_color_str == "none" || strokeColor.a == 0) ? 0.f : thickness);

    window.draw(shape);
}

// --- Render Rect (ĐÃ THÊM OPACITY) ---
void SVGRenderer::renderRect(const Rect& rect) {
    sf::RectangleShape shape(sf::Vector2f(
        static_cast<float>(rect.getWidth()),
        static_cast<float>(rect.getHeight())
    ));

    shape.setPosition(sf::Vector2f(
        static_cast<float>(rect.getX()),
        static_cast<float>(rect.getY())
    ));

    const auto& attributes = rect.getAttributes();

    // 1. FILL COLOR & OPACITY
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";

    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    fillColor = applyOpacity(fillColor, attributes, "fill-opacity"); // Áp dụng opacity
    shape.setFillColor(fillColor);

    // 2. STROKE COLOR & OPACITY
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";

    sf::Color strokeColor = stringToColor(stroke_color_str, "stroke");
    strokeColor = applyOpacity(strokeColor, attributes, "stroke-opacity"); // Áp dụng opacity
    shape.setOutlineColor(strokeColor);

    // 3. STROKE WIDTH
    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f;
    if (it_width != attributes.end()) {
        try {
            thickness = std::stof(it_width->second);
        }
        catch (...) { /* giữ giá trị mặc định */ }
    }

    shape.setOutlineThickness((stroke_color_str == "none" || strokeColor.a == 0) ? 0.f : thickness);

    window.draw(shape);
}

// --- Render Line (Giữ nguyên logic Line hiện tại) ---
void SVGRenderer::renderLine(const Line& line) {
    float x1 = static_cast<float>(line.getX1());
    float y1 = static_cast<float>(line.getY1());
    float x2 = static_cast<float>(line.getX2());
    float y2 = static_cast<float>(line.getY2());

    const auto& attributes = line.getAttributes();

    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f;
    if (it_width != attributes.end()) {
        try {
            thickness = std::stof(it_width->second);
        }
        catch (...) { /* giữ giá trị mặc định */ }
    }

    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "black";
    sf::Color stroke_color = stringToColor(stroke_color_str, "stroke");

    // Áp dụng stroke-opacity cho Line
    stroke_color = applyOpacity(stroke_color, attributes, "stroke-opacity");

    if (stroke_color == sf::Color::Transparent || thickness <= 0.f) {
        return;
    }

    // SFML không có Line, ta dùng RectangleShape
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = std::sqrt(dx * dx + dy * dy);
    float angle = std::atan2(dy, dx) * 180.f / 3.14159f;

    sf::RectangleShape lineShape(sf::Vector2f(length, thickness));
    lineShape.setPosition(sf::Vector2f(x1, y1));
    lineShape.setRotation(sf::degrees(angle));
    lineShape.setFillColor(stroke_color);

    window.draw(lineShape);
}

// --- Render Polygon ---
void SVGRenderer::renderPolygon(const Polygon& polygon) {
    // 1. Phân tích chuỗi "points"
    std::istringstream iss(polygon.getPoints());
    std::vector<sf::Vector2f> points;
    float x, y;
    char comma;

    while (iss >> x) {
        if (iss.peek() == ',') iss >> comma;
        if (iss >> y) {
            points.emplace_back(x, y);
        }
        else break;
    }

    if (points.size() < 3) return;

    // 2. Tạo hình dạng SFML (ConvexShape)
    sf::ConvexShape shape;
    shape.setPointCount(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
        shape.setPoint(i, points[i]);
    }

    // 3. Áp dụng Thuộc tính 
    const auto& attributes = polygon.getAttributes();

    // Fill & Opacity
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";
    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    fillColor = applyOpacity(fillColor, attributes, "fill-opacity");
    shape.setFillColor(fillColor);

    // Stroke & Opacity
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";
    sf::Color strokeColor = stringToColor(stroke_color_str, "stroke");
    strokeColor = applyOpacity(strokeColor, attributes, "stroke-opacity");
    shape.setOutlineColor(strokeColor);

    // Stroke Width
    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f;
    if (it_width != attributes.end()) {
        try {
            thickness = std::stof(it_width->second);
        }
        catch (...) { /* giữ giá trị mặc định */ }
    }
    shape.setOutlineThickness((stroke_color_str == "none" || strokeColor.a == 0) ? 0.f : thickness);

    window.draw(shape);
}

// --- Render Path (Giữ nguyên) ---
void SVGRenderer::renderPath(const Path& path) {
    const auto& attributes = path.getAttributes();

    // Vẽ hình chữ nhật đại diện
    sf::RectangleShape shape(sf::Vector2f(50.f, 50.f));
    shape.setPosition(sf::Vector2f(0.f, 0.f)); // Đặt ở góc

    // Fill
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";
    shape.setFillColor(stringToColor(fill_color_str, "fill"));

    // Thêm một viền màu đỏ để báo hiệu đây là "Path giả"
    shape.setOutlineColor(sf::Color::Red);
    shape.setOutlineThickness(5.f);

    window.draw(shape);
}

// --- Render Ellipse (ĐÃ THÊM OPACITY) ---
void SVGRenderer::renderEllipse(const Ellipse& ellipse) {
    float rx = static_cast<float>(ellipse.getRx());
    float ry = static_cast<float>(ellipse.getRy());
    float cx = static_cast<float>(ellipse.getCx());
    float cy = static_cast<float>(ellipse.getCy());

    float radius = std::max(rx, ry);

    sf::CircleShape shape(radius);

    shape.setOrigin(sf::Vector2f(radius, radius));
    shape.setPosition(sf::Vector2f(cx, cy));

    if (radius > 0.0f) {
        shape.setScale(sf::Vector2f(rx / radius, ry / radius));
    }
    else {
        shape.setScale(sf::Vector2f(0.0f, 0.0f));
    }

    const auto& attributes = ellipse.getAttributes();

    // 1. FILL COLOR & OPACITY
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";

    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    fillColor = applyOpacity(fillColor, attributes, "fill-opacity"); // Áp dụng opacity
    shape.setFillColor(fillColor);

    // 2. STROKE COLOR & OPACITY
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";

    sf::Color strokeColor = stringToColor(stroke_color_str, "stroke");
    strokeColor = applyOpacity(strokeColor, attributes, "stroke-opacity"); // Áp dụng opacity
    shape.setOutlineColor(strokeColor);

    // 3. STROKE WIDTH
    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f;
    if (it_width != attributes.end()) {
        try {
            thickness = std::stof(it_width->second);
        }
        catch (...) { /* giữ giá trị mặc định */ }
    }

    shape.setOutlineThickness((stroke_color_str == "none" || strokeColor.a == 0) ? 0.f : thickness);

    window.draw(shape);
}

// --- Hàm tiện ích nội bộ (giữ nguyên) ---
void SVGRenderer::drawLineBetweenPoints(const sf::Vector2f& p1, const sf::Vector2f& p2, const sf::Color& color) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float length = std::sqrt(dx * dx + dy * dy);
    float angle = std::atan2(dy, dx) * 180.f / 3.14159f;

    if (length > 0) {
        sf::RectangleShape lineShape(sf::Vector2f(length, 1.f));
        lineShape.setPosition(p1);
        lineShape.setRotation(sf::degrees(angle));
        lineShape.setFillColor(color);
        window.draw(lineShape);
    }
}