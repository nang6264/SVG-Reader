#include "SVGRenderer.h"
#include "SVGElement.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <array>
#include "Transform.h"
#include "SVGPath.h"
#include "earcut.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// --- HÀM MỚI: TẠO TEXTURE GRADIENT ---
// Dùng để tạo ra "tờ giấy màu" mịn màng thay vì tô màu đỉnh
sf::Texture generateGradientTexture(const Gradient &grad, const sf::FloatRect &bbox)
{
    sf::Image img;
    unsigned int width = 256;
    unsigned int height = 256;

    // [FIX LỖI 1]: SFML 3.0 đổi create(w, h) thành resize({w, h})
    img.resize({width, height});

    for (unsigned int y = 0; y < height; ++y)
    {
        for (unsigned int x = 0; x < width; ++x)
        {
            float worldX = bbox.position.x + (x / (float)width) * bbox.size.x;
            float worldY = bbox.position.y + (y / (float)height) * bbox.size.y;

            sf::Color c;
            if (grad.type == "linear")
            {
                c = grad.getLinearColorAt(worldX, worldY, bbox);
            }
            else
            {
                c = grad.getRadialColorAt(worldX, worldY, bbox);
            }

            // [FIX LỖI 2]: SFML 3.0 yêu cầu truyền Vector2u thay vì x, y rời rạc
            // Thêm ngoặc nhọn {} bao quanh x, y
            img.setPixel({x, y}, c);
        }
    }

    sf::Texture tex;
    // [FIX WARNING]: loadFromImage trả về bool, cần check hoặc cast void để tắt warning
    (void)tex.loadFromImage(img);

    tex.setSmooth(true);
    return tex;
}

static inline std::string trimString(const std::string &s)
{
    auto wsfront = std::find_if_not(s.begin(), s.end(), [](int c)
                                    { return std::isspace(c); });
    auto wsback = std::find_if_not(s.rbegin(), s.rend(), [](int c)
                                   { return std::isspace(c); })
                      .base();
    return (wsback <= wsfront ? std::string() : std::string(wsfront, wsback));
}

void SVGRenderer::setGradients(const std::map<std::string, Gradient> &gradients)
{
    gradients_ = gradients;
}

// --- ARC MATH HELPER ---
// Hàm bổ trợ: Chuyển đổi cung Ellipse SVG (Endpoint parametrization) -> Danh sách điểm
// Hàm bổ trợ tính toán điểm cho cung tròn (Arc)
void addArc(std::vector<sf::Vector2f> &points, sf::Vector2f currentPos,
            float rx, float ry, float xAxisRotation,
            bool largeArcFlag, bool sweepFlag, float x, float y)
{
    if (rx == 0 || ry == 0)
    {
        points.push_back({x, y});
        return;
    }

    // 1. Chuẩn hóa góc và bán kính
    float phi = xAxisRotation * (M_PI / 180.0f);
    rx = std::abs(rx);
    ry = std::abs(ry);

    // 2. Tính toán ma trận chuyển đổi
    float dx2 = (currentPos.x - x) / 2.0f;
    float dy2 = (currentPos.y - y) / 2.0f;
    float x1p = std::cos(phi) * dx2 + std::sin(phi) * dy2;
    float y1p = -std::sin(phi) * dx2 + std::cos(phi) * dy2;

    // Check radii correction
    float radiiCheck = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
    if (radiiCheck > 1.0f)
    {
        float sqrtCheck = std::sqrt(radiiCheck);
        rx *= sqrtCheck;
        ry *= sqrtCheck;
    }

    // 3. Tính tâm ellipse
    float rx_sq = rx * rx;
    float ry_sq = ry * ry;
    float x1p_sq = x1p * x1p;
    float y1p_sq = y1p * y1p;

    float sign = (largeArcFlag == sweepFlag) ? -1.0f : 1.0f;
    float sq = (rx_sq * ry_sq - rx_sq * y1p_sq - ry_sq * x1p_sq) / (rx_sq * y1p_sq + ry_sq * x1p_sq);
    if (sq < 0)
        sq = 0;
    float coef = sign * std::sqrt(sq);

    float cxp = coef * ((rx * y1p) / ry);
    float cyp = coef * (-(ry * x1p) / rx);

    float cx = std::cos(phi) * cxp - std::sin(phi) * cyp + (currentPos.x + x) / 2.0f;
    float cy = std::sin(phi) * cxp + std::cos(phi) * cyp + (currentPos.y + y) / 2.0f;

    // 4. Tính góc
    auto angle = [&](float ux, float uy, float vx, float vy)
    {
        float dot = ux * vx + uy * vy;
        float len = std::sqrt(ux * ux + uy * uy) * std::sqrt(vx * vx + vy * vy);
        float ang = std::acos(std::max(-1.0f, std::min(1.0f, dot / len)));
        if ((ux * vy - uy * vx) < 0)
            ang = -ang;
        return ang;
    };

    float theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
    float dtheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);

    if (!sweepFlag && dtheta > 0)
        dtheta -= 2 * M_PI;
    else if (sweepFlag && dtheta < 0)
        dtheta += 2 * M_PI;

    // 5. Sampling (Chia nhỏ cung thành điểm)
    int segments = static_cast<int>(std::abs(dtheta) * std::max(rx, ry));
    if (segments < 20)
        segments = 20; // Đảm bảo độ mịn tối thiểu

    for (int i = 0; i <= segments; ++i)
    {
        float t = i / (float)segments;
        float currentTheta = theta1 + t * dtheta;
        float px = rx * std::cos(currentTheta);
        float py = ry * std::sin(currentTheta);

        float finalX = std::cos(phi) * px - std::sin(phi) * py + cx;
        float finalY = std::sin(phi) * px + std::cos(phi) * py + cy;

        points.push_back({finalX, finalY});
    }
}

// --- Math Helpers ---
float getLength(const sf::Vector2f &v) { return std::sqrt(v.x * v.x + v.y * v.y); }
sf::Vector2f normalize(const sf::Vector2f &v)
{
    float len = getLength(v);
    return (len < 0.0001f) ? sf::Vector2f(0, 0) : v / len;
}

sf::Vector2f getNormal(const sf::Vector2f &v) { return {-v.y, v.x}; }
float dotProduct(const sf::Vector2f &a, const sf::Vector2f &b) { return a.x * b.x + a.y * b.y; }
float crossProduct(const sf::Vector2f &a, const sf::Vector2f &b) { return a.x * b.y - a.y * b.x; }

sf::Vector2f cubicBezier(float t, sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3)
{
    float u = 1 - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;
    return (uuu * p0) + (3 * uu * t * p1) + (3 * u * tt * p2) + (ttt * p3);
}

// --- Fill Helpers ---
// Kiểm tra điểm có nằm trong đa giác không (Ray-casting algorithm)
bool isPointInPolygon(const sf::Vector2f &point, const std::vector<sf::Vector2f> &polygon)
{
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++)
    {
        if (((polygon[i].y > point.y) != (polygon[j].y > point.y)) &&
            (point.x < (polygon[j].x - polygon[i].x) * (point.y - polygon[i].y) / (polygon[j].y - polygon[i].y) + polygon[i].x))
        {
            inside = !inside;
        }
    }
    return inside;
}

// Kiểm tra đa giác inner có nằm hoàn toàn trong đa giác outer không
bool isPolygonInside(const std::vector<sf::Vector2f> &inner, const std::vector<sf::Vector2f> &outer)
{
    if (inner.empty() || outer.empty())
        return false;
    return isPointInPolygon(inner[0], outer);
}

// Tính diện tích đa giác (Shoelace formula)
double getArea(const std::vector<sf::Vector2f> &points)
{
    double area = 0.0;
    if (points.size() < 3)
        return 0.0;
    for (size_t i = 0; i < points.size(); ++i)
    {
        size_t j = (i + 1) % points.size();
        area += (double)(points[i].x * points[j].y) - (double)(points[j].x * points[i].y);
    }
    return std::abs(area) / 2.0;
}

// Vẽ đa giác lõm sử dụng thư viện Earcut
void drawConcaveShape(sf::RenderWindow &window, const std::vector<std::vector<sf::Vector2f>> &inputShapes, sf::Color color)
{
    if (inputShapes.empty())
        return;
    using Point = std::array<double, 2>;
    std::vector<std::vector<Point>> polygon;
    std::vector<sf::Vector2f> flatPoints;

    for (const auto &shape : inputShapes)
    {
        if (shape.size() < 3)
            continue;
        std::vector<Point> ring;
        for (const auto &p : shape)
            ring.push_back({(double)p.x, (double)p.y});
        if (ring.size() >= 3)
        {
            auto &f = ring.front();
            auto &l = ring.back();
            if (std::abs(f[0] - l[0]) < 0.001 && std::abs(f[1] - l[1]) < 0.001)
                ring.pop_back();
        }
        if (ring.size() >= 3)
        {
            polygon.push_back(ring);
            for (const auto &p : ring)
                flatPoints.emplace_back((float)p[0], (float)p[1]);
        }
    }
    if (polygon.empty())
        return;
    std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);
    sf::VertexArray vertices(sf::PrimitiveType::Triangles);
    for (uint32_t index : indices)
    {
        if (index < flatPoints.size())
            // [ĐÃ SỬA]: SFML 3.0 aggregate init
            vertices.append(sf::Vertex{flatPoints[index], color});
    }
    window.draw(vertices);
}

// Tính giao điểm của hai đoạn thẳng (p1-p2 và p3-p4)
bool getLineIntersection(sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3, sf::Vector2f p4, sf::Vector2f &intersection)
{
    float d = (p1.x - p2.x) * (p3.y - p4.y) - (p1.y - p2.y) * (p3.x - p4.x);
    if (std::abs(d) < 0.001f)
        return false; // Song song

    float t = ((p1.x - p3.x) * (p3.y - p4.y) - (p1.y - p3.y) * (p3.x - p4.x)) / d;
    float u = ((p1.x - p3.x) * (p1.y - p2.y) - (p1.y - p3.y) * (p1.x - p2.x)) / d;

    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f)
    {
        intersection.x = p1.x + t * (p2.x - p1.x);
        intersection.y = p1.y + t * (p2.y - p1.y);
        return true;
    }
    return false;
}

