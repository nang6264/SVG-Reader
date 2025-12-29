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
    std::string d_;
    std::vector<PathCommand> commands_;
    void parsePathData();

public:
    // Đây là hàm mà Linker đang báo thiếu
    Path(const Attributes& attributes);

    void draw(SVGRenderer& renderer) const override;
    std::string getElementName() const override { return "path"; }
    const std::vector<PathCommand>& getCommands() const { return commands_; }
};

#endif