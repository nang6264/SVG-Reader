// FileSVGRenderer.cpp
#include "FileSVGRenderer.h"
#include "SVGElement.h" // Cần include để truy cập các lớp con (Circle, Rect, v.v.)
#include <iostream>     // Để báo lỗi (nếu có)
#include <set>          // Dùng để tạo danh sách các thuộc tính cần bỏ qua

/**
 * @brief HÀM TIỆN ÍCH: Ghi các thuộc tính chung (fill, stroke, v.v.)
 * * Hàm này lặp qua map thuộc tính và ghi tất cả, TRỪ những thuộc tính
 * trong danh sách 'skipAttributes'.
 */
static void writeCommonAttributes(std::ofstream &file, const SVGElement &element,
                                  const std::set<std::string> &skipAttributes)
{
    // Lặp qua tất cả các thuộc tính được lưu trong map
    for (const auto &attr : element.getAttributes())
    {
        // Nếu tên thuộc tính (attr.first) KHÔNG nằm trong danh sách bỏ qua (skipAttributes)
        // std::set::find() trả về .end() nếu không tìm thấy
        if (skipAttributes.find(attr.first) == skipAttributes.end())
        {
            // Ghi thuộc tính ra file (ví dụ: fill="red")
            file << " " << attr.first << "=\"" << attr.second << "\"";
        }
    }
}

// --- Constructor và Destructor ---
// (Giả định bạn đã có 2 hàm này, chúng quản lý việc mở/đóng file và ghi tag <svg>)

FileSVGRenderer::FileSVGRenderer(const std::string &filename)
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

// --- Triển khai các hàm Render ---

void FileSVGRenderer::renderCircle(const Circle &circle)
{
    if (!file_.is_open())
        return;

    // 1. Bắt đầu tag và ghi các thuộc tính riêng
    file_ << "  <circle cx=\"" << circle.getCx()
          << "\" cy=\"" << circle.getCy()
          << "\" r=\"" << circle.getR() << "\"";

    // 2. GỌI HÀM TIỆN ÍCH để ghi thuộc tính chung
    //    Bỏ qua các thuộc tính đã ghi ở trên
    writeCommonAttributes(file_, circle, {"cx", "cy", "r"});

    // 3. Đóng tag
    file_ << "/>\n";
}

void FileSVGRenderer::renderRect(const Rect &rect)
{
    if (!file_.is_open())
        return;

    // 1. Ghi thuộc tính riêng
    file_ << "  <rect x=\"" << rect.getX()
          << "\" y=\"" << rect.getY()
          << "\" width=\"" << rect.getWidth()
          << "\" height=\"" << rect.getHeight() << "\"";

    // 2. Ghi thuộc tính chung, bỏ qua các thuộc tính riêng của Rect
    writeCommonAttributes(file_, rect, {"x", "y", "width", "height"});

    // 3. Đóng tag
    file_ << "/>\n";
}

void FileSVGRenderer::renderLine(const Line &line)
{
    if (!file_.is_open())
        return;

    // 1. Ghi thuộc tính riêng
    file_ << "  <line x1=\"" << line.getX1()
          << "\" y1=\"" << line.getY1()
          << "\" x2=\"" << line.getX2()
          << "\" y2=\"" << line.getY2() << "\"";

    // 2. Ghi thuộc tính chung
    writeCommonAttributes(file_, line, {"x1", "y1", "x2", "y2"});

    // 3. Đóng tag
    file_ << "/>\n";
}

void FileSVGRenderer::renderPolygon(const Polygon &polygon)
{
    if (!file_.is_open())
        return;

    // 1. Ghi thuộc tính riêng
    file_ << "  <polygon points=\"" << polygon.getPoints() << "\"";

    // 2. Ghi thuộc tính chung
    writeCommonAttributes(file_, polygon, {"points"});

    // 3. Đóng tag
    file_ << "/>\n";
}

void FileSVGRenderer::renderPath(const Path &path)
{
    if (!file_.is_open())
        return;

    // 1. Ghi thuộc tính riêng
    file_ << "  <path d=\"" << path.getData() << "\"";

    // 2. Ghi thuộc tính chung
    writeCommonAttributes(file_, path, {"d"});

    // 3. Đóng tag
    file_ << "/>\n";
}

void FileSVGRenderer::renderEllipse(const Ellipse &ellipse)
{
    if (!file_.is_open())
        return;

    // 1. Ghi thuộc tính riêng
    file_ << "  <ellipse cx=\"" << ellipse.getCx()
          << "\" cy=\"" << ellipse.getCy()
          << "\" rx=\"" << ellipse.getRx()
          << "\" ry=\"" << ellipse.getRy() << "\"";

    // 2. Ghi thuộc tính chung
    writeCommonAttributes(file_, ellipse, {"cx", "cy", "rx", "ry"});

    // 3. Đóng tag
    file_ << "/>\n";
}