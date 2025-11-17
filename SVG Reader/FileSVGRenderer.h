#ifndef FILESVGRENDERER_H
#define FILESVGRENDERER_H

#include "SVGRenderer.h" // Kế thừa từ lớp cơ sở
#include <fstream>       // Cần cho std::ofstream
#include <string>

// Khai báo trước các lớp hình dạng
class Circle;
class Rect;
class Line;
class Polygon;
class Path;
class Ellipse;
class SVGElement; // Cần cho hàm addElement

/**
 * @brief Lớp triển khai ghi các phần tử ra file SVG.
 */
class FileSVGRenderer : public SVGRenderer
{
public:
    // Constructor và Destructor
    FileSVGRenderer(const std::string& filename);
    ~FileSVGRenderer();

    // --- Khai báo các hàm Render (sử dụng override để ghi đè hàm ảo) ---
    void renderCircle(const Circle& circle) override;
    void renderRect(const Rect& rect) override;
    void renderLine(const Line& line) override;
    void renderPolygon(const Polygon& polygon) override;
    void renderPath(const Path& path) override;
    void renderEllipse(const Ellipse& ellipse) override;

    // Triển khai các hàm từ lớp cơ sở (ghi ra file, không có logic render SFML)
    void render() override; // Ghi tất cả các phần tử ra file
    void addElement(std::shared_ptr<SVGElement> element) override;

private:
    std::ofstream file_;
    std::vector<std::shared_ptr<SVGElement>> elements_; // Để lưu trữ các phần tử
};

#endif // FILESVGRENDERER_H