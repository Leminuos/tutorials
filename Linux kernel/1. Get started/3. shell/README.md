## 1. Giới thiệu về Shell và Shell Script

### 1.1. Shell là gì?

**Shell** là chương trình trung gian giúp ta nói chuyện với OS. Khi ta gõ một lệnh như `ls`, shell sẽ nhận lệnh đó, tìm chương trình tương ứng, yêu cầu kernel chạy nó, rồi trả kết quả về cho ta.

```text
   Ta gõ lệnh           Shell dịch lệnh         Kernel thực thi
  +-----------+        +--------------+        +--------------+
  |  ls -l    | -----> |  Bash / sh   | -----> |   Linux OS   |
  +-----------+        +--------------+        +--------------+
                              ▲                        │
                              └────── kết quả ─────────┘
```

Có nhiều loại shell: `bash`, `sh`, `zsh`, `fish`...Tài liệu này tập trung vào **Bash**.

Có hai cách dùng shell:
- **Tương tác (interactive):** ta gõ từng lệnh vào terminal và nhận kết quả ngay.
- **Script:** ta gom nhiều lệnh vào một file để chạy tự động, lặp lại nhiều lần mà không phải gõ lại.

### 1.2. Shell script là gì?

Shell script là một file văn bản chứa chuỗi các lệnh shell được thực thi tuần tự từ trên xuống. Nó giúp tự động hóa những công việc lặp đi lặp lại: sao lưu dữ liệu, xử lý file hàng loạt, cài đặt phần mềm, kiểm tra hệ thống...

Hãy tạo file script đầu tiên. Mở trình soạn thảo và gõ:

```bash
#!/bin/bash

echo "Xin chào, $USER!"
echo "Hôm nay là $(date +%A)"
```

Lưu thành `hello.sh`, sau đó cấp quyền và chạy:

```bash
chmod +x hello.sh   # cấp quyền thực thi
./hello.sh          # chạy script
```

Kết quả (tùy máy):

```
Xin chào, pei!
Hôm nay là Wednesday
```

### 1.3. Shebang

```bash
#!/bin/bash
```

Dòng bắt đầu bằng `#!` (đọc là "shebang") báo cho hệ điều hành biết dùng chương trình nào để thực thi file này. Nó phải là dòng đầu tiên, không được có khoảng trắng hay dòng trống phía trước.

2 kiểu shebang phổ biến:

| Shebang | Ý nghĩa | Khi nào dùng |
| --- | --- | --- |
| `#!/bin/bash` | Chỉ định chính xác đường dẫn tới bash | Khi chắc chắn máy đích có bash ở `/bin/bash` |
| `#!/usr/bin/env bash` | Tìm bash theo biến `PATH` | Khi viết script để **chia sẻ** (linh hoạt hơn giữa các máy) |

:::warning Bash không phải sh
Trên nhiều hệ thống, `/bin/sh` được liên kết tới một shell rút gọn như `dash` chứ không phải Bash. Nếu script của ta dùng tính năng riêng của Bash như mảng, `[[ ]]`, `(( ))`, `${str^^}`... mà khai báo shebang là `#!/bin/sh`, script sẽ chạy lỗi trên một số máy. Khi cần tính năng Bash, luôn khai báo `bash`.
:::

### 1.4. Quyền thực thi và chmod

Trước khi chạy trực tiếp một script, file cần có quyền thực thi. Mỗi file trong Linux có 3 nhóm quyền: đọc (`r`), ghi (`w`), thực thi (`x`).

```bash
ls -l hello.sh
# -rw-r--r--  1 nam nam  45 ...  hello.sh   <- chưa có 'x', chưa chạy trực tiếp được
 
chmod +x hello.sh
ls -l hello.sh
# -rwxr-xr-x  1 nam nam  45 ...  hello.sh   <- đã có 'x'
```

Dùng `chmod +x` thêm quyền thực thi. Ta chỉ cần làm một lần cho mỗi file.

### 1.5. Các cách chạy script

Cách 1: chạy trực tiếp (cần quyền thực thi + đường dẫn)

```bash
chmod +x script.sh  # cấp quyền thực thi (chỉ cần làm 1 lần)
./script.sh
```

> Vì sao phải có `./`? Vì mặc định shell chỉ tìm lệnh trong các thư mục thuộc biến `PATH`. Thư mục hiện tại thường không nằm trong `PATH`, nên phải chỉ rõ chạy file ngay tại đây bằng `./`.

Cách 2: không cần chmod, gọi bash chạy trực tiếp

```bash
bash script.sh
sh script.sh
```

Cách 3: chạy trong shell hiện tại, biến và hàm định nghĩa trong script sẽ tồn tại sau khi script kết thúc

```bash
source script.sh
. script.sh         # dấu chấm là viết tắt của "source"
```

:::tip Khác biệt giữa subshell vs source
Cách 1 và 2 chạy script trong một shell con (subshell) mới. Mọi biến định nghĩa trong script sẽ biến mất khi script kết thúc.

