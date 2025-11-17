// SVGElement.h
#ifndef SVGELEMENT_H
#define SVGELEMENT_H

#include <string>
#include <map>
#include "SVGRenderer.h" 

// Định nghĩa chung cho tất cả các phần tử SVG
using Attributes = std::map<std::string, std::string>;

class SVGElement {
protected:
    Attributes attributes_;

public:
    SVGElement(const Attributes& attributes);
    virtual ~SVGElement() = default;

    virtual void draw(SVGRenderer& renderer) const = 0;
    virtual std::string getElementName() const = 0;

    // Thêm hàm getAttribute
    std::string getAttribute(const std::string& name) const {
        auto it = attributes_.find(name);
        if (it != attributes_.end()) return it->second;
        return "";
    }
    const Attributes& getAttributes() const { return attributes_; }
};

// --- Khai báo các lớp con (Circle, Rect, Line, Polygon, Path, Ellipse) ---
// (Phần này giữ nguyên các thành viên private và public đã có trong file gốc của bạn)

class Circle : public SVGElement {
private: double cx_ = 0.0, cy_ = 0.0, r_ = 0.0;
public:
    Circle(const Attributes& attributes);
    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "circle"; }
    double getCx() const { return cx_; }
    double getCy() const { return cy_; }
    double getR() const { return r_; }
};

class Rect : public SVGElement {
private: double x_ = 0.0, y_ = 0.0, width_ = 0.0, height_ = 0.0;
public:
    Rect(const Attributes& attributes);
    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "rect"; }
    double getX() const { return x_; }
    double getY() const { return y_; }
    double getWidth() const { return width_; }
    double getHeight() const { return height_; }
};

class Line : public SVGElement {
private: double x1_ = 0.0, y1_ = 0.0, x2_ = 0.0, y2_ = 0.0;
public:
    Line(const Attributes& attributes);
    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "line"; }
    double getX1() const { return x1_; }
    double getY1() const { return y1_; }
    double getX2() const { return x2_; }
    double getY2() const { return y2_; }
};

class Polygon : public SVGElement {
private: std::string points_;
public:
    Polygon(const Attributes& attributes);
    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "polygon"; }
    const std::string& getPoints() const { return points_; }
};

class Path : public SVGElement {
private: std::string d_;
public:
    Path(const Attributes& attributes);
    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "path"; }
    const std::string& getData() const { return d_; }
};

class Ellipse : public SVGElement {
private: double cx_ = 0.0, cy_ = 0.0, rx_ = 0.0, ry_ = 0.0;
public:
    Ellipse(const Attributes& attributes);
    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "ellipse"; }
    double getCx() const { return cx_; }
    double getCy() const { return cy_; }
    double getRx() const { return rx_; }
    double getRy() const { return ry_; }
};

#endif // SVGELEMENT_H