#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <cmath>
#include <string>
#include <vector>

// Định nghĩa PI nếu chưa có (cần thiết cho hàm rotate)
#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

class TransformMatrix {
private:
    // Lưu trữ: m11, m12, tx, m21, m22, ty
    float m[6];

public:
    // Constructor mặc định: Ma trận đơn vị (Identity Matrix)
    TransformMatrix();

    /**
     * @brief Áp dụng (nhân) một ma trận biến đổi khác vào ma trận hiện tại.
     */
    void combine(const TransformMatrix& other);

    // --- Các hàm tạo ma trận biến đổi cơ bản (Static Factories) ---

    static TransformMatrix translate(float x, float y);
    static TransformMatrix rotate(float degrees);
    static TransformMatrix scale(float sx, float sy);
    static TransformMatrix scale(float s);

    void transformPoint(float x, float y, float& outX, float& outY) const;

    
    static TransformMatrix parse(const std::string& transformString);

    // Cho phép SVGRenderer truy cập trực tiếp vào m[6] để chuyển sang sf::Transform
    friend class SVGRenderer;

    // Cho phép Member 4 truy cập để kiểm thử.
    friend class TransformMatrixTest;
};

#endif