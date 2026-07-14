## 1. Shell script là gì?

Shell script là một file văn bản chứa chuỗi các lệnh shell (thường là Bash) được thực thi tuần tự. Mục đích chính là tự động hóa công việc lặp đi lặp lại — từ backup file, deploy ứng dụng, đến cấu hình server.

Script đơn giản nhất:

```bash
#!/bin/bash

echo "Xin chào, $USER!"
```

Lưu thành `hello.sh`, sau đó chạy:

```bash
chmod +x hello.sh   # cấp quyền thực thi (chỉ cần 1 lần)
./hello.sh          # chạy script
```

## 1.1. Shebang

```bash
#!/bin/bash
```

Dòng shebang phải là phải là dòng đầu tiên, không được có khoảng trắng hay dòng trống phía trước. Nó báo cho hệ điều hành biết dùng chương trình nào để thực thi file này.

2 kiểu shebang phổ biến:

| Shebang | Khi nào dùng |
|---|---|
| `#!/bin/bash` | Chỉ định chính xác đường dẫn đến bash |
| `#!/usr/bin/env bash` | Tìm bash theo `$PATH` — linh hoạt hơn, **khuyến nghị dùng khi chia sẻ script** |

## 1.2. Các cách chạy script

Cách 1: chạy trực tiếp (cần quyền thực thi + đường dẫn)

```bash
chmod +x script.sh  # cấp quyền thực thi (chỉ cần làm 1 lần)
./script.sh
```

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

:::tip Khi nào dùng `source`?
Khi ta muốn script thay đổi môi trường shell hiện tại. Ví dụ: file cấu hình `.bashrc`
:::

## 2. Variable

### 2.1. Khai báo và truy xuất

Quy tắc đặt tên: Tên biến có thể chứa chữ, số, dấu gạch dưới, nhưng không được bắt đầu bằng số:

```bash
my_var="ok"      # valid
2var="loi"       # invalid
_hidden="ok"     # valid
```

:::warning Chú ý
Không sử dụng các ký tự `! * - @ $ ...` để đặt tên biến vì chúng có có ý nghĩa đặc biệt với shell.
:::

:::warning Quy tắc quan trọng
Không được có khoảng trắng trước/sau dấu `=`. Shell sẽ hiểu nhầm thành lệnh nếu ta thêm khoảng trắng.

```bash
name="Nam"          # Đúng - không có khoảng trắng quanh dấu =
name = "Nam"        # Sai - sẽ báo lỗi "command not found"
```
:::

Để truy xuất giá trị biến ta dùng dấu `$` trước tên biến:

```bash
name="Nam"
echo $name
echo "${name}"     # cách viết an toàn hơn, nên dùng khi biến đứng cạnh chữ khác
echo "Xin chao ${name}oi"   # nếu không có {}, shell sẽ tìm biến $nameoi (không tồn tại)
```

Các thao tác khác:

```bash
readonly PI=3.14    # Biến chỉ đọc, giá trị của nó không thể thay đổi
PI=3.14159          # lỗi: "PI: readonly variable"

unset name              # hủy biến
echo $name              # in ra rỗng
```

### 2.2. Các loại biến

**a) Biến local và biến environment**

```bash
# Biến thường - chỉ tồn tại trong shell hiện tại
my_var="hello"

# Biến môi trường - export ra để các tiến trình con cũng dùng được
export PATH_CUSTOM="/opt/myapp"
```

**b) Biến hệ thống có sẵn**

```bash
echo $HOME     # thư mục home của user
echo $USER     # tên user hiện tại
echo $PATH     # danh sách đường dẫn tìm lệnh
echo $PWD      # thư mục hiện tại
echo $SHELL    # shell đang dùng
echo $0        # tên script
```

**c) Biến đặc biệt**

Như đã đề cập ở trên, chúng ta không thể sử dụng các ký tự đặc biệt như `! @ $ ...` để đặt tên cho biến. Điều này là do các ký tự đó được sử dụng trong các biến đặc biệt của Linux. Các biến này được dành riêng cho các chức năng cụ thể:

