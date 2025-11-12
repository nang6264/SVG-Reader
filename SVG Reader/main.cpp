// main.cpp
#include "SVGParser.h"
#include "SVGRenderer.h"
#include "SVGElement.h"
#include <iostream>
#include <memory>
#include <utility> // Dùng cho std::move

int main() {
    std::string filename;
    std::cout << "Nhập tên file SVG cần đọc (vd: input.svg): ";
    std::cin >> filename;

    // 1️⃣ Tạo đối tượng parser
    SVGParser parser;

    // 2️⃣ Kiểm tra file hợp lệ
    if (!parser.isValidFile(filename)) {
        std::cerr << "❌ Lỗi: Không thể mở file " << filename << std::endl;
        return 1;
    }

    // 3️⃣ Phân tích file SVG
    if (!parser.parseFile(filename)) {
        std::cerr << "❌ Lỗi: Phân tích file SVG thất bại." << std::endl;
        return 1;
    }

    // 4️⃣ Tạo renderer SFML
    SVGRenderer renderer(800, 600);

    // 5️⃣ CHUYỂN GIAO QUYỀN SỞ HỮU: Lấy danh sách phần tử SVG đã parse
    // Sử dụng takeElements() để di chuyển (move) các unique_ptr ra khỏi parser.
    SVGParser::ElementList parsedElements = parser.takeElements();

    // 6️⃣ Chuyển quyền sở hữu từ unique_ptr sang shared_ptr và thêm vào renderer
    for (auto& unique_elem : parsedElements) {
        if (unique_elem) {
            // Chuyển unique_ptr sang shared_ptr. shared_ptr lúc này là chủ sở hữu
            // và đảm bảo việc giải phóng bộ nhớ chính xác.
            std::shared_ptr<SVGElement> shared_elem = std::move(unique_elem);
            renderer.addElement(shared_elem);
        }
    }

    // 7️⃣ Render hiển thị
    renderer.render();

    std::cout << "Kết thúc chương trình." << std::endl;
    return 0;
}
