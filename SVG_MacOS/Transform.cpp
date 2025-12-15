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
    std::vector<float> extractValues(std::stringstream& ss, int count) {
        std::vector<float> values;

        // Đọc phần còn lại của stream vào chuỗi
        std::string remaining;
        std::getline(ss, remaining);

        // Thay thế toàn bộ dấu phẩy bằng dấu cách để dễ đọc
        std::replace(remaining.begin(), remaining.end(), ',', ' ');

        // Đưa lại vào stringstream mới
        std::stringstream cleanSS(remaining);
        float val;

        for (int i = 0; i < count; ++i) {
            if (cleanSS >> val) {
                values.push_back(val);
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
    m[0] = 1.0f; m[1] = 0.0f; m[2] = 0.0f; // Hàng 1
    m[3] = 0.0f; m[4] = 1.0f; m[5] = 0.0f; // Hàng 2
}

void TransformMatrix::combine(const TransformMatrix& other) {
    float a = m[0], b = m[1], c = m[2];
    float d = m[3], e = m[4], f = m[5];

    // Nhân ma trận: this = this * other
    m[0] = a * other.m[0] + d * other.m[1];
    m[3] = a * other.m[3] + d * other.m[4];
    m[2] = a * other.m[2] + d * other.m[5] + c;

    m[1] = b * other.m[0] + e * other.m[1];
    m[4] = b * other.m[3] + e * other.m[4];
    m[5] = b * other.m[2] + e * other.m[5] + f;
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
    mat.m[3] = -sinA;
    mat.m[1] = sinA;
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
    TransformMatrix result;
    std::string cleanString = transformString;
    size_t pos = 0;
    while (pos < cleanString.length()) {
        // Tìm dấu mở ngoặc '('
        size_t openParen = cleanString.find('(', pos);
        if (openParen == std::string::npos) break;

        // Tìm dấu đóng ngoặc ')'
        size_t closeParen = cleanString.find(')', openParen);
        if (closeParen == std::string::npos) break;

        // Tên lệnh nằm trước dấu mở ngoặc (ví dụ: "translate")
        std::string command = cleanString.substr(pos, openParen - pos);
        // Xóa khoảng trắng quanh command
        command = trim(command);
        // Xóa dấu phẩy nếu lệnh trước đó kết thúc bằng dấu phẩy
        if (!command.empty() && command[0] == ',') command.erase(0, 1);
        command = trim(command);

        // Nội dung bên trong ngoặc (ví dụ: "640,360")
        std::string content = cleanString.substr(openParen + 1, closeParen - openParen - 1);
        std::stringstream ss(content);

        TransformMatrix currentTransform;

        if (command == "translate") {
            auto vals = extractValues(ss, 2);
            if (vals.size() == 1) currentTransform = translate(vals[0], 0.0f);
            if (vals.size() == 2) currentTransform = translate(vals[0], vals[1]);
        }
        else if (command == "rotate") {
            auto vals = extractValues(ss, 3); // rotate(a) hoặc rotate(a, cx, cy)
            if (vals.size() == 1) {
                currentTransform = rotate(vals[0]);
            }
            else if (vals.size() == 3) {
                // rotate(angle, cx, cy) = translate(cx, cy) * rotate(angle) * translate(-cx, -cy)
                TransformMatrix t1 = translate(vals[1], vals[2]);
                TransformMatrix r = rotate(vals[0]);
                TransformMatrix t2 = translate(-vals[1], -vals[2]);

                // Thứ tự combine: t1 * r * t2
                // Do combine là nhân bên phải: result = result * new.
                // Nên ta cần combine t2 vào t1, rồi r vào t1?
                // Logic combine: this = this * other.
                // Muốn t1 * r * t2 -> t1.combine(r); t1.combine(t2);
                t1.combine(r);
                t1.combine(t2);
                currentTransform = t1;
            }
        }
        else if (command == "scale") {
            auto vals = extractValues(ss, 2);
            if (vals.size() == 1) currentTransform = scale(vals[0]);
            if (vals.size() == 2) currentTransform = scale(vals[0], vals[1]);
        }
        else if (command == "matrix") {
            auto vals = extractValues(ss, 6);
            if (vals.size() == 6) {
                // SVG matrix(a, b, c, d, e, f)
                // Map to: m0=a, m1=b, m2=e, m3=c, m4=d, m5=f
                currentTransform.m[0] = vals[0];
                currentTransform.m[1] = vals[1];
                currentTransform.m[3] = vals[2];
                currentTransform.m[4] = vals[3];
                currentTransform.m[2] = vals[4];
                currentTransform.m[5] = vals[5];
            }
        }

        result.combine(currentTransform);
        pos = closeParen + 1;
    }

    return result;
}