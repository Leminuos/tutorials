## include

Direction `include` sẽ tạm dừng đọc makefile hiện tại và đọc các makefile khác trước khi tiếp tục.

```bash
include filenames…
```

Nếu muốn make bỏ qua một makefile không tồn tại hoặc không thể tạo, không có thông bảo lỗi thì có thể sử dụng câu lệnh sau. Lệnh này hoạt động giống như lệnh trên, ngoại trừ việc nó không có cảnh báo lỗi nếu không có tồn tại một makefile.

```
-include filenames
```

## vpath

Khác với biến `VPATH`, direction `vpath` được dùng để make tìm kiếm các file cụ thể khớp với pattern trong các folder thay vì áp dụng cho mọi file như `VPATH`.

Trong GNU Make, directive vpath có 3 hình thức sử dụng, tùy theo mục đích:

- Đặt thư mục tìm kiếm cho một pattern

  ```bash
  vpath <pattern> <directories>
  ```
  
  - `<pattern>`: kiểu tên file muốn áp dụng (ví dụ: %.c, config.h, main.c).
  - `<directories>`: danh sách thư mục để tìm file khớp với pattern, cách nhau bằng khoảng trắng.
  
  Ví dụ:
  
  ```bash
  vpath %.c $(SRC_DIRS)
  ```
  
  Yêu cầu make tìm kiếm các file có tên kết thúc bằng `.c` trong folder `SRC_DIRS` nếu file không được tìm thấy   trong folder hiện tại.

- Xoá bỏ thư mục tìm kiếm cho một pattern

  ```bash
  vpath <pattern>
  ```

  - Xoá mọi thiết lập trước đó (nếu có) về nơi tìm kiếm cho pattern này.
  - Pattern sẽ không được tra trong bất kỳ thư mục nào ngoài thư mục hiện hành.

- Xoá toàn bộ các đường dẫn đã được chỉ định bằng vpath trước đó

  ```bash
  vpath
  ```
  
  - Xoá toàn bộ cấu hình vpath đã được thiết lập trước đó.
  - Tất cả các pattern sẽ chỉ còn được tìm trong thư mục hiện tại.