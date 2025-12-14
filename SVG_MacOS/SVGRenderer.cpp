// SVGRenderer.cpp
#include "SVGRenderer.h"
#include "SVGElement.h" // Cần include file này để lấy định nghĩa đầy đủ của các lớp con
#include <iostream>
#include <sstream> // Dùng cho renderPolygon
#include <vector>
#include <cctype>    // Dùng cho ::tolower
#include <cmath>     // Dùng cho sqrt, atan2
#include <algorithm> // Dùng cho std::max
#include <cstdint>   // Dùng uint8_t
#include "Transform.h"
#include "SVGPath.h"
#include <list>         // Dùng cho thuật toán cắt tai

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// 1. Tính diện tích để xác định chiều xoay
float signedArea(const std::vector<sf::Vector2f>& points) {
    float area = 0.0f;
    if (points.empty()) return 0.0f;
    for (size_t i = 0; i < points.size(); ++i) {
        size_t j = (i + 1) % points.size();
        area += (points[i].x * points[j].y - points[j].x * points[i].y);
    }
    return area / 2.0f;
}

// 2. Kiểm tra điểm trong đa giác (Ray Casting)
bool isPointInPolygon(const sf::Vector2f& p, const std::vector<sf::Vector2f>& polygon) {
    bool inside = false;
    if (polygon.empty()) return false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        if (((polygon[i].y > p.y) != (polygon[j].y > p.y)) &&
            (p.x < (polygon[j].x - polygon[i].x) * (p.y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

// 3. Kiểm tra điểm trong tam giác
bool isPointInTriangle(const sf::Vector2f& p, const sf::Vector2f& a, const sf::Vector2f& b, const sf::Vector2f& c) {
    auto sign = [](const sf::Vector2f& p1, const sf::Vector2f& p2, const sf::Vector2f& p3) {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };
    float d1 = sign(p, a, b);
    float d2 = sign(p, b, c);
    float d3 = sign(p, c, a);
    bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

// 4. Kiểm tra tai (Ear Clipping)
bool isEar(const std::list<sf::Vector2f>& polygon, std::list<sf::Vector2f>::const_iterator vit) {
    if (polygon.size() < 3) return false;
    auto prev = (vit == polygon.begin()) ? std::prev(polygon.end()) : std::prev(vit);
    auto next = (vit == std::prev(polygon.end())) ? polygon.begin() : std::next(vit);
    const sf::Vector2f& a = *prev;
    const sf::Vector2f& b = *vit;
    const sf::Vector2f& c = *next;

    // Check lồi/lõm bằng cross product (đơn giản hóa)
    // float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);

    for (auto it = polygon.begin(); it != polygon.end(); ++it) {
        if (it == prev || it == vit || it == next) continue;
        if (isPointInTriangle(*it, a, b, c)) return false;
    }
    return true;
}

// 5. Thuật toán cắt đa giác thành tam giác (Triangulation)
std::vector<sf::Vector2f> triangulatePolygon(const std::vector<sf::Vector2f>& points) {
    std::vector<sf::Vector2f> triangles;
    if (points.size() < 3) return triangles;
    std::list<sf::Vector2f> polyList(points.begin(), points.end());
    
    // Chuẩn hóa chiều xoay
    if (signedArea(points) < 0) polyList.reverse();

    int safety = points.size() * points.size() * 2;
    auto it = polyList.begin();
    while (polyList.size() >= 3 && safety-- > 0) {
        if (isEar(polyList, it)) {
            auto prev = (it == polyList.begin()) ? std::prev(polyList.end()) : std::prev(it);
            auto next = (it == std::prev(polyList.end())) ? polyList.begin() : std::next(it);
            triangles.push_back(*prev); triangles.push_back(*it); triangles.push_back(*next);
            it = polyList.erase(it);
            if (it == polyList.end()) it = polyList.begin();
        } else {
            ++it;
            if (it == polyList.end()) it = polyList.begin();
        }
    }
    if (polyList.size() == 3) { for(const auto& p : polyList) triangles.push_back(p); }
    return triangles;
}

// 6. Tính diện tích hộp bao (để sắp xếp Layer)
float getBoundingBoxArea(const std::vector<sf::Vector2f>& pts) {
    if (pts.empty()) return 0.0f;
    float minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
    for (const auto& p : pts) {
        if (p.x < minX) minX = p.x; if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y; if (p.y > maxY) maxY = p.y;
    }
    return (maxX - minX) * (maxY - minY);
}

// Hàm giúp đọc giá trị opacity (mặc định là 1.0 - không trong suốt)
float getOpacity(const std::map<std::string, std::string> &attrs, std::string key)
{
    auto it = attrs.find(key);
    if (it != attrs.end())
    {
        try
        {
            return std::stof(it->second);
        }
        catch (...)
        {
        }
    }
    return 1.0f; // Mặc định là rõ nét (100%)
}

// Hàm tiện ích chuyển đổi chuỗi màu SVG sang màu SFML
// SVGRenderer.cpp
sf::Color parseColor(std::string colorStr) {
    // 1. Xóa khoảng trắng thừa
    // (Giả sử bạn đã có hàm trim, nếu chưa thì dùng hàm trim trong SVGParser)  
    if (colorStr.empty()) return sf::Color::Black; // Mặc định SVG

    // 2. QUAN TRỌNG: Xử lý "none" hoặc "transparent"
    if (colorStr == "none" || colorStr == "transparent") {
        return sf::Color::Transparent;
    }

    // 3. Xử lý mã Hex (#RRGGBB)
    if (colorStr[0] == '#') {
        colorStr.erase(0, 1);
        unsigned int hexVal = 0;
        std::stringstream ss;
        ss << std::hex << colorStr;
        ss >> hexVal;
        
        if (colorStr.length() == 6) {
            return sf::Color((hexVal >> 16) & 0xFF, (hexVal >> 8) & 0xFF, hexVal & 0xFF);
        }
        // Xử lý 3 ký tự (#RGB) nếu cần...
    }

    // 4. Xử lý tên màu cơ bản (fallback)
    if (colorStr == "red") return sf::Color::Red;
    if (colorStr == "blue") return sf::Color::Blue;
    if (colorStr == "black") return sf::Color::Black;
    if (colorStr == "white") return sf::Color::White;
    
    return sf::Color::Black; // Mặc định nếu không hiểu
}

// [CẬP NHẬT] Hàm chuyển đổi màu mạnh mẽ hơn
sf::Color SVGRenderer::stringToColor(std::string colorStr, std::string type)
{
    // 1. Chuẩn hóa chuỗi (xóa khoảng trắng, chuyển thường)
    std::string s = colorStr;
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    // 2. Xử lý "none" và "transparent" ngay lập tức
    if (s == "none" || s == "transparent")
        return sf::Color::Transparent;

    // 3. Bảng màu mở rộng (SVG Colors) - Khắc phục lỗi thiếu màu
    static const std::map<std::string, sf::Color> colors = {
        {"black", sf::Color::Black}, {"white", sf::Color::White}, {"red", sf::Color::Red}, {"green", sf::Color::Green}, {"blue", sf::Color::Blue}, {"yellow", sf::Color::Yellow}, {"magenta", sf::Color::Magenta}, {"cyan", sf::Color::Cyan}, {"gray", sf::Color(128, 128, 128)}, {"grey", sf::Color(128, 128, 128)}, {"silver", sf::Color(192, 192, 192)}, {"orange", sf::Color(255, 165, 0)}, {"purple", sf::Color(128, 0, 128)}, {"maroon", sf::Color(128, 0, 0)}, {"lime", sf::Color(0, 255, 0)}, {"olive", sf::Color(128, 128, 0)}, {"navy", sf::Color(0, 0, 128)}, {"teal", sf::Color(0, 128, 128)}, {"aqua", sf::Color::Cyan}, {"fuchsia", sf::Color::Magenta},
        // Các màu nhạt (Light colors)
        {"lightblue", sf::Color(173, 216, 230)},
        {"lightgreen", sf::Color(144, 238, 144)},
        {"lightgray", sf::Color(211, 211, 211)},
        {"lightgrey", sf::Color(211, 211, 211)},
        {"pink", sf::Color(255, 192, 203)},
        {"gold", sf::Color(255, 215, 0)}};

    auto it = colors.find(s);
    if (it != colors.end())
        return it->second;

    // 4. Mã Hex (#RRGGBB hoặc #RGB)
    if (!s.empty() && s[0] == '#')
    {
        s.erase(0, 1);
        if (s.size() == 3)
        { // #F00 -> #FF0000
            std::string temp;
            for (char c : s)
            {
                temp += c;
                temp += c;
            }
            s = temp;
        }
        if (s.size() >= 6)
        { // Xử lý cả #RRGGBBAA nếu có
            unsigned int hexValue;
            std::stringstream ss;
            ss << std::hex << s;
            ss >> hexValue;
            if (s.size() == 8) // Có Alpha
                return sf::Color((hexValue >> 24) & 0xFF, (hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);
            else
                return sf::Color((hexValue >> 16) & 0xFF, (hexValue >> 8) & 0xFF, hexValue & 0xFF);
        }
    }

    // 5. Mã RGB (rgb(255,0,0))
    if (s.find("rgb(") == 0)
    {
        size_t start = 4;
        size_t end = s.find(')', start);
        if (end != std::string::npos)
        {
            std::string content = s.substr(start, end - start);
            std::replace(content.begin(), content.end(), ',', ' ');
            std::stringstream ss(content);
            int r, g, b;
            ss >> r >> g >> b;
            return sf::Color(r, g, b);
        }
    }

    // [QUAN TRỌNG] Nếu không tìm thấy màu (lỗi parse), trả về Transparent thay vì Black
    // Điều này giúp các hình không bị đen xì nếu gặp màu lạ.
    // Trừ khi type là fill và không có khai báo gì thì SVG mặc định là black (nhưng ta nên để transparent cho đẹp)
    return sf::Color::Transparent;
}
// Hàm tính toạ độ Y trên đường chéo tại vị trí X cho trước
float getYOnDiagonal(float x, sf::Vector2f pStart, sf::Vector2f pEnd)
{
    if (std::abs(pEnd.x - pStart.x) < 0.001f)
        return pStart.y;                                 // Tránh chia cho 0
    float m = (pEnd.y - pStart.y) / (pEnd.x - pStart.x); // Hệ số góc
    return m * (x - pStart.x) + pStart.y;
}

// Hàm tính toạ độ X trên đường chéo tại vị trí Y cho trước
float getXOnDiagonal(float y, sf::Vector2f pStart, sf::Vector2f pEnd)
{
    if (std::abs(pEnd.y - pStart.y) < 0.001f)
        return pStart.x;
    float m = (pEnd.x - pStart.x) / (pEnd.y - pStart.y); // Hệ số góc đảo
    return m * (y - pStart.y) + pStart.x;
}
// --- Constructor ---
SVGRenderer::SVGRenderer(unsigned int width, unsigned int height) {
    // Sửa 1: Tăng kích thước cửa sổ lên 1200x800 để bao trọn hình vẽ
    window.create(sf::VideoMode({1200, 800}), "SVG Renderer (Fixed)");
    
    // Sửa 2: Đặt View camera lùi lại một chút và dịch sang phải để nhìn bao quát hơn
    view = window.getDefaultView();
    // (Tuỳ chọn) Bạn có thể zoom out mặc định nếu muốn thấy nhiều hơn:
    // view.zoom(1.2f); 

    if (!font.openFromFile("times.ttf")) {
         std::cerr << "Canh bao: Khong load duoc font times.ttf" << std::endl;
    }
    std::cout << "SVGRenderer Created." << std::endl;
}

// --- Add Element ---
void SVGRenderer::addElement(std::shared_ptr<SVGElement> element)
{
    if (element)
    {
        elements.push_back(element);
    }
}

// --- Zoom & Rotate ---
void SVGRenderer::zoomIn()
{
    view.zoom(0.9f);
    std::cout << "Zoomed In" << std::endl;
}

void SVGRenderer::zoomOut()
{
    view.zoom(1.1f);
    std::cout << "Zoomed Out" << std::endl;
}

void SVGRenderer::rotate(float angle)
{
    view.rotate(sf::degrees(angle));
    std::cout << "Rotated by " << angle << " degrees" << std::endl;
}

// --- Hàm tiện ích áp dụng TransformMatrix lên sf::Transform ---
sf::Transform SVGRenderer::getSFMLTransform(const TransformMatrix &matrix) const
{
    // Truy cập trực tiếp vào m[6] của TransformMatrix (vì TransformMatrix khai báo friend class SVGRenderer)
    const float *m = matrix.m;

    // return sf::Transform(
    //     m[0], m[3], m[2], // Hàng 1 (X)
    //     m[1], m[4], m[5], // Hàng 2 (Y)
    //     0.0f, 0.0f, 1.0f  // Hàng 3 (W)
    // );

    return sf::Transform(
        m[0], m[1], m[2], // Giữ nguyên m[1] ở hàng 1
        m[3], m[4], m[5], // Giữ nguyên m[3] ở hàng 2
        0.0f, 0.0f, 1.0f);
}

// --- Render Loop ---
void SVGRenderer::render()
{
    while (window.isOpen())
    {
        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
                std::cout << "Window closed by user" << std::endl;
            }
            else if (auto *keyEvent = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyEvent->scancode == sf::Keyboard::Scancode::Escape)
                {
                    window.close();
                    std::cout << "Window closed by ESC" << std::endl;
                }
                else if (keyEvent->scancode == sf::Keyboard::Scancode::L)
                {
                    rotate(10.f);
                }
                else if (keyEvent->scancode == sf::Keyboard::Scancode::R)
                {
                    rotate(-10.f);
                }
            }
            else if (auto *wheelEvent = event->getIf<sf::Event::MouseWheelScrolled>())
            {
                if (wheelEvent->delta > 0)
                    zoomIn();
                else
                    zoomOut();
            }
        }

        window.clear(sf::Color::White); // Xóa màn hình
        window.setView(view);           // Áp dụng camera

        // Vẽ tất cả các phần tử (ĐA HÌNH)
        for (auto &element : elements)
        {
            element->draw(*this); // Tự động gọi renderCircle, renderRect...
        }

        window.display(); // Hiển thị lên màn hình
    }
    std::cout << "Render loop ended" << std::endl;
}

// --- Render Circle ---
void SVGRenderer::renderCircle(const Circle &circle)
{
    float r = static_cast<float>(circle.getR());
    sf::CircleShape shape(r);

    // SVG (cx, cy) là tâm, SFML (x, y) là góc trên trái
    // Dùng sf::Vector2f cho SFML 3.0
    shape.setPosition(sf::Vector2f(
        static_cast<float>(circle.getCx() - r),
        static_cast<float>(circle.getCy() - r)));

    // Thay vì circle.getAttributes(), ta dùng hàm getEffectiveAttributes
    Attributes attributes = getEffectiveAttributes(circle.getAttributes());

    // Lấy ma trận tích lũy từ Stack (của Group cha)
    TransformMatrix finalMatrix = getCumulativeTransform();

    // Kết hợp với ma trận riêng của Circle
    finalMatrix.combine(circle.getTransform());

    // --- ÁP DỤNG TRANSFORM ---
    sf::Transform transform = getSFMLTransform(finalMatrix);

    // --- XỬ LÝ MÀU FILL & OPACITY ---
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "none";
    // 1. KHAI BÁO BIẾN TRƯỚC (Sửa lỗi undeclared identifier)
    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    // 2. Lấy độ trong suốt
    float fillOpacity = getOpacity(attributes, "fill-opacity");

    // // 3. GÁN ALPHA (Sửa lỗi sf::Uint8 thành std::uint8_t)
    if (fill_color_str != "none" && fill_color_str != "transparent")
    {
        fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);
    }
    else
    {
        // Nếu là 'transparent', đảm bảo alpha vẫn là 0 (Không bị override bởi opacity mặc định 1.0)
        fillColor.a = 0;
    }

    shape.setFillColor(fillColor);

    // --- XỬ LÝ MÀU STROKE & OPACITY ---
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";

    // 1. KHAI BÁO BIẾN TRƯỚC
    sf::Color strokeColor = stringToColor(stroke_color_str, "stroke");

    // 2. Lấy độ trong suốt
    float strokeOpacity = getOpacity(attributes, "stroke-opacity");

    // 3. GÁN ALPHA (Sửa lỗi sf::Uint8 thành std::uint8_t)
    strokeColor.a = static_cast<std::uint8_t>(strokeOpacity * 255);

    shape.setOutlineColor(strokeColor);

    // --- XỬ LÝ ĐỘ DÀY VIỀN ---
    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f;
    if (it_width != attributes.end())
    {
        try
        {
            thickness = std::stof(it_width->second);
        }
        catch (...)
        {
            thickness = 1.f;
        }
    }
    shape.setOutlineThickness((stroke_color_str == "none") ? 0.f : thickness);

    window.draw(shape, transform);
}

// --- Render Rect ---
void SVGRenderer::renderRect(const Rect &rect)
{
    sf::RectangleShape shape(sf::Vector2f(
        static_cast<float>(rect.getWidth()),
        static_cast<float>(rect.getHeight())));

    // SVG (x,y) là góc trên trái, giống SFML
    shape.setPosition(sf::Vector2f(
        static_cast<float>(rect.getX()),
        static_cast<float>(rect.getY())));

    // Nếu Rect không có màu fill, nó sẽ tự tìm trong attributes của Group cha
    Attributes attributes = getEffectiveAttributes(rect.getAttributes());

    // Lấy vị trí/xoay của Group cha
    TransformMatrix finalMatrix = getCumulativeTransform();

    // Nhân thêm vị trí/xoay của chính hình chữ nhật này
    finalMatrix.combine(rect.getTransform());

    // --- ÁP DỤNG TRANSFORM ---
    sf::Transform transform = getSFMLTransform(finalMatrix);

    // Fill
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "none";

    // 1. KHAI BÁO biến fillColor (Sửa lỗi undeclared identifier)
    sf::Color fillColor = stringToColor(fill_color_str, "fill");

    float fillOpacity = getOpacity(attributes, "fill-opacity");

    if (fill_color_str == "none" || fill_color_str == "transparent")
    {
        fillColor.a = 0;
    }
    else
    {
        // Ngược lại, áp dụng độ mờ (opacity) lên Alpha (255)
        fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);
    }
    shape.setFillColor(fillColor);

    // --- XỬ LÝ MÀU STROKE & STROKE-OPACITY ---
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";

    // 1. KHAI BÁO biến strokeColor
    sf::Color strokeColor = stringToColor(stroke_color_str, "stroke");

    float strokeOpacity = getOpacity(attributes, "stroke-opacity");

    // 2. SỬA LỖI Uint8 -> std::uint8_t
    strokeColor.a = static_cast<std::uint8_t>(strokeOpacity * 255);

    shape.setOutlineColor(strokeColor);

    // Stroke Width
    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f;
    if (it_width != attributes.end())
    {
        try
        {
            thickness = std::stof(it_width->second);
        }
        catch (...)
        { /* giữ giá trị mặc định */
        }
    }
    shape.setOutlineThickness((stroke_color_str == "none") ? 0.f : thickness);

    window.draw(shape, transform);
}

// --- Render Line ---
void SVGRenderer::renderLine(const Line &line)
{
    float x1 = static_cast<float>(line.getX1());
    float y1 = static_cast<float>(line.getY1());
    float x2 = static_cast<float>(line.getX2());
    float y2 = static_cast<float>(line.getY2());

    // Lấy thuộc tính gộp (Cha + Con)
    Attributes attributes = getEffectiveAttributes(line.getAttributes());

    // Tính toán Transform tổng hợp (Group * Local)
    TransformMatrix finalMatrix = getCumulativeTransform();
    finalMatrix.combine(line.getTransform());

    // Áp dụng Transform lên CÁC ĐIỂM ĐẦU MÚT
    // Vì ta vẽ Line bằng cách nối 2 điểm, ta cần biến đổi tọa độ 2 điểm này trước
    float t_x1, t_y1, t_x2, t_y2;
    finalMatrix.transformPoint(x1, y1, t_x1, t_y1);
    finalMatrix.transformPoint(x2, y2, t_x2, t_y2);

    // Cập nhật lại tọa độ để tính toán hình học bên dưới
    x1 = t_x1;
    y1 = t_y1;
    x2 = t_x2;
    y2 = t_y2;

    // 1. XỬ LÝ ĐỘ DÀY
    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f;
    if (it_width != attributes.end())
    {
        try
        {
            thickness = std::stof(it_width->second);
        }
        catch (...)
        {
            thickness = 1.f;
        }
    }

    // 2. XỬ LÝ MÀU
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";
    sf::Color strokeColor = stringToColor(stroke_color_str, "stroke");
    float strokeOpacity = getOpacity(attributes, "stroke-opacity");
    strokeColor.a = static_cast<std::uint8_t>(strokeOpacity * 255);

    if (strokeColor.a == 0 || thickness <= 0.f)
        return;

    // 3. TÍNH TOÁN HÌNH HỌC
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = std::sqrt(dx * dx + dy * dy);
    float angle = std::atan2(dy, dx) * 180.f / 3.14159f;

    // 4. TẠO HÌNH & CHỈNH TÂM
    sf::RectangleShape lineShape(sf::Vector2f(length, thickness));

    lineShape.setOrigin(sf::Vector2f(0.f, thickness / 2.f));

    lineShape.setPosition(sf::Vector2f(x1, y1));
    lineShape.setRotation(sf::degrees(angle));
    lineShape.setFillColor(strokeColor);

    window.draw(lineShape);
}

// --- Render Polygon ---
void SVGRenderer::renderPolygon(const Polygon &polygon)
{
    // 1. Phân tích chuỗi "points" (Ví dụ: "10,10 20,20 30,10")
    std::istringstream iss(polygon.getPoints());
    std::vector<sf::Vector2f> points;
    float x, y;
    char comma;

    // [CHUẨN] Tính ma trận tổng hợp (Group * Local)
    TransformMatrix finalMatrix = getCumulativeTransform();
    finalMatrix.combine(polygon.getTransform());
    float t_x, t_y; // Tọa độ đã transform

    while (iss >> x)
    {
        // Bỏ qua dấu phẩy giữa x và y (nếu có)
        if (iss.peek() == ',')
            iss >> comma;

        // Đọc y
        if (iss >> y)
        {
            // Áp dụng Ma trận tổng hợp lên điểm (x, y)
            finalMatrix.transformPoint(x, y, t_x, t_y);
            points.emplace_back(t_x, t_y);
        }
        else
            break;

        // Bỏ qua dấu phẩy sau cặp tọa độ nếu có (cho cặp tiếp theo)
        if (iss.peek() == ',')
            iss >> comma;
    }

    // Cần ít nhất 3 điểm để tạo thành đa giác
    if (points.size() < 3)
        return;

    // 2. Tạo hình dạng SFML (ConvexShape)
    sf::ConvexShape shape;
    shape.setPointCount(points.size());
    for (size_t i = 0; i < points.size(); ++i)
    {
        shape.setPoint(i, points[i]); // points[i] đã là sf::Vector2f
    }

    // Lấy thuộc tính gộp (Cha + Con)
    Attributes attributes = getEffectiveAttributes(polygon.getAttributes());

    // --- 3. XỬ LÝ MÀU FILL & OPACITY ---
    auto it_fill = attributes.find("fill");
    // Mặc định là màu đen (chuẩn SVG) thay vì màu xanh debug cũ
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "none";

    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    float fillOpacity = getOpacity(attributes, "fill-opacity");

    if (fill_color_str == "none" || fill_color_str == "transparent")
    {
        fillColor.a = 0;
    }
    else
    {
        fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);
    }
    shape.setFillColor(fillColor);

    // --- 4. XỬ LÝ MÀU STROKE & OPACITY ---
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";

    sf::Color strokeColor = stringToColor(stroke_color_str, "stroke");
    float strokeOpacity = getOpacity(attributes, "stroke-opacity");

    // Ép kiểu std::uint8_t
    strokeColor.a = static_cast<std::uint8_t>(strokeOpacity * 255);

    shape.setOutlineColor(strokeColor);

    // --- 5. XỬ LÝ ĐỘ DÀY VIỀN ---
    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f;
    if (it_width != attributes.end())
    {
        try
        {
            thickness = std::stof(it_width->second);
        }
        catch (...)
        {
            thickness = 1.f;
        }
    }
    shape.setOutlineThickness((stroke_color_str == "none") ? 0.f : thickness);

    window.draw(shape);
}

