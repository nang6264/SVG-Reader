#pragma once
# ifndef SVG_READER_SVGPARSER_H
# define SVG_READER_SVGPARSER_H
// 
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include "SVGElement.h"
#include "Gradient.h"

using SVGElementPtr = std::shared_ptr<SVGElement>;

class SVGParser {
public:
    // Khai báo kiểu dữ liệu cho danh sách các phần tử đã phân tích
    using ElementList = std::vector<SVGElementPtr>;
    struct SVGHeader {
        float viewBoxX = 0.0f;
        float viewBoxY = 0.0f;
        float viewBoxWidth = 0.0f;
        float viewBoxHeight = 0.0f;
        bool hasViewBox = false;

        // Kích thước mặc định nếu không có viewBox
        float width = 800.0f;
        float height = 600.0f;
    };
private:
    // Danh sách các phần tử SVG đã được phân tích thành công
    ElementList elements_;

    // Biến lưu header
    SVGHeader header_;

    // Map lưu gradient đã parse
    std::map<std::string, Gradient> gradients_;

    // Phân tích một dòng chứa tag/thuộc tính
    SVGElementPtr parseElementFromLine(const std::string& line);

    // Trích xuất tên tag và map thuộc tính từ một chuỗi
    bool extractTagAndAttributes(const std::string& line, std::string& tagName, Attributes& attributes) const;

    // Hàm parse gradient
    Gradient parseGradient(std::ifstream& file, const std::string& tagName,
        const Attributes& attributes);

    sf::Color stringToColor(const std::string& colorStr, const std::string& type) const;
public:
    SVGParser() = default;

    // Đọc và phân tích cú pháp nội dung từ file SVG.
    bool parseFile(const std::string& filename);

    // Kiểm tra tính hợp lệ cơ bản của file (chỉ kiểm tra việc mở/đọc file thành công)
    bool isValidFile(const std::string& filename) const;

    ElementList takeElements();
    // Getter
    const ElementList& getElements() const;

    const SVGHeader& getHeader() const { return header_; }

    const std::map<std::string, Gradient>& getGradients() const { return gradients_; }
    std::map<std::string, Gradient> takeGradients() { return std::move(gradients_); }
};
//
#endif // SVG_READER_SVGPARSER_H