Cách 3 chạy script ngay trong shell hiện tại, nên các biến và hàm vẫn còn tồn tại sau đó.

```bash
# file: setvar.sh
my_name="Nam"
```

```bash
./setvar.sh
echo "$my_name"      # (rỗng) - biến chết cùng subshell

source setvar.sh
echo "$my_name"      # Nam  - biến vẫn còn
```

Đây chính là lý do các file cấu hình như `~/.bashrc` được nạp bằng `source`.
:::

## 2. Variable

### 2.1. Khai báo và truy xuất

Tên biến có thể chứa chữ, số và dấu gạch dưới, nhưng không được bắt đầu bằng số:

```bash
my_var="ok"      # hợp lệ
_hidden="ok"     # hợp lệ
count2="ok"      # hợp lệ
2var="loi"       # KHÔNG hợp lệ - bắt đầu bằng số
```

:::warning Không dùng ký tự đặc biệt để đặt tên
Không dùng các ký tự như `! * - @ $` trong tên biến, vì chúng đã mang ý nghĩa riêng với shell.
:::

:::warning Không có khoảng trắng quanh dấu `=`
Đây là lỗi phổ biến nhất của người mới. Shell dùng khoảng trắng để phân tách lệnh và đối số, nên nếu ta thêm khoảng trắng quanh `=`, shell sẽ hiểu nhầm tên biến là một lệnh.

```bash
name="Nam"          # ĐÚNG
name = "Nam"        # SAI - shell hiểu "name" là lệnh -> "name: command not found"
name ="Nam"         # SAI
name= "Nam"         # SAI
```
:::

### 2.2. Truy xuất giá trị biến

Dùng dấu `$` trước tên biến để lấy giá trị:

```bash
name="Nam"
echo $name             # Nam
echo "${name}"         # Nam  (cách viết an toàn hơn)
```

>**Vì sao nên dùng `${...}`?** Khi biến đứng sát ký tự khác, shell không biết tên biến kết thúc ở đâu. Dấu `{ }` giúp khoanh vùng rõ ràng:
>
>```bash
>name="Nam"
>echo "Xin chao $nameoi"     # (rỗng phần tên) - shell tìm biến $nameoi (không tồn tại)
>echo "Xin chao ${name}oi"   # Xin chao Namoi - đúng ý muốn
>```

### 2.3. Biến chỉ đọc và hủy biến

```bash
readonly PI=3.14
PI=3.14159            # lỗi: PI: readonly variable - không được sửa

name="Nam"
unset name            # xóa biến
echo "$name"          # (rỗng)
```

### 2.4. Nhập giá trị từ người dùng với `read`
 
Lệnh `read` đọc dữ liệu người dùng gõ vào và lưu vào biến:

```bash
#!/bin/bash
 
read -p "Tên bạn là gì? " name
echo "Chào $name!"
 
read -p "Nhập tuổi: " age
echo "Bạn $age tuổi"
```

Các tùy chọn hữu ích của `read`:

| Tùy chọn | Tác dụng |
| --- | --- |
| `-p "text"` | Hiện lời nhắc (prompt) trước khi nhập |
| `-s` | Ẩn nội dung nhập (dùng cho mật khẩu) |
| `-r` | Không diễn giải dấu `\` (nên luôn dùng) |
| `-n N` | Đọc tối đa N ký tự rồi tự dừng |
| `-t N` | Chờ tối đa N giây rồi bỏ qua |

```bash
read -s -p "Nhập mật khẩu: " pass
echo    # xuống dòng vì -s không tự xuống dòng
echo "Đã nhận mật khẩu dài ${#pass} ký tự"
```

### 2.5. Các loại biến

**a) Biến thường (local) và biến môi trường (environment)**

```bash
# Biến thường: chỉ tồn tại trong shell hiện tại, process con KHÔNG thấy
my_var="hello"
 
# Biến môi trường: export để process con cũng dùng được
export APP_HOME="/opt/myapp"
```

Ví dụ: nếu script A `export` một biến rồi gọi script B, thì B đọc được biến đó. Nếu không `export`, B sẽ thấy biến rỗng.

**b) Biến hệ thống có sẵn**

Shell cung cấp sẵn nhiều biến hữu ích:

```bash
echo "$HOME"     # thư mục home của user, vd: /home/nam
echo "$USER"     # tên user hiện tại, vd: nam
echo "$PATH"     # danh sách thư mục shell tìm lệnh, ngăn cách bởi dấu :
echo "$PWD"      # thư mục làm việc hiện tại
echo "$SHELL"    # shell mặc định của user
echo "$HOSTNAME" # tên máy
```

**c) Biến đặc biệt**

Các ký tự đặc biệt không dùng làm tên biến được vì chúng đã dành cho những biến có sẵn sau đây:

| Biến | Ý nghĩa |
| --- | --- |
| `$0` | Tên file của script hiện tại |
| `$1`, `$2`, ... `$n` | Đối số thứ n truyền vào. Vd `./test.sh a b` → `$1`=a, `$2`=b |
| `$#` | Số lượng đối số truyền vào |
| `$?` | Exit status của lệnh vừa chạy (0 = thành công, khác 0 = lỗi). Phạm vi 0–255 |
| `$$` | PID của script hiện tại |
| `$!` | PID của lệnh background gần nhất |
| `$*` | Tất cả đối số, gộp thành một chuỗi |
| `$@` | Tất cả đối số, giữ riêng lẻ từng cái |

