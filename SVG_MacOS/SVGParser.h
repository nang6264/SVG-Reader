#pragma once
#ifndef SVG_READER_SVGPARSER_H
#define SVG_READER_SVGPARSER_H

#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <map>
#include "SVGElement.h"
#include "Gradient.h"

using SVGElementPtr = std::shared_ptr<SVGElement>;

class SVGParser {
public:
    using ElementList = std::vector<SVGElementPtr>;
    struct SVGHeader {
        float viewBoxX = 0.0f;
        float viewBoxY = 0.0f;
        float viewBoxWidth = 0.0f;
        float viewBoxHeight = 0.0f;
        bool hasViewBox = false;
        float width = 800.0f;
        float height = 600.0f;
    };
private:
    ElementList elements_;
    SVGHeader header_;
    std::map<std::string, Gradient> gradients_;
    
    // [MỚI] Map CSS
    std::map<std::string, Attributes> cssStyles_;

    SVGElementPtr parseElementFromLine(const std::string& line);
    bool extractTagAndAttributes(const std::string& line, std::string& tagName, Attributes& attributes) const;
    void parseCSS(const std::string& content);

public:
    SVGParser() = default;
    bool parseFile(const std::string& filename);
    bool isValidFile(const std::string& filename) const;
    ElementList takeElements();
    const ElementList& getElements() const;
    const SVGHeader& getHeader() const { return header_; }
    const std::map<std::string, Gradient>& getGradients() const { return gradients_; }
    std::map<std::string, Gradient> takeGradients() { return std::move(gradients_); }
};

#endif