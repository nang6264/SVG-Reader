#ifndef SVGRENDERER_H
#define SVGRENDERER_H

#include <SFML/Graphics.hpp>
#include "SVGElement.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include "Transform.h"   // Để dùng TransformMatrix trong RenderState

class SVGElement;
class Circle;
class Rect;
class Line;
class Polygon;
class Ellipse;
class Text;
class Polyline;

// Định nghĩa struct để lưu trữ trạng thái render (Context)
struct RenderState {
    TransformMatrix cumulativeTransform; // Transform tích lũy từ gốc đến hiện tại
    Attributes inheritedAttributes;      // Các thuộc tính cha (fill, stroke, v.v.)
};

// Lớp renderer để hiển thị các phần tử SVG sử dụng SFML
class SVGRenderer {
private:
    std::vector<RenderState> renderStack_; // Stack lưu trữ ngữ cảnh vẽ
    sf::RenderWindow window; // Cửa sổ SFML
    sf::View view;           // Camera để zoom/xoay
    
    // Sử dụng std::vector để lưu trữ các đối tượng hình dạng
    // Dùng shared_ptr vì Parser (có thể) cũng giữ tham chiếu đến chúng
    std::vector<std::shared_ptr<SVGElement>> elements;

    //  Hàm tiện ích nội bộ để vẽ đường thẳng (cho Polygon/Path sau này)
    // void drawLineBetweenPoints(const sf::Vector2f& p1, const sf::Vector2f& p2, const sf::Color& color);

    // Hàm tiện ích nội bộ để chuyển đổi màu SVG (string) sang màu SFML
    sf::Color stringToColor(std::string colorStr, std::string type);
    sf::Font font;

    //Chuyển đổi TransformMatrix sang sf::Transform của SFML.
    sf::Transform getSFMLTransform(const TransformMatrix& matrix) const;
public:
    // Constructor, tạo cửa sổ SFML
    SVGRenderer(unsigned int width = 800, unsigned int height = 600);

    // Thêm một phần tử (đã được parse) vào danh sách chờ vẽ
    void addElement(std::shared_ptr<SVGElement> element);

    // Bắt đầu vòng lặp vẽ chính (main loop)
    void render();

    // --- Các hàm render cụ thể (Interface) ---
    // Được gọi bởi hàm draw() đa hình của các lớp SVGElement
    void renderCircle(const Circle& circle);
    void renderRect(const Rect& rect);
    void renderLine(const Line& line);
    void renderPolygon(const Polygon& polygon);
    void renderEllipse(const Ellipse& ellipse);
    void renderText(const Text& text);
    void renderPolyline(const Polyline& polyline);
    // --- Điều khiển camera ---
    void zoomIn();
    void zoomOut();
    void rotate(float angle);

    // Cập nhật Context trước khi vẽ một phần tử Group hoặc Element
    void beginElement(const TransformMatrix& elementLocalTransform, const Attributes& elementLocalAttrs);

    // Khôi phục Context sau khi vẽ xong
    void endElement();

    // Hàm lấy Transform tổng hợp đang có hiệu lực
    const TransformMatrix& getCumulativeTransform() const;

    // Hàm lấy Attribute đang có hiệu lực (Merge Stack Top và Element Local)
    Attributes getEffectiveAttributes(const Attributes& localAttrs) const;
};

#endif // SVGRENDERER_H
