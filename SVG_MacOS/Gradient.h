// Gradient.h
#ifndef GRADIENT_H
#define GRADIENT_H

#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <cstdint> // [SFML 3.0] Quan trọng để dùng std::uint8_t

// Struct nhỏ dùng lưu điểm dừng màu
struct GradientStop {
    float offset;   // 0.0 đến 1.0 (tương ứng 0% -> 100%)
    sf::Color color;
};

// Struct chứa dữ liệu Gradient
struct Gradient {
    std::string id;
    std::string type; // "radial" hoặc "linear"
    
    // Tọa độ tâm và bán kính (thường là 0.0 -> 1.0 theo bounding box)
    float cx = 0.5f;
    float cy = 0.5f;
    float r = 0.5f;
    
    std::vector<GradientStop> stops;
    sf::Transform transform; 

    // --- HÀM FALLBACK (NV CỦA BẠN) ---
    // Tính màu trung bình cộng để dùng khi không vẽ được gradient
    sf::Color getAverageColor(float opacity = 1.0f) const {
        if (stops.empty()) return sf::Color::Black;

        int sumR = 0, sumG = 0, sumB = 0;
        for (const auto& s : stops) {
            sumR += s.color.r;
            sumG += s.color.g;
            sumB += s.color.b;
        }

        size_t n = stops.size();
        
        // [SFML 3.0] Ép kiểu về std::uint8_t
        return sf::Color(
            static_cast<std::uint8_t>(sumR / n),
            static_cast<std::uint8_t>(sumG / n),
            static_cast<std::uint8_t>(sumB / n),
            static_cast<std::uint8_t>(255 * opacity) 
        );
    }
};

#endif // GRADIENT_H