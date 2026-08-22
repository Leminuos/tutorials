# Command terminal

## 1. Pipeline

Biểu tượng `|` dùng để nối đầu ra của một lệnh làm đầu vào cho lệnh khác. Nó cho phép ghép nhiều lệnh thành một chuỗi xử lý:

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
| -------- | ----- |
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
| `bs=` | Block size - kích thước mỗi block đọc/ghi (vd: 512, 4K, 1M). Ví dụ `bs=4M` nghĩa là mỗi lần đọc/ghi 4MB. |
| `count=` | Số block cần sao chép. Ví dụ `count=100` với `bs=1M` thì sẽ sao chép đúng 100 MB. |
| `skip=` | Bỏ qua N block ở đầu vào trước khi bắt đầu đọc |
| `seek=` | Bỏ qua N block ở đầu ra trước khi bắt đầu ghi |
| `conv=` | Chuyển đổi dữ liệu |
| `status=` | Điều khiển thông tin hiển thị:<br>`status=progress` - hiển thị tiến trình theo thời gian thực<br>`status=none` - không hiển thị gì |

Các ví dụ thực tế:

**Ghi MLO vào sdcard**

```bash
# Ghi MLO vào đúng offset 0x20000 (sector 256)
sudo dd if=MLO of=/dev/sdc bs=512 seek=256
sync
```

Giải thích từng phần:
- `if=MLO` - file MLO đã biên dịch
- `of=/dev/sdc` - ghi vào SD card (toàn bộ card, không phải phân vùng `/dev/sdc1`)
- `bs=512` - block size 512 byte
- `seek=256` - nhảy qua 256 block đầu tiên (= 128 KB) trước khi ghi, đây chính là offset của MLO

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

Đọc từ `/dev/zero` (toàn byte 0) ghi đè lên vùng MLO - hiệu quả tương đương xóa.

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

## 9. Các device file đặc biệt

Trên Linux, thư mục `/dev` chứa các **device file**. Đây không phải là file lưu trên ổ cứng mà chúng là phương thức để ta giao tiếp với kernel. Khi ta đọc hoặc ghi vào các file này, kernel sẽ xử lý theo logic riêng của từng device.
 
Phần này giới thiệu các device file đặc biệt khi ta làm việc với terminal trong linux

### 9.1. /dev/null

Mọi dữ liệu ghi vào file này đều biến mất, không lưu ở đâu cả. Đọc từ file thì lập tức nhận EOF (end-of-life), tức không có gì để đọc. Ta có thể hình dung `/dev/null` như một thùng rác vô đáy - bỏ gì vào cũng mất, nhìn vào thì trống rỗng.
 
**Ví dụ thực tế: Ta muốn chạy một lệnh nhưng không muốn thấy thông báo lỗi trên terminal**

```bash
find / -name "*.log" 2>/dev/null
```

Ở đây `2>` chuyển hướng `stderr` vào `/dev/null`. Khi `find` quét vào các thư mục không có quyền đọc, nó sẽ phát sinh hàng loạt lỗi "Permission denied" - nhưng tất cả đều bị nuốt bởi `/dev/null`, nên terminal chỉ hiện ra những kết quả tìm thấy hợp lệ.

:::tip Mẹo hay
Ta có thể dùng `/dev/null` để xóa nội dung một file mà không xóa chính file đó: `cat /dev/null > logfile.txt`. File vẫn tồn tại nhưng nội dung bên trong bị xóa sạch - rất hữu ích khi muốn làm trống file log mà không ảnh hưởng đến các chương trình đang ghi vào nó.
:::

### 9.2. /dev/zero

Đọc file này sẽ nhận được một dòng byte `0x00` (null byte) vô tận, không bao giờ dừng. Ghi vào file thì giống `/dev/null`, dữ liệu biến mất.

**Ví dụ thực tế: Tạo một file có kích thước đúng 1GB, chứa toàn số 0**

```bash
dd if=/dev/zero of=disk.img bs=1M count=1024
```

Lệnh này đọc từ `/dev/zero` (input file), mỗi lần đọc 1MB (`bs=1M`), lặp lại 1024 lần (`count=1024`), ghi ra file `disk.img`. Kết quả là một file 1GB toàn null byte. Kỹ thuật này thường dùng để tạo file image cho máy ảo, tạo swap space, hoặc xóa sạch dữ liệu trên ổ đĩa (ghi đè toàn bộ bằng số 0).

:::warning Cẩn thận với `dd`
Lệnh `dd` rất mạnh và không có xác nhận trước khi ghi đè. Nếu ta nhầm `of=` thành tên ổ đĩa thật (ví dụ `of=/dev/sda`), toàn bộ dữ liệu trên ổ đĩa sẽ bị xóa sạch không thể khôi phục. Luôn kiểm tra kỹ lệnh trước khi nhấn Enter.
:::

### 9.3. /dev/random

Đọc từ file này sẽ nhận các byte ngẫu nhiên, được tạo ra từ **entropy pool** của kernel. Ghi vào file thì sẽ thêm dữ liệu vào entropy pool.

Entropy ở đây là độ bất ngờ mà kernel thu thập được từ các sự kiện phần cứng khó đoán như thời điểm nhấn phím, di chuột, interrupt của đĩa cứng, nhiễu mạng...

Trên các phiên bản kernel cũ (trước Linux 5.6), nếu entropy pool cạn kiệt, `/dev/random` sẽ dừng lại chờ cho đến khi có đủ entropy mới. Điều này đảm bảo chất lượng ngẫu nhiên luôn ở mức cao nhất.
 
