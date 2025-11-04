// SVGRenderer.h
#ifndef SVGRENDERER_H
#define SVGRENDERER_H

// Khai báo trước (forward declaration) các lớp SVGElement
// để tránh phụ thuộc vòng và lỗi biên dịch.
class Circle;
class Rect;
class Line;
class Polygon;
class Path;
class Ellipse;

/**
 * @brief Lớp trừu tượng (interface) định nghĩa giao diện cho việc render (vẽ) các phần tử SVG.
 * * Lớp này sử dụng mô hình Visitor hoặc Strategy đơn giản để cho phép các phần tử SVG
 * gọi hàm render cụ thể tương ứng trong một lớp Renderer.
 */
class SVGRenderer {
public:
    // Destructor ảo (virtual destructor) là cần thiết cho lớp cơ sở.
    virtual ~SVGRenderer() = default;

    // Các hàm vẽ thuần ảo (pure virtual function) cho từng loại phần tử.
    // Lớp triển khai cụ thể (ví dụ: ConsoleRenderer, FileRenderer) phải override tất cả.
    virtual void renderCircle(const Circle& circle) = 0;
    virtual void renderRect(const Rect& rect) = 0;
    virtual void renderLine(const Line& line) = 0;
    virtual void renderPolygon(const Polygon& polygon) = 0;
    virtual void renderPath(const Path& path) = 0;
    virtual void renderEllipse(const Ellipse& ellipse) = 0;
};

#endif // SVGRENDERER_H