Xét ví dụ sau:

```bash
#!/bin/sh

echo "File Name: $0"
echo "First Parameter : $1"
echo "Second Parameter : $2"
echo "Quoted Values: $@"
echo "Quoted Values: $*"
echo "Total Number of Parameters : $#"
```

Kết quả khi chạy `./test.sh Hello World`:

```
File Name: ./test.sh
First Parameter : Hello
Second Parameter : World
Quoted Values: Hello World
Quoted Values: Hello World
Total Number of Parameters : 2
```

Về cơ bản thì hai biến variable `$*` và `$@` hoạt động giống nhau, trừ khi chúng được đặt trong dấu `""`:
- `$*` : Các đối số đưa vào sẽ được nối thành một chuỗi và được phân cách nhau bởi dấu cách.
- `$@`: Các đối số đưa vào sẽ được phân biệt một cách riêng lẻ.

Xem xét ví dụ sau:

```bash
#!/bin/sh

echo "Case 1: No double quote"
echo "------------------------"
echo "+ Using \$*:"
for str in $*
do
    echo $str
done

echo "------------------------"
echo "+ Using \$@:"
for str in $@
do
    echo $str
done

echo "\nCase 2: Using with double quote"
echo "------------------------"
echo "+ Using \"\$*\":"
for str in "$*"
do
    echo $str
done

echo "------------------------"
echo "+ Using \"\$@\":"
for str in "$@"
do
    echo $str
done
```

Kết quả khi chạy `./test.sh Hello World`:

```
Case 1: No double quote
------------------------
+ Using $*:
Hello
World
------------------------
+ Using $@:
Hello
World

Case 2: Using with double quote
------------------------
+ Using "$*":
Hello World
------------------------
+ Using "$@":
Hello
World
```

:::tip Ghi nhớ
Khi lặp qua đối số, gần như luôn dùng `"$@"`. Chỉ dùng `"$*"` khi thực sự muốn nối tất cả thành một chuỗi.
:::

### 2.6. Command Substitution (Gán kết quả lệnh vào biến)

Cú pháp `$(...)` thực thi lệnh bên trong và lấy output gán vào biến:

```bash
current_date=$(date +%Y-%m-%d)
file_count=$(ls | wc -l)
os_name=$(uname -s)
 
echo "Ngày hôm nay: $current_date"
echo "Số file trong thư mục: $file_count"
 
# Lồng nhau được - nhưng hạn chế dùng vì khó đọc
top_dir=$(ls $(pwd) | head -1)
```

:::warning Lưu ý
Cú pháp cũ dùng backtick `` `command` `` vẫn hoạt động nhưng không khuyến nghị vì khó lồng nhau và khó đọc.
:::

### 2.7. Parameter Expansion (Giá trị mặc định)

Cơ chế này giúp xử lý các trường hợp biến chưa có giá trị:

```bash
# Dùng giá trị mặc định nếu biến rỗng hoặc chưa khai báo
echo "${name:-Guest}"        # in "Guest", $name vẫn rỗng
 
# Gán luôn giá trị mặc định vào biến nếu chưa có
echo "${name:=Guest}"        # $name = "Guest" từ đây
 
# Báo lỗi và thoát nếu biến rỗng
echo "${name:?'Thiếu tên!'}" # in lỗi ra stderr và thoát script
 
# Dùng giá trị thay thế chỉ khi biến đã có giá trị
echo "${name:+Hello}"        # in "Hello" nếu $name không rỗng
```

Ứng dụng thực tế - kiểm tra biến môi trường bắt buộc:
 
```bash
#!/bin/bash
DB_HOST="${DB_HOST:?'Lỗi: DB_HOST chưa được cấu hình!'}"
DB_PORT="${DB_PORT:-5432}"   # mặc định là 5432 nếu không truyền vào
```

### 2.8. String manipulation (Cắt chuỗi trong biến)

Bash có sẵn nhiều phép biến đổi chuỗi mà không cần gọi công cụ ngoài:

```bash
str="Hello World"
 
echo "${#str}"            # độ dài chuỗi: 11
echo "${str:0:5}"         # cắt từ vị trí 0, lấy 5 ký tự → "Hello"
echo "${str:6}"           # cắt từ vị trí 6 đến hết → "World"
echo "${str/World/VN}"    # thay thế lần đầu tiên → "Hello VN"
echo "${str//l/L}"        # thay thế tất cả → "HeLLo WorLd"
echo "${str^^}"           # viết hoa toàn bộ → "HELLO WORLD"
echo "${str,,}"           # viết thường toàn bộ → "hello world"
```
 
Xử lý đường dẫn file, rất hay dùng trong script:
 
