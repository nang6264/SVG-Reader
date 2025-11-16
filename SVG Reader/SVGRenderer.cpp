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
    // ... (Tạo sf::CircleShape shape và setPosition như cũ) ...
    sf::CircleShape shape(static_cast<float>(circle.getR()));
    // ...

    // Lấy map thuộc tính
    const auto& attributes = circle.getAttributes();

    // 1. Xử lý Màu FILL
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "none";
    shape.setFillColor(stringToColor(fill_color_str, "fill"));

    // 2. Xử lý Màu STROKE (Đường viền)
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "black";
    shape.setOutlineColor(stringToColor(stroke_color_str, "stroke"));

    // 3. Xử lý Độ dày STROKE
    auto it_width = attributes.find("stroke-width");
    float thickness = 0.f;
    if (it_width != attributes.end()) {
        try {
            // Chuyển chuỗi sang float (cần handle đơn vị, nhưng tạm thời bỏ qua)
            thickness = std::stof(it_width->second);
        }
        catch (...) {
            thickness = 1.f; // Mặc định nếu parse lỗi
        }
    }
    else {
        thickness = 1.f; // Mặc định nếu không có thuộc tính
    }
    shape.setOutlineThickness(thickness);

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

    // Đặt vị trí (SVG (x,y) là góc trên trái, giống SFML)
    shape.setPosition(sf::Vector2f(
        static_cast<float>(rect.getX()),
        static_cast<float>(rect.getY())
    ));

    // Lấy map thuộc tính từ lớp cơ sở SVGElement
    const auto& attributes = rect.getAttributes();

    // 1. Xử lý Màu FILL
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "none";
    // Giả định hàm stringToColor đã được định nghĩa và có thể truy cập
    shape.setFillColor(stringToColor(fill_color_str, "fill"));

    // 2. Xử lý Màu STROKE (Đường viền)
    auto it_stroke = attributes.find("stroke");
    // Mặc định stroke là 'black' nếu không có, và cần set stroke-width
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "black";
    shape.setOutlineColor(stringToColor(stroke_color_str, "stroke"));

    // 3. Xử lý Độ dày STROKE (stroke-width)
    auto it_width = attributes.find("stroke-width");
    float thickness = 0.f; // Mặc định không có đường viền
    if (it_width != attributes.end()) {
        try {
            // Chuyển chuỗi sang float
            thickness = std::stof(it_width->second);
        }
        catch (...) {
            thickness = 1.f; // Giá trị mặc định an toàn
        }
    }
    else {
        // Nếu không có stroke-width, set độ dày mặc định chỉ khi có stroke color
        thickness = (stroke_color_str != "none" && thickness == 0.f) ? 1.f : 0.f;
    }

    // Đảm bảo độ dày không âm
    shape.setOutlineThickness(std::max(0.f, thickness));

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

    // Lấy map thuộc tính từ lớp cơ sở SVGElement
    const auto& attributes = line.getAttributes();

    // 1. Xử lý Độ dày STROKE (stroke-width)
    auto it_width = attributes.find("stroke-width");
    float thickness = 1.f; // Mặc định là 1.0f nếu không được định nghĩa
    if (it_width != attributes.end()) {
        try {
            thickness = std::stof(it_width->second);
        }
        catch (...) {
            thickness = 1.f;
        }
    }
    // Đảm bảo độ dày không âm
    thickness = std::max(0.f, thickness);

    // 2. Xử lý Màu STROKE (Đường viền)
    auto it_stroke = attributes.find("stroke");
    // Mặc định stroke là 'black' nếu không có (theo quy ước SVG)
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "black";
    sf::Color stroke_color = stringToColor(stroke_color_str, "stroke");

    // Nếu màu stroke là 'none' hoặc trong suốt, không cần vẽ
    if (stroke_color == sf::Color::Transparent || thickness == 0.f) {
        return;
    }

    // 3. Tính toán hình dạng Line (sử dụng sf::RectangleShape)

    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = std::sqrt(dx * dx + dy * dy);
    // Tính góc xoay (radian -> độ)
    float angle = std::atan2(dy, dx) * 180.f / 3.14159f;

    // Tạo hình chữ nhật rất hẹp để làm đường thẳng
    sf::RectangleShape lineShape(sf::Vector2f(length, thickness));

    // Đặt vị trí bắt đầu
    lineShape.setPosition(sf::Vector2f(x1, y1));

    // Đặt góc xoay
    lineShape.setRotation(sf::degrees(angle));

    // Thiết lập màu sắc và fill (cả fill và outline đều là màu stroke)
    lineShape.setFillColor(stroke_color);
    lineShape.setOutlineThickness(0.f); // Không cần outline cho Line

    // Vẽ đường thẳng
    window.draw(lineShape);
}

