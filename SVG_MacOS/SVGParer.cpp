// SVGParser.cpp
#include "SVGParser.h"
#include"SVGPath.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <functional> // Dùng cho std::bind
#include "Group.h"
#include <stack>
#include "Gradient.h"



namespace SafeCType {
    inline bool isSpace(char c) {
        return std::isspace(static_cast<unsigned char>(c));
    }

    inline bool isAlpha(char c) {
        return std::isalpha(static_cast<unsigned char>(c));
    }

    inline bool isDigit(char c) {
        return std::isdigit(static_cast<unsigned char>(c));
    }

    inline char toLower(char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    inline char toUpper(char c) {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
}

// Hàm tiện ích: Loại bỏ khoảng trắng ở đầu và cuối chuỗi
static inline std::string trim(const std::string& s)
{
    auto wsfront = std::find_if_not(s.begin(), s.end(),
        [](char c) { return SafeCType::isSpace(c); });
    auto wsback = std::find_if_not(s.rbegin(), s.rend(),
        [](char c) { return SafeCType::isSpace(c); }).base();

    return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
}

GradientTransform parseGradientTransform(const std::string& transformStr) {
    GradientTransform result;  // Identity matrix

    if (transformStr.empty()) {
        return result;
    }

    std::string str = transformStr;
    size_t pos = 0;

    // Helper: extract numbers from string
    auto extractNumbers = [](const std::string& content) {
        std::vector<float> values;
        std::string clean = content;

        // Replace commas with spaces
        std::replace(clean.begin(), clean.end(), ',', ' ');

        std::stringstream ss(clean);
        float val;
        while (ss >> val) {
            values.push_back(val);
        }
        return values;
        };

    while (pos < str.length()) {
        // Find opening parenthesis
        size_t openParen = str.find('(', pos);
        if (openParen == std::string::npos) break;

        // Find closing parenthesis
        size_t closeParen = str.find(')', openParen);
        if (closeParen == std::string::npos) break;

        // Extract command name
        std::string command = trim(str.substr(pos, openParen - pos));

        // Remove leading comma if present
        if (!command.empty() && command[0] == ',') {
            command = trim(command.substr(1));
        }

        // Extract parameters
        std::string content = str.substr(openParen + 1, closeParen - openParen - 1);
        std::vector<float> values = extractNumbers(content);

        // Build transform matrix
        GradientTransform current;

        if (command == "matrix" && values.size() == 6) {
            // matrix(a, b, c, d, e, f)
            current = GradientTransform(values[0], values[1], values[2],
                values[3], values[4], values[5]);
        }
        else if (command == "translate") {
            if (values.size() == 1) {
                current = GradientTransform(1, 0, 0, 1, values[0], 0);
            }
            else if (values.size() >= 2) {
                current = GradientTransform(1, 0, 0, 1, values[0], values[1]);
            }
        }
        else if (command == "scale") {
            if (values.size() == 1) {
                current = GradientTransform(values[0], 0, 0, values[0], 0, 0);
            }
            else if (values.size() >= 2) {
                current = GradientTransform(values[0], 0, 0, values[1], 0, 0);
            }
        }
        else if (command == "rotate") {
            if (values.size() >= 1) {
                float angle = values[0] * M_PI / 180.0f;  // degrees to radians
                float cosA = std::cos(angle);
                float sinA = std::sin(angle);

                if (values.size() == 1) {
                    // rotate(angle)
                    current = GradientTransform(cosA, sinA, -sinA, cosA, 0, 0);
                }
                else if (values.size() >= 3) {
                    // rotate(angle, cx, cy)
                    float cx = values[1];
                    float cy = values[2];

                    current.a = cosA;
                    current.b = sinA;
                    current.c = -sinA;
                    current.d = cosA;
                    current.e = cx - cosA * cx + sinA * cy;
                    current.f = cy - sinA * cx - cosA * cy;
                }
            }
        }
        else if (command == "skewX" && values.size() >= 1) {
            float angle = values[0] * M_PI / 180.0f;
            float tanA = std::tan(angle);
            current = GradientTransform(1, 0, tanA, 1, 0, 0);
        }
        else if (command == "skewY" && values.size() >= 1) {
            float angle = values[0] * M_PI / 180.0f;
            float tanA = std::tan(angle);
            current = GradientTransform(1, tanA, 0, 1, 0, 0);
        }

        // Multiply matrices: result = result * current
        GradientTransform temp;
        temp.a = result.a * current.a + result.c * current.b;
        temp.b = result.b * current.a + result.d * current.b;
        temp.c = result.a * current.c + result.c * current.d;
        temp.d = result.b * current.c + result.d * current.d;
        temp.e = result.a * current.e + result.c * current.f + result.e;
        temp.f = result.b * current.e + result.d * current.f + result.f;
        result = temp;

        pos = closeParen + 1;
    }

    return result;
}

Gradient SVGParser::parseGradient(std::ifstream& file, const std::string& tagName,
    const Attributes& attributes)
{
    Gradient grad;

    std::string lowerTag = tagName;
    std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(),
        [](char c) { return SafeCType::toLower(c); });

    grad.type = (lowerTag == "lineargradient") ? "linear" : "radial";

    if (attributes.count("id")) {
        grad.id = attributes.at("id");
    }

    if (attributes.count("gradientUnits")) {
        grad.gradientUnits = attributes.at("gradientUnits");
    }

    std::string inheritFrom;
    if (attributes.count("xlink:href")) {
        inheritFrom = attributes.at("xlink:href");
        // Loại bỏ dấu # ở đầu
        if (!inheritFrom.empty() && inheritFrom[0] == '#') {
            inheritFrom = inheritFrom.substr(1);
        }
    }

    if (attributes.count("gradientTransform")) {
        grad.transform = parseGradientTransform(attributes.at("gradientTransform"));
    }

    auto parseValue = [](const std::string& val) -> float {
        try {
            if (val.find('%') != std::string::npos) {
                return std::stof(val.substr(0, val.length() - 1)) / 100.0f;
            }
            else {
                return std::stof(val);
            }
        }
        catch (...) {
            return 0.0f;
        }
        };

    if (grad.type == "linear") {
        if (attributes.count("x1")) grad.x1 = parseValue(attributes.at("x1"));
        if (attributes.count("y1")) grad.y1 = parseValue(attributes.at("y1"));
        if (attributes.count("x2")) grad.x2 = parseValue(attributes.at("x2"));
        if (attributes.count("y2")) grad.y2 = parseValue(attributes.at("y2"));
    }
    else {
        if (attributes.count("cx")) grad.cx = parseValue(attributes.at("cx"));
        if (attributes.count("cy")) grad.cy = parseValue(attributes.at("cy"));
        if (attributes.count("r")) grad.r = parseValue(attributes.at("r"));
        if (attributes.count("fx")) grad.fx = parseValue(attributes.at("fx"));
        if (attributes.count("fy")) grad.fy = parseValue(attributes.at("fy"));
        if (attributes.count("fr")) grad.fr = parseValue(attributes.at("fr"));
    }
    
    // PARSE <stop> TAGS

    std::string gradientContent;
    std::string line;
    std::string closingTag = "</" + lowerTag;

    std::cout << "  [parseGradient] Bat dau doc gradient: " << grad.id << std::endl;

    while (std::getline(file, line, '>')) {
        line += ">";
        gradientContent += line;

        // Chuyển về lowercase để kiểm tra
        std::string lowerLine = line;
        std::transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(),
            [](char c) { return SafeCType::toLower(c); });

        if (lowerLine.find(closingTag) != std::string::npos) {
            std::cout << "  [parseGradient] Tim thay the dong: " << closingTag << std::endl;
            break;
        }
    }