void SVGRenderer::renderEllipse(const Ellipse &ellipse)
{
    float cx = static_cast<float>(ellipse.getCx());
    float cy = static_cast<float>(ellipse.getCy());
    float rx = static_cast<float>(ellipse.getRx());
    float ry = static_cast<float>(ellipse.getRy());

    // Chỉ vẽ nếu bán kính hợp lệ
    if (rx <= 0 || ry <= 0)
        return;

    // Lấy thuộc tính gộp (Group + Element)
    Attributes attributes = getEffectiveAttributes(ellipse.getAttributes());

    // Tính toán Transform tổng hợp (Group * Local)
    TransformMatrix finalMatrix = getCumulativeTransform();
    finalMatrix.combine(ellipse.getTransform());

    // SFML Transform
    sf::Transform transform = getSFMLTransform(finalMatrix);

    // 1. TẠO HÌNH TRÒN CƠ SỞ (Dựa trên bán kính X)
    sf::CircleShape shape(rx);

    // 2. ĐẶT TÂM (ORIGIN) VỀ GIỮA
    // (Để khi scale, nó co giãn từ tâm ra chứ không phải từ góc)
    shape.setOrigin(sf::Vector2f(rx, rx));

    // 3. ĐẶT VỊ TRÍ (Tại tâm cx, cy)
    shape.setPosition(sf::Vector2f(cx, cy));

    // 4. CO GIÃN (SCALE) ĐỂ BIẾN TRÒN THÀNH ELIP
    // Giữ nguyên trục X (1.0f), co giãn trục Y theo tỉ lệ (ry / rx)
    shape.setScale(sf::Vector2f(1.0f, ry / rx));

    // --- 5. XỬ LÝ MÀU FILL & OPACITY ---
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "none";

    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    float fillOpacity = getOpacity(attributes, "fill-opacity");

    if (fill_color_str == "none" || fill_color_str == "transparent")
    {
        fillColor.a = 0; // Đảm bảo trong suốt tuyệt đối
    }
    else
    {
        // Áp dụng opacity lên Alpha (255)
        fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);
    }
    shape.setFillColor(fillColor);

    // --- 6. XỬ LÝ MÀU STROKE & OPACITY ---
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";

    sf::Color strokeColor = stringToColor(stroke_color_str, "stroke");
    float strokeOpacity = getOpacity(attributes, "stroke-opacity");

    strokeColor.a = static_cast<std::uint8_t>(strokeOpacity * 255);

    shape.setOutlineColor(strokeColor);

    // --- 7. XỬ LÝ ĐỘ DÀY VIỀN ---
    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f;
    if (it_width != attributes.end())
    {
        try
        {
            thickness = std::stof(it_width->second);
        }
        catch (...)
        {
            thickness = 1.f;
        }
    }

    // Lưu ý: Khi scale hình, độ dày viền cũng bị scale theo (viền ngang và dọc sẽ không đều nhau).
    // Đây là hạn chế của SFML khi dùng setScale, nhưng chấp nhận được ở mức cơ bản.
    shape.setOutlineThickness((stroke_color_str == "none") ? 0.f : thickness);

    window.draw(shape, transform);
}
// --- Render Text ---
void SVGRenderer::renderText(const Text &text)
{
    // Tạo đối tượng Text của SFML
    // (Biến 'font' phải được load thành công trong Constructor rồi nhé!)
    sf::Text sfText(font);

    sfText.setString(text.getContent());
    sfText.setCharacterSize(static_cast<unsigned int>(text.getFontSize()));

    // 1. ĐẶT VỊ TRÍ (Dùng sf::Vector2f cho SFML 3.0)
    // Lưu ý: SVG vẽ text từ đường baseline (chân chữ), còn SFML vẽ từ góc trên trái.
    // Để chữ không bị bay lên trên, ta đặt trực tiếp tại (x, y) hoặc điều chỉnh nhẹ.
    // Ở đây mình đặt trực tiếp để đảm bảo bạn nhìn thấy nó.
    sfText.setPosition(sf::Vector2f(
        static_cast<float>(text.getX()),
        static_cast<float>(text.getY() - sfText.getCharacterSize())));

    // Lấy thuộc tính gộp (Group + Element)
    Attributes attributes = getEffectiveAttributes(text.getAttributes());

    // Tính toán Transform tổng hợp (Group * Local)
    TransformMatrix finalMatrix = getCumulativeTransform();
    finalMatrix.combine(text.getTransform());

    // SFML Transform
    sf::Transform transform = getSFMLTransform(finalMatrix);

    // --- 2. XỬ LÝ MÀU FILL & OPACITY ---
    auto it_fill = attributes.find("fill");
    // Mặc định text màu đen
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";

    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    float fillOpacity = getOpacity(attributes, "fill-opacity");

    // Ép kiểu std::uint8_t cho SFML 3.0
    fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);

    sfText.setFillColor(fillColor);

    // --- 3. XỬ LÝ MÀU STROKE & OPACITY (Viền chữ) ---
    auto it_stroke = attributes.find("stroke");
    if (it_stroke != attributes.end() && it_stroke->second != "none")
    {

        sf::Color strokeColor = stringToColor(it_stroke->second, "stroke");
        float strokeOpacity = getOpacity(attributes, "stroke-opacity");

        // Ép kiểu std::uint8_t
        strokeColor.a = static_cast<std::uint8_t>(strokeOpacity * 255);

        sfText.setOutlineColor(strokeColor);

        // Xử lý độ dày viền
        auto it_width = attributes.find("stroke-width");
        float thickness = 0.5f; // Mặc định viền chữ mảnh thôi
        if (it_width != attributes.end())
        {
            try
            {
                thickness = std::stof(it_width->second);
            }
            catch (...)
            {
            }
        }
        sfText.setOutlineThickness(thickness);
    }
    else
    {
        sfText.setOutlineThickness(0.f);
    }

    window.draw(sfText, transform);
}

