# Makefile function

## Wildcard character

Wildcard character được dùng để thay thế nhiều tên tệp theo một pattern nhất định.

Một số wildcard character phổ biến:

| Ký tự     | Ý nghĩa                                       | Ví dụ |
| --------- | --------------------------------------------- | ----- |
| `*`	    | Khớp với bất kỳ chuỗi nào, kể cả chuỗi rỗng	| `*.c` sẽ khớp `main.c`, `syscalls.c` |
| `?`	    | Khớp với một ký tự bất kỳ	                    | `a?.txt` khớp `a1.txt`, `ab.txt`, không khớp `abc.txt` |
| `[...]`   | Khớp với một ký tự nằm trong tập chỉ định	    | `file[12].c` khớp `file1.c`, `file2.c` |

Wildcard character sẽ không hợp lệ nếu được sử dụng như sau:

```bash
objects = *.o
```

=> Lúc này, giá trị của biến objects sẽ là chuỗi `*.o`. Tuy nhiên, nếu sử dụng giá trị này của objects ở target hoặc prerequisite thì wildcard character sẽ xảy ra tại đây.

Để có thể thực hiện wildcard character khi định nghĩa variable, ta sử dụng:

```bash
objects := $(wildcard *.o)
```

Ta có thể thay đổi list các file source C thành list các file object bằng cách thay thế `.c` thành `.o` như sau:

```bash
$(patsubst %.c,%.o,$(wildcard *.c))
```

## Foreach function

Cú pháp của hàm foreach:

```bash
$(foreach var,list,text)
```

Duyệt lần lượt từng từ trong `list`, sau đó gán cho `var` và `text` sẽ được mở rộng.

Ví dụ biến files chứa danh sách tất cả các file trong các thư mục dirs:

```bash
dirs := a b c d 
files := $(foreach dir,$(dirs),$(wildcard $(dir)/*))
```

- Lần lặp lại đầu tiên thấy giá trị "a" => `$(wildcard a/*)`
- Lần lặp lại thứ hai thấy giá trị "b" => `$(wildcard b/*)`
- Lần lặp lại đầu tiên thấy giá trị "c" => `$(wildcard c/*)`

=> Kết quả của ví dụ này sẽ là:

```bash
files := $(wildcard a/*)  $(wildcard b/*) $(wildcard c/*)  $(wildcard d/*)
```

## Text function

- `$(subst from,to,text)`: Thực hiện thay thế text.

  Ví dụ:

  ```bash
  $(subst ee,EE,feet on the street)
  ```

  => tạo ra giá trị "fEEt on the strEEt".

- `$(patsubst pattern,replacement,text)`: Tìm các word được phân cách bằng space trong text khớp với pattern và thực hiện thay thế chung bằng replacement.
  
  Ví dụ:
  ```bash
  $(patsubst %.c,%.o,x.c.c bar.c)
  ```

  => tạo ra giá trị `x.c.o bar.o`.

**Substitution references**

Đây là cách đơn giản hơn tương tự với hàm `patsubst`. Cấu trúc:

```bash
$(var:pattern=replacement)
```

tương đương với

```bash
$(patsubst pattern,replacement,$(var))
```

Ví dụ, ta có một list các file object:

```bash
objects = foo.o bar.o baz.o
```

Để có list các file source tương ứng, ta chỉ cần viết:

```bash
$(objects:.o=.c)
```

thay vì sử dụng dạng nguyên thủy:

```bash
$(patsubst %.o,%.c,$(objects))
```