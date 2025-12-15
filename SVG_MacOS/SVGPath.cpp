#include "SVGPath.h"
#include <iostream>
#include <vector>
#include <cctype>
#include <string>
#include <cmath>
#include "SVGRenderer.h"

// Hàm kiểm tra ký tự số
bool isNumChar(char c) {
    return std::isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E';
}

Path::Path(const Attributes& attributes) : SVGElement(attributes) {
    auto it = attributes.find("d");
    if (it != attributes.end()) {
        d_ = it->second;
        parsePathData();
    }
}

void Path::parsePathData() {
    size_t i = 0;
    size_t len = d_.length();
    char currentCmd = 0;
    std::vector<float> argsBuffer;

    // Helper: Bỏ qua phân cách
    auto skipSeparators = [&]() {
        while (i < len && (std::isspace(d_[i]) || d_[i] == ',')) {
            i++;
        }
        };

    // Helper: Đọc số
    auto readNumber = [&]() -> float {
        size_t start = i;
        if (i < len && (d_[i] == '-' || d_[i] == '+')) i++;
        while (i < len && (std::isdigit(d_[i]) || d_[i] == '.')) i++;
        if (i < len && (d_[i] == 'e' || d_[i] == 'E')) {
            i++;
            if (i < len && (d_[i] == '-' || d_[i] == '+')) i++;
            while (i < len && std::isdigit(d_[i])) i++;
        }
        try { return std::stof(d_.substr(start, i - start)); }
        catch (...) { return 0.0f; }
        };

    // Helper: Đẩy lệnh vào danh sách (ĐÃ BỔ SUNG S, Q, T)
    auto flushCommand = [&]() {
        if (currentCmd == 0) return;

        char upperCmd = std::toupper(currentCmd);
        int requiredArgs = 0;

        // --- CẤU HÌNH SỐ LƯỢNG THAM SỐ CHO TỪNG LỆNH ---
        if (upperCmd == 'M' || upperCmd == 'L') requiredArgs = 2;      // Move, Line
        else if (upperCmd == 'H' || upperCmd == 'V') requiredArgs = 1; // Horizontal, Vertical
        else if (upperCmd == 'C') requiredArgs = 6;                    // Cubic Bezier
        else if (upperCmd == 'S') requiredArgs = 4;                    // Smooth Cubic (THÊM MỚI)
        else if (upperCmd == 'Q') requiredArgs = 4;                    // Quadratic Bezier (THÊM MỚI)
        else if (upperCmd == 'T') requiredArgs = 2;                    // Smooth Quadratic (THÊM MỚI)
        else if (upperCmd == 'Z') requiredArgs = 0;                    // Close Path
        else requiredArgs = 0;

        // Lệnh Z không cần tham số
        if (upperCmd == 'Z') {
            PathCommand cmd; cmd.type = currentCmd;
            commands_.push_back(cmd);
            argsBuffer.clear();
            return;
        }

        // Xử lý Implicit Commands (Lệnh lặp lại không cần nhắc lại chữ cái)
        size_t processed = 0;
        while (processed + requiredArgs <= argsBuffer.size()) {
            PathCommand cmd;
            cmd.type = currentCmd;
            for (int k = 0; k < requiredArgs; ++k) {
                cmd.args.push_back(argsBuffer[processed + k]);
            }
            commands_.push_back(cmd);
            processed += requiredArgs;

            // Quy tắc Implicit của SVG:
            // Sau M (Move) thì các cặp số tiếp theo là L (Line)
            if (currentCmd == 'M') currentCmd = 'L';
            else if (currentCmd == 'm') currentCmd = 'l';
        }
        argsBuffer.clear();
        };

    while (i < len) {
        skipSeparators();
        if (i >= len) break;

        char c = d_[i];

        if (std::isalpha(c)) {
            // Gặp lệnh mới -> Đẩy dữ liệu lệnh cũ vào vector
            // Vì argsBuffer được clear ngay trong flushCommand nên ở đây thường là buffer rỗng
            // Nhưng nếu có lỗi cú pháp trước đó thì clear đi cho an toàn
            argsBuffer.clear();

            currentCmd = c;
            i++;

            if (std::toupper(currentCmd) == 'Z') flushCommand();
        }
        else if (isNumChar(c)) {
            if (currentCmd == 0) { i++; continue; }

            float val = readNumber();
            argsBuffer.push_back(val);

            // Kiểm tra xem đã đủ số chưa để đẩy luôn
            char upperCmd = std::toupper(currentCmd);
            int needed = 0;
            if (upperCmd == 'M' || upperCmd == 'L' || upperCmd == 'T') needed = 2; // Thêm T
            else if (upperCmd == 'H' || upperCmd == 'V') needed = 1;
            else if (upperCmd == 'S' || upperCmd == 'Q') needed = 4;               // Thêm S, Q
            else if (upperCmd == 'C') needed = 6;

            if (argsBuffer.size() == needed) {
                flushCommand();
            }
        }
        else { i++; }
    }
}

void Path::draw(SVGRenderer& renderer) const {
    renderer.renderPath(*this);
}