// Hàm tìm hình chiếu của điểm P xuống đoạn thẳng AB
sf::Vector2f getProjectedPoint(sf::Vector2f p, sf::Vector2f a, sf::Vector2f b)
{
    sf::Vector2f ab = b - a;
    sf::Vector2f ap = p - a;

    // Tính tỉ lệ chiếu t
    float magAB2 = ab.x * ab.x + ab.y * ab.y;
    if (magAB2 < 0.0001f)
        return a; // A trùng B

    float t = (ap.x * ab.x + ap.y * ab.y) / magAB2;

    // Giới hạn t trong khoảng [0, 1] để hình chiếu không chạy ra ngoài đoạn AB
    if (t < 0.f)
        t = 0.f;
    if (t > 1.f)
        t = 1.f;

    return a + ab * t;
}

void SVGRenderer::renderPolyline(const Polyline &polyline)
{
    // 1. Tách tọa độ
    std::istringstream iss(polyline.getPoints());
    std::vector<sf::Vector2f> points;
    float x, y;
    char comma;

    // Tính ma trận tổng hợp (Group * Local)
    TransformMatrix finalMatrix = getCumulativeTransform();
    finalMatrix.combine(polyline.getTransform());
    float t_x, t_y; // Tọa độ đã transform

    while (iss >> x)
    {
        // Bỏ qua dấu phẩy giữa x và y (nếu có)
        if (iss.peek() == ',')
            iss >> comma;

        if (iss >> y)
        {
            // Áp dụng Ma trận tổng hợp lên điểm (x, y)
            finalMatrix.transformPoint(x, y, t_x, t_y);
            points.emplace_back(t_x, t_y);
        }
        else
            break;
        if (iss.peek() == ',')
            iss >> comma;
    }

    if (points.size() < 2)
        return;

    // Lấy thuộc tính gộp (Cha + Con)
    Attributes attributes = getEffectiveAttributes(polyline.getAttributes());

    // --- A. VẼ PHẦN FILL (XỬ LÝ SONG SONG THÔNG MINH) ---
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "none";
    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    float fillOpacity = getOpacity(attributes, "fill-opacity");
    fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);

    if (fillColor.a > 0)
    {
        sf::Vector2f pStart = points.front();
        sf::Vector2f pEnd = points.back();

        // [THÔNG MINH] Kiểm tra xem hình này là "Ngang" hay "Chéo"?
        // Nếu chênh lệch độ cao giữa đầu và cuối nhỏ hơn 10px -> Coi là Ngang (Răng lược)
        bool isHorizontal = std::abs(pEnd.y - pStart.y) < 10.f;

        if (isHorizontal)
        {
            // === TRƯỜNG HỢP 1: RĂNG LƯỢC (Dùng Projection Strip) ===
            // Cách này vẽ các cột màu xanh đẹp nhất, thẳng tắp xuống đáy
            sf::VertexArray vertices(sf::PrimitiveType::TriangleStrip);
            for (const auto &p : points)
            {
                sf::Vector2f pProj = getProjectedPoint(p, pStart, pEnd);
                vertices.append(sf::Vertex{p, fillColor});
                vertices.append(sf::Vertex{pProj, fillColor});
            }
            window.draw(vertices);
        }
        else
        {
            // === TRƯỜNG HỢP 2: BẬC THANG (Dùng Intersection Logic - CÁI BẠN KHEN 10Đ) ===
            // Cách này cắt gọn các tam giác theo đường chéo, không bị lem
            sf::VertexArray vertices(sf::PrimitiveType::Triangles);

            for (size_t i = 1; i < points.size() - 1; ++i)
            {
                sf::Vector2f currentP = points[i];
                sf::Vector2f prevP = points[i - 1];
                sf::Vector2f nextP = points[i + 1];

                sf::Vector2f intersection1;
                sf::Vector2f intersection2;

                // Tìm giao điểm cạnh trước
                if (std::abs(currentP.x - prevP.x) < 0.1f) // Đoạn đứng
                    intersection1 = sf::Vector2f(currentP.x, getYOnDiagonal(currentP.x, pStart, pEnd));
                else // Đoạn ngang
                    intersection1 = sf::Vector2f(getXOnDiagonal(currentP.y, pStart, pEnd), currentP.y);

                // Tìm giao điểm cạnh sau
                if (std::abs(nextP.x - currentP.x) < 0.1f)
                    intersection2 = sf::Vector2f(currentP.x, getYOnDiagonal(currentP.x, pStart, pEnd));
                else
                    intersection2 = sf::Vector2f(getXOnDiagonal(currentP.y, pStart, pEnd), currentP.y);

                vertices.append(sf::Vertex{currentP, fillColor});
                vertices.append(sf::Vertex{intersection1, fillColor});
                vertices.append(sf::Vertex{intersection2, fillColor});
            }
            window.draw(vertices);
        }
    }

    // --- B. VẼ PHẦN STROKE (VIỀN ĐỎ - GIỮ NGUYÊN 100%) ---
    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f;
    if (it_width != attributes.end())
    {
        try
        {
            thickness = std::stof(it_width->second);
        }
        catch (...)
        {
        }
    }

    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";
    sf::Color strokeColor = stringToColor(stroke_color_str, "stroke");
    float strokeOpacity = getOpacity(attributes, "stroke-opacity");
    strokeColor.a = static_cast<std::uint8_t>(strokeOpacity * 255);

    if (strokeColor.a > 0 && thickness > 0.f)
    {
        for (size_t i = 0; i < points.size() - 1; ++i)
        {
            sf::Vector2f p1 = points[i];
            sf::Vector2f p2 = points[i + 1];
            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            float length = std::sqrt(dx * dx + dy * dy);
            float angle = std::atan2(dy, dx) * 180.f / 3.14159f;

            sf::RectangleShape lineSeg(sf::Vector2f(length, thickness));
            lineSeg.setOrigin(sf::Vector2f(0.f, thickness / 2.f));
            lineSeg.setPosition(p1);
            lineSeg.setRotation(sf::degrees(angle));
            lineSeg.setFillColor(strokeColor);
            window.draw(lineSeg);

            if (thickness > 1.f)
            {
                sf::CircleShape corner(thickness / 2.f);
                corner.setOrigin(sf::Vector2f(thickness / 2.f, thickness / 2.f));
                corner.setPosition(p1);
                corner.setFillColor(strokeColor);
                window.draw(corner);
            }
        }
        if (thickness > 1.f)
        {
            sf::CircleShape endCorner(thickness / 2.f);
            endCorner.setOrigin(sf::Vector2f(thickness / 2.f, thickness / 2.f));
            endCorner.setPosition(points.back());
            endCorner.setFillColor(strokeColor);
            window.draw(endCorner);
        }
    }
}

