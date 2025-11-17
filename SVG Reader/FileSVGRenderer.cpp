// FileSVGRenderer.cpp
#include "FileSVGRenderer.h"
#include "SVGElement.h" // Cần include để truy cập các lớp con (Circle, Rect, v.v.)
#include <iostream>     
#include <set>          
#include <typeinfo>     // Cần cho việc nhận dạng kiểu (nếu không dùng pattern Visitor)

// Lưu ý: Bạn cần đảm bảo các lớp hình học có hàm draw(SVGRenderer& renderer) const

/**
 * @brief HÀM TIỆN ÍCH: Ghi các thuộc tính chung (fill, stroke, v.v.)
 */
static void writeCommonAttributes(std::ofstream& file, const SVGElement& element,
    const std::set<std::string>& skipAttributes)
{
    for (const auto& attr : element.getAttributes())
    {
        if (skipAttributes.find(attr.first) == skipAttributes.end())
        {
            file << " " << attr.first << "=\"" << attr.second << "\"";
        }
    }
}

// --- Constructor và Destructor ---

FileSVGRenderer::FileSVGRenderer(const std::string& filename)
{
    file_.open(filename);
    if (file_.is_open())
    {
        file_ << "<svg width=\"100%\" height=\"100%\" version=\"1.1\" "
            << "xmlns=\"http://www.w3.org/2000/svg\">\n";
    }
    else
    {
        std::cerr << "Lỗi: Không thể mở file để ghi: " << filename << std::endl;
    }
}

FileSVGRenderer::~FileSVGRenderer()
{
    if (file_.is_open())
    {
        file_ << "</svg>\n";
        file_.close();
    }
}

// --- Triển khai các hàm Render Ghi File ---

void FileSVGRenderer::renderCircle(const Circle& circle)
{
    if (!file_.is_open()) return;
    file_ << "  <circle cx=\"" << circle.getCx() << "\" cy=\"" << circle.getCy()
        << "\" r=\"" << circle.getR() << "\"";
    writeCommonAttributes(file_, circle, { "cx", "cy", "r" });
    file_ << "/>\n";
}

void FileSVGRenderer::renderRect(const Rect& rect)
{
    if (!file_.is_open()) return;
    file_ << "  <rect x=\"" << rect.getX() << "\" y=\"" << rect.getY()
        << "\" width=\"" << rect.getWidth() << "\" height=\"" << rect.getHeight() << "\"";
    writeCommonAttributes(file_, rect, { "x", "y", "width", "height" });
    file_ << "/>\n";
}

void FileSVGRenderer::renderLine(const Line& line)
{
    if (!file_.is_open()) return;
    file_ << "  <line x1=\"" << line.getX1() << "\" y1=\"" << line.getY1()
        << "\" x2=\"" << line.getX2() << "\" y2=\"" << line.getY2() << "\"";
    writeCommonAttributes(file_, line, { "x1", "y1", "x2", "y2" });
    file_ << "/>\n";
}

void FileSVGRenderer::renderPolygon(const Polygon& polygon)
{
    if (!file_.is_open()) return;
    file_ << "  <polygon points=\"" << polygon.getPoints() << "\"";
    writeCommonAttributes(file_, polygon, { "points" });
    file_ << "/>\n";
}

void FileSVGRenderer::renderPath(const Path& path)
{
    if (!file_.is_open()) return;
    file_ << "  <path d=\"" << path.getData() << "\"";
    writeCommonAttributes(file_, path, { "d" });
    file_ << "/>\n";
}

void FileSVGRenderer::renderEllipse(const Ellipse& ellipse)
{
    if (!file_.is_open()) return;
    file_ << "  <ellipse cx=\"" << ellipse.getCx() << "\" cy=\"" << ellipse.getCy()
        << "\" rx=\"" << ellipse.getRx() << "\" ry=\"" << ellipse.getRy() << "\"";
    writeCommonAttributes(file_, ellipse, { "cx", "cy", "rx", "ry" });
    file_ << "/>\n";
}

// --- Triển khai hàm chung ---

void FileSVGRenderer::addElement(std::shared_ptr<SVGElement> element)
{
    if (element) {
        elements_.push_back(element);
    }
}

void FileSVGRenderer::render()
{
    if (!file_.is_open()) return;

    // Ghi từng phần tử đã được lưu ra file
    for (const auto& element : elements_) {
        // Gọi hàm draw của phần tử, hàm này sẽ gọi lại render* của FileSVGRenderer (Pattern Visitor)
        element->draw(*this);
    }
    std::cout << "✅ Đã ghi " << elements_.size() << " phần tử ra file SVG." << std::endl;
}