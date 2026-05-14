# Command terminal

## 1. Pipeline ( `|` )

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

## 2. Output redirection

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

## 3. Input Redirection

`<` dùng để lấy dữ liệu input cho lệnh từ file, thay vì từ bàn phím.

Ví dụ:

```bash
sort < names.txt
```

→ sort sẽ đọc nội dung file names.txt rồi sắp xếp.

## 4. Lệnh cat

Lệnh `cat` (viết tắt của concatenate) được sử dụng để hiển thị, ghép, tạo và sao chép nội dung của file trong Linux.

Hiển thị nội dung file: `cat <tên_file>`
Hiển thị nội dung file và đánh số dòng: `cat -n <tên_file>`
Hiển thị nội dung nhiều file: `cat <tên_file> <tên_file>`
Ghép nhiều file thành nhiều file: `cat <tên_file> <tên_file> > <output_file>`

## 5. Lệnh grep

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

## 6. Lệnh find

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

## 7. Lệnh dd

`dd` (viết tắt của data duplicator hoặc disk dump) là công cụ dòng lệnh dùng để sao chép và chuyển đổi dữ liệu ở cấp độ raw, đọc/ghi trực tiếp vào block device mà không qua filesystem.

Cú pháp cơ bản:

```bash
dd if=<nguồn> of=<đích> [tùy_chọn]
```

| Tham số | Ý nghĩa |
| --- | --- |
| `if=` | Input file - Nguồn dữ liệu (file, thiết bị, /dev/...). Nếu không chỉ định, `dd` đọc từ `stdin`. |
| `of=` | Output file - Đích ghi dữ liệu. Nếu không chỉ định, `dd` đọc từ `stdout`. |
| `bs=` | Block size — kích thước mỗi block đọc/ghi (vd: 512, 4K, 1M). Ví dụ `bs=4M` nghĩa là mỗi lần đọc/ghi 4MB. |
| `count=` | Số block cần sao chép. Ví dụ `count=100` với `bs=1M` thì sẽ sao chép đúng 100 MB. |
| `skip=` | Bỏ qua N block ở đầu vào trước khi bắt đầu đọc |
| `seek=` | Bỏ qua N block ở đầu ra trước khi bắt đầu ghi |
| `conv=` | Chuyển đổi dữ liệu |
| `status=` | Điều khiển thông tin hiển thị:<br>`status=progress` — hiển thị tiến trình theo thời gian thực<br>`status=none` — không hiển thị gì |

Các ví dụ thực tế:

**Ghi MLO vào sdcard**

```bash
# Ghi MLO vào đúng offset 0x20000 (sector 256)
sudo dd if=MLO of=/dev/sdc bs=512 seek=256
sync
```

Giải thích từng phần:
- `if=MLO` — file MLO đã biên dịch
- `of=/dev/sdc` — ghi vào SD card (toàn bộ card, không phải phân vùng `/dev/sdc1`)
- `bs=512` — block size 512 byte
- `seek=256` — nhảy qua 256 block đầu tiên (= 128 KB) trước khi ghi, đây chính là offset của MLO

**Đọc MLO từ sdcard ra file**

```bash
# Xác định SD card (giả sử là /dev/sdc)
lsblk

# Đọc MLO: bắt đầu từ offset 0x20000 = 128KB
# skip=256 vì mỗi block = 512 byte, 128KB / 512 = 256 block
# count=200 -> tương ứng với 100KB - đủ cho MLO thông thường
sudo dd if=/dev/sdc of=MLO.bin bs=512 skip=256 count=200
```

**Đọc MLO từ sdcard ra terminal**

```bash
sudo dd if=/dev/sdc bs=512 skip=256 count=1 2>/dev/null | hexdump -C
```

Phần `2>/dev/null` dùng để ẩn dòng thống kê của `dd` (kiểu "1+0 records in, 1+0 records out"), chỉ giữ lại dữ liệu thực đi qua pipe.

**Xóa vùng MLO**

```bash
# Xóa vùng MLO: từ sector 256, xóa 200 sector (100KB)
sudo dd if=/dev/zero of=/dev/sdc bs=512 seek=256 count=200
sync
```

Đọc từ `/dev/zero` (toàn byte 0) ghi đè lên vùng MLO — hiệu quả tương đương xóa.

## 8. Lệnh apt

Lệnh apt _(Advanced Package Tool)_ là trình quản lý gói phần mềm trên các hệ điều hành dựa trên Debian như Ubuntu, Debian, Linux Mint, Raspberry Pi OS. Nó giúp cài đặt, cập nhật, xóa và quản lý các gói phần mềm dễ dàng từ kho lưu trữ của hệ thống.

**Cấu trúc cơ bản của lệnh `apt`**

```bash
apt [tùy_chọn] [chức_năng] [tên_gói]
```

- Tùy chọn: Có thể dùng -y để tự động xác nhận, --fix-missing để bỏ qua lỗi...
- Chức năng: install, remove, update, upgrade...
- Tên gói: Tên phần mềm bạn muốn cài đặt hoặc gỡ bỏ.

**Các lệnh `apt` thông dụng**

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