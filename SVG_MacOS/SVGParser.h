#pragma once
# ifndef SVG_READER_SVGPARSER_H
# define SVG_READER_SVGPARSER_H
// 
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include "SVGElement.h"

using SVGElementPtr = std::shared_ptr<SVGElement>;

class SVGParser {
public:
    // Khai báo kiểu dữ liệu cho danh sách các phần tử đã phân tích
    using ElementList = std::vector<SVGElementPtr>;

private:
    // Danh sách các phần tử SVG đã được phân tích thành công
    ElementList elements_;

    // Phân tích một dòng chứa tag/thuộc tính
    SVGElementPtr parseElementFromLine(const std::string& line);

    // Trích xuất tên tag và map thuộc tính từ một chuỗi
    bool extractTagAndAttributes(const std::string& line, std::string& tagName, Attributes& attributes) const;

public:
    SVGParser() = default;

    // Đọc và phân tích cú pháp nội dung từ file SVG.
    bool parseFile(const std::string& filename);

    // Kiểm tra tính hợp lệ cơ bản của file (chỉ kiểm tra việc mở/đọc file thành công)
    bool isValidFile(const std::string& filename) const;

    ElementList takeElements();
    // Getter
    const ElementList& getElements() const;
};
//
#endif // SVG_READER_SVGPARSER_H
