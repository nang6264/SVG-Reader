// FILE: SVGParser.cpp
#include "SVGParser.h"
#include "SVGPath.h"
#include "Group.h"
#include "SVGElement.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stack>
#include <cctype>
#include <vector>
#include <cmath>
// [QUAN TRỌNG] Include file này để dùng bộ xử lý ma trận xịn
#include "Transform.h" 

// Helper functions
namespace {
    inline std::string trim(const std::string& s) {
        auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c){return std::isspace(c);});
        auto wsback = std::find_if_not(s.rbegin(), s.rend(), [](int c){return std::isspace(c);}).base();
        return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
    }

    // Hàm xử lý toạ độ có %
    float parseCoord(const std::string& s) {
        if (s.empty()) return 0.0f;
        try {
            float val = std::stof(s);
            if (s.find('%') != std::string::npos) {
                val /= 100.0f;
            }
            return val;
        } catch (...) { return 0.0f; }
    }
}

// --- SVGParser Implementation ---

bool SVGParser::isValidFile(const std::string& filename) const {
    std::ifstream file(filename);
    return file.good();
}

bool SVGParser::extractTagAndAttributes(const std::string& content, std::string& tagName, Attributes& attributes) const {
    if (content.empty()) return false;
    if (content[0] == '/' || content[0] == '?' || content[0] == '!') return false;

    std::stringstream ss(content);
    ss >> tagName; 
    if (!tagName.empty() && tagName.back() == '/') tagName.pop_back();

    std::string remaining;
    std::getline(ss, remaining); 
    size_t i = 0, len = remaining.length();

    while (i < len) {
        while (i < len && (std::isspace(remaining[i]) || remaining[i] == '/')) i++; 
        if (i >= len) break;

        size_t keyStart = i;
        while (i < len && remaining[i] != '=' && !std::isspace(remaining[i])) i++;
        std::string key = remaining.substr(keyStart, i - keyStart);

        while (i < len && (std::isspace(remaining[i]) || remaining[i] == '=')) i++;
        if (i >= len) break;

        char quoteType = remaining[i];
        if (quoteType == '"' || quoteType == '\'') {
            i++; 
            size_t valStart = i;
            while (i < len && remaining[i] != quoteType) i++;
            attributes[key] = remaining.substr(valStart, i - valStart);
            if (i < len) i++; 
        }
    }
    return true;
}

