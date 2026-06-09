# Vim editor

## Trình soạn thảo vi

Lệnh vi trong Linux là một trình soạn thảo văn bản phổ biến được sử dụng trong môi trường Linux và Unix, cho phép bạn tạo, chỉnh sửa và điều hướng nội dung văn bản một cách hiệu quả.

Cú pháp:

```bash
vi [FILENAME]
```

**Lưu ý: Trong cú pháp trên, từ `FILENAME` được đặt trong ngoặc vuông cho biết ta chỉ có thể chỉ định một file tại một thời điểm.**

Một số lệnh điều khiển hữu ích cho trình soạn thảo:

| Lệnh | Chức năng |
|------|-----------|
| `i`  | Chuyển sang chế độ nhập văn bản. |
| `o`  | Mở dòng mới ở chế độ nhập văn bản. |
| `Esc`| Thoát khỏi chế độ chèn và trở về chế độ lệnh. |
| `D`  | Xóa toàn bộ nội dùng từ vị trí con trỏ đến cuối dòng. |
| `x`  | Xóa một ký tự tại vị trí con trỏ. |
| `dd` | Xóa cả dòng. |
| `dw` | Xóa một từ. |
| `cw` | Thay thế một từ. |
| `u`  | Hoàn tác thay đổi cuối cùng vừa thực hiện - Undo. |
| `.`  | Làm lại thay đổi vừa thực hiện - Redo. |
| `r`  | Thay thế một ký tự. |
| `s`  | Xóa một ký tự tại vị trí con trỏ và chuyển sang chế độ chèn. |
| `S`  | Xóa toàn bộ dòng hiện tại và chuyển sang chế độ chèn ở đầu dòng. |
| `~`  | Đổi chữ thường thành chữ hoa hoặc ngược lại. |
| `:w` | Lưu file. |
| `:wq`| Lưu file và thoát khỏi trình soạn thảo. |
| `:q` | Thoát khỏi trình soạn thảo. |
| `/`  | Tìm kiếm chuỗi cần tìm trong file. |
| `n`  | Tìm tiếp về phía cuối file. |
| `Shift + n` | Tìm tiếp về phía đầu file. |
| `gg` | Di chuyển con trỏ đến đầu file. |
| `Shift + gg` | Di chuyển con trỏ đến cuối file. |
| `Ctr + 4` | Đi đến cuối dòng. |
| `Ctr + 6` | Đi đến đầu dòng. |

**Lưu ý: các lệnh sau chỉ được sử dụng khi ở chế độ lệnh.**
