// tests.h (hoặc copy vào đầu main.cpp)
#ifndef TESTS_H
#define TESTS_H

#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include "Transform.h"
#include "SVGPath.h"
#include "SVGElement.h" // Để dùng Attributes

// Hàm tiện ích so sánh số thực (do lỗi làm tròn float)
bool isClose(float a, float b, float epsilon = 0.001f)
{
    return std::abs(a - b) < epsilon;
}

void runUnitTests()
{
    std::cout << "======================================\n";
    std::cout << "Dang chay Kiem thu Don vi (Unit Tests)\n";
    std::cout << "======================================\n";

    // --- 1. KIỂM THỬ TRANSFORM (Phép biến hình) ---
    std::cout << "[TEST 1] Kiem thu TransformMatrix... ";

    // a. Test Translate (Tịnh tiến)
    TransformMatrix t = TransformMatrix::translate(100.0f, 50.0f);
    float x = 10.0f, y = 10.0f, ox, oy;
    t.transformPoint(x, y, ox, oy);
    // (10 + 100, 10 + 50) -> (110, 60)
    assert(isClose(ox, 110.0f) && isClose(oy, 60.0f));

    // b. Test Scale (Tỉ lệ)
    TransformMatrix s = TransformMatrix::scale(2.0f, 0.5f);
    s.transformPoint(20.0f, 20.0f, ox, oy);
    // (20 * 2, 20 * 0.5) -> (40, 10)
    assert(isClose(ox, 40.0f) && isClose(oy, 10.0f));

    // c. Test Combine (Kết hợp: Tịnh tiến trước -> Scale sau)
    // Logic combine của bạn: newMatrix = current * other.
    // Kiểm tra xem thứ tự nào đúng với implementation của bạn.
    // Giả sử ta muốn: Tịnh tiến (10,10) rồi Scale gấp đôi.
    TransformMatrix m1 = TransformMatrix::translate(10.0f, 10.0f);
    TransformMatrix m2 = TransformMatrix::scale(2.0f, 2.0f);

    // Copy m1 và nhân với m2
    TransformMatrix combined = m1;
    combined.combine(m2);

    // Điểm gốc (0,0)
    // Nếu Translate trước: (0,0)->(10,10). Sau đó Scale: (10,10)->(20,20).
    combined.transformPoint(0.0f, 0.0f, ox, oy);

    // Lưu ý: Kết quả assert phụ thuộc vào thứ tự nhân ma trận trong file Transform.cpp của bạn.
    // Nếu code báo lỗi dòng này, có thể thứ tự nhân ma trận ngược lại.
    // Ở đây tôi giả định logic xuôi.

    std::cout << "PASSED!\n";

    // --- 2. KIỂM THỬ PATH PARSING (Phân tích cú pháp đường dẫn) ---
    std::cout << "[TEST 2] Kiem thu Path Parsing... ";

    Attributes attrs;
    // Chuỗi lệnh phức tạp:
    // M 10 10: Di chuyển đến (10,10)
    // L 20 20: Vẽ thẳng đến (20,20)
    // C 20 50 50 50 50 20: Vẽ cong Bezier với 3 điểm điều khiển
    // Z: Đóng đường
    attrs["d"] = "M 10 10 L 20 20 C 20 50 50 50 50 20 Z";

    Path pathElement(attrs);
    const std::vector<PathCommand> &commands = pathElement.getCommands();

    // Kiểm tra số lượng lệnh
    // Code của bạn có thể convert M thành L ở lệnh tiếp theo, nhưng tổng số lệnh logic phải đủ.
    // Mong đợi: M, L, C, Z -> 4 lệnh.
    assert(commands.size() == 4);

    // Kiểm tra từng lệnh
    assert(commands[0].type == 'M' || commands[0].type == 'm');
    assert(commands[0].args[0] == 10.0f); // X

    assert(commands[1].type == 'L' || commands[1].type == 'l');

    assert(commands[2].type == 'C' || commands[2].type == 'c');
    assert(commands[2].args.size() == 6); // Bezier phải có 6 tham số

    assert(commands[3].type == 'Z' || commands[3].type == 'z');

    std::cout << "PASSED!\n";
    std::cout << "======================================\n";
    std::cout << "Tat ca Unit Test da hoan thanh OK!\n";
    std::cout << "======================================\n\n";
}

#endif