// xu li fallback
std::pair<bool, sf::Color> SVGRenderer::resolveColor(const std::string &fillStr, float opacity)
{
    if (fillStr.find("url(") != std::string::npos)
    {
        size_t start = fillStr.find("#");
        size_t end = fillStr.find(")");

        if (start != std::string::npos && end != std::string::npos)
        {
            std::string id = fillStr.substr(start + 1, end - start - 1);

            auto it = gradients_.find(id);
            if (it != gradients_.end())
            {
                // TÌM THẤY GRADIENT
                return {true, it->second.getAverageColor(opacity)};
            }
            else
            {
                // KHÔNG TÌM THẤY -> Fallback
                return {false, sf::Color::Black};
            }
        }
    }

    // Xử lý màu thường
    sf::Color color = stringToColor(fillStr, "fill");
    color.a = static_cast<std::uint8_t>(color.a * opacity);
    return {false, color};
}

// Sửa lại điểm đa giác ngôi sao thành đa giác đơn bao quanh
std::vector<sf::Vector2f> fixStarPolygon(const std::vector<sf::Vector2f> &pts)
{
    // Chỉ xử lý nếu đúng là 5 điểm (Pentagram)
    if (pts.size() != 5)
        return pts;

    std::vector<sf::Vector2f> newPts;

    // Thứ tự đỉnh của ngôi sao vẽ đan chéo: 0 -> 1 -> 2 -> 3 -> 4 -> 0
    // Thứ tự đỉnh của đa giác đơn bao quanh:
    // 0(A) -> Int(01, 23) -> 2(C) -> Int(12, 34) -> 4(E) -> Int(40, 12) -> 1(B) -> Int(01, 34) -> 3(D) -> Int(23, 40)

    // Mảng chỉ mục ánh xạ theo logic trên
    int sequence[] = {0, 2, 4, 1, 3};

    for (int i = 0; i < 5; ++i)
    {
        int curr = sequence[i];
        int next = sequence[(i + 1) % 5];

        // 1. Thêm đỉnh nhọn (Tip)
        newPts.push_back(pts[curr]);

        // 2. Tính toán điểm lõm (Armpit) - Giao điểm của các cạnh đối diện
        sf::Vector2f p1 = pts[curr];           // Start of Edge 1
        sf::Vector2f p2 = pts[(curr + 1) % 5]; // End of Edge 1

        sf::Vector2f p3 = pts[next];           // Start of Edge 2
        sf::Vector2f p4 = pts[(next + 1) % 5]; // End of Edge 2

        sf::Vector2f intersection;
        if (getLineIntersection(p1, p2, p3, p4, intersection))
        {
            newPts.push_back(intersection);
        }
        else
        {
            // Nếu không cắt nhau (không phải ngôi sao), trả về nguyên gốc để vẽ như đa giác thường
            return pts;
        }
    }

    return newPts;
}

// --- Stroke Helpers (Triangle Strip) ---
void drawSharpStroke(sf::RenderWindow &window, const std::vector<sf::Vector2f> &points, float thickness, sf::Color color, bool isClosed)
{
    if (points.size() < 2)
        return;
    std::vector<sf::Vector2f> path = points;

    if (isClosed)
    {
        if (getLength(path.front() - path.back()) < 0.1f)
            path.pop_back();
        path.insert(path.begin(), path.back());
        path.push_back(path[1]);
    }
    else
    {
        path.insert(path.begin(), path[0] + (path[0] - path[1]));
        path.push_back(path.back() + (path.back() - path[path.size() - 2]));
    }

    float halfW = thickness / 2.0f;
    sf::VertexArray strip(sf::PrimitiveType::TriangleStrip);

    for (size_t i = 1; i < path.size() - 1; ++i)
    {
        sf::Vector2f cur = path[i], prev = path[i - 1], next = path[i + 1];
        sf::Vector2f dir1 = normalize(cur - prev);
        sf::Vector2f dir2 = normalize(next - cur);
        sf::Vector2f tangent = normalize(dir1 + dir2);
        sf::Vector2f miter = {-tangent.y, tangent.x};
        sf::Vector2f normal = {-dir1.y, dir1.x};

        float dot = dotProduct(miter, normal);
        if (std::abs(dot) < 0.01f)
            dot = 1.0f;
        float miterLen = halfW / dot;
        if (miterLen > thickness * 5.0f)
            miterLen = thickness * 5.0f;

        // [ĐÃ SỬA]: SFML 3.0 aggregate init
        strip.append(sf::Vertex{cur + miter * miterLen, color});
        strip.append(sf::Vertex{cur - miter * miterLen, color});
    }
    if (isClosed)
    {
        strip.append(strip[0]);
        strip.append(strip[1]);
    }
    window.draw(strip);
}

// --- Main Class ---

void SVGRenderer::initializeHelpMenu()
{
    sf::Vector2u windowSize = window.getSize();

    float menuWidth = 220.f;
    float menuHeight = 100.f;
    float margin = 10.f;

    // Vị trí: Góc Dưới Phải
    float fixedX = (float)windowSize.x - margin - menuWidth;
    float fixedY = (float)windowSize.y - margin - menuHeight;

    // --- NỀN MENU ---
    helpMenuBackground_.setSize({menuWidth, menuHeight});
    helpMenuBackground_.setFillColor(sf::Color(30, 30, 30, 200));
    helpMenuBackground_.setOutlineThickness(1.f);
    helpMenuBackground_.setOutlineColor(sf::Color(255, 255, 255));
    helpMenuBackground_.setPosition({fixedX, fixedY});

    // --- NỘI DUNG TEXT ---
    std::string menuContent =
        "[CONTROLS]\n"
        "Zoom: Mouse Wheel\n"
        "Rotate: Q / E keys (Left / Right)\n"
        "Move: Hold Left Button\n"
        "Exit: Escape key";

    helpMenuText_.setFont(font); // Đảm bảo font đã load
    helpMenuText_.setString(menuContent);
    helpMenuText_.setCharacterSize(14);
    helpMenuText_.setFillColor(sf::Color::White);

    // Đặt vị trí text (hơi vào trong so với góc của nền)
    helpMenuText_.setPosition({fixedX + margin, fixedY + margin});
}

void SVGRenderer::drawHelpMenu()
{
    sf::View currentView = window.getView();
    window.setView(window.getDefaultView());

    window.draw(helpMenuBackground_);
    window.draw(helpMenuText_);

    window.setView(currentView);
}

// Triển khai các hàm của SVGRenderer
SVGRenderer::SVGRenderer(unsigned int width, unsigned int height) : helpMenuText_(font)
{
    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    // [ĐÃ SỬA]: Dùng tham số width, height thay vì hardcode
    window.create(sf::VideoMode({width, height}), "SVG Renderer", sf::Style::Default, sf::State::Windowed, settings);
    view = window.getDefaultView();
    if (!font.openFromFile("times.ttf"))
    {
    }
    initializeHelpMenu();
}

// --- Triển khai các hàm thêm phần tử và render ---
void SVGRenderer::addElement(std::shared_ptr<SVGElement> element)
{
    if (element)
        elements.push_back(element);
}
void SVGRenderer::setViewBox(float viewBoxX, float viewBoxY, float viewBoxW, float viewBoxH)
{
    sf::Vector2u winSize = window.getSize();

    // Tỉ lệ khung hình của cửa sổ và của SVG
    float windowRatio = static_cast<float>(winSize.x) / winSize.y;
    float svgRatio = viewBoxW / viewBoxH;

    // Kích thước View (Camera) mặc định bằng kích thước SVG
    sf::Vector2f viewSize = {viewBoxW, viewBoxH};

    // Tâm của View nằm giữa SVG
    sf::Vector2f viewCenter = {viewBoxX + viewBoxW / 2.0f, viewBoxY + viewBoxH / 2.0f};

    // ĐIỀU CHỈNH CAMERA ĐỂ KHỚP TỈ LỆ MÀN HÌNH
    if (windowRatio > svgRatio)
    {
        // Màn hình bè hơn SVG (Widescreen) -> Mở rộng chiều ngang của Camera
        // Giữ nguyên chiều cao để hình không bị cắt trên/dưới
        viewSize.x = viewBoxH * windowRatio;
    }
    else
    {
        // Màn hình cao hơn SVG (Portrait) -> Mở rộng chiều dọc của Camera
        // Giữ nguyên chiều rộng để hình không bị cắt trái/phải
        viewSize.y = viewBoxW / windowRatio;
    }

    // Cập nhật View
    view.setSize(viewSize);
    view.setCenter(viewCenter);

    // [QUAN TRỌNG] Luôn vẽ trên toàn bộ cửa sổ (0,0 -> 1,1)
    // Không dùng viewport co nhỏ nữa để tránh bị cắt hình khi zoom
    view.setViewport(sf::FloatRect({0.f, 0.f}, {1.f, 1.f}));

    // Áp dụng ngay lập tức
    window.setView(view);
}

void SVGRenderer::zoomIn() { view.zoom(0.9f); }
void SVGRenderer::zoomOut() { view.zoom(1.1f); }
void SVGRenderer::rotate(float angle) { view.rotate(sf::degrees(angle)); }
sf::Transform SVGRenderer::getSFMLTransform(const TransformMatrix &m) const { return sf::Transform(m.m[0], m.m[3], m.m[2], m.m[1], m.m[4], m.m[5], 0, 0, 1); }

