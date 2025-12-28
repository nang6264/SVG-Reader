// Gradient.h
#ifndef GRADIENT_H
#define GRADIENT_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// GradientStop: Điểm dừng màu trong gradient
struct GradientStop {
    float offset;      // 0.0 đến 1.0 (0% -> 100%)
    sf::Color color;

    GradientStop(float off = 0.0f, const sf::Color& col = sf::Color::Black)
        : offset(off), color(col) {
    }

    bool operator<(const GradientStop& other) const {
        return offset < other.offset;
    }
};

// Simple 2D Transform Matrix for Gradients
struct GradientTransform {
    float a, b, c, d, e, f;  // matrix(a, b, c, d, e, f)

    // Constructor - Identity matrix by default
    GradientTransform() : a(1), b(0), c(0), d(1), e(0), f(0) {}

    GradientTransform(float a_, float b_, float c_, float d_, float e_, float f_)
        : a(a_), b(b_), c(c_), d(d_), e(e_), f(f_) {
    }

    // Transform a point
    void transformPoint(float x, float y, float& outX, float& outY) const {
        outX = a * x + c * y + e;
        outY = b * x + d * y + f;
    }

    // Get inverse transform (for transforming coordinates back)
    GradientTransform inverse() const {
        float det = a * d - b * c;
        if (std::abs(det) < 0.0001f) {
            return GradientTransform(); // Return identity if singular
        }

        float invDet = 1.0f / det;
        return GradientTransform(
            d * invDet,
            -b * invDet,
            -c * invDet,
            a * invDet,
            (c * f - d * e) * invDet,
            (b * e - a * f) * invDet
        );
    }

    // Check if this is identity matrix
    bool isIdentity() const {
        return std::abs(a - 1.0f) < 0.0001f &&
            std::abs(b) < 0.0001f &&
            std::abs(c) < 0.0001f &&
            std::abs(d - 1.0f) < 0.0001f &&
            std::abs(e) < 0.0001f &&
            std::abs(f) < 0.0001f;
    }
};

// Gradient: Struct chứa dữ liệu Gradient
struct Gradient {
    std::string id;
    std::string type;  // "linear" hoặc "radial"

    std::vector<GradientStop> stops;
    GradientTransform transform;
    std::string gradientUnits = "objectBoundingBox";

    // Linear Gradient Properties
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 1.0f;
    float y2 = 0.0f;

    // Radial Gradient Properties
    float cx = 0.5f;
    float cy = 0.5f;
    float r = 0.5f;
    float fx = -1.0f;
    float fy = -1.0f;
    float fr = 0.0f;

    // UTILITY FUNCTIONS

    void sortStops() {
        std::sort(stops.begin(), stops.end());
    }

    void validateStops() {
        if (stops.empty()) {
            stops.push_back(GradientStop(0.0f, sf::Color::Black));
            stops.push_back(GradientStop(1.0f, sf::Color::White));
        }
        sortStops();
        for (auto& stop : stops) {
            stop.offset = std::max(0.0f, std::min(1.0f, stop.offset));
        }
    }

    // COLOR INTERPOLATION - Core algorithm

    sf::Color interpolateColor(float t) const {
        t = std::max(0.0f, std::min(1.0f, t));

        if (stops.empty()) return sf::Color::Black;
        if (stops.size() == 1) return stops[0].color;

        // Tìm 2 stops bao quanh t
        size_t leftIdx = 0;
        size_t rightIdx = stops.size() - 1;

        for (size_t i = 0; i < stops.size(); i++) {
            if (stops[i].offset <= t) leftIdx = i;
            if (stops[i].offset >= t) {
                rightIdx = i;
                break;
            }
        }

        if (leftIdx == rightIdx) return stops[leftIdx].color;

        const GradientStop& left = stops[leftIdx];
        const GradientStop& right = stops[rightIdx];

        float localT = (t - left.offset) / (right.offset - left.offset);

        return sf::Color(
            static_cast<std::uint8_t>(left.color.r + (right.color.r - left.color.r) * localT),
            static_cast<std::uint8_t>(left.color.g + (right.color.g - left.color.g) * localT),
            static_cast<std::uint8_t>(left.color.b + (right.color.b - left.color.b) * localT),
            static_cast<std::uint8_t>(left.color.a + (right.color.a - left.color.a) * localT)
        );
    }

    // Linear Gradient Implementation WITH TRANSFORM

    sf::Color getLinearColorAt(float x, float y, const sf::FloatRect& boundingBox) const {
        // Step 1: Apply inverse transform to the point
        float tx = x, ty = y;
        if (!transform.isIdentity()) {
            GradientTransform invTransform = transform.inverse();
            invTransform.transformPoint(x, y, tx, ty);
        }

        // Step 2: Convert gradient coordinates to user space
        float px1 = x1, py1 = y1, px2 = x2, py2 = y2;

        if (gradientUnits == "objectBoundingBox") {
            px1 = boundingBox.position.x + x1 * boundingBox.size.x;
            py1 = boundingBox.position.y + y1 * boundingBox.size.y;
            px2 = boundingBox.position.x + x2 * boundingBox.size.x;
            py2 = boundingBox.position.y + y2 * boundingBox.size.y;
        }

        // Step 3: Calculate gradient position
        sf::Vector2f start(px1, py1);
        sf::Vector2f end(px2, py2);

        float dx = end.x - start.x;
        float dy = end.y - start.y;
        float lengthSq = dx * dx + dy * dy;

        if (lengthSq < 0.0001f) {
            return stops.empty() ? sf::Color::Black : stops[0].color;
        }

        float t = ((tx - start.x) * dx + (ty - start.y) * dy) / lengthSq;
        return interpolateColor(t);
    }

