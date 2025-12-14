#include "SVGPath.h"
#include <iostream>
#include <vector>
#include <cctype>
#include <string>
#include <cmath>
#include "SVGRenderer.h"

// Hàm kiểm tra ký tự có phải là một phần của số không (số, dấu chấm, dấu trừ, e)
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

    // Helper: Bỏ qua khoảng trắng và dấu phẩy
    auto skipSeparators = [&]() {
        while (i < len && (std::isspace(d_[i]) || d_[i] == ',')) {
            i++;
        }
    };

    // Helper: Đọc một số thực từ vị trí hiện tại
    auto readNumber = [&]() -> float {
        size_t start = i;
        if (i < len && (d_[i] == '-' || d_[i] == '+')) i++;
        while (i < len && (std::isdigit(d_[i]) || d_[i] == '.')) i++;
        // Xử lý notation khoa học (1.5e-3)
        if (i < len && (d_[i] == 'e' || d_[i] == 'E')) {
            i++;
            if (i < len && (d_[i] == '-' || d_[i] == '+')) i++;
            while (i < len && std::isdigit(d_[i])) i++;
        }
        std::string numStr = d_.substr(start, i - start);
        try {
            return std::stof(numStr);
        } catch (...) {
            return 0.0f;
        }
    };

    // Helper: Đẩy lệnh vào danh sách và xử lý Implicit Command
    auto flushCommand = [&]() {
        if (currentCmd == 0) return;

        char upperCmd = std::toupper(currentCmd);
        int requiredArgs = 0;
        if (upperCmd == 'M' || upperCmd == 'L') requiredArgs = 2;
        else if (upperCmd == 'H' || upperCmd == 'V') requiredArgs = 1;
        else if (upperCmd == 'C') requiredArgs = 6;
        else if (upperCmd == 'Z') requiredArgs = 0;
        else requiredArgs = 0; // Unknown

        // Nếu lệnh Z, không cần tham số
        if (upperCmd == 'Z') {
            PathCommand cmd;
            cmd.type = currentCmd;
            commands_.push_back(cmd);
            argsBuffer.clear();
            return;
        }

        // Logic lặp lệnh (Implicit): M 10 10 20 20 -> M 10 10, L 20 20
        size_t processed = 0;
        while (processed + requiredArgs <= argsBuffer.size()) {
            PathCommand cmd;
            cmd.type = currentCmd;
            for (int k = 0; k < requiredArgs; ++k) {
                cmd.args.push_back(argsBuffer[processed + k]);
            }
            commands_.push_back(cmd);
            processed += requiredArgs;

            // Sau lệnh M đầu tiên, các lệnh tiếp theo hiểu là L
            if (currentCmd == 'M') currentCmd = 'L';
            else if (currentCmd == 'm') currentCmd = 'l';
        }
        
        // Giữ lại phần dư (nếu có lỗi) hoặc clear
        argsBuffer.clear();
    };

    while (i < len) {
        skipSeparators();
        if (i >= len) break;

        char c = d_[i];

        if (std::isalpha(c)) {
            // Gặp lệnh mới -> Xử lý lệnh cũ trước (nếu còn dư số)
            // (Thường thì argsBuffer đã được clear sau mỗi vòng số, nhưng để chắc chắn)
            argsBuffer.clear(); 
            
            currentCmd = c;
            i++; // Tiêu thụ ký tự lệnh
            
            // Nếu là Z, flush ngay lập tức
            if (std::toupper(currentCmd) == 'Z') {
                flushCommand();
            }
        } 
        else if (isNumChar(c)) {
            // Gặp số -> Đọc số
            if (currentCmd == 0) { i++; continue; } // Chưa có lệnh nào mà có số -> Lỗi, bỏ qua
            float val = readNumber();
            argsBuffer.push_back(val);

            // Kiểm tra xem đã đủ số cho lệnh hiện tại chưa?
            char upperCmd = std::toupper(currentCmd);
            int needed = 0;
            if (upperCmd == 'M' || upperCmd == 'L') needed = 2;
            else if (upperCmd == 'H' || upperCmd == 'V') needed = 1;
            else if (upperCmd == 'C') needed = 6;
            
            if (argsBuffer.size() == needed) {
                flushCommand();
            }
        } 
        else {
            i++; // Ký tự rác
        }
    }
}

void Path::draw(SVGRenderer& renderer) const {
    renderer.renderPath(*this);
}