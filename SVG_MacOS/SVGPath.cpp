#include "SVGPath.h"
#include <iostream>
#include <vector>
#include <cctype>
#include <string>
#include <cmath>
#include "SVGRenderer.h"

// Hàm ti?n ích ki?m tra ký t? có th? là m?t ph?n c?a s?
bool isNumChar(char c) {
    return std::isdigit(c) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E';
}

// --- ??nh ngh?a Path ---
Path::Path(const Attributes& attributes) : SVGElement(attributes) {
    auto it = attributes.find("d");
    if (it != attributes.end()) {
        d_ = it->second;
        parsePathData();
    }
}

// Phân tích chu?i l?nh trong thu?c tính "d"
void Path::parsePathData() {
    size_t i = 0;
    size_t len = d_.length();
    char currentCmd = 0;
    std::vector<float> argsBuffer;

    auto skipSeparators = [&]() {
        while (i < len && (std::isspace(d_[i]) || d_[i] == ',')) i++;
        };

    auto readNumber = [&]() -> float {
        size_t start = i;
        if (i < len && (d_[i] == '-' || d_[i] == '+')) i++;
        bool hasDot = false;
        while (i < len) {
            if (std::isdigit(d_[i])) i++;
            else if (d_[i] == '.' && !hasDot) { hasDot = true; i++; }
            else break;
        }
        if (i < len && (d_[i] == 'e' || d_[i] == 'E')) {
            i++;
            if (i < len && (d_[i] == '-' || d_[i] == '+')) i++;
            while (i < len && std::isdigit(d_[i])) i++;
        }
        try {
            return std::stof(d_.substr(start, i - start));
        }
        catch (...) {
            return 0.0f;
        }
        };

    auto flushCommand = [&]() {
        if (currentCmd == 0) return;
        char upperCmd = std::toupper(currentCmd);
        int requiredArgs = 0;

        if (upperCmd == 'M' || upperCmd == 'L' || upperCmd == 'T')
            requiredArgs = 2;
        else if (upperCmd == 'H' || upperCmd == 'V')
            requiredArgs = 1;
        else if (upperCmd == 'S' || upperCmd == 'Q')
            requiredArgs = 4;
        else if (upperCmd == 'C')
            requiredArgs = 6;
        else if (upperCmd == 'A')
            requiredArgs = 7;
        else if (upperCmd == 'Z')
            requiredArgs = 0;

        if (upperCmd == 'Z') {
            commands_.push_back({ currentCmd, {} });
            argsBuffer.clear();
            return;
        }

        size_t processed = 0;
        while (processed + requiredArgs <= argsBuffer.size()) {
            PathCommand cmd;
            cmd.type = currentCmd;
            for (int k = 0; k < requiredArgs; ++k)
                cmd.args.push_back(argsBuffer[processed + k]);
            commands_.push_back(cmd);
            processed += requiredArgs;

            // M -> L chaining
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
            argsBuffer.clear();
            currentCmd = c;
            i++;
            if (std::toupper(currentCmd) == 'Z')
                flushCommand();
        }
        else if (isNumChar(c)) {
            if (currentCmd == 0) { i++; continue; }
            argsBuffer.push_back(readNumber());

            char upperCmd = std::toupper(currentCmd);
            int needed = 0;
            if (upperCmd == 'M' || upperCmd == 'L' || upperCmd == 'T') needed = 2;
            else if (upperCmd == 'H' || upperCmd == 'V') needed = 1;
            else if (upperCmd == 'S' || upperCmd == 'Q') needed = 4;
            else if (upperCmd == 'C') needed = 6;
            else if (upperCmd == 'A') needed = 7;

            if (argsBuffer.size() == needed)
                flushCommand();
        }
        else i++;
    }
}
// V? Path s? d?ng SVGRenderer
void Path::draw(SVGRenderer& renderer) const { renderer.renderPath(*this); }