```bash
path="/home/user/file.txt"
 
echo "${path##*/}"    # lấy tên file (xóa prefix dài nhất đến /) → "file.txt"
echo "${path%/*}"     # lấy thư mục cha (xóa suffix từ / cuối) → "/home/user"
echo "${path%.txt}"   # bỏ đuôi .txt → "/home/user/file"
echo "${path##*.}"    # lấy extension → "txt"
```

:::tip Mẹo nhớ # và %
Trên bàn phím, `#` nằm bên trái -> cắt từ đầu chuỗi. `%` nằm bên phải -> cắt từ cuối chuỗi.

Một ký tự (`#`, `%`) = khớp ngắn nhất; hai ký tự (`##`, `%%`) = khớp dài nhất.
:::

### 2.9. Mảng

**Mảng chỉ số (indexed array):** lưu danh sách phần tử theo thứ tự:

```bash
fruits=("apple" "banana" "cherry")

echo "${fruits[0]}"        # phần tử đầu: apple
echo "${fruits[@]}"        # tất cả:      apple banana cherry
echo "${#fruits[@]}"       # số lượng:    3

fruits+=("orange")         # thêm phần tử vào cuối

# Duyệt qua các phần tử
for f in "${fruits[@]}"; do
    echo "- $f"
done

# Duyệt kèm chỉ số
for i in "${!fruits[@]}"; do
    echo "$i => ${fruits[$i]}"
done
```

**Mảng kết hợp (associative array):** lưu theo cặp key–value, cần Bash 4 trở lên:

```bash
declare -A user            # BẮT BUỘC khai báo bằng declare -A
user[name]="Nam"
user[age]=25

echo "${user[name]}"       # Nam
echo "${!user[@]}"         # tất cả key:   name age
echo "${user[@]}"          # tất cả value: Nam 25
```

## 3. Basic operators

### 3.1. Arithmetic operators (Toán tử số học)

Bash không tự nhận dạng phép tính trong chuỗi thông thường. Phải dùng một trong các cú pháp sau:

```bash
a=10
b=3

# Cách 1: $(( )) - khuyến nghị, nhanh và dễ đọc
echo $((a + b))    # 13
echo $((a - b))    # 7
echo $((a * b))    # 30
echo $((a / b))    # 3  (chỉ lấy phần nguyên)
echo $((a % b))    # 1  (phần dư)
echo $((a ** b))   # 1000 (lũy thừa)

# Gán kết quả
result=$((a * b + 5))

# Cách 2: let
let "result = a * b"
let "a++"           # tăng a lên 1

# Cách 3: expr (cũ, ít dùng - lưu ý phải có khoảng trắng)
result=$(expr $a + $b)
result=$(expr $a \* $b)   # dấu * phải escape
```

:::warning Chú ý
Bash chỉ tính toán số nguyên. Nếu cần số thực, dùng `bc` hoặc `awk`:

```bash
echo "scale=2; 10 / 3" | bc             # -> 3.33
awk "BEGIN { printf \"%.2f\n\", 10/3 }" # -> 3.33
```
:::

### 3.2. Numeric Comparison (Toán tử so sánh số)

Dùng trong `[ ]` hoặc `[[ ]]`:

| Toán tử | Ý nghĩa | Ví dụ |
| --- | --- | --- |
| `-eq` | bằng (equal) | `[ $a -eq $b ]` |
| `-ne` | khác (not equal) | `[ $a -ne $b ]` |
| `-gt` | lớn hơn (greater than) | `[ $a -gt $b ]` |
| `-lt` | nhỏ hơn (less than) | `[ $a -lt $b ]` |
| `-ge` | lớn hơn hoặc bằng (greater or equal) | `[ $a -ge $b ]` |
| `-le` | nhỏ hơn hoặc bằng (less or equal) | `[ $a -le $b ]` |

Hoặc dùng `(( ))` với ký hiệu toán học quen thuộc như `<`, `>`, `==`, `!=`:

```bash
a=10; b=20
 
if (( a < b )); then
    echo "$a nhỏ hơn $b"
fi
 
if (( a >= 5 && b <= 100 )); then
    echo "Điều kiện kép"
fi
```

:::warning Chú ý
Các toán tử `-eq`, `-gt`... không hoạt động cho các giá trị chuỗi, shell chỉ hỗ trợ chúng cho các giá trị số.
:::

### 3.3. String Comparison (Toán tử so sánh chuỗi)

| Toán tử | Ý nghĩa |
| --- | --- |
| `=` hoặc `==` | bằng nhau |
| `!=` | khác nhau |
| `<` | nhỏ hơn |
| `>` | lớn hơn |
| `-z` | chuỗi rỗng (zero length) |
| `-n` | chuỗi không rỗng (non-zero) |

Ví dụ:

