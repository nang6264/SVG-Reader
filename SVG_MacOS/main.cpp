// main.cpp
#include "SVGParser.h"
#include "SVGRenderer.h"
#include "SVGElement.h"
#include <iostream>
#include <memory>
#include <utility>
#include <limits>
#include <sstream> // Dùng để format chuỗi
#include <iomanip> // Dùng để set chiều rộng số (01, 02...)

// Hàm tiện ích: Thực hiện toàn bộ quy trình đọc và vẽ 1 file SVG
void loadAndRender(const std::string& filename) {
    std::cout << "\n----------------------------------------\n";
    std::cout << "--> Dang xu ly file: " << filename << "\n";

    // 1️⃣ Tạo đối tượng parser
    SVGParser parser;

    // 2️⃣ Kiểm tra file hợp lệ
    if (!parser.isValidFile(filename))
    {
        std::cerr << "❌ LOI: Khong tim thay file '" << filename << "'\n";
        std::cerr << "   (Hay dam bao file nay nam cung thu muc voi file .exe)\n";
        return;
    }

    // 3️⃣ Phân tích file SVG
    if (!parser.parseFile(filename))
    {
        std::cerr << "❌ LOI: Parse file that bai.\n";
        return;
    }

    std::cout << "✅ Parse thanh cong! Dang mo cua so do hoa...\n";
    std::cout << "   (Nhan ESC hoac dong cua so de quay lai menu)\n";

    // 4️⃣ Tạo renderer SFML
    SVGRenderer renderer(800, 600);

    auto header = parser.getHeader();
    if (header.hasViewBox) {
        renderer.setViewBox(header.viewBoxX, header.viewBoxY, header.viewBoxWidth, header.viewBoxHeight);
    }
    else {
        renderer.setViewBox(0, 0, header.width, header.height);
    }

    // 5️⃣ & 6️⃣ Chuyển giao dữ liệu
    SVGParser::ElementList parsedElements = parser.takeElements();
    for (auto& unique_elem : parsedElements)
    {
        if (unique_elem)
        {
            std::shared_ptr<SVGElement> shared_elem = std::move(unique_elem);
            renderer.addElement(shared_elem);
        }
    }

    // 7️⃣ Render hiển thị (Chương trình sẽ dừng ở đây cho đến khi tắt cửa sổ)
    renderer.render();

    std::cout << "--> Da dong cua so.\n";
}

int main()
{
    int choice = 0;

    while (true) {
        // --- HIỂN THỊ MENU ---
        std::cout << "\n========================================\n";
        std::cout << "       HE THONG TEST SVG TU DONG        \n";
        std::cout << "========================================\n";
        std::cout << "Danh sach: svg-01.svg -> svg-18.svg\n";
        std::cout << "Nhap so thu tu file (1-18) de chay.\n";
        std::cout << "Nhap 0 de thoat.\n";
        std::cout << "========================================\n";
        std::cout << "Lua chon cua ban: ";

        if (!(std::cin >> choice)) {
            // Xử lý khi nhập sai (không phải số)
            std::cout << "❌ Vui long chi nhap so nguyen!\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }

        // Xóa bộ đệm
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice == 0) {
            std::cout << "Tam biet!\n";
            break;
        }

        if (choice >= 1 && choice <= 18) {
            // Tự động tạo tên file từ số nhập vào
            // Sử dụng stringstream để thêm số 0 đằng trước nếu nhỏ hơn 10
            // Ví dụ: nhập 1 -> "svg-01.svg", nhập 10 -> "svg-10.svg"
            std::stringstream ss;
            ss << "svg-" << std::setw(2) << std::setfill('0') << choice << ".svg";

            std::string filename = ss.str();
            loadAndRender(filename);
        }
        else {
            std::cout << "⚠️ So thu tu khong nam trong khoang 1-18!\n";
            // Tùy chọn: Nếu bạn muốn nhập tên file tùy ý khi gõ số lạ
            // thì có thể bỏ comment đoạn dưới đây:
            /*
            std::cout << "Ban co muon nhap ten file thu cong khong? (y/n): ";
            char c; std::cin >> c;
            if(c == 'y' || c == 'Y') {
                std::string manualName;
                std::cout << "Nhap ten file: ";
                std::cin >> manualName;
                loadAndRender(manualName);
            }
            */
        }
    }

    return 0;
}