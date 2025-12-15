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

// Hàm tiện ích: Loại bỏ khoảng trắng ở đầu và cuối chuỗi
static inline std::string trim(const std::string &s)
{
    auto wsfront = std::find_if_not(s.begin(), s.end(), ::isspace);
    auto wsback = std::find_if_not(s.rbegin(), s.rend(), ::isspace).base();
    return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
}

// --- Triển khai Hàm tiện ích ---

// HÀM TIỆN ÍCH: Trích xuất tên tag và map thuộc tính từ một chuỗi
// [SVGParser.cpp] Thay thế hàm extractTagAndAttributes

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
        while (ss.good() && std::isspace(nextChar)) {
            ss.ignore();
            nextChar = ss.peek();
        }

        // [SỬA LỖI QUAN TRỌNG] Hỗ trợ cả nháy kép (") và nháy đơn (')
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
    std::transform(tagName.begin(), tagName.end(), tagName.begin(), ::tolower);

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
bool SVGParser::parseFile(const std::string &filename)
{
    elements_.clear();
    header_ = SVGHeader();
    std::ifstream file(filename);

    if (!file.is_open())
    {
        std::cerr << "Lỗi: Không thể mở file SVG: " << filename << std::endl;
        return false;
    }

    // [THÊM MỚI] Stack để theo dõi ta đang ở trong Group nào
    std::stack<Group*> groupStack;

    std::string segment;
    while (std::getline(file, segment, '>'))
    {
        // getline này đọc đến dấu '>', nhưng nó vứt dấu '>' đi.
        // Ta cần cộng lại dấu '>' để thành tag hoàn chỉnh.
        segment += ">";

        // xử lý thẻ đóng group </g> ---
        if (segment.find("</g>") != std::string::npos) {
            if (!groupStack.empty()) {
                groupStack.pop(); // <--- Thoát khỏi Group hiện tại, quay về cha
            }
            continue;
        }

        if (segment.find("<text") != std::string::npos && segment.find("/>") == std::string::npos)
        {

            // Đây là thẻ mở <text ...>. Chúng ta cần phân tích thuộc tính trước.
            std::string tagName;
            Attributes attributes;
            extractTagAndAttributes(segment, tagName, attributes);

            // 2. Đọc tiếp phần nội dung chữ cho đến khi gặp dấu '<' của thẻ đóng </text>
            std::string content;
            std::getline(file, content, '<');
            content = decodeXMLEntities(content);
            content = trim(content);
            // Sau lệnh này, con trỏ file đang đứng ở "</text>" nhưng dấu '<' đã bị bỏ qua.
            // Chúng ta cần đọc nốt phần "/text>" để dọn dẹp
            std::string closingTag;
            std::getline(file, closingTag, '>');

            auto textObj = std::make_shared<Text>(attributes, content);
            if (!groupStack.empty()) {
                groupStack.top()->addElement(std::move(textObj)); // Thêm vào Group đang mở
            }
            else {
                elements_.push_back(std::move(textObj)); // Thêm vào root
            }
            continue;
        }
        // Kiểm tra xem segment có chứa dấu '<' không (để lọc bỏ khoảng trắng giữa các tag)
        if (segment.find('<') == std::string::npos)
        {
            continue;
        }

        // xử lý thẻ mở group <g> ---
        bool isGroupTag = false;
        std::string checkStr = segment;
        checkStr.erase(std::remove_if(checkStr.begin(), checkStr.end(), ::isspace), checkStr.end());

        // Kiểm tra xem có phải <g> thực sự không (tránh nhầm với polygon/image)
        if (checkStr.find("<g") != std::string::npos &&
            checkStr.find("</g>") == std::string::npos &&
            segment.find("polygon") == std::string::npos &&
            segment.find("image") == std::string::npos)
        {
            isGroupTag = true;
        }

        if (isGroupTag) {
            std::string tagName; Attributes attributes;
            extractTagAndAttributes(segment, tagName, attributes);

            // [LOGIC MỚI] Tạo Group và đẩy vào Stack
            auto group = std::make_shared<Group>(attributes);
            Group* groupPtr = group.get(); // Lấy con trỏ thô

            if (!groupStack.empty()) {
                groupStack.top()->addElement(std::move(group)); // <--- Group con nằm trong Group cha
            }
            else {
                elements_.push_back(std::move(group)); // <--- Group gốc
            }

            groupStack.push(groupPtr); // <--- Đẩy lên đỉnh Stack để hứng các con tiếp theo
            continue;
        }

        // --- 4. [SỬA ĐỔI] XỬ LÝ CÁC HÌNH CƠ BẢN ---
        if (segment.find('<') == std::string::npos) continue;

        SVGElementPtr element = parseElementFromLine(segment);

        if (element)
        {
            // [SỬA ĐỔI QUAN TRỌNG] Kiểm tra xem có đang ở trong Group nào không
            if (!groupStack.empty()) {
                groupStack.top()->addElement(std::move(element)); // <--- Nhét vào Group
            }
            else {
                elements_.push_back(std::move(element)); // <--- Nhét vào Root
            }
        }
        if (segment.find("<svg") != std::string::npos && segment.find("</svg>") == std::string::npos) {
            std::string tagName;
            Attributes attrs;
            if (extractTagAndAttributes(segment, tagName, attrs)) {
                // 1. Đọc width/height (nếu có)
                if (attrs.count("width")) try { header_.width = std::stof(attrs["width"]); }
                catch (...) {}
                if (attrs.count("height")) try { header_.height = std::stof(attrs["height"]); }
                catch (...) {}

                // 2. Đọc viewBox="x y w h"
                if (attrs.count("viewBox")) {
                    std::stringstream ss(attrs["viewBox"]);
                    float vals[4] = { 0 };
                    int i = 0;
                    // Đọc 4 số, xử lý dấu phẩy hoặc khoảng trắng
                    while (ss.good() && i < 4) {
                        // Bỏ qua dấu phẩy
                        if (ss.peek() == ',') ss.ignore();
                        ss >> vals[i];
                        i++;
                        // Bỏ qua khoảng trắng sau số
                        ss >> std::ws;
                    }

                    if (i == 4) { // Đọc đủ 4 số
                        header_.viewBoxX = vals[0];
                        header_.viewBoxY = vals[1];
                        header_.viewBoxWidth = vals[2];
                        header_.viewBoxHeight = vals[3];
                        header_.hasViewBox = true;
                    }
                }
            }
        }
    }

    if (elements_.empty())
    {
        std::cout << "Cảnh báo: Không tìm thấy phần tử hình học SVG nào trong file." << std::endl;
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
