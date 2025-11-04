// FileSVGRenderer.h
#include "SVGRenderer.h"
#include <fstream>

class FileSVGRenderer : public SVGRenderer {
private:
    std::ofstream file_;
    // Thêm các thuộc tính cần thiết khác

public:
    FileSVGRenderer(const std::string& filename);
    ~FileSVGRenderer();

    // Triển khai tất cả các hàm thuần ảo
    void renderCircle(const Circle& circle) override;
    void renderRect(const Rect& rect) override;
    void renderLine(const Line& line) override;
    void renderPolygon(const Polygon& polygon) override;
    void renderPath(const Path& path) override;
    void renderEllipse(const Ellipse& ellipse) override;
};