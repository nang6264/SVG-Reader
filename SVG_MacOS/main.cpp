#include "SVGParser.h"
#include "SVGRenderer.h"
#include "SVGElement.h"
#include <iostream>
#include <memory>
#include <utility> // Dùng cho std::move
#include <string>

// Hàm thực hiện quy trình đọc và vẽ cho 1 file duy nhất
void loadAndRender(const std::string& filename) {
    std::cout << "\n----------------------------------------\n";
    std::cout << "--> Dang xu ly file: " << filename << "\n";

    // 1. Tạo Parser mới (đảm bảo sạch dữ liệu cũ)
    SVGParser parser;

    // 2. Kiểm tra file có tồn tại không
    if (!parser.isValidFile(filename)) {
        std::cerr << "❌ LOI: Khong tim thay file '" << filename << "'\n";
        std::cerr << "   (Hay dam bao file nam cung thu muc voi file .exe)\n";
        return;
    }

    // 3. Phân tích cú pháp
    if (!parser.parseFile(filename)) {
        std::cerr << "❌ LOI: File SVG bi loi cu phap, khong the doc.\n";
        return;
    }

    std::cout << "✅ Doc file thanh cong! Dang mo cua so do hoa...\n";

    // 4. Tạo Renderer và thiết lập kích thước
    SVGRenderer renderer(800, 600);

    auto header = parser.getHeader();
    if (header.hasViewBox) {
        renderer.setViewBox(header.viewBoxX, header.viewBoxY, header.viewBoxWidth, header.viewBoxHeight);
    }
    else {
        renderer.setViewBox(0, 0, header.width, header.height);
    }

    // 5. Chuyển dữ liệu từ Parser sang Renderer
    SVGParser::ElementList parsedElements = parser.takeElements();
    for (auto& unique_elem : parsedElements) {
        if (unique_elem) {
            std::shared_ptr<SVGElement> shared_elem = std::move(unique_elem);
            renderer.addElement(shared_elem);
        }
    }

    // 6. Vẽ và giữ cửa sổ (Chương trình sẽ dừng tại đây cho đến khi bạn tắt cửa sổ)
    renderer.render();

    std::cout << "--> Da dong cua so hien thi.\n";
}

int main() {
    std::string filename;

    while (true) {
        std::cout << "\n========================================\n";
        std::cout << "Nhap ten file SVG de ve (vd: sample.svg)\n";
        std::cout << "Nhap 'exit' de thoat chuong trinh.\n";
        std::cout << ">> Nhap ten file: ";

        // Dùng getline để đọc được cả tên file có dấu cách
        std::getline(std::cin, filename);

        // Xóa khoảng trắng thừa đầu đuôi (nếu có)
        if (!filename.empty()) {
            size_t first = filename.find_first_not_of(' ');
            size_t last = filename.find_last_not_of(' ');
            if (first != std::string::npos && last != std::string::npos)
                filename = filename.substr(first, (last - first + 1));
        }

        // Kiểm tra lệnh thoát
        if (filename == "exit" || filename == "EXIT") {
            std::cout << "Tam biet!\n";
            break;
        }

        if (filename.empty()) continue;

        // Gọi hàm xử lý
        loadAndRender(filename);
    }

    return 0;
}