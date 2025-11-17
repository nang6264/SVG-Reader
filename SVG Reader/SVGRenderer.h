// SVGRenderer.h
#ifndef SVGRENDERER_H
#define SVGRENDERER_H

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>
#include <string>

// Khai báo trước các lớp hình dạng (Data Model)
// Chúng ta KHÔNG include "SVGElement.h" ở đây để tránh phụ thuộc vòng
class SVGElement;
class Circle;
class Rect;
class Line;
class Polygon;
class Path;
class Ellipse;

/**
 * @brief Lớp renderer để hiển thị các phần tử SVG sử dụng SFML
 */
class SVGRenderer {
private:
    sf::RenderWindow window; // Cửa sổ SFML
    sf::View view;           // Camera để zoom/xoay

    // Sử dụng std::vector để lưu trữ các đối tượng hình dạng
    // Dùng shared_ptr vì Parser (có thể) cũng giữ tham chiếu đến chúng
    std::vector<std::shared_ptr<SVGElement>> elements;

    /**
     * @brief Hàm tiện ích nội bộ để vẽ đường thẳng (cho Polygon/Path sau này)
     */
    void drawLineBetweenPoints(const sf::Vector2f& p1, const sf::Vector2f& p2, const sf::Color& color);

    /**
     * @brief Hàm tiện ích nội bộ để chuyển đổi màu SVG (string) sang màu SFML
     */
    sf::Color stringToColor(std::string colorStr, std::string type);

public:
    /**
     * @brief Constructor, tạo cửa sổ SFML
     */
    SVGRenderer(unsigned int width = 800, unsigned int height = 600);

    /**
     * @brief Thêm một phần tử (đã được parse) vào danh sách chờ vẽ
     */
    virtual void addElement(std::shared_ptr<SVGElement> element);

    /**
     * @brief Bắt đầu vòng lặp vẽ chính (main loop)
     */
    virtual void render();

    // --- Các hàm render cụ thể (Interface) ---
    // Được gọi bởi hàm draw() đa hình của các lớp SVGElement
    virtual void renderCircle(const Circle& circle);
    virtual void renderRect(const Rect& rect);
    virtual void renderLine(const Line& line);
    virtual void renderPolygon(const Polygon& polygon);
    virtual void renderPath(const Path& path);
    virtual void renderEllipse(const Ellipse& ellipse);

    // --- Điều khiển camera ---
    void zoomIn();
    void zoomOut();
    void rotate(float angle);
};

// Khai báo hàm manual_clamp nếu nó là global (như đã định nghĩa trong SVGRenderer.cpp)
template <typename T>
constexpr const T manual_clamp(const T& v, const T& lo, const T& hi);

#endif // SVGRENDERER_H