void parseGradientStops(const std::string& gradientContent, Gradient& grad) {
    bool stopsCleared = false;
    size_t pos = 0;
    while (true) {
        size_t start = gradientContent.find("<stop", pos);
        if (start == std::string::npos) break;
        size_t end = gradientContent.find(">", start);
        if (end == std::string::npos) break;

        if (!stopsCleared) { grad.stops.clear(); stopsCleared = true; }

        std::string tagContent = gradientContent.substr(start + 1, end - start - 1);
        std::string attrStr = tagContent.substr(4); 
        
        float offset = 0.0f;
        sf::Color color = sf::Color::Black;
        float opacity = 1.0f; 
        
        auto getAttr = [&](const std::string& key) -> std::string {
            size_t kPos = attrStr.find(key + "=\"");
            if (kPos != std::string::npos) {
                size_t vStart = kPos + key.length() + 2;
                size_t vEnd = attrStr.find("\"", vStart);
                return attrStr.substr(vStart, vEnd - vStart);
            }
            return "";
        };

        offset = parseCoord(getAttr("offset"));
        std::string style = getAttr("style");
        std::string stopColor = getAttr("stop-color");
        std::string stopOpacity = getAttr("stop-opacity");

        if (!style.empty()) {
            if (style.find("stop-color") != std::string::npos) {
                size_t p = style.find("stop-color"), c = style.find(":", p), s = style.find(";", p);
                if (s == std::string::npos) s = style.length();
                if (c != std::string::npos) stopColor = trim(style.substr(c + 1, s - c - 1));
            }
             if (style.find("stop-opacity") != std::string::npos) {
                size_t p = style.find("stop-opacity"), c = style.find(":", p), s = style.find(";", p);
                if (s == std::string::npos) s = style.length();
                if (c != std::string::npos) stopOpacity = trim(style.substr(c + 1, s - c - 1));
            }
        }

        if (!stopColor.empty()) {
            if (stopColor[0] == '#') {
                unsigned int hex;
                std::stringstream hss; hss << std::hex << stopColor.substr(1); hss >> hex;
                if (stopColor.length() == 7) color = sf::Color((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
                else if (stopColor.length() == 4) { 
                    unsigned int r = (hex >> 8) & 0xF; r |= r << 4;
                    unsigned int g = (hex >> 4) & 0xF; g |= g << 4;
                    unsigned int b = hex & 0xF; b |= b << 4;
                    color = sf::Color(r, g, b);
                }
            }
            else if (stopColor == "white") color = sf::Color::White;
            else if (stopColor == "black") color = sf::Color::Black;
            else if (stopColor == "yellow") color = sf::Color::Yellow;
            else if (stopColor == "red") color = sf::Color::Red;
            else if (stopColor == "blue") color = sf::Color::Blue;
        }

        if (!stopOpacity.empty()) try { opacity = std::stof(stopOpacity); } catch(...) {}
        color.a = static_cast<std::uint8_t>(opacity * 255);
        grad.stops.push_back(GradientStop(offset, color));
        pos = end + 1;
    }
    grad.validateStops();
}


bool SVGParser::parseFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    std::replace(content.begin(), content.end(), '\n', ' ');
    std::replace(content.begin(), content.end(), '\r', ' ');

    std::stack<Group*> groupStack;
    size_t pos = 0;

    while (true) {
        size_t start = content.find('<', pos);
        if (start == std::string::npos) break;
        size_t end = content.find('>', start);
        if (end == std::string::npos) break;

        std::string tagContent = content.substr(start + 1, end - start - 1);
        pos = end + 1; 

        if (tagContent[0] == '/') {
            std::string tagName = tagContent.substr(1);
            if (tagName == "g" || tagName == "g ") if (!groupStack.empty()) groupStack.pop();
            continue;
        }

        std::string tagName;
        Attributes attrs; 
        if (extractTagAndAttributes(tagContent, tagName, attrs)) {
            
            if (tagName == "svg") {
                if (attrs.count("width")) try { header_.width = std::stof(attrs["width"]); } catch(...) {}
                if (attrs.count("height")) try { header_.height = std::stof(attrs["height"]); } catch(...) {}
                if (attrs.count("viewBox")) {
                    std::stringstream ss(attrs["viewBox"]);
                    ss >> header_.viewBoxX >> header_.viewBoxY >> header_.viewBoxWidth >> header_.viewBoxHeight;
                    header_.hasViewBox = true;
                }
            }
            else if (tagName == "linearGradient" || tagName == "radialGradient") {
                Gradient grad;
                
                // 1. Kế thừa từ cha
                std::string href;
                if (attrs.count("href")) href = attrs["href"];
                if (attrs.count("xlink:href")) href = attrs["xlink:href"];
                if (!href.empty() && href[0] == '#') {
                    std::string parentId = href.substr(1);
                    if (gradients_.count(parentId)) grad = gradients_[parentId];
                }

                // 2. Parse thuộc tính riêng
                if (attrs.count("id")) grad.id = attrs["id"];
                grad.type = (tagName == "linearGradient") ? "linear" : "radial";
                
                if (attrs.count("gradientUnits")) grad.gradientUnits = attrs["gradientUnits"];

                // [ĐÃ SỬA] Dùng TransformMatrix::parse để xử lý Scale/Rotate phức tạp
                if (attrs.count("gradientTransform")) {
                    TransformMatrix tm = TransformMatrix::parse(attrs["gradientTransform"]);
                    
                    // Map trực tiếp: TransformMatrix (m[0]=a, m[1]=b, m[2]=e, m[3]=c, m[4]=d, m[5]=f)
                    // Lưu ý: Trong Transform.cpp: x' = m[0]x + m[3]y + m[2]
                    // GradientTransform: x' = a*x + c*y + e
                    // => a=m[0], c=m[3], e=m[2]
                    
                    grad.transform.a = tm.m[0];
                    grad.transform.b = tm.m[1];
                    grad.transform.c = tm.m[3];
                    grad.transform.d = tm.m[4];
                    grad.transform.e = tm.m[2];
                    grad.transform.f = tm.m[5];
                }

                if (attrs.count("x1")) grad.x1 = parseCoord(attrs["x1"]);
                if (attrs.count("y1")) grad.y1 = parseCoord(attrs["y1"]);
                if (attrs.count("x2")) grad.x2 = parseCoord(attrs["x2"]);
                if (attrs.count("y2")) grad.y2 = parseCoord(attrs["y2"]);
                
                if (attrs.count("cx")) grad.cx = parseCoord(attrs["cx"]);
                if (attrs.count("cy")) grad.cy = parseCoord(attrs["cy"]);
                if (attrs.count("r")) grad.r = parseCoord(attrs["r"]);
                if (attrs.count("fx")) grad.fx = parseCoord(attrs["fx"]);
                if (attrs.count("fy")) grad.fy = parseCoord(attrs["fy"]);

                // 3. Parse stops con
                std::string closeTag = (grad.type == "linear") ? "</linearGradient>" : "</radialGradient>";
                size_t closePos = content.find(closeTag, pos);
                if (closePos != std::string::npos) {
                    std::string innerContent = content.substr(pos, closePos - pos);
                    parseGradientStops(innerContent, grad);
                }
                gradients_[grad.id] = grad;
            }
            else {
                SVGElementPtr element = nullptr;
                if (tagName == "g") {
                    auto group = std::make_shared<Group>(attrs);
                    if (!groupStack.empty()) groupStack.top()->addElement(group);
                    else elements_.push_back(group);
                    groupStack.push(group.get());
                    continue;
                }
                if (tagName == "rect") element = std::make_shared<Rect>(attrs);
                else if (tagName == "circle") element = std::make_shared<Circle>(attrs);
                else if (tagName == "line") element = std::make_shared<Line>(attrs);
                else if (tagName == "ellipse") element = std::make_shared<Ellipse>(attrs);
                else if (tagName == "polyline") element = std::make_shared<Polyline>(attrs);
                else if (tagName == "polygon") element = std::make_shared<Polygon>(attrs); 
                else if (tagName == "path") element = std::make_shared<Path>(attrs);
                else if (tagName == "text") {
                    size_t contentStart = pos;
                    size_t contentEnd = content.find("</text>", pos);
                    if (contentEnd != std::string::npos) 
                        element = std::make_shared<Text>(attrs, content.substr(contentStart, contentEnd - contentStart));
                }

                if (element) {
                    if (!groupStack.empty()) groupStack.top()->addElement(element);
                    else elements_.push_back(element);
                }
            }
        }
    }
    
    // Fallback href
    for (auto& pair : gradients_) {
        if (pair.second.stops.empty()) {
            // Logic tìm lại cha nếu cần
        }
    }
    return true;
}

SVGParser::ElementList SVGParser::takeElements() { return std::move(elements_); }
const SVGParser::ElementList& SVGParser::getElements() const { return elements_; }