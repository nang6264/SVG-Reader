// FILE: SVGPath.cpp
#include "SVGPath.h"
#include <iostream>
#include <vector>
#include <cctype>
#include <string>
#include <cmath>
#include "SVGRenderer.h"

// --- Constructor ---
Path::Path(const Attributes& attributes) : SVGElement(attributes) {
    auto it = attributes.find("d");
    if (it != attributes.end()) {
        d_ = it->second;
        parsePathData();
    }
}

// --- Hàm Draw ---
void Path::draw(SVGRenderer& renderer) const {
    renderer.renderPath(*this);
}

// --- Hàm xử lý cắt số thông minh ---
// Xử lý các trường hợp khó: "10.5.5", "10-5", ".5", "1e-5"
void Path::parsePathData() {
    size_t len = d_.length();
    size_t i = 0;
    char currentCmd = 0;
    std::vector<float> argsBuffer;

    // Bỏ qua khoảng trắng và dấu phẩy
    auto skipSeparators = [&]() {
        while (i < len && (std::isspace(d_[i]) || d_[i] == ',')) i++;
    };

    // Đọc một số thực từ chuỗi hỗn độn
    auto readNumber = [&]() -> float {
        skipSeparators();
        size_t start = i;
        
        // 1. Xử lý dấu (+/-) đầu tiên
        if (i < len && (d_[i] == '-' || d_[i] == '+')) i++;
        
        bool hasDot = false;
        bool hasExponent = false;

        while (i < len) {
            char c = d_[i];

            if (std::isdigit(c)) {
                i++;
            }
            else if (c == '.') {
                if (hasDot) {
                    // Đã có dấu chấm rồi mà gặp chấm nữa -> DỪNG LẠI
                    // Ví dụ: 14.672.057 -> Đọc được 14.672 thì dừng. .057 là số sau.
                    break; 
                }
                hasDot = true;
                i++;
            }
            else if (c == 'e' || c == 'E') {
                if (hasExponent) break; // Chỉ được có 1 chữ e
                hasExponent = true;
                i++;
                // Sau chữ e có thể là dấu +/- (ví dụ: 1e-5)
                if (i < len && (d_[i] == '-' || d_[i] == '+')) i++;
            }
            else if (c == '-' || c == '+') {
                // Đang đọc số mà gặp dấu +/- (không phải sau e) -> DỪNG LẠI
                // Ví dụ: 10-5 -> Đọc được 10 thì dừng. -5 là số sau.
                break;
            }
            else {
                // Gặp ký tự lạ hoặc lệnh (M, L, z...) -> Dừng
                break;
            }
        }
        
        if (start == i) return 0.0f;
        try { return std::stof(d_.substr(start, i - start)); } 
        catch (...) { return 0.0f; }
    };

    while (i < len) {
        skipSeparators();
        if (i >= len) break;
        char c = d_[i];

        if (std::isalpha(c)) { 
            // --- GẶP LỆNH MỚI ---
            currentCmd = c;
            argsBuffer.clear();
            i++;
            
            // Lệnh Z (đóng path) không có tham số
            if (std::toupper(currentCmd) == 'Z') {
                commands_.push_back({ currentCmd, {} });
            }
        }
        else {
            // --- GẶP SỐ (THAM SỐ CHO LỆNH HIỆN TẠI) ---
            if (currentCmd == 0) { i++; continue; } // Bỏ qua rác đầu file

            float val = readNumber();
            argsBuffer.push_back(val);

            // Xác định số lượng tham số cần thiết cho lệnh hiện tại
            char upperCmd = std::toupper(currentCmd);
            size_t needed = 0;
            
            if (upperCmd == 'H' || upperCmd == 'V') needed = 1;
            else if (upperCmd == 'M' || upperCmd == 'L' || upperCmd == 'T') needed = 2;
            else if (upperCmd == 'S' || upperCmd == 'Q') needed = 4;
            else if (upperCmd == 'C') needed = 6;
            else if (upperCmd == 'A') needed = 7;
            else if (upperCmd == 'Z') needed = 0;

            // Nếu đủ tham số -> Đẩy lệnh vào danh sách
            if (needed > 0 && argsBuffer.size() >= needed) {
                commands_.push_back({ currentCmd, argsBuffer });
                argsBuffer.clear();

                // Xử lý Implicit Command (Lệnh lặp lại không cần viết lại chữ cái)
                // Ví dụ: L 10 10 20 20 -> Hiểu là L 10 10 rồi L 20 20
                // Riêng M (Move) sẽ biến thành L (Line) ở các lần sau
                if (currentCmd == 'M') currentCmd = 'L';
                else if (currentCmd == 'm') currentCmd = 'l';
            }
        }
    }
}