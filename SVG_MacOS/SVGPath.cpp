// FILE: SVGPath.cpp
#include "SVGPath.h"
#include <iostream>
#include <vector>
#include <cctype>
#include <string>
#include <cmath>
#include "SVGRenderer.h"

Path::Path(const Attributes& attributes) : SVGElement(attributes) {
    auto it = attributes.find("d");
    if (it != attributes.end()) {
        d_ = it->second;
        parsePathData();
    }
}

void Path::draw(SVGRenderer& renderer) const {
    renderer.renderPath(*this);
}

void Path::parsePathData() {
    size_t len = d_.length();
    size_t i = 0;
    char currentCmd = 0;
    std::vector<float> argsBuffer;

    auto skipSeparators = [&]() {
        while (i < len && (std::isspace(d_[i]) || d_[i] == ',')) i++;
    };

    auto readNumber = [&]() -> float {
        skipSeparators();
        size_t start = i;
        if (i < len && (d_[i] == '-' || d_[i] == '+')) i++;
        bool hasDot = false;
        bool hasExponent = false;

        while (i < len) {
            char c = d_[i];
            if (std::isdigit(c)) { i++; }
            else if (c == '.') {
                if (hasDot) break; 
                hasDot = true; i++;
            }
            else if (c == 'e' || c == 'E') {
                if (hasExponent) break; 
                hasExponent = true; i++;
                if (i < len && (d_[i] == '-' || d_[i] == '+')) i++;
            }
            else if (c == '-' || c == '+') { break; }
            else { break; }
        }
        if (start == i) return 0.0f;
        try { return std::stof(d_.substr(start, i - start)); } 
        catch (...) { return 0.0f; }
    };

    while (i < len) {
        skipSeparators();
        if (i >= len) break;
        char c = d_[i];

        if (std::isalpha(c) && c != 'e' && c != 'E') { 
            currentCmd = c;
            argsBuffer.clear();
            i++;
            if (std::toupper(currentCmd) == 'Z') {
                commands_.push_back({ currentCmd, {} });
            }
        }
        else {
            if (currentCmd == 0) { i++; continue; } 

            // --- [FIX QUAN TRỌNG: CHỐNG TREO MÁY/SEGFAULT] ---
            // Đây là đoạn code BẮT BUỘC PHẢI CÓ để không bị tràn RAM
            size_t prevI = i; 
            
            float val = 0.0f;
            char upperCmd = std::toupper(currentCmd);
            bool isArcFlag = (upperCmd == 'A') && (argsBuffer.size() == 3 || argsBuffer.size() == 4);

            if (isArcFlag) {
                if (i < len && (d_[i] == '0' || d_[i] == '1')) {
                    val = (float)(d_[i] - '0'); i++;
                } else val = readNumber();
            } else {
                val = readNumber();
            }

            // Nếu vị trí i không thay đổi (do gặp ký tự lạ) -> Cưỡng ép nhảy qua
            if (i == prevI) {
                i++; 
                continue; 
            }
            // --------------------------------------------------

            argsBuffer.push_back(val);

            size_t needed = 0;
            if (upperCmd == 'H' || upperCmd == 'V') needed = 1;
            else if (upperCmd == 'M' || upperCmd == 'L' || upperCmd == 'T') needed = 2; 
            else if (upperCmd == 'S' || upperCmd == 'Q') needed = 4;
            else if (upperCmd == 'C') needed = 6;
            else if (upperCmd == 'A') needed = 7;
            else if (upperCmd == 'Z') needed = 0;

            if (needed > 0 && argsBuffer.size() >= needed) {
                commands_.push_back({ currentCmd, argsBuffer });
                argsBuffer.clear();
                if (currentCmd == 'M') currentCmd = 'L';
                else if (currentCmd == 'm') currentCmd = 'l';
            }
        }
    }
}