// Hàm đẩy Context mới vào Stack
void SVGRenderer::beginElement(const TransformMatrix &elementLocalTransform, const Attributes &elementLocalAttrs)
{
    RenderState newState;

    if (renderStack_.empty())
    {
        // Nếu là phần tử cấp cao nhất, Transform tích lũy = Transform cục bộ
        newState.cumulativeTransform = elementLocalTransform;
        newState.inheritedAttributes = elementLocalAttrs;
    }
    else
    {
        const RenderState &parentState = renderStack_.back();

        // tính cumulative transform: Parent Cumulative * Child Local
        newState.cumulativeTransform = parentState.cumulativeTransform;
        newState.cumulativeTransform.combine(elementLocalTransform);

        // tình toán inherited attributes
        // Bắt đầu từ thuộc tính Cha (đã được merge), sau đó merge với thuộc tính của Element hiện tại
        newState.inheritedAttributes = parentState.inheritedAttributes;

        // Merge thuộc tính cục bộ của Element hiện tại vào thuộc tính kế thừa mới
        for (const auto &kv : elementLocalAttrs)
        {
            newState.inheritedAttributes[kv.first] = kv.second;
        }
    }

    renderStack_.push_back(newState);
}

// Hàm khôi phục context
void SVGRenderer::endElement()
{
    if (!renderStack_.empty())
    {
        renderStack_.pop_back();
    }
}