```bash
str1="apple"
str2="banana"
 
if [ "$str1" = "$str2" ]; then
    echo "Bằng nhau"
else
    echo "Khác nhau"
fi
 
# Dùng [[ ]] để so sánh thứ tự từ điển và pattern matching
if [[ "$str1" < "$str2" ]]; then
    echo "$str1 đứng trước $str2"
fi
 
# Kiểm tra pattern (chỉ dùng được trong [[ ]])
filename="report_2024.csv"
if [[ "$filename" == *.csv ]]; then
    echo "Là file CSV"
fi
 
# Kiểm tra chuỗi rỗng
if [ -z "$empty_var" ]; then echo "Biến rỗng"; fi
if [ -n "$str1" ]; then echo "Biến không rỗng"; fi
```

### 3.4. Logical operators (Toán tử logic)

| Toán tử | Ý nghĩa |
| --- | --- |
| `!` | NOT |
| `\|\|` | OR |
| `&&` | AND |

Ví dụ:

```bash
a=10
b=20

if [[ $a -gt 5 && $b -gt 15 ]]; then
    echo "Ca hai dieu kien deu dung"
fi

if [[ $a -gt 100 || $b -gt 15 ]]; then
    echo "It nhat mot dieu kien dung"
fi

if [[ ! $a -eq $b ]]; then
    echo "a khac b"
fi

# Trong [ ] cũ hơn dùng -a (and) và -o (or)
if [ $a -gt 5 -a $b -gt 15 ]; then
    echo "Ca hai dieu kien deu dung"
fi
```

:::tip Ưu tiên `[[ ]]` thay cho `[ ]`
Trong `[ ]` kiểu cũ, AND/OR viết là `-a`/`-o`, ví dụ `[ $a -gt 5 -a $b -gt 15 ]`. Nhưng cú pháp này khó đọc và dễ lỗi với biến rỗng. Trong Bash, hãy dùng `[[ ]]` với `&&`, `||` - rõ ràng và an toàn hơn, đồng thời hỗ trợ so khớp mẫu và regex (`=~`).
:::

### 3.5. File Test operators (Toán tử kiểm tra file)

| Toán tử | Ý nghĩa |
| --- | --- |
| `-e` | file/thư mục tồn tại |
| `-f` | tồn tại và là file thường |
| `-d` | tồn tại và là thư mục |
| `-r` | có quyền đọc |
| `-w` | có quyền ghi |
| `-x` | có quyền thực thi |
| `-s` | file tồn tại và có kích thước > 0 |
| `-L` | là symbolic link |
| `-nt` | file này mới hơn (newer than) file kia |
| `-ot` | file này cũ hơn (older than) file kia |

Ví dụ:

```bash
file="/etc/passwd"

if [ -e "$file" ]; then
    echo "File ton tai"
fi

if [ -f "$file" ]; then
    echo "La file thuong"
fi

if [ -d "/home/user" ]; then
    echo "La thu muc"
fi

if [ -r "$file" ] && [ -w "$file" ]; then
    echo "Co quyen doc va ghi"
fi

if [ -s "$file" ]; then
    echo "File khong rong"
fi

if [ file1.txt -nt file2.txt ]; then
    echo "file1 moi hon file2"
fi
```

## 4. Câu lệnh điều kiện

### 4.1. Câu lệnh if

`if` cho phép script ra quyết định chạy khối lệnh này hay khối lệnh khác tùy điều kiện.

```bash
# Dạng đầy đủ
if [ điều_kiện ]; then
    # chạy khi điều kiện ĐÚNG
elif [ điều_kiện_khác ]; then
    # chạy khi điều kiện đầu sai, điều kiện này đúng
else
    # chạy khi mọi điều kiện trên đều sai
fi
```

`if` kết thúc bằng `fi` (chính là `if` viết ngược). Tương tự, `case` kết thúc bằng `esac`.

Ví dụ:

```bash
#!/bin/bash

read -p "Nhập điểm: " score

if (( score >= 80 )); then
    echo "Xếp loại: Giỏi"
elif (( score >= 50 )); then
    echo "Xếp loại: Đạt"
else
    echo "Xếp loại: Chưa đạt"
fi
```

```
Nhập điểm: 72
Xếp loại: Đạt
```

:::tip `; then` hay xuống dòng?
Dấu `;` cho phép viết `then` cùng dòng với `if`. Nếu tách dòng thì bỏ dấu `;`:

```bash
if [ -f "$file" ]; then     # cách 1 (gọn)
    echo "OK"
fi

if [ -f "$file" ]           # cách 2
then
    echo "OK"
fi
```
:::

### 4.2. Câu lệnh case

`case` phù hợp khi cần so sánh một biến với nhiều giá trị, thay thế chuỗi `if/elif` dài.

```bash
#!/bin/bash

read -p "Nhập lệnh (start/stop/restart): " action

case "$action" in
    start)
        echo "Đang khởi động dịch vụ..."
        ;;
    stop)
        echo "Đang dừng dịch vụ..."
        ;;
    restart|reload)                 # nhiều mẫu, ngăn cách bằng |
        echo "Đang khởi động lại..."
        ;;
    *)                              # mặc định, khớp mọi trường hợp còn lại
        echo "Lệnh không hợp lệ: $action"
        ;;
esac
```

