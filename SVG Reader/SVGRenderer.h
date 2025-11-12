
﻿#ifndef SVGRENDERER_H
#define SVGRENDERER_H

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>

// CHỈ forward declarations - KHÔNG include các file .h cụ thể
class SVGElement;
class Circle;
class Rect;
class Line;
class Polygon;
class Path;

/**
 * @brief Lớp renderer để hiển thị các phần tử SVG sử dụng SFML
 */
class SVGRenderer {
private:
    sf::RenderWindow window;
    sf::View view;
    std::vector<std::shared_ptr<SVGElement>> elements;
    void drawLineBetweenPoints(const sf::Vector2f& p1, const sf::Vector2f& p2, const sf::Color& color);
public:
    SVGRenderer(unsigned int width = 800, unsigned int height = 600);

    void addElement(std::shared_ptr<SVGElement> element);
    void render();

    // Các hàm render cụ thể cho từng loại phần tử SVG
    void renderCircle(const Circle& circle);
    void renderRect(const Rect& rect);
    void renderLine(const Line& line);
    void renderPolygon(const Polygon& polygon);
    void renderPath(const Path& path);

    // Điều khiển camera
    void zoomIn();
    void zoomOut();
    void rotate(float angle);
};


﻿#ifndef SVGRENDERER_H
#define SVGRENDERER_H

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>

// CHỈ forward declarations - KHÔNG include các file .h cụ thể
class SVGElement;
class Circle;
class Rect;
class Line;
class Polygon;
class Path;

/**
 * @brief Lớp renderer để hiển thị các phần tử SVG sử dụng SFML
 */
class SVGRenderer {
private:
    sf::RenderWindow window;
    sf::View view;
    std::vector<std::shared_ptr<SVGElement>> elements;
    void drawLineBetweenPoints(const sf::Vector2f& p1, const sf::Vector2f& p2, const sf::Color& color);
public:
    SVGRenderer(unsigned int width = 800, unsigned int height = 600);

    void addElement(std::shared_ptr<SVGElement> element);
    void render();

    // Các hàm render cụ thể cho từng loại phần tử SVG
    void renderCircle(const Circle& circle);
    void renderRect(const Rect& rect);
    void renderLine(const Line& line);
    void renderPolygon(const Polygon& polygon);
    void renderPath(const Path& path);

    // Điều khiển camera
    void zoomIn();
    void zoomOut();
    void rotate(float angle);
};


#endif
