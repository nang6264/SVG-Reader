#pragma once
# ifndef SVG_READER_SVGPARSER_H
# define SVG_READER_SVGPARSER_H
// 
#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include "SVGElement.h"
//
using SVGElementPtr = std::unique_ptr<SVGElement>;

class SVGParser {
public:
    // Khai báo kiểu dữ liệu cho danh sách các phần tử đã phân tích
    using ElementList = std::vector<SVGElementPtr>;

private:
    // Danh sách các phần tử SVG đã được phân tích thành công
    ElementList elements_;

    /**
     * @brief HÀM TIỆN ÍCH: Phân tích một dòng chứa tag/thuộc tính
     * @param line Chuỗi chứa tag SVG
     * @return Một đối tượng SVGElementPtr hoặc nullptr nếu thất bại.
     */
    SVGElementPtr parseElementFromLine(const std::string& line);

    /**
     * @brief HÀM TIỆN ÍCH: Trích xuất tên tag và map thuộc tính từ một chuỗi
     * @param line Chuỗi đầu vào.
     * @param tagName Chuỗi đầu ra (tên tag).
     * @param attributes Map đầu ra (các thuộc tính).
     * @return true nếu trích xuất thành công.
     */
    bool extractTagAndAttributes(const std::string& line, std::string& tagName, Attributes& attributes) const;

public:
    SVGParser() = default;

    /**
     * @brief Đọc và phân tích cú pháp nội dung từ file SVG.
     * @param filename Tên file SVG cần đọc.
     * @return true nếu quá trình phân tích thành công, false nếu có lỗi file.
     */
    bool parseFile(const std::string& filename);

    /**
     * @brief Kiểm tra tính hợp lệ cơ bản của file (chỉ kiểm tra việc mở/đọc file thành công)
     * @param filename Tên file.
     * @return true nếu file tồn tại và có thể mở.
     */
    bool isValidFile(const std::string& filename) const;

    ElementList takeElements();
    // Getter
    const ElementList& getElements() const;
};
//
#endif // SVG_READER_SVGPARSER_H