| Biến | Ý nghĩa |
| --- | --- |
| `$0` | Tên tệp của lệnh/tập lệnh hiện tại |
| `$n` | Các biến này tương ứng với các đối số truyền vào, n là số nguyên dương. Ví dụ: `./test.sh a b` thì đối số `$1`, `$2` lần lượt là a và b |
| `$#` | Số lượng đối số truyền vào. Ví dụ: `./test.sh a b` sẽ có 2 đối số |
| `$?` | Trạng thái thoát ra của lệnh trước được chạy, thường là 0 đại diện cho lệnh trước chạy thành công, khác 0 là failed. Max range [0 – 255] |
| `$$` | PID của shell hiện tại. Đối với shell script thì đây là số PID mà nó đang chạy |
| `$!` | Process number của lệnh background cuối cùng |
| `$*` | Chứa tất cả các đối số truyền vào. Nếu có 3 đối số truyền vào thì giá trị sẽ là `$1 $2 $3` khi sử dụng |
| `$@` | Chứa tất cả các đối số truyền vào nhưng phân tách thành các đối số riêng lẻ không như `$*` |

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

Ta sẽ thấy variable `$*` và `$@` khá giống nhau. Về cơ bản thì 2 biến này hoạt động giống nhau, trừ khi chúng được đặt trong dấu "":
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

### 2.3. Command Substitution (Gán kết quả lệnh vào biến)

Cú pháp `$(...)` thực thi lệnh bên trong và lấy output gán vào biến:

```bash
current_date=$(date +%Y-%m-%d)
file_count=$(ls | wc -l)
os_name=$(uname -s)
 
echo "Ngày hôm nay: $current_date"
echo "Số file trong thư mục: $file_count"
 
# Lồng nhau được — nhưng hạn chế dùng vì khó đọc
top_dir=$(ls $(pwd) | head -1)
```

:::warning Lưu ý
Cú pháp cũ dùng backtick `` `command` `` vẫn hoạt động nhưng không khuyến nghị vì khó lồng nhau và khó đọc.
:::

### 2.4. Parameter Expansion (Giá trị mặc định cho biến)

```bash
name=""
 
# Dùng giá trị mặc định nếu biến rỗng hoặc chưa khai báo
echo "${name:-Guest}"        # in "Guest", $name vẫn rỗng
 
# Gán luôn giá trị mặc định vào biến nếu chưa có
echo "${name:=Guest}"        # $name = "Guest" từ đây
 
# Báo lỗi và thoát nếu biến rỗng
echo "${name:?'Thiếu tên!'}" # in lỗi ra stderr và thoát script
 
# Dùng giá trị thay thế chỉ khi biến đã có giá trị
echo "${name:+Hello}"        # in "Hello" nếu $name không rỗng
```

Ứng dụng thực tế — kiểm tra biến môi trường bắt buộc:
 
```bash
#!/bin/bash
DB_HOST="${DB_HOST:?'Lỗi: DB_HOST chưa được cấu hình!'}"
DB_PORT="${DB_PORT:-5432}"   # mặc định là 5432 nếu không truyền vào
```

### 2.5. String manipulation (Cắt chuỗi trong biến)

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
 
Xử lý đường dẫn file — rất hay dùng trong script:
 
```bash
path="/home/user/file.txt"
 
echo "${path##*/}"    # lấy tên file (xóa prefix dài nhất đến /) → "file.txt"
echo "${path%/*}"     # lấy thư mục cha (xóa suffix từ / cuối) → "/home/user"
echo "${path%.txt}"   # bỏ đuôi .txt → "/home/user/file"
echo "${path##*.}"    # lấy extension → "txt"
```

## 3. Basic operators

### 3.1. Arithmetic operators (Toán tử số học)

Bash không tự nhận dạng phép tính trong chuỗi thông thường. Phải dùng một trong các cú pháp sau:

```bash
a=10
b=3
 
# Cách 1: $(( )) — khuyến nghị, nhanh và dễ đọc
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
 
# Cách 3: expr (cũ, ít dùng — lưu ý phải có khoảng trắng)
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

Cách viết khác dùng `(( ))` với ký hiệu toán học quen thuộc như `<`, `>`, `==`, `!=`:

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
Các toán tử trên không hoạt động cho các giá trị chuỗi, shell chỉ hỗ trợ chúng cho các giá trị số.
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

:::warning `[ ]` vs `[[ ]]`
- `[ ]` là POSIX: tương thích mọi shell, nhưng cần quote biến cẩn thận
- `[[ ]]` là Bash extension: hỗ trợ regex, pattern, so sánh chuỗi an toàn hơn
- Nếu dùng `#!/bin/bash`, ưu tiên `[[ ]]`
:::

