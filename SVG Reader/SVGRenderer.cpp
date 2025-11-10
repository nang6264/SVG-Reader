#include "SVGRenderer.h"
#include "SVGElement.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cctype>
/*#include "Circle.h"
#include "Rect.h"
#include "Line.h"
#include "Polygon.h"
#include "Path.h"*/
using namespace std;

// --- Constructor ---
SVGRenderer::SVGRenderer(unsigned int width, unsigned int height) {
    // Tạo cửa sổ SFML với kích thước cho trước
    window.create(sf::VideoMode({ width, height }), "SVG Renderer");

    // Lấy view mặc định từ cửa sổ (view giống như camera trong game)
    view = window.getDefaultView();

    cout << "SVGRenderer: Window created " << width << "x" << height << std::endl;
    cout << "Controls: Mouse Wheel = Zoom, R = Rotate, ESC = Exit" << std::endl;
}

// --- Add Element ---
void SVGRenderer::addElement(std::shared_ptr<SVGElement> element) {
    // Thêm phần tử SVG vào danh sách để render sau này
    elements.push_back(element);
    cout << "Element added. Total: " << elements.size() << std::endl;
}

// --- Zoom & Rotate ---
void SVGRenderer::zoomIn() {
    // Zoom in bằng cách thu nhỏ view (0.9f = 90% kích thước hiện tại)
    view.zoom(0.9f);
    cout << "Zoomed In" << std::endl;
}

void SVGRenderer::zoomOut() {
    // Zoom out bằng cách phóng to view (1.1f = 110% kích thước hiện tại)
    view.zoom(1.1f);
    cout << "Zoomed Out" << std::endl;
}

void SVGRenderer::rotate(float angle) {
    // Xoay view theo góc cho trước (đơn vị độ)
    view.rotate(sf::degrees(angle));
    cout << "Rotated by " << angle << " degrees" << std::endl;
}

// --- Render Loop ---
void SVGRenderer::render() {
    // Vòng lặp chính - chạy cho đến khi cửa sổ đóng
    while (window.isOpen()) {
        // Xử lý tất cả sự kiện trong hàng đợi
        while (auto event = window.pollEvent()) {

            // Sự kiện đóng cửa sổ (click nút X)
            if (event->is<sf::Event::Closed>()) {
                window.close();
                cout << "Window closed by user" << std::endl;
            }

            // Sự kiện nhấn phím
            else if (auto* keyEvent = event->getIf<sf::Event::KeyPressed>()) {

                // ESC - thoát chương trình
                if (keyEvent->scancode == sf::Keyboard::Scancode::Escape) {
                    window.close();
                    cout << "Window closed by ESC" << std::endl;
                }

                // R - xoay 10 độ
                else if (keyEvent->scancode == sf::Keyboard::Scancode::R) {
                    rotate(10.f);
                }
            }

            // Sự kiện cuộn chuột - DÙNG CHO ZOOM
            else if (auto* wheelEvent = event->getIf<sf::Event::MouseWheelScrolled>()) {
                // delta > 0: cuộn lên (zoom in), delta < 0: cuộn xuống (zoom out)
                if (wheelEvent->delta > 0) {
                    zoomIn();
                }
                else {
                    zoomOut();
                }
            }
        }

        // Xóa màn hình với màu trắng
        window.clear(sf::Color::White);

        // Áp dụng view hiện tại (có thể đã bị zoom/rotate)
        window.setView(view);

        // Vẽ tất cả các phần tử SVG
        for (auto& element : elements) {
            element->draw(*this); // Gọi hàm draw của từng phần tử (đa hình)
        }

        // Hiển thị everything lên màn hình
        window.display();
    }

    cout << "Render loop ended" << std::endl;
}

// --- Render Circle ---
void SVGRenderer::renderCircle(const Circle& circle) {
    // Tạo hình tròn SFML với bán kính từ Circle
    sf::CircleShape shape(static_cast<float>(circle.getR()));

    // Đặt vị trí: trong SVG, (cx,cy) là tâm, nhưng SFML dùng góc trên trái
    // nên cần trừ bán kính để căn giữa
    shape.setPosition(sf::Vector2f(
        static_cast<float>(circle.getCx() - circle.getR()),
        static_cast<float>(circle.getCy() - circle.getR())
    ));

    // Thiết lập màu sắc và đường viền
    shape.setFillColor(sf::Color::Red);
    shape.setOutlineColor(sf::Color::Black);
    shape.setOutlineThickness(3.f);

    // Vẽ hình tròn lên cửa sổ
    window.draw(shape);
}