    // Radial Gradient Implementation WITH TRANSFORM

    sf::Color getRadialColorAt(float x, float y, const sf::FloatRect& boundingBox) const {
        // Step 1: Apply inverse transform to the point
        float tx = x, ty = y;
        if (!transform.isIdentity()) {
            GradientTransform invTransform = transform.inverse();
            invTransform.transformPoint(x, y, tx, ty);
        }

        // Step 2: Convert gradient coordinates to user space
        float pcx = cx, pcy = cy, pr = r;
        float pfx = (fx < 0.0f) ? cx : fx;
        float pfy = (fy < 0.0f) ? cy : fy;

        if (gradientUnits == "objectBoundingBox") {
            pcx = boundingBox.position.x + cx * boundingBox.size.x;
            pcy = boundingBox.position.y + cy * boundingBox.size.y;
            pr = r * std::max(boundingBox.size.x, boundingBox.size.y);

            pfx = boundingBox.position.x + pfx * boundingBox.size.x;
            pfy = boundingBox.position.y + pfy * boundingBox.size.y;
        }

        // Step 3: Calculate distance from focal point
        float dx = tx - pfx;
        float dy = ty - pfy;
        float distToFocal = std::sqrt(dx * dx + dy * dy);

        if (pr < 0.0001f) {
            return stops.empty() ? sf::Color::Black : stops[0].color;
        }

        float t = distToFocal / pr;
        return interpolateColor(t);
    }

    // FALLBACK MECHANISMS

    // Convert radial → linear fallback
    Gradient toLinearFallback() const {
        Gradient linear;
        linear.id = id + "_linear_fallback";
        linear.type = "linear";
        linear.stops = stops;
        linear.gradientUnits = gradientUnits;
        linear.transform = transform;  // Copy transform

        float pfx = (fx < 0.0f) ? cx : fx;
        float pfy = (fy < 0.0f) ? cy : fy;

        float dx = cx - pfx;
        float dy = cy - pfy;
        float len = std::sqrt(dx * dx + dy * dy);

        if (len < 0.0001f) {
            linear.x1 = cx - r;
            linear.y1 = cy;
            linear.x2 = cx + r;
            linear.y2 = cy;
        }
        else {
            dx /= len;
            dy /= len;

            linear.x1 = pfx;
            linear.y1 = pfy;
            linear.x2 = pfx + dx * r;
            linear.y2 = pfy + dy * r;
        }

        return linear;
    }

    // Weighted average color
    sf::Color getAverageColor(float opacity = 1.0f) const {
        if (stops.empty()) return sf::Color::Black;
        if (stops.size() == 1) {
            sf::Color c = stops[0].color;
            c.a = static_cast<std::uint8_t>(c.a * opacity);
            return c;
        }

        float totalWeight = 0.0f;
        float weightedR = 0.0f;
        float weightedG = 0.0f;
        float weightedB = 0.0f;
        float weightedA = 0.0f;

        for (size_t i = 0; i < stops.size(); i++) {
            float weight = 1.0f;

            if (i > 0) {
                weight = stops[i].offset - stops[i - 1].offset;
            }
            else if (stops.size() > 1) {
                weight = stops[i + 1].offset - stops[i].offset;
            }

            weightedR += stops[i].color.r * weight;
            weightedG += stops[i].color.g * weight;
            weightedB += stops[i].color.b * weight;
            weightedA += stops[i].color.a * weight;
            totalWeight += weight;
        }

        if (totalWeight < 0.0001f) totalWeight = 1.0f;

        return sf::Color(
            static_cast<std::uint8_t>(weightedR / totalWeight),
            static_cast<std::uint8_t>(weightedG / totalWeight),
            static_cast<std::uint8_t>(weightedB / totalWeight),
            static_cast<std::uint8_t>((weightedA / totalWeight) * opacity)
        );
    }

    // Get color với auto-fallback

    sf::Color getColorAt(float x, float y, const sf::FloatRect& boundingBox,
        bool useRadialFallback = true) const {
        try {
            if (type == "linear") {
                return getLinearColorAt(x, y, boundingBox);
            }
            else if (type == "radial") {
                if (useRadialFallback) {
                    return getRadialColorAt(x, y, boundingBox);
                }
                else {
                    Gradient linear = toLinearFallback();
                    return linear.getLinearColorAt(x, y, boundingBox);
                }
            }
        }
        catch (...) {
            return getAverageColor();
        }

        return sf::Color::Black;
    }
};

#endif // GRADIENT_H