    // Parse tất cả <stop> từ gradientContent
    size_t pos = 0;
    int stopCount = 0;

    while ((pos = gradientContent.find("<stop", pos)) != std::string::npos) {
        // Tìm thẻ đóng
        size_t endPos = gradientContent.find("/>", pos);
        if (endPos == std::string::npos) {
            endPos = gradientContent.find("</stop>", pos);
            if (endPos != std::string::npos) {
                endPos += 7;
            }
        }
        else {
            endPos += 2;
        }

        if (endPos == std::string::npos) break;

        std::string stopLine = gradientContent.substr(pos, endPos - pos);

        // Parse stop
        std::string stopTag;
        Attributes stopAttrs;

        if (extractTagAndAttributes(stopLine, stopTag, stopAttrs)) {
            GradientStop stop;

            // Giá trị mặc định
            stop.offset = 0.0f;
            stop.color = sf::Color::Black;

            // Parse offset
            if (stopAttrs.count("offset")) {
                std::string off = stopAttrs["offset"];
                off.erase(std::remove_if(off.begin(), off.end(), ::isspace), off.end());

                try {
                    if (off.find('%') != std::string::npos) {
                        stop.offset = std::stof(off) / 100.0f;
                    }
                    else {
                        stop.offset = std::stof(off);
                    }
                    stop.offset = std::max(0.0f, std::min(1.0f, stop.offset));
                }
                catch (...) {
                    stop.offset = 0.0f;
                }
            }

            // Parse stop-color
            sf::Color baseColor = sf::Color::Black;
            float opacity = 1.0f;

            if (stopAttrs.count("stop-color")) {
                baseColor = stringToColor(stopAttrs["stop-color"], "fill");
            }

            if (stopAttrs.count("stop-opacity")) {
                try {
                    opacity = std::stof(stopAttrs["stop-opacity"]);
                    opacity = std::max(0.0f, std::min(1.0f, opacity));
                }
                catch (...) {
                    opacity = 1.0f;
                }
            }

            // Parse từ style attribute
            if (stopAttrs.count("style")) {
                std::string style = stopAttrs["style"];

                // Parse stop-color từ style
                size_t colorPos = style.find("stop-color:");
                if (colorPos != std::string::npos) {
                    size_t startPos = colorPos + 11;
                    size_t endPos = style.find(';', startPos);
                    std::string colorValue = (endPos != std::string::npos)
                        ? style.substr(startPos, endPos - startPos)
                        : style.substr(startPos);

                    // Trim
                    size_t first = colorValue.find_first_not_of(" \t");
                    size_t last = colorValue.find_last_not_of(" \t");
                    if (first != std::string::npos && last != std::string::npos) {
                        colorValue = colorValue.substr(first, last - first + 1);
                        baseColor = stringToColor(colorValue, "fill");
                    }
                }

                // Parse stop-opacity từ style
                size_t opacityPos = style.find("stop-opacity:");
                if (opacityPos != std::string::npos) {
                    size_t startPos = opacityPos + 13;
                    size_t endPos = style.find(';', startPos);
                    std::string opacityValue = (endPos != std::string::npos)
                        ? style.substr(startPos, endPos - startPos)
                        : style.substr(startPos);

                    try {
                        opacity = std::stof(opacityValue);
                        opacity = std::max(0.0f, std::min(1.0f, opacity));
                    }
                    catch (...) {
                        opacity = 1.0f;
                    }
                }
            }

            // Áp dụng opacity vào alpha channel
            stop.color = sf::Color(
                baseColor.r,
                baseColor.g,
                baseColor.b,
                static_cast<std::uint8_t>(baseColor.a * opacity)
            );

            grad.stops.push_back(stop);
            stopCount++;

            std::cout << "    Stop #" << stopCount
                << ": offset=" << stop.offset
                << ", color=RGB(" << (int)stop.color.r << ","
                << (int)stop.color.g << "," << (int)stop.color.b << ")" << std::endl;
        }

        pos = endPos;
    }