Ghi nhớ về cú pháp `case`:
- Mỗi nhánh kết thúc bằng `;;`.
- `*)` là nhánh mặc định (giống `else`).
- Mẫu hỗ trợ ký tự đại diện. Ví dụ trả lời Yes/No linh hoạt:

Ví dụ:

```bash
#!/bin/bash

day=$(date +%A)   # Lấy tên thứ trong tuần: Monday, Tuesday,...

case "$day" in
    Monday|Tuesday|Wednesday|Thursday|Friday)
        echo "Ngày làm việc"
        ;;
    Saturday|Sunday)
        echo "Cuối tuần!"
        ;;
    *)
        echo "Không xác định"
        ;;
esac
```

### 4.3. Biểu thức điều kiện nâng cao với `[[ ]]`

```bash
# So sánh regex
email="user@example.com"
if [[ "$email" =~ ^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$ ]]; then
    echo "Email hợp lệ"
fi

# Wildcard matching
file="backup_2024-01-15.tar.gz"
if [[ "$file" == backup_*.tar.gz ]]; then
    echo "Là file backup"
fi
```

## 5. Vòng lặp

Vòng lặp giúp lặp lại một khối lệnh nhiều lần - xử lý hàng loạt file, đọc từng dòng dữ liệu, thử lại một thao tác...

### 5.1. Vòng lặp for

**Lặp qua danh sách:**

```bash
# Danh sách giá trị
for fruit in apple banana orange mango; do
    echo "Quả: $fruit"
done

# Lặp qua file trong thư mục
for file in /var/log/*.log; do
    echo "Xử lý: $file"
done

# Lặp qua đầu ra lệnh
for user in $(cat /etc/passwd | cut -d: -f1); do
    echo "User: $user"
done
```

**Dạng khoảng (range):**
 
```bash
for i in {1..5}; do
    echo "$i"                 # in 1 2 3 4 5
done
 
for i in {0..10..2}; do       # thêm bước nhảy: {đầu..cuối..bước}
    echo "$i"                 # in 0 2 4 6 8 10
done
```

**Vòng lặp số (C-style):**

```bash
# Tăng dần
for (( i=1; i<=5; i++ )); do
    echo "Lần $i"
done

# Bước nhảy
for (( i=0; i<=20; i+=5 )); do
    echo "$i"
done

# Dùng seq (linh hoạt hơn)
for i in $(seq 1 10); do echo $i; done
for i in $(seq 1 2 10); do echo $i; done    # bước nhảy 2: 1 3 5 7 9
for i in $(seq 10 -1 1); do echo $i; done   # đếm ngược
```

**Lặp qua file trong thư mục:**

```bash
for file in *.txt; do
    echo "Đang xử lý: $file"
done
```

**Lặp qua đầu ra của một lệnh:**

```bash
for user in $(cut -d: -f1 /etc/passwd); do
    echo "User: $user"
done
```

**Lặp qua phần tử của mảng:**

```bash
fruits=("táo" "cam" "xoài" "ổi")

for fruit in "${fruits[@]}"; do
    echo "$fruit"
done

# Lặp kèm chỉ số
for i in "${!fruits[@]}"; do
    echo "[$i] ${fruits[$i]}"
done
```

### 5.2. Vòng lặp while
 
`while` lặp trong khi điều kiện còn đúng:

```bash
count=1
while [ "$count" -le 5 ]; do
    echo "Lần $count"
    (( count++ ))             # nhớ cập nhật biến, nếu không sẽ lặp vô hạn!
done
```

:::warning Coi chừng lặp vô hạn
Nếu quên cập nhật biến điều kiện (`(( count++ ))` ở trên), vòng lặp sẽ chạy mãi mãi. Nhấn `Ctrl+C` để dừng khi lỡ gặp.
:::

**Ứng dụng quan trọng nhất - đọc file theo từng dòng:**

```bash
while IFS= read -r line; do
    echo "Dòng: $line"
done < input.txt
```

:::tip Giải thích `while IFS= read -r line`
- `IFS=` : tạm bỏ trống biến phân tách, để không cắt mất khoảng trắng đầu/cuối dòng.
- `-r` : không diễn giải dấu `\`, giữ nguyên nội dung thô.
- `< input.txt` : lấy nội dung file làm đầu vào cho vòng lặp.
Đây là cú pháp chuẩn để đọc file an toàn.
:::

### 5.3. Vòng lặp until

`until` ngược với `while`, nó lặp cho đến khi điều kiện trở thành đúng (tức lặp trong khi điều kiện còn sai):

```bash
count=1
until [ "$count" -gt 5 ]; do
    echo "Lần $count"
    (( count++ ))
done
```

Dùng `until` khi cách diễn đạt "lặp cho tới khi đạt được X" tự nhiên hơn "lặp trong khi chưa đạt X". Ví dụ: chờ tới khi một dịch vụ khởi động xong.

### 5.4. break và continue

```bash
# break: dừng khi gặp giá trị 5
for i in {1..10}; do
    if (( i == 5 )); then
        break
    fi
    echo "$i"                 # in 1 2 3 4