**Ví dụ thực tế: Tạo một khóa mã hóa 256-bit (32 byte) có chất lượng ngẫu nhiên cao:**
 
```bash
head -c 32 /dev/random | xxd
```

Lệnh `head -c 32` đọc đúng 32 byte ngẫu nhiên, sau đó `xxd` chuyển thành dạng hex để hiển thị trên màn hình. Kết quả là một chuỗi 64 ký tự hex hoàn toàn ngẫu nhiên, đủ chất lượng để dùng làm khóa mật mã. Đây là cách các hệ thống sinh SSL certificate, GPG key, hay SSH key lấy entropy.
 
:::tip Khi nào dùng `/dev/random`?
Chỉ cần dùng `/dev/random` khi ta cần tạo khóa mật mã cực kỳ quan trọng. Với hầu hết trường hợp khác, `/dev/urandom` là đủ tốt và tiện hơn.
:::
 
### 9.4. /dev/urandom
 
Giống `/dev/random`, trả về byte ngẫu nhiên. Điểm khác biệt là nó không bao giờ dừng lại chờ. Nếu entropy pool cạn, nó dùng thuật toán mật mã để tiếp tục sinh byte ngẫu nhiên dựa trên những gì đã có.

Chữ "u" trong urandom nghĩa là "unlimited" (không giới hạn) hoặc "unblocking" (không chặn).

**Ví dụ thực tế: Sinh mật khẩu ngẫu nhiên 20 ký tự:**
 
```bash
head -c 15 /dev/urandom | base64
```

Đọc 15 byte ngẫu nhiên rồi mã hóa base64, cho ra chuỗi khoảng 20 ký tự gồm chữ hoa, chữ thường, số và +/=. Dùng `/dev/urandom` thay vì `/dev/random` vì ta không muốn lệnh phải treo chờ entropy - với việc sinh password, mức ngẫu nhiên của `urandom` là quá đủ.

:::warning random vs urandom - chọn cái nào?
Trên Linux hiện đại (phiên bản kernel 5.6 trở lên), cả hai gần như giống nhau về chất lượng ngẫu nhiên sau khi hệ thống đã khởi động xong. Sự khác biệt chỉ thực sự quan trọng trên kernel cũ hoặc trong giai đoạn hệ thống mới bật lên (early boot) khi entropy pool chưa có đủ dữ liệu.
:::
 
### 9.5. /dev/full

Đọc từ nó thì giống `/dev/zero` (trả về null byte). Nhưng khi ghi vào nó, luôn trả về lỗi `ENOSPC` (No space left on device) - giả lập tình trạng ổ đĩa hết dung lượng.

**Ví dụ thực tế: Kiểm tra xem script của ta có xử lý đúng khi ghi file thất bại không:**

```bash
echo "dữ liệu test" > /dev/full
# Kết quả: bash: echo: write error: No space left on device
```

Điều này cực kỳ hữu ích khi ta viết script backup hoặc logging và muốn test xem chương trình có crash hay mất dữ liệu khi đĩa đầy không, mà không cần thực sự lấp đầy ổ đĩa.

:::tip Dùng `/dev/full` để test phần mềm
Nếu ta viết ứng dụng có ghi file, hãy thử chuyển hướng output sang `/dev/full` để xem ứng dụng có bắt được lỗi hay không. Nhiều bug nghiêm trọng trong phần mềm xảy ra vì lập trình viên quên kiểm tra lỗi khi ghi file.
:::
 
### 9.6. /dev/tty

Luôn trỏ đến terminal đang điều khiển tiến trình hiện tại. Ghi vào nó thì nội dung hiện ra trên terminal, đọc từ nó thì lấy input từ bàn phím.
 
**Ví dụ thực tế: Giả sử ta có script nhận dữ liệu qua pipe, nhưng vẫn cần hỏi mật khẩu từ bàn phím:**
 
```bash
# Gọi script với dữ liệu qua pipe
echo "dữ liệu đầu vào" | ./my_script.sh
```
 
Bên trong `my_script.sh`:
 
```bash
#!/bin/bash
# Lúc này stdin là pipe, không phải bàn phím
# Nhưng /dev/tty luôn kết nối đến bàn phím thật

read -p "Nhập mật khẩu: " password < /dev/tty
 
echo "Đã nhận mật khẩu, đang xử lý..."
```

Dù `stdin` đã bị chiếm bởi pipe, `/dev/tty` vẫn kết nối trực tiếp đến terminal thật nên `read` vẫn đọc được input từ bàn phím. Các lệnh như `ssh`, `sudo`, `gpg` đều dùng kỹ thuật này để hỏi mật khẩu.

:::warning Lưu ý khi dùng `/dev/tty`
`/dev/tty` chỉ hoạt động khi tiến trình có terminal thật (ví dụ người dùng đang mở cửa sổ dòng lệnh). Nếu script chạy tự động trong nền (cron job, systemd service...), không có terminal nào gắn liền, và việc đọc từ `/dev/tty` sẽ gây lỗi.
:::
 
### 9.7. Kết luận

Các device file trong `/dev` là một phần thiết kế cốt lõi của Linux - triết lý **"mọi thứ đều là file"**. Nhờ có chúng, ta có thể dùng các công cụ dòng lệnh quen thuộc (`cat`, `echo`, `dd`, `head`...) để tương tác với phần cứng và các tính năng đặc biệt của kernel mà không cần viết chương trình phức tạp.
 
Hiểu rõ các device file này sẽ giúp ta viết script hiệu quả hơn, xử lý lỗi tốt hơn, và hiểu sâu hơn cách Linux vận hành ở phía sau.