    if (!inheritFrom.empty() && grad.stops.empty()) {
        auto it = gradients_.find(inheritFrom);
        if (it != gradients_.end()) {
            grad.stops = it->second.stops;
            std::cout << "  [parseGradient] Ke thua stops tu: " << inheritFrom << std::endl;
        }
    }

    // Validate và sort stops
    grad.validateStops();

    return grad;
}


// Hàm tiện ích: Giải mã các thực thể XML cơ bản
std::string decodeXMLEntities(std::string text)
{
    // Xử lý 5 thực thể cơ bản

    // &amp; -> &
    size_t pos = text.find("&amp;");
    while (pos != std::string::npos)
    {
        text.replace(pos, 5, "&");
        pos = text.find("&amp;", pos + 1);
    }

    // &lt; -> <
    pos = text.find("&lt;");
    while (pos != std::string::npos)
    {
        text.replace(pos, 4, "<");
        pos = text.find("&lt;", pos + 1);
    }

    // &gt; -> >
    pos = text.find("&gt;");
    while (pos != std::string::npos)
    {
        text.replace(pos, 4, ">");
        pos = text.find("&gt;", pos + 1);
    }

    // Xử lý thêm các thực thể khác nếu cần (&quot;, &apos;...)

    return text;
}

// --- Triển khai Hàm tiện ích ---
bool SVGParser::extractTagAndAttributes(const std::string& line, std::string& tagName, Attributes& attributes) const
{
    std::string trimmedLine = trim(line);
    attributes.clear();

    if (trimmedLine.length() < 3 || trimmedLine.front() != '<' || trimmedLine.back() != '>')
        return false;

    // Bỏ '<' ở đầu và '/>' hoặc '>' ở cuối
    std::string content = trimmedLine.substr(1, trimmedLine.length() - 2);
    if (content.back() == '/') content.pop_back();
    content = trim(content);
    if (content.empty()) return false;

    std::stringstream ss(content);

    // 1. Lấy tên Tag
    ss >> tagName;
    if (tagName.empty()) return false;

    while (ss.good())
    {
        std::string attrName;
        ss >> std::ws; // Bỏ qua khoảng trắng
        if (ss.eof()) break;

        // Đọc tên thuộc tính đến dấu '='
        if (!std::getline(ss, attrName, '=')) break;
        attrName = trim(attrName);
        if (attrName.empty()) continue;

        std::string value;
        char nextChar = ss.peek();

        // Bỏ qua khoảng trắng sau dấu =
        while (ss.good() && SafeCType::isSpace(nextChar)) {
            ss.ignore();
            nextChar = ss.peek();
        }

        if (nextChar == '"' || nextChar == '\'')
        {
            char quoteType = nextChar; // Lưu loại dấu nháy đang dùng
            ss.ignore(); // Bỏ qua dấu nháy mở
            std::getline(ss, value, quoteType); // Đọc đến khi gặp dấu nháy đóng tương ứng
        }
        else
        {
            // Trường hợp không có ngoặc (vd: width=100)
            ss >> value;
        }

        attributes[attrName] = value;
    }

    return true;
}

