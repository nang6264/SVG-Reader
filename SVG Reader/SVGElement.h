// SVGElement.h
#ifndef SVGELEMENT_H
#define SVGELEMENT_H

#include <string>
#include <map>
#include "SVGRenderer.h" // Cần bao gồm để sử dụng SVGRenderer

// Định nghĩa chung cho tất cả các phần tử SVG
using Attributes = std::map<std::string, std::string>;

/**
 * @brief Lớp cơ sở trừu tượng cho tất cả các phần tử SVG (Circle, Rect, v.v.).
 * * Lớp này định nghĩa giao diện chung, bao gồm hàm draw() thuần ảo cho đa hình
 * và constructor để đọc các thuộc tính chung từ một map.
 */
class SVGElement {
protected:
    // Lưu trữ các thuộc tính của phần tử (ví dụ: "stroke", "fill", "stroke-width")
    Attributes attributes_;

public:
    // Constructor cơ bản.
    SVGElement(const Attributes& attributes);

    // Destructor ảo (virtual destructor) là bắt buộc cho lớp cơ sở.
    virtual ~SVGElement() = default;

    // ham ve
    virtual void draw(SVGRenderer& renderer) const = 0;

    // lay ten phan tu: circle,...
    virtual std::string getElementName() const = 0;
    
    // Hàm truy cập (getter) đơn giản để lấy thuộc tính
    const Attributes& getAttributes() const { return attributes_; }
};

// --- Khai báo các lớp con ---

// Biểu diễn phần tử SVG <circle>.
class Circle : public SVGElement {
private:
    // Thuộc tính cụ thể của Circle: cx, cy, r
    double cx_ = 0.0, cy_ = 0.0, r_ = 0.0;

public:
    // constructor
    Circle(const Attributes& attributes);
    
    // Override hàm draw() để gọi hàm renderCircle() của renderer.
    void draw(SVGRenderer& renderer) const override;

    // Override hàm lấy tên phần tử.
    std::string getElementName() const override { return "circle"; }

    // Getters cho thuộc tính cụ thể
    double getCx() const { return cx_; }
    double getCy() const { return cy_; }
    double getR() const { return r_; }
};

// Biểu diễn phần tử SVG <rect>.
class Rect : public SVGElement {
private:
    double x_ = 0.0, y_ = 0.0, width_ = 0.0, height_ = 0.0;
    // Có thể thêm rx, ry

public:
    // constructor
    Rect(const Attributes& attributes);
    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "rect"; }

    double getX() const { return x_; }
    double getY() const { return y_; }
    double getWidth() const { return width_; }
    double getHeight() const { return height_; }
};

// Biểu diễn phần tử SVG <line>.
class Line : public SVGElement {
private:
    // Thuộc tính cụ thể: x1, y1, x2, y2
    double x1_ = 0.0, y1_ = 0.0, x2_ = 0.0, y2_ = 0.0;

public:
    // constructor
    Line(const Attributes& attributes);

    // Override hàm draw()
    void draw(SVGRenderer& renderer) const override;

    // Override hàm lấy tên phần tử.
    std::string getElementName() const override { return "line"; }

    // Getters cho thuộc tính cụ thể
    double getX1() const { return x1_; }
    double getY1() const { return y1_; }
    double getX2() const { return x2_; }
    double getY2() const { return y2_; }
};

// Biểu diễn phần tử SVG <polygon>.
class Polygon : public SVGElement {
private:
    // Thuộc tính "points" được lưu dưới dạng một chuỗi
    std::string points_;

public:
    // constructor
    Polygon(const Attributes& attributes);

    // Override hàm draw()
    void draw(SVGRenderer& renderer) const override;

    // Override hàm lấy tên phần tử.
    std::string getElementName() const override { return "polygon"; }

    // Getter cho thuộc tính points
    const std::string& getPoints() const { return points_; }
};

// Biểu diễn phần tử SVG <path>.
class Path : public SVGElement {
private:
    // Thuộc tính "d" (path data) được lưu dưới dạng một chuỗi
    std::string d_; // "d" là tên thuộc tính SVG cho path data

public:
    // constructor
    Path(const Attributes& attributes);

    // Override hàm draw()
    void draw(SVGRenderer& renderer) const override;

    // Override hàm lấy tên phần tử.
    std::string getElementName() const override { return "path"; }

    // Getter cho thuộc tính 'd'
    const std::string& getData() const { return d_; }
};

// Biểu diễn phần tử SVG <ellipse>.
class Ellipse : public SVGElement {
private:
    // Thuộc tính riêng: cx, cy, rx, ry
    double cx_ = 0.0, cy_ = 0.0, rx_ = 0.0, ry_ = 0.0;

public:
    // constructor
    Ellipse(const Attributes& attributes);

    // Override hàm draw()
    void draw(SVGRenderer& renderer) const override;

    // Override hàm lấy tên phần tử
    std::string getElementName() const override { return "ellipse"; }

    // Getters cho thuộc tính cụ thể
    double getCx() const { return cx_; }
    double getCy() const { return cy_; }
    double getRx() const { return rx_; }
    double getRy() const { return ry_; }
};

/**
 * @brief Biểu diễn phần tử SVG <text>.
 */
class Text : public SVGElement {
private:
    double x_ = 0.0, y_ = 0.0;
    double fontSize_ = 12.0;
    std::string content_; // Lưu nội dung chữ ("Hello World")

public:
    Text(const Attributes& attributes, const std::string& content);

    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "text"; }

    double getX() const { return x_; }
    double getY() const { return y_; }
    double getFontSize() const { return fontSize_; }
    const std::string& getContent() const { return content_; }
};


/**
 * @brief Biểu diễn phần tử SVG <polyline> (Đường gấp khúc hở).
 */
class Polyline : public SVGElement {
private:
    std::string points_;

public:
    Polyline(const Attributes& attributes);
    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "polyline"; }
    const std::string& getPoints() const { return points_; }
};
#endif // SVGELEMENT_H