void SVGRenderer::render()
{
    while (window.isOpen())
    {
        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
            else if (auto *k = event->getIf<sf::Event::KeyPressed>())
            {
                if (k->scancode == sf::Keyboard::Scancode::Escape)
                    window.close();
                else if (k->scancode == sf::Keyboard::Scancode::Q)
                    rotate(10.f);
                else if (k->scancode == sf::Keyboard::Scancode::E)
                    rotate(-10.f);
            }
            else if (auto *w = event->getIf<sf::Event::MouseWheelScrolled>())
            {
                w->delta > 0 ? zoomIn() : zoomOut();
            }
            else if (auto *p = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (p->button == sf::Mouse::Button::Left)
                {
                    isPanning = true;
                    lastMousePos = p->position;
                }
            }
            else if (auto *r = event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (r->button == sf::Mouse::Button::Left)
                    isPanning = false;
            }
            else if (auto *m = event->getIf<sf::Event::MouseMoved>())
            {
                if (isPanning)
                {
                    sf::Vector2f delta = window.mapPixelToCoords(lastMousePos) - window.mapPixelToCoords(m->position);
                    view.move(delta);
                    window.setView(view);
                    lastMousePos = m->position;
                }
            }
        }
        window.clear(sf::Color::White);
        window.setView(view);
        for (auto &e : elements)
            e->draw(*this);
        drawHelpMenu();
        window.display();
    }
}

// --- Helpers ---
float getOpacity(const std::map<std::string, std::string> &attrs, std::string key)
{
    auto it = attrs.find(key);
    if (it != attrs.end())
        try
        {
            return std::stof(it->second);
        }
        catch (...)
        {
        }
    return 1.0f;
}

// Hàm loại bỏ các điểm trùng nhau liên tiếp để tránh lỗi Earcut
// Hàm loại bỏ điểm trùng lặp (đã chỉnh epsilon xuống cực nhỏ)
std::vector<sf::Vector2f> cleanPolygonPoints(const std::vector<sf::Vector2f> &points)
{
    if (points.empty())
        return {};

    std::vector<sf::Vector2f> cleaned;
    cleaned.push_back(points[0]);

    for (size_t i = 1; i < points.size(); ++i)
    {
        // Chỉ loại bỏ nếu điểm TRÙNG KHÍT (khoảng cách < 0.0001)
        // Logo Firefox có nhiều điểm rất gần nhau, nếu xoá bừa sẽ bị hở hình
        if (getLength(points[i] - cleaned.back()) > 0.0001f)
        {
            cleaned.push_back(points[i]);
        }
    }

    // Lưu ý: Không tự động xoá điểm cuối trùng đầu ở đây để Earcut tự xử lý
    return cleaned;
}