// Tạo đối tượng SVGElement dựa trên tên tag.
SVGElementPtr SVGParser::parseElementFromLine(const std::string &line)
{
    std::string tagName;
    Attributes attributes;

    if (!extractTagAndAttributes(line, tagName, attributes))
    {
        // Có thể là dòng trống, thẻ đóng, hoặc cú pháp không hợp lệ
        return nullptr;
    }

    // Chuyển tagName về chữ thường để so sánh không phân biệt chữ hoa/thường (tùy chọn)
    std::transform(tagName.begin(), tagName.end(), tagName.begin(),
        [](char c) { return SafeCType::toLower(c); });

    // Dựa vào tên tag, tạo đối tượng lớp con tương ứng
    if (tagName == "circle")
    {
        return std::make_shared<Circle>(attributes);
    }
    else if (tagName == "rect")
    {
        return std::make_shared<Rect>(attributes);
    }
    else if (tagName == "line")
    {
        return std::make_shared<Line>(attributes);
    }
    else if (tagName == "polygon")
    {
        return std::make_shared<Polygon>(attributes);
    }
    else if (tagName == "ellipse")
    {
        return std::make_shared<Ellipse>(attributes);
    }
    else if (tagName == "polyline")
    {
        return std::make_shared<Polyline>(attributes);
    }
    else if (tagName == "path")
    {
        return std::make_shared<Path>(attributes);
    }
    // Bỏ qua các tag khác như <svg>, <g>, ...

    return nullptr;
}

