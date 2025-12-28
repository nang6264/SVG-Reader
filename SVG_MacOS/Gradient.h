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

// GradientStop: Điểm dừng màu
struct GradientStop {
    float offset;      // 0.0 đến 1.0
    sf::Color color;

    GradientStop(float off = 0.0f, const sf::Color& col = sf::Color::Black)
        : offset(off), color(col) {}

    bool operator<(const GradientStop& other) const {
        return offset < other.offset;
    }
};

// GradientTransform: Matrix 2x3 để biến đổi gradient
struct GradientTransform {
    float a, b, c, d, e, f;  // matrix(a, b, c, d, e, f)

    // Constructor mặc định (Identity)
    GradientTransform() : a(1), b(0), c(0), d(1), e(0), f(0) {}

    // [ĐÃ THÊM LẠI] Constructor 6 tham số để sửa lỗi biên dịch
    GradientTransform(float a_, float b_, float c_, float d_, float e_, float f_)
        : a(a_), b(b_), c(c_), d(d_), e(e_), f(f_) {}

    // Biến đổi xuôi: (x, y) -> (x', y')
    void transformPoint(float x, float y, float& outX, float& outY) const {
        outX = a * x + c * y + e;
        outY = b * x + d * y + f;
    }

    // Tính ma trận nghịch đảo
    GradientTransform inverse() const {
        float det = a * d - b * c;
        if (std::abs(det) < 0.000001f) return GradientTransform(); // Identity if singular

        float invDet = 1.0f / det;
        // Gọi constructor 6 tham số
        return GradientTransform(
            d * invDet,
            -b * invDet,
            -c * invDet,
            a * invDet,
            (c * f - d * e) * invDet,
            (b * e - a * f) * invDet
        );
    }

    bool isIdentity() const {
        return std::abs(a - 1) < 1e-5 && std::abs(b) < 1e-5 && 
               std::abs(c) < 1e-5 && std::abs(d - 1) < 1e-5 && 
               std::abs(e) < 1e-5 && std::abs(f) < 1e-5;
    }
};

// Gradient Struct: Chứa toàn bộ logic tính màu
struct Gradient {
    std::string id;
    std::string type;  // "linear" hoặc "radial"
    std::vector<GradientStop> stops;
    GradientTransform transform;
    std::string gradientUnits = "objectBoundingBox";

    // Linear properties
    float x1 = 0.0f, y1 = 0.0f, x2 = 1.0f, y2 = 0.0f;
    // Radial properties
    float cx = 0.5f, cy = 0.5f, r = 0.5f, fx = 0.5f, fy = 0.5f;

    // --- Validate & Sort ---
    void validateStops() {
        if (stops.empty()) {
            stops.push_back(GradientStop(0.0f, sf::Color::Black));
            stops.push_back(GradientStop(1.0f, sf::Color::White));
        }
        std::sort(stops.begin(), stops.end());
        for (auto& stop : stops) {
            stop.offset = std::max(0.0f, std::min(1.0f, stop.offset));
        }
    }

    // --- Core Interpolation ---
    sf::Color interpolateColor(float t) const {
        t = std::max(0.0f, std::min(1.0f, t));
        if (stops.empty()) return sf::Color::Black;
        
        size_t leftIdx = 0;
        size_t rightIdx = stops.size() - 1;
        for (size_t i = 0; i < stops.size(); i++) {
            if (stops[i].offset <= t) leftIdx = i;
            if (stops[i].offset >= t) { rightIdx = i; break; }
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

    // [ĐÃ THÊM LẠI] Weighted Average (để sửa lỗi SVGRenderer.cpp gọi hàm này)
    sf::Color getAverageColor(float opacity = 1.0f) const {
        if (stops.empty()) return sf::Color::Black;
        float r = 0, g = 0, b = 0, a = 0;
        for (const auto& s : stops) { 
            r += s.color.r; 
            g += s.color.g; 
            b += s.color.b; 
            a += s.color.a; 
        }
        float n = static_cast<float>(stops.size());
        if (n == 0) return sf::Color::Black;
        
        return sf::Color(
            static_cast<std::uint8_t>(r/n), 
            static_cast<std::uint8_t>(g/n), 
            static_cast<std::uint8_t>(b/n), 
            static_cast<std::uint8_t>((a/n) * opacity)
        );
    }

    // --- MAIN LOGIC: LINEAR (SFML 3.0 FIX: size.x/size.y) ---
    sf::Color getLinearColorAt(float x, float y, const sf::FloatRect& bbox) const {
        float u, v; // Toạ độ chuẩn hóa (0..1)
        
        if (gradientUnits == "objectBoundingBox") {
            if (bbox.size.x == 0 || bbox.size.y == 0) return stops.back().color;
            u = (x - bbox.position.x) / bbox.size.x;
            v = (y - bbox.position.y) / bbox.size.y;
        } else {
            u = x; v = y;
        }

        // Apply Inverse Transform (về không gian Gradient)
        if (!transform.isIdentity()) {
            transform.inverse().transformPoint(u, v, u, v);
        }

        // Project điểm (u,v) lên vector gradient (x1,y1)->(x2,y2)
        float dx = x2 - x1;
        float dy = y2 - y1;
        float lenSq = dx*dx + dy*dy;
        
        if (lenSq < 0.000001f) return stops[0].color;

        float t = ((u - x1) * dx + (v - y1) * dy) / lenSq;
        return interpolateColor(t);
    }

    // --- MAIN LOGIC: RADIAL (SFML 3.0 FIX: size.x/size.y) ---
    sf::Color getRadialColorAt(float x, float y, const sf::FloatRect& bbox) const {
        float u, v; // Toạ độ chuẩn hóa (0..1)

        if (gradientUnits == "objectBoundingBox") {
            if (bbox.size.x == 0 || bbox.size.y == 0) return stops.back().color;
            u = (x - bbox.position.x) / bbox.size.x;
            v = (y - bbox.position.y) / bbox.size.y;
        } else {
            u = x; v = y;
        }

        // Apply Inverse Transform
        if (!transform.isIdentity()) {
            transform.inverse().transformPoint(u, v, u, v);
        }

        float dx = u - cx;
        float dy = v - cy;
        float dist = std::sqrt(dx*dx + dy*dy);

        if (r < 0.000001f) return stops.back().color;

        float t = dist / r;
        return interpolateColor(t);
    }
};

#endif