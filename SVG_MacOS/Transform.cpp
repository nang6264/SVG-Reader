#include "Transform.h"
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

// --- Helper Functions ---
namespace {

    // Hàm tiện ích: Loại bỏ khoảng trắng ở đầu và cuối chuỗi
    std::string trim(const std::string& s)
    {
        auto wsfront = std::find_if_not(s.begin(), s.end(), ::isspace);
        auto wsback = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
        return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
    }

    // Hàm tiện ích phân tích cú pháp (dùng trong parse)
    std::vector<float> extractValues(std::stringstream& ss, int count) {
        std::vector<float> values;
        std::string token;
        for (int i = 0; i < count; ++i) {
            // Bỏ qua khoảng trắng và dấu phẩy
            while (ss.peek() == ' ' || ss.peek() == ',' || ss.peek() == '\t') {
                ss.ignore();
            }
            if (ss >> token) {
                try {
                    // Chuyển đổi và lưu giá trị
                    values.push_back(std::stof(token));
                }
                catch (...) {
                    // Bỏ qua giá trị không hợp lệ
                    break;
                }
            }
            else {
                break;
            }
        }
        return values;
    }

} // end anonymous namespace

// --- Triển khai Lớp TransformMatrix ---

TransformMatrix::TransformMatrix() {
    m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f; // Hàng 1 (m11, m12, tx)
    m[3] = 0.0f; m[4] = 1.0f; m[5] = 0.0f; // Hàng 2 (m21, m22, ty)
}

void TransformMatrix::combine(const TransformMatrix& other) {
    float a = m[0], b = m[1], c = m[2];
    float d = m[3], e = m[4], f = m[5]; 

    // Ma trận hiện tại (Parent) nhân Ma trận mới (Child/Other)
    // Công thức chuẩn:
    // [ a  d  c ]   [ oa od oc ]   [ a*oa+d*ob  a*od+d*oe  a*oc+d*of+c ]
    // [ b  e  f ] x [ ob oe of ] = [ b*oa+e*ob  b*od+e*oe  b*oc+e*of+f ]
    // [ 0  0  1 ]   [ 0  0  1  ]   [ 0          0          1           ]

    // Hàng 1 (X)
    m[0] = a * other.m[0] + d * other.m[1];
    m[3] = a * other.m[3] + d * other.m[4]; // Lưu ý: m[3] là m01 (theo cách dùng trong transformPoint)
    m[2] = a * other.m[2] + d * other.m[5] + c; // <-- ĐÃ SỬA: Tx bị ảnh hưởng bởi a, d

    // Hàng 2 (Y)
    m[1] = b * other.m[0] + e * other.m[1];
    m[4] = b * other.m[3] + e * other.m[4];
    m[5] = b * other.m[2] + e * other.m[5] + f; // <-- ĐÃ SỬA: Ty bị ảnh hưởng bởi b, e
}

TransformMatrix TransformMatrix::translate(float x, float y) {
    TransformMatrix mat;
    mat.m[2] = x; // tx
    mat.m[5] = y; // ty
    return mat;
}

TransformMatrix TransformMatrix::rotate(float degrees) {
    float radians = degrees * (M_PI / 180.0f);
    float cosA = std::cos(radians);
    float sinA = std::sin(radians);

    TransformMatrix mat;
    mat.m[0] = cosA;
    mat.m[1] = sinA;
    mat.m[3] = -sinA; // sin(-A) = -sin(A)
    mat.m[4] = cosA;
    return mat;
}

TransformMatrix TransformMatrix::scale(float sx, float sy) {
    TransformMatrix mat;
    mat.m[0] = sx;
    mat.m[4] = sy;
    return mat;
}

TransformMatrix TransformMatrix::scale(float s) {
    return scale(s, s);
}

void TransformMatrix::transformPoint(float x, float y, float& outX, float& outY) const {
    outX = m[0] * x + m[3] * y + m[2];
    outY = m[1] * x + m[4] * y + m[5];
}

TransformMatrix TransformMatrix::parse(const std::string& transformString) {
    TransformMatrix result; // Khởi tạo là ma trận đơn vị
    std::stringstream ss(transformString);
    std::string segment;

    // Tách chuỗi theo dấu ngoặc đóng ')', sau đó tìm hàm
    while (std::getline(ss, segment, ')')) {
        size_t openParen = segment.find_last_of('(');
        if (openParen == std::string::npos) continue;

        // Tên lệnh (ví dụ: "translate")
        std::string command = trim(segment.substr(0, openParen));
        // Giá trị bên trong ngoặc (ví dụ: "10,20")
        std::string valuesStr = segment.substr(openParen + 1);

        std::stringstream valueSS(valuesStr);
        TransformMatrix currentTransform;

        if (command == "translate") {
            auto vals = extractValues(valueSS, 2);
            if (vals.size() == 1) currentTransform = translate(vals[0], 0.0f);
            if (vals.size() == 2) currentTransform = translate(vals[0], vals[1]);
        }
        else if (command == "rotate") {
            auto vals = extractValues(valueSS, 1);
            if (vals.size() >= 1) currentTransform = rotate(vals[0]);
        }
        else if (command == "scale") {
            auto vals = extractValues(valueSS, 2);
            if (vals.size() == 1) currentTransform = scale(vals[0]);
            if (vals.size() == 2) currentTransform = scale(vals[0], vals[1]);
        }

        // Tích lũy ma trận (result = result * currentTransform)
        result.combine(currentTransform);
    }
    return result;
}