// --- Triển khai Hàm chính ---
// fine
// kiem tra file ton tai khong
bool SVGParser::isValidFile(const std::string &filename) const
{
    std::ifstream file(filename);
    return file.is_open();
}

// Đọc và phân tích cú pháp nội dung từ file SVG.
bool SVGParser::parseFile(const std::string& filename)
{
    elements_.clear();
    header_ = SVGHeader();
    gradients_.clear();
    std::ifstream file(filename);

    if (!file.is_open())
    {
        std::cerr << "Lỗi: Không thể mở file SVG: " << filename << std::endl;
        return false;
    }

    // Stack để theo dõi ta đang ở trong Group nào
    std::stack<Group*> groupStack;

    std::string segment;
    bool inDefs = false;

    while (std::getline(file, segment, '>'))
    {
        // getline này đọc đến dấu '>', nhưng nó vứt dấu '>' đi.
        // Ta cần cộng lại dấu '>' để thành tag hoàn chỉnh.
        segment += ">";

        // 1. Xử lý thẻ đóng Group </g>
        if (segment.find("</g>") != std::string::npos) {
            if (!groupStack.empty()) groupStack.pop();
            continue;
        }

        // 2. Phân tích tên thẻ và thuộc tính
        std::string tagName;
        Attributes attrs;
        if (!extractTagAndAttributes(segment, tagName, attrs)) {
            continue; // Bỏ qua nếu không phải thẻ hợp lệ
        }

        // Chuyển tên thẻ về chữ thường
        std::string lowerTag = tagName;
        std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(),
            [](char c) { return SafeCType::toLower(c); });

        // Track <defs>
        if (lowerTag == "defs") {
            inDefs = true;
            continue;
        }
        if (lowerTag == "/defs") {
            inDefs = false;
            continue;
        }

        // 3. Xử lý các thẻ đặc biệt
        if (lowerTag == "lineargradient" || lowerTag == "radialgradient") {
            Gradient grad = parseGradient(file, lowerTag, attrs);
            if (!grad.id.empty()) {
                gradients_[grad.id] = grad;
            }
            continue;
        }
    }

    file.clear();
    file.seekg(0);

    // Đọc các elements
    while (std::getline(file, segment, '>'))
    {
        segment += ">";

        // Xử lý thẻ đóng Group
        if (segment.find("</g>") != std::string::npos) {
            if (!groupStack.empty()) groupStack.pop();
            continue;
        }

        std::string tagName;
        Attributes attrs;
        if (!extractTagAndAttributes(segment, tagName, attrs)) {
            continue;
        }

        std::string lowerTag = tagName;
        std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(),
            [](char c) { return SafeCType::toLower(c); });

        // Bỏ qua gradient (đã parse rồi)
        if (lowerTag == "lineargradient" || lowerTag == "radialgradient") {
            // Skip đến thẻ đóng
            while (std::getline(file, segment, '>')) {
                segment += ">";
                std::string lower = segment;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                    [](char c) { return SafeCType::toLower(c); });
                if (lower.find("</" + lowerTag) != std::string::npos) {
                    break;
                }
            }
            continue;
        }

        // Bỏ qua các thẻ cấu trúc
        if (lowerTag == "defs" || lowerTag == "svg" || lowerTag == "/defs" || lowerTag == "/svg") {
            // Nếu là thẻ mở <svg>, đọc kích thước
            if (lowerTag == "svg") {
                if (attrs.count("width")) try { header_.width = std::stof(attrs["width"]); }
                catch (...) {}
                if (attrs.count("height")) try { header_.height = std::stof(attrs["height"]); }
                catch (...) {}
            }
            continue;
        }

        // 4. Xử lý Group <g>
        if (lowerTag == "g") {
            auto group = std::make_shared<Group>(attrs);
            Group* groupPtr = group.get();

            if (!groupStack.empty()) groupStack.top()->addElement(std::move(group));
            else elements_.push_back(std::move(group));

            groupStack.push(groupPtr);
            continue;
        }

        // 5. Xử lý Text
        if (lowerTag == "text") {
            std::string content;
            std::getline(file, content, '<');
            content = decodeXMLEntities(content);
            content = trim(content);
            std::string closingTag; std::getline(file, closingTag, '>'); // Đọc nốt thẻ đóng

            auto textObj = std::make_shared<Text>(attrs, content);
            if (!groupStack.empty()) groupStack.top()->addElement(std::move(textObj));
            else elements_.push_back(std::move(textObj));
            continue;
        }

        // 6. Xử lý các hình học cơ bản (Rect, Path, Circle...)
        SVGElementPtr element = parseElementFromLine(segment);
        if (element) {
            if (!groupStack.empty()) {
                groupStack.top()->addElement(std::move(element));
            }
            else {
                elements_.push_back(std::move(element));
            }
        }
    }

    if (elements_.empty()) {
        std::cout << "Canh bao: Khong tim thay phan tu hinh hoc SVG nao." << std::endl;
    }

    return true;
 }

