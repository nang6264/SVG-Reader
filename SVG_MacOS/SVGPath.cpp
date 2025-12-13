
#include "SVGPath.h"
#include <sstream>
#include <iostream>
#include "SVGRenderer.h"



Path::Path(const Attributes& attributes) : SVGElement(attributes) {
    auto it = attributes.find("d");
    if (it != attributes.end()) {
        d_ = it->second;
        parsePathData();
    }
}
// [File: SVGPath.cpp]

void Path::parsePathData() {
    std::stringstream ss(d_);
    char cmd;
    float val;
    char currentCmd = 0;

    auto skipComma = [&](std::stringstream& s) {
        char c;
        while (s >> std::ws && s.peek() == ',') s.ignore();
        };

    while (ss >> std::ws) {
        // 1. Kiểm tra lệnh mới
        if (std::isalpha(ss.peek())) {
            ss >> cmd;
            currentCmd = cmd;
        }
        else {
            // [FIX QUAN TRỌNG] Tự động chuyển M thành L nếu gặp nhiều tọa độ
            // Nếu không có dòng này, "M 10 10 20 20" sẽ bị hiểu là "Move 10 10" rồi "Move 20 20" (nhấc bút)
            // Thay vì đúng là: "Move 10 10" rồi "Line 20 20" (đặt bút vẽ)
            if (currentCmd == 'M') currentCmd = 'L';
            else if (currentCmd == 'm') currentCmd = 'l';
        }

        PathCommand command;
        command.type = currentCmd;

        // 2. Đọc tham số
        if (currentCmd == 'M' || currentCmd == 'm' ||
            currentCmd == 'L' || currentCmd == 'l') {
            for (int i = 0; i < 2; ++i) { skipComma(ss); ss >> val; command.args.push_back(val); }
        }
        else if (currentCmd == 'H' || currentCmd == 'h' ||
            currentCmd == 'V' || currentCmd == 'v') {
            skipComma(ss); ss >> val; command.args.push_back(val);
        }
        else if (currentCmd == 'C' || currentCmd == 'c') {
            for (int i = 0; i < 6; ++i) { skipComma(ss); ss >> val; command.args.push_back(val); }
        }
        else if (currentCmd == 'Z' || currentCmd == 'z') {
            // Z không có tham số
        }
        else { break; } // Gặp lỗi thì dừng

        commands_.push_back(command);
        skipComma(ss);
    }
}

void Path::draw(SVGRenderer& renderer) const {
    renderer.renderPath(*this);
}