// Hàm lấy Transform tổng hợp đang có hiệu lực
const TransformMatrix &SVGRenderer::getCumulativeTransform() const
{
    if (renderStack_.empty())
    {
        static const TransformMatrix identity;
        return identity;
    }
    return renderStack_.back().cumulativeTransform;
}

// Hàm tiện ích: Xóa khoảng trắng ở đầu chuỗi
static inline void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch)
                                    { return !std::isspace(ch); }));
}

// Hàm tiện ích: Xóa khoảng trắng ở cuối chuỗi
static inline void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
                         { return !std::isspace(ch); })
                .base(),
            s.end());
}
// Hàm lấy Attribute đang có hiệu lực (Merge Stack Top và Element Local)
// Hàm lấy Attribute đang có hiệu lực (Merge Stack Top + Element Local + Style CSS)
Attributes SVGRenderer::getEffectiveAttributes(const Attributes &localAttrs) const
{
    // 1. Bắt đầu với các thuộc tính thừa kế từ Group cha (nếu có)
    Attributes effectiveAttrs;
    if (!renderStack_.empty())
    {
        effectiveAttrs = renderStack_.back().inheritedAttributes;
    }

    // 2. Ghi đè bằng các thuộc tính riêng của thẻ (ví dụ: fill="red")
    for (const auto &kv : localAttrs)
    {
        effectiveAttrs[kv.first] = kv.second;
    }

    // 3. [QUAN TRỌNG] Xử lý thuộc tính "style" (CSS inline)
    // Ví dụ: style="fill:#ff0000; stroke:none; opacity:0.5"
    if (effectiveAttrs.count("style"))
    {
        std::string styleStr = effectiveAttrs["style"];
        std::stringstream ss(styleStr);
        std::string segment;

        // Cắt chuỗi theo dấu chấm phẩy ';'
        while (std::getline(ss, segment, ';'))
        {
            size_t colonPos = segment.find(':');
            if (colonPos != std::string::npos)
            {
                std::string key = segment.substr(0, colonPos);
                std::string value = segment.substr(colonPos + 1);

                // Xóa khoảng trắng thừa (Trim)
                ltrim(key);
                rtrim(key);
                ltrim(value);
                rtrim(value);

                // Ghi đè thuộc tính từ style vào danh sách cuối cùng
                if (!key.empty())
                {
                    effectiveAttrs[key] = value;
                }
            }
        }
    }

    return effectiveAttrs;
}