// --- Render Polygon ---
void SVGRenderer::renderPolygon(const Polygon& polygon) {
    // 1. Phân tích cú pháp chuỗi points
    std::istringstream iss(polygon.getPoints());
    std::vector<sf::Vector2f> points;
    float x, y;
    char comma;

    // Đọc từng cặp tọa độ (x,y), cho phép dấu phẩy hoặc khoảng trắng
    while (iss >> x) {
        // Kiểm tra xem ký tự tiếp theo là dấu phẩy hay khoảng trắng
        if (iss.peek() == ',') {
            iss >> comma; // Đọc dấu phẩy
        }
        if (iss >> y) {
            points.emplace_back(x, y);
        }
        else {
            // Trường hợp lỗi: đã đọc x nhưng không đọc được y
            break;
        }
    }

    // Chỉ vẽ nếu có ít nhất 3 điểm
    if (points.size() < 3) {
        return;
    }

    // 2. Tạo hình dạng SFML
    sf::ConvexShape shape;
    shape.setPointCount(points.size());

    // Thiết lập từng điểm
    for (size_t i = 0; i < points.size(); ++i) {
        shape.setPoint(i, points[i]);
    }

    // 3. Áp dụng Thuộc tính SVG
    const auto& attributes = polygon.getAttributes();

    // --- FILL ---
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black"; // Mặc định fill là đen
    shape.setFillColor(stringToColor(fill_color_str, "fill"));

    // --- STROKE WIDTH ---
    auto it_width = attributes.find("stroke-width");
    float thickness = 0.f;
    if (it_width != attributes.end()) {
        try {
            thickness = std::stof(it_width->second);
        }
        catch (...) {
            thickness = 1.f;
        }
    }
    // Đảm bảo độ dày không âm
    thickness = std::max(0.f, thickness);

    // --- STROKE COLOR ---
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";

    // Nếu có stroke-width nhưng stroke color là "none", đặt màu mặc định
    if (thickness > 0.f && stroke_color_str == "none") {
        stroke_color_str = "black";
    }

    shape.setOutlineColor(stringToColor(stroke_color_str, "stroke"));
    shape.setOutlineThickness(thickness);

    // 4. Vẽ đa giác
    window.draw(shape);
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
    std::string pathData = path.getData();
    // Lấy map thuộc tính từ lớp cơ sở SVGElement
    const auto& attributes = path.getAttributes();

    // === CÁC BƯỚC NÀY CHỈ LÀ TẠM THỜI VÀ ĐẠI DIỆN CHO PATH ===

    // Tạo hình chữ nhật đơn giản để đại diện cho Path
    sf::RectangleShape simpleShape;
    simpleShape.setSize(sf::Vector2f(100.f, 100.f));
    // Sử dụng tọa độ mặc định, hoặc có thể đọc từ 'transform' nếu có
    simpleShape.setPosition(sf::Vector2f(100.f, 100.f));

    // 1. Áp dụng Thuộc tính SVG

    // --- FILL ---
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";
    simpleShape.setFillColor(stringToColor(fill_color_str, "fill"));

    // --- STROKE WIDTH ---
    auto it_width = attributes.find("stroke-width");
    float thickness = 0.f;
    if (it_width != attributes.end()) {
        try {
            thickness = std::stof(it_width->second);
        }
        catch (...) {
            thickness = 1.f;
        }
    }
    thickness = std::max(0.f, thickness);

    // --- STROKE COLOR ---
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";

    if (thickness > 0.f && stroke_color_str == "none") {
        stroke_color_str = "black";
    }

    simpleShape.setOutlineColor(stringToColor(stroke_color_str, "stroke"));
    simpleShape.setOutlineThickness(thickness);

    // Vẽ hình đại diện
    window.draw(simpleShape);

    // Cảnh báo (tùy chọn)
    // std::cout << "Path rendered as simple square. Data: " << pathData << std::endl;
}

// --- Render Ellipse ---
void SVGRenderer::renderEllipse(const Ellipse& ellipse) {
    // 1. Tạo hình tròn SFML (vì SFML không có lớp Ellipse riêng biệt)
    // Dùng bán kính X hoặc Y, sau đó dùng scale để tạo hình ellipse.
    float baseRadius = static_cast<float>(ellipse.getRx());
    sf::CircleShape shape(baseRadius);

    // 2. Đặt vị trí
    // Tương tự Circle, vị trí SFML là góc trên trái, cần trừ bán kính (baseRadius)
    shape.setPosition(sf::Vector2f(
        static_cast<float>(ellipse.getCx() - baseRadius),
        static_cast<float>(ellipse.getCy() - baseRadius)
    ));

    // 3. Áp dụng Tỉ lệ (Scale)
    // Tỉ lệ Y = (Bán kính Y) / (Bán kính cơ sở)
    float scaleY = static_cast<float>(ellipse.getRy()) / baseRadius;
    shape.setScale(1.0f, scaleY); // Scale X là 1.0f (vì đã dùng rx làm baseRadius)

    // 4. Áp dụng Thuộc tính Màu sắc và Đường viền (Tương tự Circle/Rect)
    const auto& attributes = ellipse.getAttributes();

    // --- FILL ---
    auto it_fill = attributes.find("fill");
    std::string fill_color_str = (it_fill != attributes.end()) ? it_fill->second : "black";
    shape.setFillColor(stringToColor(fill_color_str, "fill"));

    // --- STROKE WIDTH ---
    auto it_width = attributes.find("stroke-width");
    float thickness = 0.f;
    if (it_width != attributes.end()) {
        try {
            // Lưu ý: Độ dày đường viền cũng sẽ bị scale, nhưng ta bỏ qua để đơn giản
            thickness = std::stof(it_width->second);
        }
        catch (...) {
            thickness = 1.f;
        }
    }
    thickness = std::max(0.f, thickness);

    // --- STROKE COLOR ---
    auto it_stroke = attributes.find("stroke");
    std::string stroke_color_str = (it_stroke != attributes.end()) ? it_stroke->second : "none";
    if (thickness > 0.f && stroke_color_str == "none") {
        stroke_color_str = "black";
    }

    shape.setOutlineColor(stringToColor(stroke_color_str, "stroke"));
    shape.setOutlineThickness(thickness);

    // 5. Vẽ hình ellipse
    window.draw(shape);
}