# SVG-Reader

Overview:
Mục tiêu của đồ án này là thiết kế và triển khai một ứng dụng đọc và hiển thị file SVG sử dụng ngôn ngữ lập trình C++ và các nguyên tắc Lập trình Hướng đối tượng (OOP). Ứng dụng phải có khả năng phân tích (parse) file SVG, kết xuất (render) đồ họa vector.

Functional requirements: 
1. SVG Reading and Parsing
   - Có  khả năng mở file SVG và phân tích nội dung.
   - Việc phân tích phải xử lý các phần tử SVG khác nhau, các thuộc tính tương ứng và cấu trúc phân cấp (nested/hierarchical) của tài liệu SVG.
   - Cài đặt các lớp đại diện cho các phần tử SVG (ví dụ: Circle, Path, Text, v.v.) theo cách định nghĩa hướng đối tượng.
2. Rendering
   - Sử dụng dữ liệu đã phân tích để vẽ đồ họa vector lên màn hình.
   - Sử dụng thiết kế hướng đối tượng để biểu diễn các phần tử SVG khác nhau như các đối tượng (objects) và kết xuất chúng tương ứng.
3. Interactivity
   - Zoom in/out
   - rotate
     
Object-Oriented Design:
 - Xây dựng hệ thống lớp (class hierarchy) có tổ chức, mô tả các phần tử SVG khác nhau.
 - Vận dụng kế thừa (inheritance), đóng gói (encapsulation) và đa hình (polymorphism) để tạo thiết kế mạnh mẽ và dễ mở rộng.
 - Xem xét áp dụng một số mẫu thiết kế (design patterns) phù hợp cho việc tạo đối tượng và duyệt cây (ví dụ: Factory, Visitor, Composite — tuỳ lựa chọn).
   
Documentation:
- Cung cấp tài liệu chi tiết mô tả:
  + Hệ thống lớp, mối quan hệ giữa các lớp.
  + Các phương thức chính và chức năng của chúng.
- Bao gồm sơ đồ UML (class diagram, sequence diagram nếu cần) thể hiện quan hệ và tương tác trong module kết xuất.
- Chú thích (comment) mã nguồn đầy đủ để tăng tính dễ đọc và hỗ trợ phát triển sau này.


