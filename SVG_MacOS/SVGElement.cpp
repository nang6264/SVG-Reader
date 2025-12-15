// SVGElement.cpp
#include "SVGElement.h"
#include "Transform.h"
#include "SVGRenderer.h"
#include <iostream> // Để sử dụng std::stod

// Hàm tiện ích để chuyển string sang double an toàn (nếu cần)
double safeStod(const std::string &s)
{
    try
    {
        return std::stod(s);
    }
    catch (...)
    {
        // Xử lý lỗi hoặc trả về giá trị mặc định
        return 0.0;
    }
}

void SVGElement::parseTransform()
{
    auto it = attributes_.find("transform");
    if (it != attributes_.end())
    {
        try
        {
            // Gọi hàm parse static của TransformMatrix để chuyển chuỗi thành ma trận
            transform_ = TransformMatrix::parse(it->second);
        }
        catch (...)
        {
            std::cerr << "Cảnh báo: Lỗi phân tích thuộc tính transform: " << it->second << std::endl;
            // Giữ nguyên transform_ là Identity Matrix
        }
    }
}

// --- Định nghĩa SVGElement ---

SVGElement::SVGElement(const Attributes &attributes) : attributes_(attributes)
{
    // Có thể thực hiện xử lý hoặc kiểm tra các thuộc tính chung ở đây
    parseTransform();
}

// --- Định nghĩa Circle ---

Circle::Circle(const Attributes &attributes) : SVGElement(attributes)
{
    // Lấy các thuộc tính riêng của Circle từ map và chuyển sang double
    auto it = attributes.find("cx");
    if (it != attributes.end())
        cx_ = safeStod(it->second);

    it = attributes.find("cy");
    if (it != attributes.end())
        cy_ = safeStod(it->second);

    it = attributes.find("r");
    if (it != attributes.end())
        r_ = safeStod(it->second);
}

void Circle::draw(SVGRenderer &renderer) const
{
    // Gọi hàm render cụ thể của renderer, truyền tham chiếu 'this'
    renderer.renderCircle(*this);
}

// --- Định nghĩa Rect ---

Rect::Rect(const Attributes &attributes) : SVGElement(attributes)
{
    auto it = attributes.find("x");
    if (it != attributes.end())
        x_ = safeStod(it->second);

    it = attributes.find("y");
    if (it != attributes.end())
        y_ = safeStod(it->second);

    it = attributes.find("width");
    if (it != attributes.end())
        width_ = safeStod(it->second);

    it = attributes.find("height");
    if (it != attributes.end())
        height_ = safeStod(it->second);
}

void Rect::draw(SVGRenderer &renderer) const
{
    renderer.renderRect(*this);
}

// --- Định nghĩa Line ---

Line::Line(const Attributes &attributes) : SVGElement(attributes)
{
    // Sử dụng hàm tiện ích safeStod đã định nghĩa trước đó
    auto it = attributes.find("x1");
    if (it != attributes.end())
        x1_ = safeStod(it->second);

    it = attributes.find("y1");
    if (it != attributes.end())
        y1_ = safeStod(it->second);

    it = attributes.find("x2");
    if (it != attributes.end())
        x2_ = safeStod(it->second);

    it = attributes.find("y2");
    if (it != attributes.end())
        y2_ = safeStod(it->second);
}

void Line::draw(SVGRenderer &renderer) const
{
    // Gọi hàm renderLine() của renderer
    renderer.renderLine(*this);
}

// --- Định nghĩa Polygon ---

Polygon::Polygon(const Attributes &attributes) : SVGElement(attributes)
{
    // Thuộc tính "points" là một chuỗi, không cần chuyển đổi
    auto it = attributes.find("points");
    if (it != attributes.end())
    {
        points_ = it->second;
    }
}

void Polygon::draw(SVGRenderer &renderer) const
{
    // Gọi hàm renderPolygon() của renderer
    renderer.renderPolygon(*this);
}

// --- Định nghĩa Ellipse ---

Ellipse::Ellipse(const Attributes &attributes) : SVGElement(attributes)
{
    // Lấy các thuộc tính riêng của Ellipse từ map
    auto it = attributes.find("cx");
    if (it != attributes.end())
        cx_ = safeStod(it->second);

    it = attributes.find("cy");
    if (it != attributes.end())
        cy_ = safeStod(it->second);

    it = attributes.find("rx");
    if (it != attributes.end())
        rx_ = safeStod(it->second);

    it = attributes.find("ry");
    if (it != attributes.end())
        ry_ = safeStod(it->second);
}

void Ellipse::draw(SVGRenderer &renderer) const
{
    // Yêu cầu renderer vẽ ellipse
    // LƯU Ý: Bạn sẽ cần thêm hàm renderEllipse() vào lớp SVGRenderer
    renderer.renderEllipse(*this);
}

// --- Định nghĩa Text ---

Text::Text(const Attributes &attributes, const std::string &content)
    : SVGElement(attributes), content_(content)
{

    auto it = attributes.find("x");
    if (it != attributes.end())
        x_ = safeStod(it->second);

    it = attributes.find("y");
    if (it != attributes.end())
        y_ = safeStod(it->second);
    // [THÊM MỚI] Đọc dx, dy
    it = attributes.find("dx");
    if (it != attributes.end()) dx_ = safeStod(it->second);

    it = attributes.find("dy");
    if (it != attributes.end()) dy_ = safeStod(it->second);
    // Đọc font-size (nếu có)
    it = attributes.find("font-size");
    if (it != attributes.end())
        fontSize_ = safeStod(it->second);
}

void Text::draw(SVGRenderer &renderer) const
{
    renderer.renderText(*this);
}

// --- Định nghĩa Polyline ---
Polyline::Polyline(const Attributes &attributes) : SVGElement(attributes)
{
    auto it = attributes.find("points");
    if (it != attributes.end())
    {
        points_ = it->second;
    }
}

void Polyline::draw(SVGRenderer &renderer) const
{
    renderer.renderPolyline(*this);
}