### 3.4. Logical operators (Toán tử logic)

| Toán tử | Ý nghĩa |
| --- | --- |
| `!` | NOT |
| `||` | OR |
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

### 4.1. if / elif / else
 
```bash
#!/bin/bash
 
score=$1
 
if [ -z "$score" ]; then
    echo "Dùng: $0 <điểm>"
    exit 1
fi
 
if [ "$score" -ge 90 ]; then
    echo "Xuất sắc"
elif [ "$score" -ge 75 ]; then
    echo "Khá"
elif [ "$score" -ge 50 ]; then
    echo "Trung bình"
else
    echo "Yếu"
fi
```
 
Một dòng (inline) — thường dùng để kiểm tra nhanh:
 
```bash
[ -f "config.cfg" ] && echo "Config tồn tại" || echo "Thiếu config"
 
# Hoặc rõ ràng hơn
if [ -f "config.cfg" ]; then echo "Config tồn tại"; fi
```
 
---
 
### 4.2. case
 
`case` phù hợp khi cần so sánh một biến với nhiều giá trị — thay thế chuỗi `if/elif` dài.
 
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
 
Dùng case để xử lý tham số script:
 
```bash
#!/bin/bash
 
case "$1" in
    start)
        echo "Khởi động service..."
        ;;
    stop)
        echo "Dừng service..."
        ;;
    restart)
        $0 stop
        $0 start
        ;;
    status)
        echo "Đang kiểm tra..."
        ;;
    *)
        echo "Dùng: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac
```
 
---
 
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
 
---
 
## 5. Vòng lặp
 
### 5.1. for
 
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
 
**Lặp qua mảng:**
 
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
 
---
 
### 5.2. while
 
```bash
# Lặp khi điều kiện đúng
count=1
while [ $count -le 5 ]; do
    echo "Lần $count"
    ((count++))
done
 
# Đọc file từng dòng — cách làm chuẩn
while IFS= read -r line; do
    echo "Dòng: $line"
done < input.txt
 
# Đọc kết quả lệnh từng dòng
while IFS= read -r line; do
    echo "Log: $line"
done < <(grep "ERROR" /var/log/app.log)
 
# Vòng lặp vô tận — dùng khi cần chờ sự kiện
while true; do
    if ping -c 1 google.com &>/dev/null; then
        echo "Đã có mạng"
        break
    fi
    echo "Chờ kết nối..."
    sleep 5
done
```
 
---
 
### 5.3. until
 
Ngược với `while` — lặp **cho đến khi** điều kiện đúng:
 
```bash
count=1
until [ $count -gt 5 ]; do
    echo "Lần $count"
    ((count++))
done
```
 
---
 
### 5.4. Điều khiển vòng lặp
 
```bash
# break — thoát khỏi vòng lặp
for i in {1..10}; do
    if [ $i -eq 5 ]; then
        echo "Dừng tại $i"
        break
    fi
    echo $i
done
 
# continue — bỏ qua lần lặp hiện tại
for i in {1..10}; do
    if (( i % 2 == 0 )); then
        continue    # bỏ qua số chẵn
    fi
    echo "$i"       # chỉ in số lẻ
done
 
# break/continue với vòng lặp lồng nhau — chỉ định tầng
for i in {1..3}; do
    for j in {1..3}; do
        if [ $j -eq 2 ]; then
            break 2    # thoát cả 2 vòng lặp
        fi
        echo "$i $j"
    done
done
```
 
---
 
### 5.5. Mảng (Array)
 
```bash
# Khai báo
fruits=("táo" "cam" "xoài")
nums=(1 2 3 4 5)
 
# Truy xuất
echo "${fruits[0]}"         # phần tử đầu: táo
echo "${fruits[-1]}"        # phần tử cuối: xoài
echo "${fruits[@]}"         # tất cả phần tử
echo "${#fruits[@]}"        # số phần tử: 3
echo "${fruits[@]:1:2}"     # cắt: lấy 2 phần tử từ vị trí 1 → "cam xoài"
 
# Thêm phần tử
fruits+=("ổi")
fruits[10]="nho"            # gán vị trí bất kỳ (sparse array)
 
# Xóa phần tử
unset fruits[1]             # xóa phần tử vị trí 1
 
# Associative array (Bash 4+)
declare -A person
person[name]="Nam"
person[age]=25
person[city]="Hà Nội"
 
echo "${person[name]}"      # Nam
echo "${!person[@]}"        # in tất cả key: name age city
echo "${person[@]}"         # in tất cả value
```
 
