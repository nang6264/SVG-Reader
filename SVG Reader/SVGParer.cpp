// SVGParser.cpp
#include "SVGParser.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional> // Dùng cho std::bind

// Hàm tiện ích: Loại bỏ khoảng trắng ở đầu và cuối chuỗi
static inline std::string trim(const std::string& s) {
    auto wsfront = std::find_if_not(s.begin(), s.end(), ::isspace);
    auto wsback = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
    return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
}

// --- Triển khai Hàm tiện ích ---

/**
 * @brief HÀM TIỆN ÍCH: Trích xuất tên tag và map thuộc tính từ một chuỗi
 */
bool SVGParser::extractTagAndAttributes(const std::string& line, std::string& tagName, Attributes& attributes) const {
    std::string trimmedLine = trim(line);
    attributes.clear();

    if (trimmedLine.length() < 3 || trimmedLine.front() != '<' || trimmedLine.back() != '>') {
        return false; // Không phải tag hợp lệ
    }

    // Bỏ '<' ở đầu và '/>' hoặc '>' ở cuối
    std::string content = trimmedLine.substr(1, trimmedLine.length() - 2);
    if (content.back() == '/') {
        content.pop_back(); // Bỏ '/' cho self-closing tag
    }
    content = trim(content);

    if (content.empty()) return false;

    std::stringstream ss(content);
    std::string token;

    // 1. Lấy tên Tag
    ss >> tagName;

    if (tagName.empty()) return false;

    // 2. Lấy các thuộc tính (attribute = "value")
    std::string attrPair;
    while (ss >> attrPair) {
        size_t eqPos = attrPair.find('=');
        if (eqPos != std::string::npos) {
            std::string attrName = attrPair.substr(0, eqPos);
            std::string value = attrPair.substr(eqPos + 1);

            // Xử lý dấu ngoặc kép
            if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                attributes[trim(attrName)] = value.substr(1, value.length() - 2);
            }
            else {
                // Xử lý giá trị không có ngoặc kép
                attributes[trim(attrName)] = value;
            }
        }
    }
    return true;
}

/**
 * @brief HÀM TIỆN ÍCH: Tạo đối tượng SVGElement dựa trên tên tag.
 */
SVGElementPtr SVGParser::parseElementFromLine(const std::string& line) {
    std::string tagName;
    Attributes attributes;

    if (!extractTagAndAttributes(line, tagName, attributes)) {
        // Có thể là dòng trống, thẻ đóng, hoặc cú pháp không hợp lệ
        return nullptr;
    }

    // Chuyển tagName về chữ thường để so sánh không phân biệt chữ hoa/thường (tùy chọn)
    std::transform(tagName.begin(), tagName.end(), tagName.begin(), ::tolower);

    // Dựa vào tên tag, tạo đối tượng lớp con tương ứng
    if (tagName == "circle") {
        return std::make_unique<Circle>(attributes);
    }
    else if (tagName == "rect") {
        return std::make_unique<Rect>(attributes);
    }
    else if (tagName == "line") {
        return std::make_unique<Line>(attributes);
    }
    else if (tagName == "polygon") {
        return std::make_unique<Polygon>(attributes);
    }
    else if (tagName == "path") {
        return std::make_unique<Path>(attributes);
    }
    else if (tagName == "ellipse") {
        return std::make_unique<Ellipse>(attributes);
    }
    // Bỏ qua các tag khác như <svg>, <g>, ...

    return nullptr;
}

// --- Triển khai Hàm chính ---

bool SVGParser::isValidFile(const std::string& filename) const {
    std::ifstream file(filename);
    return file.is_open();
}

/**
 * @brief Đọc và phân tích cú pháp nội dung từ file SVG.
 */
bool SVGParser::parseFile(const std::string& filename) {
    elements_.clear();
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Lỗi: Không thể mở file SVG: " << filename << std::endl;
        return false;
    }

    std::string line;
    // Đọc từng dòng của file
    while (std::getline(file, line)) {
        // Phân tích cú pháp dòng này để tìm phần tử SVG
        SVGElementPtr element = parseElementFromLine(line);

        if (element) {
            elements_.push_back(std::move(element));
        }
    }

    // Kiểm tra tính hợp lệ cơ bản của file: kiểm tra thẻ <svg> mở/đóng

    if (elements_.empty()) {
        std::cout << "Cảnh báo: Không tìm thấy phần tử hình học SVG nào trong file." << std::endl;
    }

    return true;
}