done

# continue: bỏ qua số chẵn
for i in {1..6}; do
    if (( i % 2 == 0 )); then
        continue
    fi
    echo "$i"                 # in 1 3 5
done
```

## 6. Hàm

### 6.1. Khai báo và gọi hàm

```bash
#!/bin/bash

# Cú pháp 1: phổ biến
greet() {
    echo "Xin chào, $1!"
}

# Cú pháp 2: dùng từ khóa function
function greet {
    echo "Xin chào, $1!"
}

# Gọi hàm: không dùng dấu ()
greet "Nam"          # In: Xin chào, Nam!
greet "Thế giới"
```

:::warning Định nghĩa trước, gọi sau
Shell đọc script tuần tự từ trên xuống. Hàm phải được định nghĩa trước dòng gọi nó, nếu không sẽ báo lỗi "command not found".
:::

### 6.2. Tham số và giá trị trả về

Trong hàm, `$1`, `$2`... là tham số của hàm (không phải của script):

```bash
add() {
    local a=$1
    local b=$2
    echo $((a + b))    # "trả về" qua stdout
}

# Lấy kết quả qua command substitution
result=$(add 10 20)
echo "Kết quả: $result"    # 30
```

**Trả về trạng thái (exit code):**

```bash
is_even() {
    if (( $1 % 2 == 0 )); then
        return 0    # true / thành công
    else
        return 1    # false / thất bại
    fi
}

if is_even 4; then
    echo "4 là số chẵn"
fi

is_even 7
echo "Exit code: $?"    # 1
```

### 6.3. Biến local

Luôn dùng `local` cho biến trong hàm để tránh ảnh hưởng ra ngoài:

```bash
count=100   # biến global

increment() {
    local count=0   # biến local - không ảnh hưởng global
    ((count++))
    echo "Trong hàm: $count"    # 1
}

increment
echo "Ngoài hàm: $count"        # 100 - không đổi
```

### 6.4. Các pattern hữu ích

**Hàm in log có màu:**

```bash
# Màu ANSI
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'   # No Color

log_info()    { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn()    { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error()   { echo -e "${RED}[ERROR]${NC} $*" >&2; }

log_info "Bắt đầu quá trình..."
log_warn "Bỏ qua file không tồn tại"
log_error "Không kết nối được database"
```

**Hàm kiểm tra lệnh tồn tại:**

```bash
require_cmd() {
    if ! command -v "$1" &>/dev/null; then
        log_error "Lệnh '$1' không tìm thấy. Hãy cài đặt trước."
        exit 1
    fi
}

require_cmd curl
require_cmd jq
require_cmd docker
```

**Hàm nhận input từ người dùng:**

```bash
ask_yes_no() {
    local question="$1"
    local answer
 
    while true; do
        read -r -p "$question [y/N] " answer
        case "${answer,,}" in    # ,, = chuyển sang lowercase
            y|yes) return 0 ;;
            n|no|"") return 1 ;;
            *) echo "Vui lòng nhập y hoặc n" ;;
        esac
    done
}

if ask_yes_no "Bạn có muốn tiếp tục?"; then
    echo "Tiếp tục..."
else
    echo "Đã hủy"
    exit 0
fi
```

### 6.5. Truyền mảng vào hàm

Bash không truyền mảng trực tiếp, cần dùng `nameref` (Bash 4.3+) hoặc truyền từng phần tử:

```bash
# Cách 1: truyền từng phần tử
print_array() {
    for item in "$@"; do
        echo "  - $item"
    done
}

fruits=("táo" "cam" "xoài")
print_array "${fruits[@]}"

# Cách 2: nameref (Bash 4.3+)
process_array() {
    local -n arr=$1    # arr là tham chiếu đến mảng truyền vào
    for item in "${arr[@]}"; do
        echo "$item"
    done
}

process_array fruits
```

## 7. Xử lý lỗi và debug

Script chạy được là một chuyện, script an toàn khi có sự cố lại là chuyện khác. Phần này giúp ta viết script biết dừng đúng lúc và dễ tìm lỗi

### 7.1. Exit status

Mỗi lệnh khi kết thúc đều trả về một mã trạng thái (exit status): `0` là thành công, khác `0` là có lỗi. Biến `$?` lưu mã của lệnh vừa chạy:

```bash
ls /thu_muc_khong_ton_tai
echo "$?"        # in ra khác 0 (ví dụ: 2) - lệnh thất bại
 
ls /home
echo "$?"        # 0 - lệnh thành công
```

Trong script, dùng `exit` để kết thúc sớm và trả mã cho lời gọi:
 
```bash
if [ ! -f "config.txt" ]; then
    echo "Lỗi: thiếu file config.txt" >&2    # ghi thông báo lỗi ra stderr
    exit 1                                   # thoát với mã lỗi
