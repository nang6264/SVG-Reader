// SVGRenderer.cpp
#include "SVGRenderer.h"
#include "SVGElement.h" // Cần include file này để lấy định nghĩa đầy đủ của các lớp con
#include <iostream>
#include <sstream> // Dùng cho renderPolygon
#include <vector>
#include <cctype>    // Dùng cho ::tolower
#include <cmath>     // Dùng cho sqrt, atan2
#include <algorithm> // Dùng cho std::max
#include <cstdint>   // <--- THÊM DÒNG NÀY ĐỂ DÙNG uint8_t
// Dùng std::cout, std::endl, v.v.
using namespace std;

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

/**
 * @brief Hàm tiện ích chuyển đổi chuỗi màu SVG sang màu SFML
 */
sf::Color SVGRenderer::stringToColor(std::string colorStr, std::string type)
{
    // Chuyển sang chữ thường và xóa khoảng trắng
    std::transform(colorStr.begin(), colorStr.end(), colorStr.begin(), ::tolower);
    colorStr.erase(std::remove_if(colorStr.begin(), colorStr.end(), ::isspace), colorStr.end());

    if (colorStr == "none")
    {
        return sf::Color::Transparent;
    }
    if (colorStr == "black")
    {
        return sf::Color::Black;
    }
    if (colorStr == "white")
    {
        return sf::Color::White;
    }
    if (colorStr == "red")
    {
        return sf::Color::Red;
    }
    if (colorStr == "green")
    {
        return sf::Color::Green;
    }
    if (colorStr == "blue")
    {
        return sf::Color::Blue;
    }
    // ... (thêm các màu khác nếu cần) ...

    // Xử lý mã Hex (ví dụ: #FF0000)
    if (colorStr.length() > 0 && colorStr[0] == '#')
    {
        try
        {
            unsigned int hexValue = std::stoul(colorStr.substr(1), nullptr, 16);
            if (colorStr.length() == 7)
            { // #RRGGBB
                return sf::Color(
                    (hexValue >> 16) & 0xFF, // R
                    (hexValue >> 8) & 0xFF,  // G
                    (hexValue) & 0xFF        // B
                );
            }
        }
        catch (...)
        { /* Parse lỗi */
        }
    }

    // [MỚI] Xử lý RGB (ví dụ: rgb(200,100,150))
    if (colorStr.rfind("rgb(", 0) == 0)
    { // Nếu chuỗi bắt đầu bằng "rgb("
        try
        {
            // Xóa "rgb(" ở đầu và ")" ở cuối
            std::string values = colorStr.substr(4, colorStr.length() - 5);

            std::stringstream ss(values);
            std::string segment;
            int r, g, b;

            // Tách R
            if (std::getline(ss, segment, ','))
            {
                r = std::stoi(segment);
            }
            else
                return sf::Color::Magenta; // Lỗi

            // Tách G
            if (std::getline(ss, segment, ','))
            {
                g = std::stoi(segment);
            }
            else
                return sf::Color::Magenta; // Lỗi

            // Tách B
            if (std::getline(ss, segment))
            { // Đọc phần còn lại
                b = std::stoi(segment);
            }
            else
                return sf::Color::Magenta; // Lỗi

            return sf::Color(r, g, b);
        }
        catch (...)
        {
            // Lỗi chuyển đổi stoi
            return sf::Color::Magenta; // Báo lỗi parse
        }
    }

    // Mặc định
    if (type == "fill")
        return sf::Color::Black;
    if (type == "stroke")
        return sf::Color::Transparent;

    return sf::Color::Magenta; // Trả về màu Magenta (tím) để dễ nhận biết lỗi
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
SVGRenderer::SVGRenderer(unsigned int width, unsigned int height)
{
    window.create(sf::VideoMode({width, height}), "SVG Renderer (SFML)");
    view = window.getDefaultView();
    if (!font.openFromFile("times.ttf"))
    {
        // Nếu trên Mac/Linux không có file này, thử đường dẫn hệ thống
        // Hoặc in ra lỗi để biết
        std::cerr << "Cảnh báo: Không thể load font arial.ttf! Chữ sẽ không hiện." << std::endl;
    }
    cout << "SVGRenderer: Window created " << width << "x" << height << std::endl;
    cout << "Controls: Mouse Wheel = Zoom, R = Rotate, ESC = Exit" << std::endl;
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
    cout << "Zoomed In" << std::endl;
}

void SVGRenderer::zoomOut()
{
    view.zoom(1.1f);
    cout << "Zoomed Out" << std::endl;
}

void SVGRenderer::rotate(float angle)
{
    view.rotate(sf::degrees(angle));
    cout << "Rotated by " << angle << " degrees" << std::endl;
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
                cout << "Window closed by user" << std::endl;
            }
            else if (auto *keyEvent = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyEvent->scancode == sf::Keyboard::Scancode::Escape)
                {
                    window.close();
                    cout << "Window closed by ESC" << std::endl;
                }
                else if (keyEvent->scancode == sf::Keyboard::Scancode::R)
                {
                    rotate(10.f);
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
    cout << "Render loop ended" << std::endl;
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

    const auto &attributes = circle.getAttributes();

    // --- XỬ LÝ MÀU FILL & OPACITY ---
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";

    // 1. KHAI BÁO BIẾN TRƯỚC (Sửa lỗi undeclared identifier)
    sf::Color fillColor = stringToColor(fill_color_str, "fill");

    // 2. Lấy độ trong suốt
    float fillOpacity = getOpacity(attributes, "fill-opacity");

    // 3. GÁN ALPHA (Sửa lỗi sf::Uint8 thành std::uint8_t)
    fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);

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

    window.draw(shape);
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

    const auto &attributes = rect.getAttributes();

    // Fill
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";

    // 1. KHAI BÁO biến fillColor (Sửa lỗi undeclared identifier)
    sf::Color fillColor = stringToColor(fill_color_str, "fill");

    float fillOpacity = getOpacity(attributes, "fill-opacity");

    // 2. SỬA LỖI Uint8 -> std::uint8_t (Sửa lỗi SFML 3.0)
    fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);

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

    window.draw(shape);
}