SVGParser::ElementList SVGParser::takeElements()
{
    // std::move sẽ chuyển quyền sở hữu của vector elements_ cho bên gọi
    return std::move(elements_);
}

// Getter
const SVGParser::ElementList &SVGParser::getElements() const
{
    return elements_;
}

sf::Color SVGParser::stringToColor(const std::string& colorStr, const std::string& type) const
{
    std::string s = colorStr;

    // [FIX] Cast sang unsigned char
    s.erase(std::remove_if(s.begin(), s.end(),
        [](unsigned char c) { return std::isspace(c); }), s.end());
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });

    if (s == "none" || s == "transparent" || s.empty())
        return sf::Color::Transparent;

    // Xử lý URL
    if (s.find("url(") != std::string::npos)
    {
        return sf::Color::Black;  // Placeholder
    }

    // Xử lý RGB
    if (s.rfind("rgb(", 0) == 0 && s.back() == ')')
    {
        std::string content = s.substr(4, s.length() - 5);
        std::stringstream ss(content);
        std::string segment;
        std::vector<int> rgb;
        while (std::getline(ss, segment, ','))
        {
            try {
                rgb.push_back(std::stoi(segment));
            }
            catch (...) {
                return sf::Color::Black;
            }
        }
        if (rgb.size() >= 3)
        {
            auto clamp = [](int v) { return std::max(0, std::min(255, v)); };
            return sf::Color(
                static_cast<std::uint8_t>(clamp(rgb[0])),
                static_cast<std::uint8_t>(clamp(rgb[1])),
                static_cast<std::uint8_t>(clamp(rgb[2]))
            );
        }
    }

    // Danh sách màu
    static const std::map<std::string, sf::Color> colors = {
        {"black", sf::Color::Black},
        {"white", sf::Color::White},
        {"red", sf::Color::Red},
        {"blue", sf::Color::Blue},
        {"green", sf::Color::Green},
        {"yellow", sf::Color::Yellow},
        {"cyan", sf::Color::Cyan},
        {"magenta", sf::Color::Magenta},
        {"gray", sf::Color(128, 128, 128)},
        {"orange", sf::Color(255, 165, 0)},
        {"purple", sf::Color(128, 0, 128)},
        { "#fd5", sf::Color(255, 221, 85) },
        {"#60f", sf::Color(102, 0, 255)},
        {"#ff543e", sf::Color(255, 84, 62)},
        {"#c837ab", sf::Color(200, 55, 171)},
        {"#3771c8", sf::Color(55, 113, 200)}
    };

    if (colors.count(s))
        return colors.at(s);

    // Xử lý HEX
    if (s[0] == '#')
    {
        s.erase(0, 1);
        if (s.size() == 3)
        {
            std::string t;
            for (char c : s) {
                t += c;
                t += c;
            }
            s = t;
        }
        if (s.size() >= 6)
        {
            unsigned int hex;
            std::stringstream ss;
            ss << std::hex << s;
            ss >> hex;
            return sf::Color((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
        }
    }

    if (type == "fill")
        return sf::Color::Black;
    return sf::Color::Transparent;
}

