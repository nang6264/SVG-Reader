#ifndef SVGPATH_H
#define SVGPATH_H

#include "SVGElement.h"
#include <vector>
#include <string>

struct PathCommand {
    char type;
    std::vector<float> args;
};

class Path : public SVGElement {
private:
    std::string d_;                      // Biến lưu chuỗi lệnh
    std::vector<PathCommand> commands_;  // Biến lưu lệnh đã phân tích
    void parsePathData();

public:
    Path(const Attributes& attributes);
    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "path"; }
    const std::vector<PathCommand>& getCommands() const { return commands_; }
};

#endif