// HÀM MỚI: Tính điểm trên đường cong Bezier bậc 3
// B(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t) t^2 P2 + t^3 P3
sf::Vector2f cubicBezier(float t, sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3)
{
    float u = 1 - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    sf::Vector2f p = uuu * p0; // (1-t)^3 * P0
    p += 3 * uu * t * p1;      // 3(1-t)^2 * t * P1
    p += 3 * u * tt * p2;      // 3(1-t) * t^2 * P2
    p += ttt * p3;             // t^3 * P3

    return p;
}

// SVGRenderer.cpp
// SVGRenderer.cpp

// bool isPointInPolygon(const sf::Vector2f& p, const std::vector<sf::Vector2f>& polygon) {
//     bool inside = false;
//     for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
//         if (((polygon[i].y > p.y) != (polygon[j].y > p.y)) &&
//             (p.x < (polygon[j].x - polygon[i].x) * (p.y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x)) {
//             inside = !inside;
//         }
//     }
//     return inside;
// }


void SVGRenderer::renderPath(const Path& path) {
    const auto& commands = path.getCommands();
    if (commands.empty()) return;

    Attributes attributes = getEffectiveAttributes(path.getAttributes());
    TransformMatrix finalMatrix = getCumulativeTransform();
    finalMatrix.combine(path.getTransform());

    // 1. CHUYỂN LỆNH THÀNH CÁC TẬP HỢP ĐIỂM (SUB-PATHS)
    std::vector<std::vector<sf::Vector2f>> subPaths;
    std::vector<sf::Vector2f> currentPath;
    sf::Vector2f currentPos(0, 0);
    sf::Vector2f startPos(0, 0);

    for (const auto& cmd : commands) {
        if ((cmd.type == 'M' || cmd.type == 'm') && !currentPath.empty()) {
            subPaths.push_back(currentPath);
            currentPath.clear();
        }
        if (cmd.type == 'M' || cmd.type == 'm') {
            float tx, ty; finalMatrix.transformPoint(cmd.args[0], cmd.args[1], tx, ty);
            currentPos = sf::Vector2f(tx, ty); startPos = currentPos; currentPath.push_back(currentPos);
        }
        else if (cmd.type == 'L' || cmd.type == 'l') {
            float tx, ty; finalMatrix.transformPoint(cmd.args[0], cmd.args[1], tx, ty);
            currentPos = sf::Vector2f(tx, ty); currentPath.push_back(currentPos);
        }
        else if (cmd.type == 'C' || cmd.type == 'c') {
            float tx1, ty1, tx2, ty2, tx3, ty3;
            finalMatrix.transformPoint(cmd.args[0], cmd.args[1], tx1, ty1);
            finalMatrix.transformPoint(cmd.args[2], cmd.args[3], tx2, ty2);
            finalMatrix.transformPoint(cmd.args[4], cmd.args[5], tx3, ty3);
            sf::Vector2f p1(tx1, ty1), p2(tx2, ty2), p3(tx3, ty3);
            // Độ mịn 30 điểm cho đường cong
            for (int i = 1; i <= 30; ++i) {
                // Lưu ý: Nếu hàm cubicBezier là member function thì dùng this->cubicBezier
                // Nếu là hàm tự do bên ngoài thì cứ gọi cubicBezier
                currentPath.push_back(cubicBezier(i / 30.f, currentPos, p1, p2, p3));
            }
            currentPos = p3;
        }
        else if (cmd.type == 'Z' || cmd.type == 'z') {
            if (currentPos != startPos) currentPath.push_back(startPos);
        }
    }
    if (!currentPath.empty()) subPaths.push_back(currentPath);

    // 2. XỬ LÝ MÀU SẮC
    sf::Color fillColor = sf::Color::Black;
    std::string fillRule = "nonzero";
    if (attributes.count("fill-rule")) fillRule = attributes["fill-rule"];

    if (attributes.count("fill") && attributes["fill"] != "none") {
        fillColor = stringToColor(attributes["fill"], "fill");
    } else if (attributes.count("fill") && attributes["fill"] == "none") {
        fillColor = sf::Color::Transparent;
    }
    float fillOpacity = getOpacity(attributes, "fill-opacity");
    if (fillColor != sf::Color::Transparent) fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);

    sf::Color strokeColor = sf::Color::Transparent;
    if (attributes.count("stroke") && attributes["stroke"] != "none") {
        strokeColor = stringToColor(attributes["stroke"], "stroke");
    }
    float strokeOpacity = getOpacity(attributes, "stroke-opacity");
    if (strokeColor != sf::Color::Transparent) strokeColor.a = static_cast<std::uint8_t>(strokeOpacity * 255);
    
    float strokeWidth = 1.0f;
    if (attributes.count("stroke-width")) try { strokeWidth = std::stof(attributes["stroke-width"]); } catch(...) {}
    if (strokeColor.a > 0 && strokeWidth < 1.0f) strokeWidth = 1.0f;

    // 3. VẼ FILL (Kết hợp Layering + Triangulation)
    if (fillColor.a > 0 && !subPaths.empty()) {
        // Sắp xếp theo diện tích (Hình TO vẽ trước)
        std::vector<std::pair<float, size_t>> sortedIndices;
        for(size_t i=0; i<subPaths.size(); ++i) sortedIndices.push_back({getBoundingBoxArea(subPaths[i]), i});
        std::sort(sortedIndices.rbegin(), sortedIndices.rend()); 

        for (size_t i = 0; i < sortedIndices.size(); ++i) {
            size_t idx = sortedIndices[i].second;
            const auto& rawPoints = subPaths[idx];
            if (rawPoints.size() < 3) continue;

            // Xác định "lỗ" (Hole) dựa trên mức độ lồng nhau
            int nestingLevel = 0;
            if (!rawPoints.empty()) {
                for (size_t j = 0; j < i; ++j) {
                    size_t biggerIdx = sortedIndices[j].second;
                    if (isPointInPolygon(rawPoints[0], subPaths[biggerIdx])) nestingLevel++;
                }
            }

            sf::Color currentFillColor = fillColor;
            // Nếu là lỗ (level lẻ) và rule là evenodd -> Tô màu TRẮNG
            if (fillRule == "evenodd" && nestingLevel % 2 != 0) {
                currentFillColor = sf::Color::White; 
            }

            // Dùng Triangulation để tô kín hình (kể cả hình lõm)
            std::vector<sf::Vector2f> triangles = triangulatePolygon(rawPoints);
            if (!triangles.empty()) {
                sf::VertexArray va(sf::PrimitiveType::Triangles, triangles.size());
                for (size_t k = 0; k < triangles.size(); ++k) {
                    va[k].position = triangles[k];
                    va[k].color = currentFillColor;
                }
                window.draw(va);
            }
        }
    }

    // 4. VẼ STROKE (Vẽ thủ công để đảm bảo độ dày và vị trí)
    if (strokeColor.a > 0) {
        for (const auto& points : subPaths) {
            for (size_t i = 0; i < points.size() - 1; ++i) {
                sf::Vector2f p1 = points[i], p2 = points[i+1];
                sf::Vector2f dir = p2 - p1;
                float len = std::sqrt(dir.x*dir.x + dir.y*dir.y);
                if (len > 0.01f) {
                    sf::RectangleShape line(sf::Vector2f(len, strokeWidth));
                    line.setOrigin(sf::Vector2f(0.f, strokeWidth / 2.f)); // SFML 3.0
                    line.setPosition(p1); 
                    line.setFillColor(strokeColor);
                    line.setRotation(sf::degrees(std::atan2(dir.y, dir.x) * 180.f / M_PI)); // SFML 3.0
                    window.draw(line);
                    
                    sf::CircleShape joint(strokeWidth / 2.f);
                    joint.setOrigin(sf::Vector2f(strokeWidth/2.f, strokeWidth/2.f)); // SFML 3.0
                    joint.setPosition(p2); 
                    joint.setFillColor(strokeColor);
                    window.draw(joint);
                }
            }
        }
    }
}