// --- Render Rect ---
void SVGRenderer::renderRect(const Rect& rect) {
    // Tạo hình chữ nhật SFML với kích thước từ Rect
    sf::RectangleShape shape(sf::Vector2f(
        static_cast<float>(rect.getWidth()),
        static_cast<float>(rect.getHeight())
    ));

    // Đặt vị trí (trong SVG, (x,y) là góc trên trái)
    shape.setPosition(sf::Vector2f(
        static_cast<float>(rect.getX()),
        static_cast<float>(rect.getY())
    ));

    // Thiết lập màu sắc và đường viền
    shape.setFillColor(sf::Color::Blue);
    shape.setOutlineColor(sf::Color::Black);
    shape.setOutlineThickness(3.f);

    // Vẽ hình chữ nhật
    window.draw(shape);
}

// --- Render Line ---
void SVGRenderer::renderLine(const Line& line) {
    // Lấy tọa độ từ đối tượng Line
    float x1 = static_cast<float>(line.getX1());
    float y1 = static_cast<float>(line.getY1());
    float x2 = static_cast<float>(line.getX2());
    float y2 = static_cast<float>(line.getY2());

    // Tính chiều dài và góc của đường thẳng
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = std::sqrt(dx * dx + dy * dy);
    float angle = std::atan2(dy, dx) * 180.f / 3.14159f;

    // Tạo hình chữ nhật rất hẹp để làm đường thẳng dày
    sf::RectangleShape lineShape(sf::Vector2f(length, 5.f)); // 5.f = độ dày đường thẳng

    // Đặt vị trí (dùng sf::Vector2f)
    lineShape.setPosition(sf::Vector2f(x1, y1));

    // Đặt góc xoay (dùng sf::degrees)
    lineShape.setRotation(sf::degrees(angle));

    // Thiết lập màu sắc
    lineShape.setFillColor(sf::Color::Green);

    // Vẽ đường thẳng
    window.draw(lineShape);
}

// --- Render Polygon ---
void SVGRenderer::renderPolygon(const Polygon& polygon) {
    // Parse chuỗi points từ SVG (định dạng: "x1,y1 x2,y2 x3,y3 ...")
    std::istringstream iss(polygon.getPoints());
    std::vector<sf::Vector2f> points;
    float x, y;
    char comma;

    // Đọc từng cặp tọa độ (x,y)
    while (iss >> x >> comma >> y) {
        points.emplace_back(x, y);
    }

    // Chỉ vẽ nếu có ít nhất 3 điểm (tạo thành đa giác)
    if (points.size() >= 3) {
        sf::ConvexShape shape;
        shape.setPointCount(points.size());

        // Thiết lập từng điểm cho đa giác
        for (size_t i = 0; i < points.size(); ++i) {
            shape.setPoint(i, points[i]);
        }

        // Thiết lập màu sắc
        shape.setFillColor(sf::Color::Yellow);
        shape.setOutlineColor(sf::Color::Black);
        shape.setOutlineThickness(2.f);

        // Vẽ đa giác
        window.draw(shape);
    }
}

// Hàm helper để vẽ đường thẳng giữa hai điểm (PRIVATE)
void SVGRenderer::drawLineBetweenPoints(const sf::Vector2f& p1, const sf::Vector2f& p2, const sf::Color& color) {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    float length = std::sqrt(dx * dx + dy * dy);
    float angle = std::atan2(dy, dx) * 180.f / 3.14159f;

    if (length > 0) { // Chỉ vẽ nếu có chiều dài
        sf::RectangleShape lineShape(sf::Vector2f(length, 3.f)); // Đường dày 3px
        lineShape.setPosition(p1);
        lineShape.setRotation(sf::degrees(angle));
        lineShape.setFillColor(color);
        lineShape.setOutlineColor(sf::Color::Black);
        lineShape.setOutlineThickness(1.f);
        window.draw(lineShape);
    }
}

// --- Render Path ---
void SVGRenderer::renderPath(const Path& path) {
    string pathData = path.getData();
    cout << "Path found: " << pathData << std::endl;

    // Vẽ một hình vuông đơn giản màu tím để đại diện cho Path
    sf::RectangleShape simpleShape;
    simpleShape.setSize(sf::Vector2f(100.f, 100.f));
    simpleShape.setPosition(sf::Vector2f(100.f, 100.f));
    simpleShape.setFillColor(sf::Color::Magenta);
    simpleShape.setOutlineColor(sf::Color::Black);
    simpleShape.setOutlineThickness(2.f);

    window.draw(simpleShape);

    // In thông báo console
    cout << "Path rendered as simple square (complex paths not supported yet)" << std::endl;

}
