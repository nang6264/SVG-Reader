#ifndef PATH_H
#define PATH_H

#include "SVGElement.h"
#include <vector>
#include <string>


// Cấu trúc lưu một lệnh đơn lẻ trong Path (ví dụ: M 10 20)
struct PathCommand {
    char type; // 'M', 'L', 'C', 'Z'
    std::vector<float> args; // Các tham số (x, y, x1, y1,...)
};

class Path : public SVGElement {
private:
    std::string d_; // Chuỗi dữ liệu gốc
    std::vector<PathCommand> commands_; // Dữ liệu đã parse

    // Hàm nội bộ để phân tích chuỗi d=""
    void parsePathData();

public:
    Path(const Attributes& attributes);

    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "path"; }

    // Getter để Renderer truy cập
    const std::vector<PathCommand>& getCommands() const { return commands_; }
};

#endif