#include "SVGParser.h"
#include "SVGRenderer.h"
#include "SVGElement.h"
#include <iostream>
#include <memory>
#include <utility>
#include <string>

void loadAndRender(const std::string& filename) {
    std::cout << "\n----------------------------------------\n";
    std::cout << "--> Dang xu ly file: " << filename << "\n";

    SVGParser parser;

    if (!parser.isValidFile(filename)) {
        std::cerr << "❌ LOI: Khong tim thay file '" << filename << "'\n";
        return;
    }

    if (!parser.parseFile(filename)) {
        std::cerr << "❌ LOI: File SVG bi loi cu phap, khong the doc.\n";
        return;
    }

    std::cout << "✅ Doc file thanh cong! Dang mo cua so do hoa...\n";

    SVGRenderer renderer(800, 600);

    auto header = parser.getHeader();
    if (header.hasViewBox) {
        renderer.setViewBox(header.viewBoxX, header.viewBoxY,
            header.viewBoxWidth, header.viewBoxHeight);
    }
    else {
        renderer.setViewBox(0, 0, header.width, header.height);
    }

    // [QUAN TRỌNG] Truyền gradients từ Parser sang Renderer
    renderer.setGradients(parser.getGradients());
    std::cout << "✅ Da tai " << parser.getGradients().size() << " gradients\n";

    // Chuyển elements
    SVGParser::ElementList parsedElements = parser.takeElements();
    for (auto& unique_elem : parsedElements) {
        if (unique_elem) {
            std::shared_ptr<SVGElement> shared_elem = std::move(unique_elem);
            renderer.addElement(shared_elem);
        }
    }

    renderer.render();
    std::cout << "--> Da dong cua so hien thi.\n";
}

int main() {
    std::setlocale(LC_NUMERIC, "C");
    std::string filename;

    while (true) {
        std::cout << "\n========================================\n";
        std::cout << "Nhap ten file SVG de ve (vd: sample.svg)\n";
        std::cout << "Nhap 'exit' de thoat chuong trinh.\n";
        std::cout << ">> Nhap ten file: ";

        std::getline(std::cin, filename);

        if (!filename.empty()) {
            size_t first = filename.find_first_not_of(' ');
            size_t last = filename.find_last_not_of(' ');
            if (first != std::string::npos && last != std::string::npos)
                filename = filename.substr(first, (last - first + 1));
        }

        if (filename == "exit" || filename == "EXIT") {
            std::cout << "Tam biet!\n";
            break;
        }

        if (filename.empty()) continue;

        loadAndRender(filename);
    }

    return 0;
}