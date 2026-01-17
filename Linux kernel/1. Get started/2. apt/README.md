# Lệnh apt

Lệnh apt _(Advanced Package Tool)_ là trình quản lý gói phần mềm trên các hệ điều hành dựa trên Debian như Ubuntu, Debian, Linux Mint, Raspberry Pi OS. Nó giúp cài đặt, cập nhật, xóa và quản lý các gói phần mềm dễ dàng từ kho lưu trữ của hệ thống.

### Cấu trúc cơ bản của lệnh `apt`

```bash
apt [tùy_chọn] [chức_năng] [tên_gói]
```

- Tùy chọn: Có thể dùng -y để tự động xác nhận, --fix-missing để bỏ qua lỗi...
- Chức năng: install, remove, update, upgrade...
- Tên gói: Tên phần mềm bạn muốn cài đặt hoặc gỡ bỏ.

### Các lệnh `apt` thông dụng

| Lệnh                          | Chức năng                      |
| ----------------------------- | ------------------------------ |
| `sudo apt update`             | Cập nhật danh sách gói         |
| `sudo apt upgrade`            | Cập nhật tất cả các gói đã cài |
| `sudo apt install <tên_gói>`  | Cài đặt một gói phần mềm       |
| `sudo apt remove <tên_gói>`   | Gỡ bỏ một gói (giữ cấu hình)   |
| `sudo apt purge <tên_gói>`    | Gỡ bỏ cả gói và cấu hình       |
| `sudo apt autoremove`         | Xóa các gói không cần thiết    |
| `apt search <từ_khóa>`        | Tìm kiếm gói trong kho         |
| `apt show <tên_gói>`          | Xem thông tin về một gói       |

