# Command terminal

## Pipeline ( `|` )

Biểu tượng `|` dùng để nối đầu ra của một lệnh → làm đầu vào cho lệnh khác.

**Ví dụ 1:**

```bash
ls | grep ".c"
```

- `ls` liệt kê tất cả file trong thư mục.
- `grep ".c"` lọc ra chỉ những file có đuôi .c.

→ Lấy kết quả của ls rồi đưa vào grep để lọc tiếp.

**Ví dụ 2:**

```bash
dmesg | grep error | less
```

- `dmesg`: in log kernel
- `grep error`: lọc chỉ các dòng có chữ “error”
- `less`: cho phép bạn cuộn lên xuống để đọc kết quả

## Output redirection

`>` được sử dụng để chuyển hướng output của lệnh vào file, ghi đè nội dung cũ.

**Ví dụ:**

```bash
ls > files.txt
```

→ Tạo file `files.txt` chứa danh sách file hiện tại.
→ Nếu `files.txt` đã tồn tại → nó sẽ bị ghi đè.

`>>` được sử dụng để chuyển hướng output và nối vào cuối file (không xóa nội dung cũ).

**Ví dụ:**

```bash
echo "New entry" >> files.txt
```

→ Thêm dòng "New entry" vào cuối file `files.txt`.

## Input Redirection

`<` dùng để lấy dữ liệu input cho lệnh từ file, thay vì từ bàn phím.

Ví dụ:

```bash
sort < names.txt
```

→ sort sẽ đọc nội dung file names.txt rồi sắp xếp.

## Lệnh cat

Lệnh `cat` (viết tắt của concatenate) được sử dụng để hiển thị, ghép, tạo và sao chép nội dung của file trong Linux.

Hiển thị nội dung file: `cat <tên_file>`
Hiển thị nội dung file và đánh số dòng: `cat -n <tên_file>`
Hiển thị nội dung nhiều file: `cat <tên_file> <tên_file>`
Ghép nhiều file thành nhiều file: `cat <tên_file> <tên_file> > <output_file>`

## Lệnh grep

Lệnh `grep` được sử dụng để tìm kiếm văn bản trong file hoặc đầu ra của lệnh khác theo một pattern nhất định.

```bash
grep [tùy chọn] "chuỗi_cần_tìm" [tên_file]
```

| Tùy chọn | Mô tả |
| `-i`     | Không phân biệt chữ hoa/thường |
| `-n`     | Hiển thị số dòng |
| `-H`     | In tên file |
| `-r`     | Tìm đệ quy trong thư mục |
| `-l`     | Chỉ hiển thị tên file có kết quả |
| `-w`     | Tìm word match |

Ví dụ:

```bash
grep -nrwi printk System.map
```

→ Tìm tất cả các dòng có chứa từ printk trong mã kernel, không phân biệt hoa/thường, hiển thị số dòng.

## Lệnh find

Lệnh find được dùng để tìm kiếm file.

```bash
find [đường_dẫn_bắt_đầu] [tùy_chọn_điều_kiện] [hành_động]
```

Ví dụ đơn giản nhất:

```bash
find /home/nguyen -name "main.c"
```

→ Tìm file tên chính xác là `main.c` bắt đầu từ thư mục hiện tại (`/home/nguyen`).

**Tùy chọn phổ biến**

| Tùy chọn              | Ý nghĩa                                     |
| --------------------- | ------------------------------------------- |
| `-name "pattern"`     | Tìm theo tên (phân biệt hoa/thường)         |
| `-iname "pattern"`    | Tìm theo tên **không phân biệt hoa/thường** |
| `-type f`             | Chỉ tìm **file**                            |
| `-type d`             | Chỉ tìm **thư mục**                         |
| `-size +100k`         | Tìm file **lớn hơn 100KB**                  |
| `-mtime -1`           | Tìm file **chỉnh sửa trong 1 ngày gần đây** |
| `-user root`          | Tìm file thuộc về user `root`               |
| `-exec command {} \;` | Thực thi lệnh với mỗi file tìm được         |

**Kết hợp `find` với `grep`**

Ta có thể kết hợp hai lệnh này để tìm đoạn text bên trong tất cả các file trong một thư mục. Ví dụ:

```bash
find . -name "*.c" -exec grep -Hn "text_can_tim" {} \;
```

Trong đó:
- `-exec ... {} \;`: Thực thi lệnh đối với mỗi file.

## Lệnh apt

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