fi
```

:::tip Vì sao ghi lỗi ra stderr bằng `>&2`?
Ghi thông báo lỗi ra stderr thay vì stdout giúp lỗi không lẫn vào dữ liệu kết quả. Nhờ đó khi script được dùng trong pipe, phần dữ liệu vẫn sạch còn lỗi vẫn hiện ra màn hình.
:::

### 7.2. Các tùy chọn xử lý lỗi

Đặt các cờ này ngay sau shebang để script chặt chẽ hơn, bắt lỗi sớm thay vì âm thầm chạy sai:

```bash
#!/bin/bash
set -euo pipefail
```

| Tùy chọn | Ý nghĩa |
|---|---|
| `set -e` | Dừng script ngay khi có lệnh trả về exit code khác 0 |
| `set -u` | Báo lỗi khi dùng biến chưa khai báo (thay vì coi là rỗng) |
| `set -o pipefail` | Pipe trả về lỗi nếu bất kỳ lệnh nào trong pipe lỗi |
| `set -x` | In mỗi lệnh trước khi thực thi - dùng để debug |

```bash
set -e
false           # lệnh này trả về exit 1 → script dừng ngay
echo "Không bao giờ chạy đến đây"
```

```bash
set -u
echo $undefined_var   # lỗi: "unbound variable"
```

Tắt tạm thời:

```bash
set +e    # tắt tùy chọn -e
command_có_thể_lỗi || true    # hoặc cách này - bắt lỗi thủ công
set -e    # bật lại
```
 
### 7.3. Dọn dẹp và bắt tín hiệu

`trap` cho phép chạy một lệnh khi script gặp sự kiện nhất định (kết thúc, gặp lỗi, bị nhấn Ctrl+C...). Rất hữu ích để dọn dẹp tài nguyên:

```bash
#!/bin/bash
set -euo pipefail

# Hàm dọn dẹp - luôn chạy khi script thoát
cleanup() {
    echo "Dọn dẹp..."
    rm -f /tmp/script_$$.tmp
}
trap cleanup EXIT           # gọi cleanup khi thoát (dù thành công hay lỗi)

# Xử lý Ctrl+C
handle_interrupt() {
    echo "Nhận Ctrl+C - đang dừng..."
    exit 130
}
trap handle_interrupt INT

# Bắt lỗi
handle_error() {
    local line=$1
    echo "LỖI tại dòng $line" >&2
}
trap 'handle_error $LINENO' ERR

# --- Script chính ---
tmp_file="/tmp/script_$$.tmp"
touch "$tmp_file"
echo "Làm việc..." > "$tmp_file"
# cleanup() sẽ tự xóa $tmp_file khi script kết thúc
```

Các signal thường dùng với `trap`:

| Signal | Khi nào kích hoạt |
|---|---|
| `EXIT` | Script thoát (bất kỳ lý do gì) |
| `ERR` | Có lệnh trả về exit code khác 0 |
| `INT` | Người dùng nhấn Ctrl+C |
| `TERM` | Nhận lệnh `kill` |
| `HUP` | Terminal đóng |

### 7.4. Kỹ thuật debug

**Chạy script ở chế độ debug:**

```bash
bash -x script.sh           # in tất cả lệnh trước khi chạy
bash -v script.sh           # in từng dòng script khi đọc
bash -n script.sh           # kiểm tra cú pháp mà không chạy
```

**Debug một đoạn trong script:**

```bash
#!/bin/bash

echo "Bình thường"

set -x   # bật debug
result=$(complex_calculation)
echo "Kết quả: $result"
set +x   # tắt debug

echo "Tiếp tục bình thường"
```

**In thông tin debug có điều kiện:**

```bash
#!/bin/bash

DEBUG=${DEBUG:-0}   # mặc định tắt

debug() {
    if [ "$DEBUG" -eq 1 ]; then
        echo "[DEBUG] $*" >&2
    fi
}

debug "Bắt đầu xử lý file: $1"
# ...

# Gọi script với DEBUG=1 ./script.sh để xem log debug
```

### 7.5. Xử lý lỗi thực tế - Ví dụ tổng hợp

```bash
#!/bin/bash
set -euo pipefail

# --- Màu log ---
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
log_info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

# --- Dọn dẹp ---
TMPDIR=$(mktemp -d)
trap "rm -rf '$TMPDIR'" EXIT

# --- Kiểm tra môi trường ---
require_cmd() {
    command -v "$1" &>/dev/null || {
        log_error "Cần cài '$1' trước khi chạy script này"
        exit 1
    }
}
require_cmd curl
require_cmd jq

# --- Kiểm tra tham số ---
if [ "$#" -lt 1 ]; then
    log_error "Dùng: $0 <URL>"
    exit 1
fi

URL="$1"

# --- Xử lý chính ---
log_info "Tải dữ liệu từ $URL..."
response=$(curl -sf --max-time 30 "$URL") || {
    log_error "Không tải được dữ liệu từ $URL"
    exit 1
}

log_info "Xử lý JSON..."
echo "$response" | jq '.' > "$TMPDIR/output.json"

log_info "Hoàn tất! Kết quả lưu tại $TMPDIR/output.json"
cat "$TMPDIR/output.json"
```