---
 
## 6. Hàm (Function)
 
### 6.1. Khai báo và gọi hàm
 
```bash
#!/bin/bash
 
# Cú pháp 1 — phổ biến
greet() {
    echo "Xin chào, $1!"
}
 
# Cú pháp 2 — dùng từ khóa function
function greet {
    echo "Xin chào, $1!"
}
 
# Gọi hàm — không dùng dấu ()
greet "Nam"          # In: Xin chào, Nam!
greet "Thế giới"
```
 
> ⚠️ Hàm phải được **khai báo trước khi gọi** trong script.
 
---
 
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
 
**Trả về trạng thái (exit code)** — dùng `return`:
 
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
 
---
 
### 6.3. Biến local
 
**Luôn dùng `local` cho biến trong hàm** để tránh ảnh hưởng ra ngoài:
 
```bash
count=100   # biến global
 
increment() {
    local count=0   # biến local — không ảnh hưởng global
    ((count++))
    echo "Trong hàm: $count"    # 1
}
 
increment
echo "Ngoài hàm: $count"        # 100 — không đổi
```
 
---
 
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
 
---
 
### 6.5. Truyền mảng vào hàm
 
Bash không truyền mảng trực tiếp — cần dùng nameref (Bash 4.3+) hoặc truyền từng phần tử:
 
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
 
---
 
## 7. Xử lý lỗi và Debug
 
### 7.1. Exit code
 
Mọi lệnh đều trả về exit code sau khi chạy:
 
```bash
ls /tmp
echo "Exit code: $?"    # 0 — thành công
 
ls /không-tồn-tại
echo "Exit code: $?"    # 2 — lỗi
 
# Quy ước:
# 0       = thành công
# 1       = lỗi chung
# 2       = lỗi dùng sai lệnh / thiếu file
# 126     = không có quyền thực thi
# 127     = lệnh không tìm thấy
# 128+N   = thoát do signal N (ví dụ: 130 = Ctrl+C)
```
 
Đặt exit code cho script:
 
```bash
#!/bin/bash
 
process() {
    # ... làm gì đó ...
    return 0
}
 
if ! process; then
    echo "Lỗi xử lý" >&2
    exit 1
fi
 
exit 0  # thành công
```
 
---
 
### 7.2. Các tùy chọn xử lý lỗi — `set`
 
Đặt các tùy chọn này ở đầu script để tránh lỗi âm thầm:
 
```bash
#!/bin/bash
set -euo pipefail
```
 
| Tùy chọn | Ý nghĩa |
|---|---|
| `set -e` | Dừng script ngay khi có lệnh trả về exit code khác 0 |
| `set -u` | Báo lỗi khi dùng biến chưa khai báo (thay vì coi là rỗng) |
| `set -o pipefail` | Pipe trả về lỗi nếu bất kỳ lệnh nào trong pipe lỗi |
| `set -x` | In mỗi lệnh trước khi thực thi — dùng để debug |
 
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
command_có_thể_lỗi || true    # hoặc cách này — bắt lỗi thủ công
set -e    # bật lại
```
 
---
 
### 7.3. trap — Bẫy lỗi và dọn dẹp
 
`trap` đăng ký hàm được gọi khi script nhận signal hoặc thoát:
 
```bash
#!/bin/bash
set -euo pipefail
 
# Hàm dọn dẹp — luôn chạy khi script thoát
cleanup() {
    echo "Dọn dẹp..."
    rm -f /tmp/script_$$.tmp
}
trap cleanup EXIT           # gọi cleanup khi thoát (dù thành công hay lỗi)
 
# Xử lý Ctrl+C
handle_interrupt() {
    echo "Nhận Ctrl+C — đang dừng..."
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
 
---
 
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
 
---
 
### 7.5. Xử lý lỗi thực tế — Ví dụ tổng hợp
 
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