// Chuyển đổi chuỗi màu SVG sang sf::Color
sf::Color SVGRenderer::stringToColor(std::string colorStr, std::string type)
{
    std::string s = colorStr;
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s == "none" || s == "transparent" || s.empty())
        return sf::Color::Transparent;
    if (s.find("url(") != std::string::npos)
        return sf::Color::Black;

    if (s.rfind("rgb(", 0) == 0 && s.back() == ')')
    {
        std::string content = s.substr(4, s.length() - 5);
        std::stringstream ss(content);
        std::string segment;
        std::vector<int> rgb;
        while (std::getline(ss, segment, ','))
        {
            try
            {
                rgb.push_back(std::stoi(segment));
            }
            catch (...)
            {
                return sf::Color::Black;
            }
        }
        if (rgb.size() >= 3)
        {
            auto clamp = [](int v)
            { return std::max(0, std::min(255, v)); };
            return sf::Color(static_cast<std::uint8_t>(clamp(rgb[0])), static_cast<std::uint8_t>(clamp(rgb[1])), static_cast<std::uint8_t>(clamp(rgb[2])));
        }
    }

    static const std::map<std::string, sf::Color> colors = {
        {"black", sf::Color::Black}, {"white", sf::Color::White}, {"red", sf::Color::Red}, {"lime", sf::Color::Green}, {"blue", sf::Color::Blue}, {"yellow", sf::Color::Yellow}, {"cyan", sf::Color::Cyan}, {"magenta", sf::Color::Magenta}, {"gray", sf::Color(128, 128, 128)}, {"orange", sf::Color(255, 165, 0)}, {"purple", sf::Color(128, 0, 128)}, {"green", sf::Color(0, 128, 0)}, {"#3bb553", sf::Color(59, 181, 83)}, // Chrome Green
        {"#ea4335", sf::Color(234, 67, 53)},                                                                                                                                                                                                                                                                                                                                                                                   // Chrome Red
        {"#fbbc05", sf::Color(251, 188, 5)},                                                                                                                                                                                                                                                                                                                                                                                   // Chrome Yellow
        {"#4285f4", sf::Color(66, 133, 244)}                                                                                                                                                                                                                                                                                                                                                                                   // Chrome Blue
    };

    if (colors.count(s))
        return colors.at(s);

    if (s[0] == '#')
    {
        s.erase(0, 1);
        if (s.size() == 3)
        {
            std::string t;
            for (char c : s)
            {
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
            if (s.size() == 8)
                return sf::Color((hex >> 24) & 0xFF, (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
            return sf::Color((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
        }
    }
    if (type == "fill")
        return sf::Color::Black;
    return sf::Color::Transparent;
}

// Hàm lấy thuộc tính có hiệu lực (Xử lý cả Style)
Attributes SVGRenderer::getEffectiveAttributes(const Attributes &localAttrs) const
{
    Attributes e;
    // 1. Kế thừa từ cha
    if (!renderStack_.empty())
        e = renderStack_.back().inheritedAttributes;

    // 2. Ghi đè bằng thuộc tính cục bộ
    for (auto &k : localAttrs)
        e[k.first] = k.second;

    // 3. Bóc tách chuỗi 'style' (để đọc fill:url(#b) của Chrome)
    if (e.count("style"))
    {
        std::string styleStr = e["style"];
        std::stringstream ss(styleStr);
        std::string segment; // [SỬA LỖI] Đổi tên biến thành 'segment' cho rõ ràng

        while (std::getline(ss, segment, ';'))
        {
            size_t p = segment.find(':');
            if (p != std::string::npos)
            {
                std::string k = segment.substr(0, p);
                std::string v = segment.substr(p + 1);

                // Xóa khoảng trắng thừa
                k.erase(0, k.find_first_not_of(" \t"));
                if (k.find_last_not_of(" \t") != std::string::npos)
                    k.erase(k.find_last_not_of(" \t") + 1);

                v.erase(0, v.find_first_not_of(" \t"));
                if (v.find_last_not_of(" \t") != std::string::npos)
                    v.erase(v.find_last_not_of(" \t") + 1);

                if (!k.empty())
                    e[k] = v;
            }
        }
    }
    return e;
}

// --- Basic Renderers ---

// renderCircle với Multi-Stop Support
void SVGRenderer::renderCircle(const Circle &c)
{
    Attributes attrs = getEffectiveAttributes(c.getAttributes());
    float opacity = getOpacity(attrs, "fill-opacity");
    std::string fillStr = attrs.count("fill") ? attrs["fill"] : "black";

    auto result = resolveColor(fillStr, opacity);
    bool isGradient = result.first;
    sf::Color solidColor = result.second;

    TransformMatrix tm = getCumulativeTransform();
    tm.combine(c.getTransform());
    sf::RenderStates states;
    states.transform = getSFMLTransform(tm);

    bool gradientDrawn = false;

    // Xử lý Gradient (Linear và Radial)
    if (isGradient)
    {
        size_t start = fillStr.find("#") + 1;
        size_t end = fillStr.find(")");
        std::string id = fillStr.substr(start, end - start);

        auto it = gradients_.find(id);
        if (it != gradients_.end())
        {
            const Gradient &grad = it->second;

            float cx = static_cast<float>(c.getCx());
            float cy = static_cast<float>(c.getCy());
            float r = static_cast<float>(c.getR());

            const int segments = 64; // Số cạnh hình tròn
            const int rings = 20;    // Số vòng tròn đồng tâm

            // Tính bounding box cho Linear Gradient
            sf::FloatRect bbox({cx - r, cy - r}, {2.0f * r, 2.0f * r});

            sf::VertexArray vertices(sf::PrimitiveType::Triangles);

            // RADIAL GRADIENT
            if (grad.type == "radial")
            {
                for (int ring = 0; ring < rings; ring++)
                {
                    float innerRadius = (ring / (float)rings) * r;
                    float outerRadius = ((ring + 1) / (float)rings) * r;

                    // Tính màu tại 2 vòng tròn
                    float innerT = ring / (float)rings;
                    float outerT = (ring + 1) / (float)rings;

                    sf::Color innerColor = grad.interpolateColor(innerT);
                    sf::Color outerColor = grad.interpolateColor(outerT);

                    innerColor.a = static_cast<std::uint8_t>(innerColor.a * opacity);
                    outerColor.a = static_cast<std::uint8_t>(outerColor.a * opacity);

                    float angleStep = 2.0f * M_PI / segments;

                    for (int i = 0; i < segments; i++)
                    {
                        float angle1 = i * angleStep;
                        float angle2 = (i + 1) * angleStep;

                        // Inner ring points
                        float ix1 = cx + std::cos(angle1) * innerRadius;
                        float iy1 = cy + std::sin(angle1) * innerRadius;
                        float ix2 = cx + std::cos(angle2) * innerRadius;
                        float iy2 = cy + std::sin(angle2) * innerRadius;

                        // Outer ring points
                        float ox1 = cx + std::cos(angle1) * outerRadius;
                        float oy1 = cy + std::sin(angle1) * outerRadius;
                        float ox2 = cx + std::cos(angle2) * outerRadius;
                        float oy2 = cy + std::sin(angle2) * outerRadius;

                        // Triangle 1
                        vertices.append(sf::Vertex{sf::Vector2f(ix1, iy1), innerColor});
                        vertices.append(sf::Vertex{sf::Vector2f(ox1, oy1), outerColor});
                        vertices.append(sf::Vertex{sf::Vector2f(ix2, iy2), innerColor});

                        // Triangle 2
                        vertices.append(sf::Vertex{sf::Vector2f(ix2, iy2), innerColor});
                        vertices.append(sf::Vertex{sf::Vector2f(ox1, oy1), outerColor});
                        vertices.append(sf::Vertex{sf::Vector2f(ox2, oy2), outerColor});
                    }
                }

                window.draw(vertices, states);
                gradientDrawn = true;
            }
            // LINEAR GRADIENT
            else if (grad.type == "linear")
            {
                for (int ring = 0; ring < rings; ring++)
                {
                    float innerRadius = (ring / (float)rings) * r;
                    float outerRadius = ((ring + 1) / (float)rings) * r;

                    float angleStep = 2.0f * M_PI / segments;

                    for (int i = 0; i < segments; i++)
                    {
                        float angle1 = i * angleStep;
                        float angle2 = (i + 1) * angleStep;

                        // Inner ring points
                        float ix1 = cx + std::cos(angle1) * innerRadius;
                        float iy1 = cy + std::sin(angle1) * innerRadius;
                        float ix2 = cx + std::cos(angle2) * innerRadius;
                        float iy2 = cy + std::sin(angle2) * innerRadius;

                        // Outer ring points
                        float ox1 = cx + std::cos(angle1) * outerRadius;
                        float oy1 = cy + std::sin(angle1) * outerRadius;
                        float ox2 = cx + std::cos(angle2) * outerRadius;
                        float oy2 = cy + std::sin(angle2) * outerRadius;

                        // Lấy màu Linear Gradient cho từng đỉnh
                        sf::Color c1 = grad.getLinearColorAt(ix1, iy1, bbox);
                        sf::Color c2 = grad.getLinearColorAt(ix2, iy2, bbox);
                        sf::Color c3 = grad.getLinearColorAt(ox1, oy1, bbox);
                        sf::Color c4 = grad.getLinearColorAt(ox2, oy2, bbox);

                        c1.a = static_cast<std::uint8_t>(c1.a * opacity);
                        c2.a = static_cast<std::uint8_t>(c2.a * opacity);
                        c3.a = static_cast<std::uint8_t>(c3.a * opacity);
                        c4.a = static_cast<std::uint8_t>(c4.a * opacity);

                        // Triangle 1
                        vertices.append(sf::Vertex{sf::Vector2f(ix1, iy1), c1});
                        vertices.append(sf::Vertex{sf::Vector2f(ox1, oy1), c3});
                        vertices.append(sf::Vertex{sf::Vector2f(ix2, iy2), c2});

                        // Triangle 2
                        vertices.append(sf::Vertex{sf::Vector2f(ix2, iy2), c2});
                        vertices.append(sf::Vertex{sf::Vector2f(ox1, oy1), c3});
                        vertices.append(sf::Vertex{sf::Vector2f(ox2, oy2), c4});
                    }
                }

                window.draw(vertices, states);
                gradientDrawn = true;
            }
        }
    }

    // Vẽ CircleShape cho stroke và fallback
    float r = static_cast<float>(c.getR());
    sf::CircleShape s(r);
    s.setOrigin({r, r});
    s.setPosition({static_cast<float>(c.getCx()), static_cast<float>(c.getCy())});
    s.setPointCount(100);

    if (gradientDrawn)
    {
        s.setFillColor(sf::Color::Transparent);
    }
    else
    {
        s.setFillColor(solidColor);
    }

    // Xử lý stroke
    bool hasStroke = false;
    std::string strokeStr = attrs.count("stroke") ? attrs["stroke"] : "none";
    if (strokeStr != "none")
    {
        sf::Color st = stringToColor(strokeStr, "stroke");
        if (st != sf::Color::Transparent)
        {
            hasStroke = true;
            st.a = static_cast<std::uint8_t>(getOpacity(attrs, "stroke-opacity") * 255);
            s.setOutlineColor(st);
            float w = 1.0f;
            if (attrs.count("stroke-width"))
                try
                {
                    w = std::stof(attrs["stroke-width"]);
                }
                catch (...)
                {
                }
            s.setOutlineThickness(w);
        }
    }

    if (!gradientDrawn || hasStroke)
    {
        window.draw(s, states);
    }
}

// vẽ hình chữ nhật
void SVGRenderer::renderRect(const Rect &r)
{
    // Lấy transform
    Attributes a = getEffectiveAttributes(r.getAttributes());
    TransformMatrix tm = getCumulativeTransform();
    tm.combine(r.getTransform());

    // Tính bounding box
    float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();

    // Xử lý Fill
    std::string fillStr = a.count("fill") ? a["fill"] : "black";
    float fillOpacity = getOpacity(a, "fill-opacity");

    // Check gradient và gọi hàm render gradient
    if (fillStr.find("url(#") != std::string::npos)
    {
        size_t start = fillStr.find("#") + 1;
        size_t end = fillStr.find(")");
        std::string gradId = fillStr.substr(start, end - start);

        auto it = gradients_.find(gradId);
        if (it != gradients_.end())
        {
            const Gradient &grad = it->second;

            // Vẽ Linear Gradient
            if (grad.type == "linear")
            {
                renderLinearGradientRect(r, grad, tm, fillOpacity);

                // Vẽ stroke nếu có
                std::string strokeStr = a.count("stroke") ? a["stroke"] : "none";
                sf::Color st = stringToColor(strokeStr, "stroke");
                if (st != sf::Color::Transparent)
                {
                    st.a = static_cast<std::uint8_t>(getOpacity(a, "stroke-opacity") * 255);
                    float th = 1.f;
                    if (a.count("stroke-width"))
                        try
                        {
                            th = std::stof(a["stroke-width"]);
                        }
                        catch (...)
                        {
                        }

                    if (st.a > 0 && th > 0)
                    {
                        std::vector<sf::Vector2f> pts;
                        pts.push_back({x, y});
                        pts.push_back({x + w, y});
                        pts.push_back({x + w, y + h});
                        pts.push_back({x, y + h});

                        for (auto &p : pts)
                        {
                            float tx, ty;
                            tm.transformPoint(p.x, p.y, tx, ty);
                            p.x = tx;
                            p.y = ty;
                        }
                        drawSharpStroke(window, pts, th, st, true);
                    }
                }
                return; // Xong, không cần vẽ solid color
            }
            // Vẽ Radial Gradient
            else if (grad.type == "radial")
            {
                renderRadialGradientRect(r, grad, tm, fillOpacity);

                // Vẽ stroke nếu có (tương tự linear)
                std::string strokeStr = a.count("stroke") ? a["stroke"] : "none";
                sf::Color st = stringToColor(strokeStr, "stroke");
                if (st != sf::Color::Transparent)
                {
                    st.a = static_cast<std::uint8_t>(getOpacity(a, "stroke-opacity") * 255);
                    float th = 1.f;
                    if (a.count("stroke-width"))
                        try
                        {
                            th = std::stof(a["stroke-width"]);
                        }
                        catch (...)
                        {
                        }

                    if (st.a > 0 && th > 0)
                    {
                        std::vector<sf::Vector2f> pts;
                        pts.push_back({x, y});
                        pts.push_back({x + w, y});
                        pts.push_back({x + w, y + h});
                        pts.push_back({x, y + h});

                        for (auto &p : pts)
                        {
                            float tx, ty;
                            tm.transformPoint(p.x, p.y, tx, ty);
                            p.x = tx;
                            p.y = ty;
                        }
                        drawSharpStroke(window, pts, th, st, true);
                    }
                }
                return;
            }
        }
        else
        {
            // Gradient không tìm thấy - dùng màu fallback xám
            fillStr = "gray";
        }
    }

    // Vẽ solid color (nếu không phải gradient)
    std::vector<sf::Vector2f> pts;
    pts.push_back({x, y});
    pts.push_back({x + w, y});
    pts.push_back({x + w, y + h});
    pts.push_back({x, y + h});

    for (auto &p : pts)
    {
        float tx, ty;
        tm.transformPoint(p.x, p.y, tx, ty);
        p.x = tx;
        p.y = ty;
    }

    sf::Color f = stringToColor(fillStr, "fill");
    if (f != sf::Color::Transparent)
    {
        f.a = static_cast<std::uint8_t>(getOpacity(a, "fill-opacity") * 255);
        drawConcaveShape(window, {pts}, f);
    }

    sf::Color st = stringToColor(a.count("stroke") ? a["stroke"] : "none", "stroke");
    if (st != sf::Color::Transparent)
        st.a = static_cast<std::uint8_t>(getOpacity(a, "stroke-opacity") * 255);
    float th = 1.f;
    if (a.count("stroke-width"))
        try
        {
            th = std::stof(a["stroke-width"]);
        }
        catch (...)
        {
        };
    if (st.a > 0 && th > 0)
        drawSharpStroke(window, pts, th, st, true);
}

// vẽ đường thẳng
void SVGRenderer::renderLine(const Line &l)
{
    Attributes a = getEffectiveAttributes(l.getAttributes());
    TransformMatrix tm = getCumulativeTransform();
    tm.combine(l.getTransform());
    float x1 = l.getX1(), y1 = l.getY1(), x2 = l.getX2(), y2 = l.getY2();
    tm.transformPoint(x1, y1, x1, y1);
    tm.transformPoint(x2, y2, x2, y2);
    sf::Color st = stringToColor(a.count("stroke") ? a["stroke"] : "none", "stroke");
    st.a = getOpacity(a, "stroke-opacity") * 255;
    float w = 1.f;
    if (a.count("stroke-width"))
        try
        {
            w = std::stof(a["stroke-width"]);
        }
        catch (...)
        {
        };
    if (st.a > 0 && w > 0)
        drawSharpStroke(window, {{x1, y1}, {x2, y2}}, w, st, false);
}

// vẽ chữ
void SVGRenderer::renderText(const Text &t)
{
    if (t.getContent().empty())
        return;

    Attributes attrs = getEffectiveAttributes(t.getAttributes());

    // 1. Font Size
    unsigned int finalFontSize = static_cast<unsigned int>(t.getFontSize());
    if (attrs.count("font-size"))
    {
        try
        {
            finalFontSize = static_cast<unsigned int>(std::stof(attrs["font-size"]));
        }
        catch (...)
        {
        }
    }

    // 2. Setup SFML Text
    sf::Text textShape(font);
    textShape.setString(t.getContent());
    textShape.setCharacterSize(finalFontSize);

    std::uint32_t style = sf::Text::Regular;
    if (attrs.count("font-weight") && (attrs["font-weight"] == "bold" || attrs["font-weight"] == "700"))
    {
        style |= sf::Text::Bold;
    }
    if (attrs.count("font-style") && attrs["font-style"] == "italic")
    {
        style |= sf::Text::Italic;
    }
    textShape.setStyle(style);

    // 3. Màu sắc & Viền
    std::string fillStr = attrs.count("fill") ? attrs["fill"] : "black";
    sf::Color fillColor = stringToColor(fillStr, "fill");
    if (fillColor != sf::Color::Transparent)
    {
        fillColor.a = static_cast<std::uint8_t>(getOpacity(attrs, "fill-opacity") * 255);
        textShape.setFillColor(fillColor);
    }
    else
    {
        textShape.setFillColor(sf::Color::Transparent);
    }

    std::string strokeStr = attrs.count("stroke") ? attrs["stroke"] : "none";
    if (strokeStr != "none")
    {
        sf::Color strokeColor = stringToColor(strokeStr, "stroke");
        if (strokeColor != sf::Color::Transparent)
        {
            strokeColor.a = static_cast<std::uint8_t>(getOpacity(attrs, "stroke-opacity") * 255);
            textShape.setOutlineColor(strokeColor);
            float strokeWidth = 1.0f;
            if (attrs.count("stroke-width"))
            {
                try
                {
                    strokeWidth = std::stof(attrs["stroke-width"]);
                }
                catch (...)
                {
                }
            }
            textShape.setOutlineThickness(strokeWidth);
        }
    }

    // 4. Transform & Position
    TransformMatrix tm = getCumulativeTransform();
    tm.combine(t.getTransform());

    float finalX = static_cast<float>(t.getX() + t.getDx());
    float finalY = static_cast<float>(t.getY() + t.getDy());

    finalY -= finalFontSize;

    // Cập nhật truy cập biến của sf::FloatRect cho SFML 3.0
    if (attrs.count("text-anchor"))
    {
        std::string anchor = attrs["text-anchor"];
        sf::FloatRect bounds = textShape.getLocalBounds();

        float originX = 0.0f;

        if (anchor == "middle")
        {
            // width -> size.x
            originX = bounds.size.x / 2.0f;
        }
        else if (anchor == "end")
        {
            // width -> size.x
            originX = bounds.size.x;
        }

        // left -> position.x
        textShape.setOrigin({originX + bounds.position.x, 0});
    }

    textShape.setPosition({finalX, finalY});

    sf::RenderStates states;
    states.transform = getSFMLTransform(tm);
    window.draw(textShape, states);
}

// vẽ đa giác
void SVGRenderer::renderPolygon(const Polygon &p)
{
    // 1. Parse điểm
    std::string pointsStr = p.getPoints();
    std::replace(pointsStr.begin(), pointsStr.end(), ',', ' ');
    std::stringstream iss(pointsStr);
    std::vector<sf::Vector2f> localPts;
    float x, y;
    while (iss >> x >> y)
    {
        localPts.emplace_back(x, y);
    }

    // Clean điểm trùng (Giữ lại để fix lỗi hình thoi)
    localPts = cleanPolygonPoints(localPts);
    if (localPts.size() < 3)
        return;

    Attributes a = getEffectiveAttributes(p.getAttributes());

    // 2. RenderStates (Transform)
    TransformMatrix tm = getCumulativeTransform();
    tm.combine(p.getTransform());
    sf::RenderStates states;
    states.transform = getSFMLTransform(tm);

    // 3. Tô màu (Fill)
    std::string fillStr = a.count("fill") ? a["fill"] : "black";
    sf::Color fillColor = stringToColor(fillStr, "fill");

    if (fillColor != sf::Color::Transparent)
    {
        fillColor.a = static_cast<std::uint8_t>(getOpacity(a, "fill-opacity") * 255);

        // TriangleFan sẽ nối điểm đầu với tất cả các cặp điểm tiếp theo -> Tạo ra hình sao đặc
        if (localPts.size() == 5)
        {
            sf::VertexArray fan(sf::PrimitiveType::TriangleFan, localPts.size());
            // Tính tâm trung bình để làm điểm neo cho Fan (giúp tô màu đều hơn)
            sf::Vector2f center(0, 0);
            for (auto &pt : localPts)
                center += pt;
            center /= 5.0f;

            // Thêm tâm vào đầu
            sf::VertexArray va(sf::PrimitiveType::TriangleFan);
            // [ĐÃ SỬA]: SFML 3.0 aggregate init
            va.append(sf::Vertex{center, fillColor});
            for (auto &pt : localPts)
            {
                va.append(sf::Vertex{pt, fillColor});
            }
            // Khép kín vòng
            va.append(sf::Vertex{localPts[0], fillColor});

            window.draw(va, states);
        }
        else
        {
            // Với các hình khác (như hình thoi), dùng Earcut như cũ
            // Cần transform điểm sang World Space cho hàm drawConcaveShape cũ
            std::vector<sf::Vector2f> worldPts = localPts;
            for (auto &pt : worldPts)
            {
                float tx, ty;
                tm.transformPoint(pt.x, pt.y, tx, ty);
                pt.x = tx;
                pt.y = ty;
            }
            std::vector<std::vector<sf::Vector2f>> worldPolygons = {worldPts};
            drawConcaveShape(window, worldPolygons, fillColor);
        }
    }

    // 4. Vẽ viền (Stroke)
    std::string strokeStr = a.count("stroke") ? a["stroke"] : "none";
    if (strokeStr != "none")
    {
        sf::Color strokeColor = stringToColor(strokeStr, "stroke");
        if (strokeColor != sf::Color::Transparent)
        {
            strokeColor.a = static_cast<std::uint8_t>(getOpacity(a, "stroke-opacity") * 255);
            float strokeWidth = 1.0f;
            if (a.count("stroke-width"))
                try
                {
                    strokeWidth = std::stof(a["stroke-width"]);
                }
                catch (...)
                {
                }

            if (strokeWidth > 0)
            {
                // Vẽ viền bằng World Space
                std::vector<sf::Vector2f> worldPts = localPts;
                for (auto &pt : worldPts)
                {
                    float tx, ty;
                    tm.transformPoint(pt.x, pt.y, tx, ty);
                    pt.x = tx;
                    pt.y = ty;
                }
                drawSharpStroke(window, worldPts, strokeWidth, strokeColor, true);
            }
        }
    }
}

// vẽ đa tuyến
void SVGRenderer::renderPolyline(const Polyline &p)
{
    // Logic tương tự Polygon nhưng Fill mặc định là None và Stroke mặc định hở
    std::string pointsStr = p.getPoints();
    std::replace(pointsStr.begin(), pointsStr.end(), ',', ' ');

    std::stringstream iss(pointsStr);
    std::vector<sf::Vector2f> rawPts;
    float x, y;
    while (iss >> x >> y)
    {
        rawPts.emplace_back(x, y);
    }

    if (rawPts.size() < 2)
        return;

    TransformMatrix tm = getCumulativeTransform();
    tm.combine(p.getTransform());

    for (auto &pt : rawPts)
    {
        float tx, ty;
        tm.transformPoint(pt.x, pt.y, tx, ty);
        pt.x = tx;
        pt.y = ty;
    }

    // Polyline thường không cần cleanPoints gắt gao như Polygon nhưng làm cho an toàn
    std::vector<sf::Vector2f> finalPts = cleanPolygonPoints(rawPts);
    if (finalPts.size() < 2)
        return;

    Attributes a = getEffectiveAttributes(p.getAttributes());

    // Polyline fill: mặc định là none, nhưng nếu có fill thì vẫn vẽ
    std::string fillStr = a.count("fill") ? a["fill"] : "none";
    sf::Color fillColor = stringToColor(fillStr, "fill");

    if (fillColor != sf::Color::Transparent)
    {
        fillColor.a = static_cast<std::uint8_t>(getOpacity(a, "fill-opacity") * 255);
        if (finalPts.size() >= 3)
        {
            std::vector<std::vector<sf::Vector2f>> polygons = {finalPts};
            drawConcaveShape(window, polygons, fillColor);
        }
    }

    std::string strokeStr = a.count("stroke") ? a["stroke"] : "none";
    if (strokeStr != "none")
    {
        sf::Color strokeColor = stringToColor(strokeStr, "stroke");
        if (strokeColor != sf::Color::Transparent)
        {
            strokeColor.a = static_cast<std::uint8_t>(getOpacity(a, "stroke-opacity") * 255);

            float strokeWidth = 1.0f;
            if (a.count("stroke-width"))
            {
                try
                {
                    strokeWidth = std::stof(a["stroke-width"]);
                }
                catch (...)
                {
                }
            }

            if (strokeWidth > 0)
            {
                drawSharpStroke(window, finalPts, strokeWidth, strokeColor, false); // false = open loop
            }
        }
    }
}

// vẽ elip
void SVGRenderer::renderEllipse(const Ellipse &e)
{
    Attributes attrs = getEffectiveAttributes(e.getAttributes());

    float rx = static_cast<float>(e.getRx());
    float ry = static_cast<float>(e.getRy());

    sf::CircleShape s(1.0f);
    s.setPointCount(100);

    // SFML 3.0: Dùng ngoặc nhọn {} cho setOrigin và setScale
    s.setOrigin({1.0f, 1.0f});
    s.setScale({rx, ry});

    s.setPosition({static_cast<float>(e.getCx()), static_cast<float>(e.getCy())});

    // [SỬA LỖI QUAN TRỌNG]
    // Nếu không có thuộc tính fill, mặc định là "black" (theo chuẩn SVG) thay vì "none"
    std::string fillStr = attrs.count("fill") ? attrs["fill"] : "black";

    sf::Color f = stringToColor(fillStr, "fill");
    if (f != sf::Color::Transparent)
    {
        f.a = static_cast<std::uint8_t>(getOpacity(attrs, "fill-opacity") * 255);
        s.setFillColor(f);
    }
    else
    {
        s.setFillColor(sf::Color::Transparent);
    }

    // Xử lý viền (Stroke) - Mặc định là none
    std::string strokeStr = attrs.count("stroke") ? attrs["stroke"] : "none";
    sf::Color st = stringToColor(strokeStr, "stroke");
    if (st != sf::Color::Transparent)
    {
        st.a = static_cast<std::uint8_t>(getOpacity(attrs, "stroke-opacity") * 255);
        s.setOutlineColor(st);

        float w = 1.0f;
        if (attrs.count("stroke-width"))
            try
            {
                w = std::stof(attrs["stroke-width"]);
            }
            catch (...)
            {
            }

        float avgScale = (rx + ry) / 2.0f;
        if (avgScale > 0)
            s.setOutlineThickness(w / avgScale);
    }

    TransformMatrix tm = getCumulativeTransform();
    tm.combine(e.getTransform());

    sf::RenderStates states;
    states.transform = getSFMLTransform(tm);

    window.draw(s, states);
}

// [Thay thế hàm renderPath trong SVGRenderer.cpp]

// Helper tính trọng tâm đa giác (để check lỗ thủng chính xác hơn)
sf::Vector2f getCentroid(const std::vector<sf::Vector2f> &points)
{
    if (points.empty())
        return {0, 0};
    sf::Vector2f sum(0, 0);
    for (const auto &p : points)
        sum += p;
    return sum / (float)points.size();
}

// Helper tính Bounding Box của một đa giác đơn
sf::FloatRect getPolygonBounds(const std::vector<sf::Vector2f> &points)
{
    if (points.empty())
        return sf::FloatRect();
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (const auto &p : points)
    {
        if (p.x < minX)
            minX = p.x;
        if (p.y < minY)
            minY = p.y;
        if (p.x > maxX)
            maxX = p.x;
        if (p.y > maxY)
            maxY = p.y;
    }
    return sf::FloatRect({minX, minY}, {maxX - minX, maxY - minY});
}

// Hàm đệ quy chia nhỏ tam giác để Gradient mịn hơn
void subdivideAndAppend(
    const sf::Vector2f &p1, const sf::Vector2f &p2, const sf::Vector2f &p3,
    const Gradient *grad, const sf::FloatRect &bbox, float opacity, sf::Color solidColor,
    std::vector<sf::Vertex> &outVertices, int depth)
{
    // [OPTIMIZATION]
    // 1. Kiểm tra kích thước tam giác. Nếu cạnh < 4 pixel thì dừng chia nhỏ (mắt thường không thấy được)
    // Giả sử bbox pixel coordinate.
    float edgeSq1 = (p1.x - p2.x) * (p1.x - p2.x) + (p1.y - p2.y) * (p1.y - p2.y);
    float edgeSq2 = (p2.x - p3.x) * (p2.x - p3.x) + (p2.y - p3.y) * (p2.y - p3.y);

    // Nếu tam giác quá nhỏ (dưới 16 đơn vị diện tích) HOẶC hết depth -> Dừng
    if (depth <= 0 || (edgeSq1 < 16.0f && edgeSq2 < 16.0f))
    {
        sf::Color c1, c2, c3;
        if (grad)
        {
            c1 = (grad->type == "linear") ? grad->getLinearColorAt(p1.x, p1.y, bbox) : grad->getRadialColorAt(p1.x, p1.y, bbox);
            c2 = (grad->type == "linear") ? grad->getLinearColorAt(p2.x, p2.y, bbox) : grad->getRadialColorAt(p2.x, p2.y, bbox);
            c3 = (grad->type == "linear") ? grad->getLinearColorAt(p3.x, p3.y, bbox) : grad->getRadialColorAt(p3.x, p3.y, bbox);
        }
        else
        {
            c1 = c2 = c3 = solidColor;
        }
        c1.a = static_cast<std::uint8_t>(c1.a * opacity);
        c2.a = static_cast<std::uint8_t>(c2.a * opacity);
        c3.a = static_cast<std::uint8_t>(c3.a * opacity);

        outVertices.push_back(sf::Vertex{p1, c1});
        outVertices.push_back(sf::Vertex{p2, c2});
        outVertices.push_back(sf::Vertex{p3, c3});
        return;
    }

    // Chia thành 4 tam giác con
    sf::Vector2f m12 = (p1 + p2) * 0.5f;
    sf::Vector2f m23 = (p2 + p3) * 0.5f;
    sf::Vector2f m31 = (p3 + p1) * 0.5f;

    subdivideAndAppend(p1, m12, m31, grad, bbox, opacity, solidColor, outVertices, depth - 1);
    subdivideAndAppend(m12, p2, m23, grad, bbox, opacity, solidColor, outVertices, depth - 1);
    subdivideAndAppend(m31, m23, p3, grad, bbox, opacity, solidColor, outVertices, depth - 1);
    subdivideAndAppend(m12, m23, m31, grad, bbox, opacity, solidColor, outVertices, depth - 1);
}
void SVGRenderer::renderPath(const Path &path)
{
    const auto &commands = path.getCommands();
    if (commands.empty())
        return;

    Attributes a = getEffectiveAttributes(path.getAttributes());

    // Transform
    TransformMatrix tm = getCumulativeTransform();
    tm.combine(path.getTransform());

    float scaleX = std::sqrt(tm.m[0] * tm.m[0] + tm.m[1] * tm.m[1]);
    float scaleY = std::sqrt(tm.m[3] * tm.m[3] + tm.m[4] * tm.m[4]);
    float avgScale = (scaleX + scaleY) / 2.0f;

    // --- PARSING PATH DATA ---
    std::vector<std::vector<sf::Vector2f>> rawSubPaths; // Toạ độ User Space
    std::vector<sf::Vector2f> curPts;
    sf::Vector2f curPos(0, 0), startPos(0, 0);
    sf::Vector2f lastControlPoint(0, 0);
    bool hadControlPoint = false;

    auto addBezier = [&](sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3)
    {
        float len = getLength(p1 - p0) + getLength(p2 - p1) + getLength(p3 - p2);
        int steps = static_cast<int>(len * 1.5f);
        if (steps < 30)
            steps = 30;
        if (steps > 200)
            steps = 200;
        for (int i = 1; i <= steps; ++i)
        {
            float t = i / (float)steps;
            curPts.push_back(cubicBezier(t, p0, p1, p2, p3));
        }
    };

    for (const auto &cmd : commands)
    {
        char t = cmd.type;
        const auto &args = cmd.args;

        size_t requiredArgs = 0;
        char upperT = std::toupper(t);
        if (upperT == 'M' || upperT == 'L' || upperT == 'T')
            requiredArgs = 2;
        else if (upperT == 'H' || upperT == 'V')
            requiredArgs = 1;
        else if (upperT == 'C')
            requiredArgs = 6;
        else if (upperT == 'S' || upperT == 'Q')
            requiredArgs = 4;
        else if (upperT == 'A')
            requiredArgs = 7;

        if (args.size() < requiredArgs)
        {
            // Nếu lệnh bị lỗi thiếu tham số, bỏ qua để không crash chương trình
            continue;
        }

        if ((t == 'M' || t == 'm') && !curPts.empty())
        {
            rawSubPaths.push_back(curPts);
            curPts.clear();
        }

        if (t == 'M')
        {
            curPos = {args[0], args[1]};
            startPos = curPos;
            curPts.push_back(curPos);
            hadControlPoint = false;
        }
        else if (t == 'm')
        {
            curPos += {args[0], args[1]};
            startPos = curPos;
            curPts.push_back(curPos);
            hadControlPoint = false;
        }
        else if (t == 'L')
        {
            curPos = {args[0], args[1]};
            curPts.push_back(curPos);
            hadControlPoint = false;
        }
        else if (t == 'l')
        {
            curPos += {args[0], args[1]};
            curPts.push_back(curPos);
            hadControlPoint = false;
        }
        else if (t == 'H')
        {
            curPos.x = args[0];
            curPts.push_back(curPos);
            hadControlPoint = false;
        }
        else if (t == 'h')
        {
            curPos.x += args[0];
            curPts.push_back(curPos);
            hadControlPoint = false;
        }
        else if (t == 'V')
        {
            curPos.y = args[0];
            curPts.push_back(curPos);
            hadControlPoint = false;
        }
        else if (t == 'v')
        {
            curPos.y += args[0];
            curPts.push_back(curPos);
            hadControlPoint = false;
        }
        else if (t == 'C')
        {
            sf::Vector2f p1(args[0], args[1]), p2(args[2], args[3]), p3(args[4], args[5]);
            addBezier(curPos, p1, p2, p3);
            lastControlPoint = p2;
            hadControlPoint = true;
            curPos = p3;
        }
        else if (t == 'c')
        {
            sf::Vector2f p1 = curPos + sf::Vector2f(args[0], args[1]);
            sf::Vector2f p2 = curPos + sf::Vector2f(args[2], args[3]);
            sf::Vector2f p3 = curPos + sf::Vector2f(args[4], args[5]);
            addBezier(curPos, p1, p2, p3);
            lastControlPoint = p2;
            hadControlPoint = true;
            curPos = p3;
        }
        else if (t == 'S' || t == 's')
        {
            sf::Vector2f p1 = hadControlPoint ? curPos + (curPos - lastControlPoint) : curPos;
            sf::Vector2f p2 = (t == 'S') ? sf::Vector2f(args[0], args[1]) : curPos + sf::Vector2f(args[0], args[1]);
            sf::Vector2f p3 = (t == 'S') ? sf::Vector2f(args[2], args[3]) : curPos + sf::Vector2f(args[2], args[3]);
            addBezier(curPos, p1, p2, p3);
            lastControlPoint = p2;
            hadControlPoint = true;
            curPos = p3;
        }
        else if (t == 'Q' || t == 'q')
        {
            sf::Vector2f P1 = (t == 'Q') ? sf::Vector2f(args[0], args[1]) : curPos + sf::Vector2f(args[0], args[1]);
            sf::Vector2f P2 = (t == 'Q') ? sf::Vector2f(args[2], args[3]) : curPos + sf::Vector2f(args[2], args[3]);
            sf::Vector2f C1 = curPos + (2.0f / 3.0f) * (P1 - curPos);
            sf::Vector2f C2 = P2 + (2.0f / 3.0f) * (P1 - P2);
            addBezier(curPos, C1, C2, P2);

            // [THÊM DÒNG NÀY ĐỂ T HOẠT ĐỘNG]
            lastControlPoint = P1;
            hadControlPoint = true;
            curPos = P2;
        }

        // --- [THÊM MỚI HOÀN TOÀN] Lệnh T / t bị thiếu ---
        else if (t == 'T' || t == 't')
        {
            sf::Vector2f P1;
            if (hadControlPoint)
            {
                // Phản chiếu
                P1 = curPos + (curPos - lastControlPoint);
            }
            else
            {
                P1 = curPos;
            }

            sf::Vector2f P2 = (t == 'T') ? sf::Vector2f(args[0], args[1]) : curPos + sf::Vector2f(args[0], args[1]);

            // Quadratic -> Cubic
            sf::Vector2f C1 = curPos + (2.0f / 3.0f) * (P1 - curPos);
            sf::Vector2f C2 = P2 + (2.0f / 3.0f) * (P1 - P2);

            addBezier(curPos, C1, C2, P2);

            lastControlPoint = P1;
            hadControlPoint = true;
            curPos = P2;
        }
        else if (t == 'A' || t == 'a')
        {
            float rx = args[0], ry = args[1], rot = args[2];
            bool large = (args[3] > 0.5f), sweep = (args[4] > 0.5f);
            sf::Vector2f endP = (t == 'A') ? sf::Vector2f(args[5], args[6]) : curPos + sf::Vector2f(args[5], args[6]);
            addArc(curPts, curPos, rx, ry, rot, large, sweep, endP.x, endP.y);
            curPos = endP;
            hadControlPoint = false;
        }
        else if (t == 'Z' || t == 'z')
        {
            if (!curPts.empty())
            {
                curPts.push_back(startPos);
                curPos = startPos;
            }
            hadControlPoint = false;
        }
    }
    if (!curPts.empty())
        rawSubPaths.push_back(curPts);

    // --- PROCESSING SHAPES (OUTER vs HOLES) ---
    // Giữ nguyên User Space cho Fill
    std::vector<std::vector<sf::Vector2f>> fillPaths;

    // Transform ra World Space cho Stroke (để vẽ nét không bị méo)
    struct StrokePart
    {
        std::vector<sf::Vector2f> points;
        bool isClosed;
    };
    std::vector<StrokePart> strokePaths;

    for (const auto &pathPts : rawSubPaths)
    {
        std::vector<sf::Vector2f> cleaned = cleanPolygonPoints(pathPts);
        if (cleaned.size() >= 3)
            fillPaths.push_back(cleaned);

        if (cleaned.size() >= 2)
        {
            std::vector<sf::Vector2f> worldPts = pathPts;
            for (auto &p : worldPts)
            {
                float tx, ty;
                tm.transformPoint(p.x, p.y, tx, ty);
                p.x = tx;
                p.y = ty;
            }
            bool closed = (getLength(pathPts.front() - pathPts.back()) < 0.1f);
            strokePaths.push_back({worldPts, closed});
        }
    }

    struct ShapeGroup
    {
        std::vector<sf::Vector2f> outer;
        std::vector<std::vector<sf::Vector2f>> holes;
        sf::FloatRect outerBounds;
    };
    std::vector<ShapeGroup> shapeGroups;

    if (!fillPaths.empty())
    {
        // [CẢI TIẾN] Logic phát hiện Hole
        // 1. Sắp xếp theo diện tích giảm dần
        std::sort(fillPaths.begin(), fillPaths.end(), [](const auto &a, const auto &b)
                  { return getArea(a) > getArea(b); });

        for (const auto &poly : fillPaths)
        {
            if (poly.size() < 3)
                continue;

            sf::FloatRect polyBounds = getPolygonBounds(poly);
            sf::Vector2f centroid = getCentroid(poly);
            bool addedToGroup = false;

            for (auto &g : shapeGroups)
            {
                // Check 1: Lỗ phải nằm gọn trong Bounding Box của Outer (Điều kiện cần)
                if (g.outerBounds.position.x <= polyBounds.position.x &&
                    g.outerBounds.position.y <= polyBounds.position.y &&
                    (g.outerBounds.position.x + g.outerBounds.size.x) >= (polyBounds.position.x + polyBounds.size.x) &&
                    (g.outerBounds.position.y + g.outerBounds.size.y) >= (polyBounds.position.y + polyBounds.size.y))
                {
                    // Check 2: Điểm trọng tâm phải nằm trong Outer (Điều kiện đủ)
                    // Nếu đã là lỗ của lỗ (đảo), logic này có thể cần recursive, nhưng với Firefox thì chỉ cần 1 cấp.
                    bool insideExistingHole = false;
                    for (const auto &h : g.holes)
                    {
                        // Nếu nằm trong một cái lỗ đã có -> Nó là đảo (Island) -> Coi là Outer mới
                        if (isPointInPolygon(centroid, h))
                        {
                            insideExistingHole = true;
                            break;
                        }
                    }

                    if (!insideExistingHole && isPointInPolygon(centroid, g.outer))
                    {
                        g.holes.push_back(poly);
                        addedToGroup = true;
                        break;
                    }
                }
            }

            if (!addedToGroup)
            {
                shapeGroups.push_back({poly, {}, polyBounds});
            }
        }
    }

    // Earcut Helper
    auto getTriangulatedVerts = [](const std::vector<std::vector<sf::Vector2f>> &floatPoly, const Gradient *grad, const sf::FloatRect &bbox, float opacity, sf::Color solidColor)
    {
        std::vector<sf::Vertex> vertices;
        if (floatPoly.empty())
            return vertices;

        const double SCALE = 100000.0;
        using IntPoint = std::array<int64_t, 2>;
        std::vector<std::vector<IntPoint>> intPolygon;
        std::vector<sf::Vector2f> flatFloatPoints;

        for (const auto &ring : floatPoly)
        {
            std::vector<IntPoint> intRing;
            for (const auto &p : ring)
            {
                if (std::abs(p.x) > 1e7 || std::abs(p.y) > 1e7)
                    continue;
                intRing.push_back({static_cast<int64_t>(p.x * SCALE), static_cast<int64_t>(p.y * SCALE)});
            }
            // Earcut yêu cầu đa giác không được rỗng
            if (intRing.size() >= 3)
            {
                intPolygon.push_back(intRing);
                flatFloatPoints.insert(flatFloatPoints.end(), ring.begin(), ring.end());
            }
        }

        std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(intPolygon);

        for (uint32_t idx : indices)
        {
            if (idx < flatFloatPoints.size())
            {
                sf::Vector2f pt = flatFloatPoints[idx];
                sf::Color finalColor;
                if (grad)
                {
                    finalColor = (grad->type == "linear") ? grad->getLinearColorAt(pt.x, pt.y, bbox) : grad->getRadialColorAt(pt.x, pt.y, bbox);
                }
                else
                {
                    finalColor = solidColor;
                }
                finalColor.a = static_cast<std::uint8_t>(finalColor.a * opacity);
                vertices.push_back(sf::Vertex{pt, finalColor});
            }
        }
        // [NEW LOGIC] Thay vì push thẳng vertex, ta gọi hàm chia nhỏ
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            uint32_t idx1 = indices[i];
            uint32_t idx2 = indices[i + 1];
            uint32_t idx3 = indices[i + 2];

            if (idx1 < flatFloatPoints.size() && idx2 < flatFloatPoints.size() && idx3 < flatFloatPoints.size())
            {
                sf::Vector2f p1 = flatFloatPoints[idx1];
                sf::Vector2f p2 = flatFloatPoints[idx2];
                sf::Vector2f p3 = flatFloatPoints[idx3];

                // Nếu có Gradient, chia nhỏ tam giác (Depth = 2 hoặc 3)
                // Nếu màu Solid, không cần chia nhỏ (Depth = 0) để tối ưu
                // int depth = (grad != nullptr) ? 3 : 0;
                int depth = (grad != nullptr) ? 5 : 0;
                subdivideAndAppend(p1, p2, p3, grad, bbox, opacity, solidColor, vertices, depth);
            }
        }
        return vertices;
    };

    // --- DRAW FILL ---
    // --- DRAW FILL (CODE MỚI HOÀN TOÀN) ---
    std::string fillStr = a.count("fill") ? a["fill"] : "black";
    float fillOpacity = getOpacity(a, "fill-opacity");

    // 1. Kiểm tra xem có phải Gradient không
    bool isGradient = false;
    Gradient *activeGrad = nullptr;
    if (fillStr.find("url(#") != std::string::npos)
    {
        size_t start = fillStr.find("#") + 1;
        size_t end = fillStr.find(")");
        std::string gradId = fillStr.substr(start, end - start);
        if (gradients_.count(gradId))
        {
            isGradient = true;
            activeGrad = &gradients_.at(gradId);
        }
    }

    // 2. Xử lý màu Solid (nếu không phải Gradient)
    sf::Color solidColor = sf::Color::Transparent;
    if (!isGradient)
    {
        solidColor = stringToColor(fillStr, "fill");
    }

    // 3. Tiến hành vẽ nếu có màu
    if (isGradient || solidColor != sf::Color::Transparent)
    {
        // A. Tính Bounding Box tổng cho toàn bộ Path (để Gradient phủ đều)
        sf::FloatRect bbox;
        float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
        bool hasPoints = false;

        for (const auto &g : shapeGroups)
        {
            if (g.outerBounds.position.x < minX)
                minX = g.outerBounds.position.x;
            if (g.outerBounds.position.y < minY)
                minY = g.outerBounds.position.y;
            if (g.outerBounds.position.x + g.outerBounds.size.x > maxX)
                maxX = g.outerBounds.position.x + g.outerBounds.size.x;
            if (g.outerBounds.position.y + g.outerBounds.size.y > maxY)
                maxY = g.outerBounds.position.y + g.outerBounds.size.y;
            hasPoints = true;
        }
        if (hasPoints)
        {
            bbox = sf::FloatRect({minX, minY}, {maxX - minX, maxY - minY});
        }

        // B. Chuẩn bị Texture (nếu là Gradient)
        sf::Texture gradTex;
        if (isGradient && activeGrad)
        {
            gradTex = generateGradientTexture(*activeGrad, bbox);
        }

        sf::RenderStates states;
        states.transform = getSFMLTransform(tm);

        // Nếu là Gradient thì gắn Texture vào, nếu không thì để NULL
        if (isGradient)
        {
            states.texture = &gradTex;
        }

        // C. Duyệt qua từng nhóm hình (Outer + Holes) để cắt tam giác
        for (const auto &g : shapeGroups)
        {
            // Gom outer và holes vào một danh sách để Earcut xử lý
            std::vector<std::vector<sf::Vector2f>> polyToTriangulate;
            polyToTriangulate.push_back(g.outer);
            for (const auto &h : g.holes)
                polyToTriangulate.push_back(h);

            // --- BẮT ĐẦU LOGIC EARCUT ---
            const double SCALE = 100000.0;
            using IntPoint = std::array<int64_t, 2>;
            std::vector<std::vector<IntPoint>> intPolygon;
            std::vector<sf::Vector2f> flatPoints;

            for (const auto &ring : polyToTriangulate)
            {
                std::vector<IntPoint> intRing;
                for (const auto &p : ring)
                {
                    // Chống tràn số khi convert sang int
                    if (std::abs(p.x) > 1e7 || std::abs(p.y) > 1e7)
                        continue;
                    intRing.push_back({static_cast<int64_t>(p.x * SCALE), static_cast<int64_t>(p.y * SCALE)});
                }
                if (intRing.size() >= 3)
                {
                    intPolygon.push_back(intRing);
                    // Lưu lại điểm thực để lát nữa lấy ra vẽ
                    flatPoints.insert(flatPoints.end(), ring.begin(), ring.end());
                }
            }

            if (intPolygon.empty())
                continue;

            // Gọi thư viện earcut để lấy danh sách chỉ số tam giác
            std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(intPolygon);
            // --- KẾT THÚC LOGIC EARCUT ---

            // D. Tạo VertexArray để vẽ
            sf::VertexArray va(sf::PrimitiveType::Triangles);

            for (uint32_t idx : indices)
            {
                if (idx < flatPoints.size())
                {
                    sf::Vector2f pt = flatPoints[idx];
                    sf::Color finalColor;
                    sf::Vector2f uv(0.f, 0.f);

                    if (isGradient)
                    {
                        // KỸ THUẬT MỚI: TEXTURE MAPPING
                        // Tính tọa độ UV dựa trên vị trí điểm so với Bounding Box
                        // Công thức: (Vị trí hiện tại - Gốc) / Kích thước * Kích thước Texture
                        float u = ((pt.x - bbox.position.x) / bbox.size.x) * gradTex.getSize().x;
                        float v = ((pt.y - bbox.position.y) / bbox.size.y) * gradTex.getSize().y;

                        uv = sf::Vector2f(u, v);
                        finalColor = sf::Color::White; // Màu trắng để hiển thị đúng màu gốc của Texture
                    }
                    else
                    {
                        // KỸ THUẬT CŨ: SOLID COLOR
                        finalColor = solidColor;
                    }

                    // Áp dụng độ trong suốt (Opacity)
                    finalColor.a = static_cast<std::uint8_t>(fillOpacity * 255);

                    // Thêm đỉnh vào mảng vẽ (bao gồm cả tọa độ UV nếu có)
                    // [SFML 3.0 Init]
                    va.append(sf::Vertex{pt, finalColor, uv});
                }
            }

            if (va.getVertexCount() > 0)
            {
                window.draw(va, states);
            }
        }
    }

    // --- DRAW STROKE ---
    std::string strokeStr = a.count("stroke") ? a["stroke"] : "none";
    if (strokeStr != "none")
    {
        sf::Color strokeColor = stringToColor(strokeStr, "stroke");
        if (strokeColor != sf::Color::Transparent)
        {
            strokeColor.a = static_cast<std::uint8_t>(getOpacity(a, "stroke-opacity") * 255);
            float w = 1.0f;
            if (a.count("stroke-width"))
                try
                {
                    w = std::stof(a["stroke-width"]);
                }
                catch (...)
                {
                }
            float finalStrokeWidth = w * avgScale;
            if (finalStrokeWidth > 0)
            {
                for (const auto &part : strokePaths)
                    drawSharpStroke(window, part.points, finalStrokeWidth, strokeColor, part.isClosed);
            }
        }
    }
}
// Render Linear Gradient cho Rect

void SVGRenderer::renderLinearGradientRect(const Rect &r, const Gradient &grad,
                                           const TransformMatrix &tm, float opacity)
{
    float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    sf::FloatRect bbox({x, y}, {w, h});

    // Chia rect thành grid nhỏ (trade-off giữa chất lượng và performance)
    const int GRID_SIZE = 50; // Có thể điều chỉnh: 20 = nhanh, 100 = đẹp

    float stepX = w / GRID_SIZE;
    float stepY = h / GRID_SIZE;

    sf::VertexArray vertices(sf::PrimitiveType::Triangles);

    for (int iy = 0; iy < GRID_SIZE; iy++)
    {
        for (int ix = 0; ix < GRID_SIZE; ix++)
        {
            float px = x + ix * stepX;
            float py = y + iy * stepY;
            float px2 = px + stepX;
            float py2 = py + stepY;

            // Tính màu tại 4 góc của cell
            sf::Color c1 = grad.getLinearColorAt(px, py, bbox);
            sf::Color c2 = grad.getLinearColorAt(px2, py, bbox);
            sf::Color c3 = grad.getLinearColorAt(px2, py2, bbox);
            sf::Color c4 = grad.getLinearColorAt(px, py2, bbox);

            // Apply opacity
            c1.a = static_cast<std::uint8_t>(c1.a * opacity);
            c2.a = static_cast<std::uint8_t>(c2.a * opacity);
            c3.a = static_cast<std::uint8_t>(c3.a * opacity);
            c4.a = static_cast<std::uint8_t>(c4.a * opacity);

            // Transform tọa độ
            float tx1, ty1, tx2, ty2, tx3, ty3, tx4, ty4;
            tm.transformPoint(px, py, tx1, ty1);
            tm.transformPoint(px2, py, tx2, ty2);
            tm.transformPoint(px2, py2, tx3, ty3);
            tm.transformPoint(px, py2, tx4, ty4);

            // Tạo 2 tam giác cho mỗi cell
            // Triangle 1: top-left, top-right, bottom-right
            // [ĐÃ SỬA]: SFML 3.0 aggregate init
            vertices.append(sf::Vertex{sf::Vector2f(tx1, ty1), c1});
            vertices.append(sf::Vertex{sf::Vector2f(tx2, ty2), c2});
            vertices.append(sf::Vertex{sf::Vector2f(tx3, ty3), c3});

            // Triangle 2: top-left, bottom-right, bottom-left
            vertices.append(sf::Vertex{sf::Vector2f(tx1, ty1), c1});
            vertices.append(sf::Vertex{sf::Vector2f(tx3, ty3), c3});
            vertices.append(sf::Vertex{sf::Vector2f(tx4, ty4), c4});
        }
    }

    window.draw(vertices);
}

// Render Radial Gradient cho Rect

void SVGRenderer::renderRadialGradientRect(const Rect &r, const Gradient &grad,
                                           const TransformMatrix &tm, float opacity)
{
    float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    sf::FloatRect bbox({x, y}, {w, h});

    // Tương tự linear nhưng dùng getRadialColorAt()
    const int GRID_SIZE = 50;

    float stepX = w / GRID_SIZE;
    float stepY = h / GRID_SIZE;

    sf::VertexArray vertices(sf::PrimitiveType::Triangles);

    for (int iy = 0; iy < GRID_SIZE; iy++)
    {
        for (int ix = 0; ix < GRID_SIZE; ix++)
        {
            float px = x + ix * stepX;
            float py = y + iy * stepY;
            float px2 = px + stepX;
            float py2 = py + stepY;

            // [QUANG] Dùng getRadialColorAt thay vì getLinearColorAt
            sf::Color c1 = grad.getRadialColorAt(px, py, bbox);
            sf::Color c2 = grad.getRadialColorAt(px2, py, bbox);
            sf::Color c3 = grad.getRadialColorAt(px2, py2, bbox);
            sf::Color c4 = grad.getRadialColorAt(px, py2, bbox);

            c1.a = static_cast<std::uint8_t>(c1.a * opacity);
            c2.a = static_cast<std::uint8_t>(c2.a * opacity);
            c3.a = static_cast<std::uint8_t>(c3.a * opacity);
            c4.a = static_cast<std::uint8_t>(c4.a * opacity);

            float tx1, ty1, tx2, ty2, tx3, ty3, tx4, ty4;
            tm.transformPoint(px, py, tx1, ty1);
            tm.transformPoint(px2, py, tx2, ty2);
            tm.transformPoint(px2, py2, tx3, ty3);
            tm.transformPoint(px, py2, tx4, ty4);

            // [ĐÃ SỬA]: SFML 3.0 aggregate init
            vertices.append(sf::Vertex{sf::Vector2f(tx1, ty1), c1});
            vertices.append(sf::Vertex{sf::Vector2f(tx2, ty2), c2});
            vertices.append(sf::Vertex{sf::Vector2f(tx3, ty3), c3});

            vertices.append(sf::Vertex{sf::Vector2f(tx1, ty1), c1});
            vertices.append(sf::Vertex{sf::Vector2f(tx3, ty3), c3});
            vertices.append(sf::Vertex{sf::Vector2f(tx4, ty4), c4});
        }
    }

    window.draw(vertices);
}

// Context Functions
void SVGRenderer::beginElement(const TransformMatrix &t, const Attributes &a)
{
    if (renderStack_.empty())
    {
        renderStack_.push_back({t, a});
    }
    else
    {
        RenderState s = renderStack_.back();
        s.cumulativeTransform.combine(t);
        for (auto &k : a)
            s.inheritedAttributes[k.first] = k.second;
        renderStack_.push_back(s);
    }
}
void SVGRenderer::endElement()
{
    if (!renderStack_.empty())
        renderStack_.pop_back();
}

const TransformMatrix &SVGRenderer::getCumulativeTransform() const
{
    static TransformMatrix i;
    return renderStack_.empty() ? i : renderStack_.back().cumulativeTransform;
}