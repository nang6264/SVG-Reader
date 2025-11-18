// SVGParser.cpp
#include "SVGParser.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional> // Dùng cho std::bind

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
bool SVGParser::extractTagAndAttributes(const std::string &line, std::string &tagName, Attributes &attributes) const
{
    // std::cout << line << "\n";
    std::string trimmedLine = trim(line);
    // std::cout << trimmedLine << "\n";
    attributes.clear();

    if (trimmedLine.length() < 3 || trimmedLine.front() != '<' || trimmedLine.back() != '>')
    {
        return false; // Không phải tag hợp lệ
    }

    // Bỏ '<' ở đầu và '/>' hoặc '>' ở cuối
    std::string content = trimmedLine.substr(1, trimmedLine.length() - 2);
    if (content.back() == '/')
    {
        content.pop_back(); // Bỏ '/' cho self-closing tag
    }
    content = trim(content);

    if (content.empty())
        return false;

    std::stringstream ss(content);
    std::string token;

    // 1. Lấy tên Tag
    ss >> tagName;

    if (tagName.empty())
        return false;

    while (ss.good())
    {
        std::string attrName;

        // Bỏ qua khoảng trắng trước tên thuộc tính
        ss >> std::ws;

        // Nếu hết dòng thì dừng
        if (ss.eof())
            break;

        // Đọc tên thuộc tính cho đến khi gặp dấu '='
        if (!std::getline(ss, attrName, '='))
            break;

        attrName = trim(attrName); // Xóa khoảng trắng thừa nếu có
        if (attrName.empty())
            continue;

        std::string value;
        // Kiểm tra ký tự tiếp theo để xem có phải là dấu ngoặc kép không
        // ss >> std::ws đã được xử lý ngầm bởi getline ở vòng sau hoặc ta peek ngay

        char nextChar;
        // Bỏ qua khoảng trắng (nếu có) giữa dấu = và giá trị
        while (ss.good() && std::isspace(ss.peek()))
        {
            ss.ignore();
        }

        if (ss.peek() == '"')
        {
            // TRƯỜNG HỢP 1: Giá trị nằm trong ngoặc kép (points="10 20 30")
            ss.ignore();                  // Bỏ qua dấu " mở đầu
            std::getline(ss, value, '"'); // Đọc cho đến khi gặp dấu " đóng
        }
        else
        {
            // TRƯỜNG HỢP 2: Giá trị không có ngoặc (width=100)
            ss >> value;
        }

        // Lưu vào map
        attributes[attrName] = value;

        // std::cout << "Debug: " << attrName << "\t" << value << "\n";
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
        return std::make_unique<Circle>(attributes);
    }
    else if (tagName == "rect")
    {
        return std::make_unique<Rect>(attributes);
    }
    else if (tagName == "line")
    {
        return std::make_unique<Line>(attributes);
    }
    else if (tagName == "polygon")
    {
        return std::make_unique<Polygon>(attributes);
    }
    else if (tagName == "ellipse")
    {
        return std::make_unique<Ellipse>(attributes);
    }
    else if (tagName == "polyline")
    {
        return std::make_unique<Polyline>(attributes);
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
    std::ifstream file(filename);

    if (!file.is_open())
    {
        std::cerr << "Lỗi: Không thể mở file SVG: " << filename << std::endl;
        return false;
    }

    // Cách đọc mới: Đọc theo từng cụm thẻ (đến khi gặp dấu '>')
    std::string segment;
    while (std::getline(file, segment, '>'))
    {
        // getline này đọc đến dấu '>', nhưng nó vứt dấu '>' đi.
        // Ta cần cộng lại dấu '>' để thành tag hoàn chỉnh.
        segment += ">";
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

            // 3. Tạo đối tượng Text và thêm vào list
            // Xóa khoảng trắng thừa trong content (nếu muốn)
            // content = trim(content);
            if (!content.empty())
            {
                elements_.push_back(std::make_unique<Text>(attributes, content));
            }

            continue; // Xong thẻ text, quay lại vòng lặp
        }
        // Kiểm tra xem segment có chứa dấu '<' không (để lọc bỏ khoảng trắng giữa các tag)
        if (segment.find('<') == std::string::npos)
        {
            continue;
        }

        // Phân tích cú pháp đoạn tag này
        SVGElementPtr element = parseElementFromLine(segment);

        if (element)
        {
            elements_.push_back(std::move(element));
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