// --- Render Line ---
void SVGRenderer::renderLine(const Line &line)
{
    float x1 = static_cast<float>(line.getX1());
    float y1 = static_cast<float>(line.getY1());
    float x2 = static_cast<float>(line.getX2());
    float y2 = static_cast<float>(line.getY2());

    const auto &attributes = line.getAttributes();

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
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "black";
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

    // [QUAN TRỌNG] Đặt tâm về giữa cạnh trái (0, thickness/2)
    // Điều này giúp đường thẳng dày được vẽ đều sang hai bên trục
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

    while (iss >> x)
    {
        // Bỏ qua dấu phẩy giữa x và y nếu có
        if (iss.peek() == ',')
            iss >> comma;

        if (iss >> y)
        {
            points.emplace_back(x, y);
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

    const auto &attributes = polygon.getAttributes();

    // --- 3. XỬ LÝ MÀU FILL & OPACITY ---
    auto it_fill = attributes.find("fill");
    // Mặc định là màu đen (chuẩn SVG) thay vì màu xanh debug cũ
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";

    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    float fillOpacity = getOpacity(attributes, "fill-opacity");

    // Ép kiểu std::uint8_t cho SFML 3.0
    fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);

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

// --- Render Path ---
void SVGRenderer::renderPath(const Path &path)
{
    // LƯU Ý: Đây chỉ là hình đại diện (Placeholder).
    // Phân tích cú pháp chuỗi 'd' của Path rất phức tạp.
    // Ta vẽ một hình chữ nhật nhỏ tại tọa độ (0,0) hoặc vị trí cố định để báo hiệu.

    // 1. Tạo hình đại diện (Dùng sf::Vector2f cho SFML 3.0)
    sf::RectangleShape shape(sf::Vector2f(100.f, 100.f));
    shape.setPosition(sf::Vector2f(50.f, 50.f)); // Đặt tạm tại vị trí dễ thấy

    const auto &attributes = path.getAttributes();

    // --- 2. XỬ LÝ MÀU FILL & OPACITY ---
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";

    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    float fillOpacity = getOpacity(attributes, "fill-opacity");

    // Ép kiểu std::uint8_t
    fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);

    shape.setFillColor(fillColor);

    // --- 3. XỬ LÝ MÀU STROKE & OPACITY ---
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";

    sf::Color strokeColor = stringToColor(stroke_color_str, "stroke");
    float strokeOpacity = getOpacity(attributes, "stroke-opacity");

    strokeColor.a = static_cast<std::uint8_t>(strokeOpacity * 255);

    shape.setOutlineColor(strokeColor);

    // --- 4. XỬ LÝ ĐỘ DÀY VIỀN ---
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

    // Nếu không có màu stroke thì độ dày = 0
    shape.setOutlineThickness((stroke_color_str == "none") ? 0.f : thickness);

    // In ra console để biết là có Path nhưng chưa vẽ chi tiết được
    // std::cout << "Info: Rendering Path placeholder. Data: " << path.getData().substr(0, 20) << "..." << std::endl;

    window.draw(shape);
}
// --- Hàm tiện ích nội bộ (nếu cần) ---
void SVGRenderer::drawLineBetweenPoints(const sf::Vector2f &p1, const sf::Vector2f &p2, const sf::Color &color)
{
    // (Code này chưa được dùng ở trên, nhưng hữu ích nếu bạn muốn render Path chi tiết)
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float length = std::sqrt(dx * dx + dy * dy);
    float angle = std::atan2(dy, dx) * 180.f / 3.14159f;

    if (length > 0)
    {
        sf::RectangleShape lineShape(sf::Vector2f(length, 1.f)); // Dày 1px
        lineShape.setPosition(p1);
        lineShape.setRotation(sf::degrees(angle));
        lineShape.setFillColor(color);
        window.draw(lineShape);
    }
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

    const auto &attributes = ellipse.getAttributes();

    // --- 5. XỬ LÝ MÀU FILL & OPACITY ---
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";

    sf::Color fillColor = stringToColor(fill_color_str, "fill");
    float fillOpacity = getOpacity(attributes, "fill-opacity");

    // Ép kiểu std::uint8_t cho SFML 3.0
    fillColor.a = static_cast<std::uint8_t>(fillOpacity * 255);

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

    window.draw(shape);
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
        static_cast<float>(text.getY() - text.getFontSize()) // - text.getFontSize() nếu muốn chỉnh theo baseline chuẩn
        ));

    const auto &attributes = text.getAttributes();

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

    window.draw(sfText);
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
    while (iss >> x)
    {
        if (iss.peek() == ',')
            iss >> comma;
        if (iss >> y)
            points.emplace_back(x, y);
        else
            break;
        if (iss.peek() == ',')
            iss >> comma;
    }

    if (points.size() < 2)
        return;

    const auto &attributes = polyline.getAttributes();

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