Trong C, khi muốn một hàm thay đổi được biến của nơi gọi, ta phải truyền con trỏ: nơi gọi viết `&x`, bên trong hàm viết `*p` ở mọi chỗ dùng và nếu cẩn thận thì cần phải kiểm tra con trỏ có bằng `NULL` không. Còn khi truyền một struct lớn theo giá trị, toàn bộ struct bị sao chép mỗi lần gọi hàm, tốn thời gian và bộ nhớ stack, đây là điều cần tránh trên thiết bị nhúng.

C++ giải quyết cả hai vấn đề bằng **tham chiếu** (reference). Đây là tính năng ta sẽ gặp ở gần như mọi hàm trong C++ hiện đại.

## Tham chiếu là gì?

Tham chiếu là một tên khác cho một biến đã có. Khai báo bằng cách thêm `&` sau tên kiểu. Mọi thao tác trên tham chiếu thực chất là thao tác trên biến gốc.

```cpp
int temperature = 25;
int& temp = temperature;   // temp là tên khác của temperature

temp = 30;
std::cout << temperature;  // in ra: 30

temperature = 40;
std::cout << temp;         // in ra: 40
```

Tham chiếu có ba quy tắc:

```cpp
int& ref;              // lỗi: tham chiếu bắt buộc phải khởi tạo ngay

int a = 1, b = 2;
int& r = a;
r = b;                 // Không làm r trỏ sang b, mà gán giá trị của b cho a
std::cout << a;        // in ra: 2

// Không có tham chiếu rỗng như con trỏ NULL
```

Nói cách khác: tham chiếu phải gắn với một biến ngay khi khai báo, gắn rồi thì không đổi sang biến khác được, và luôn luôn gắn với một biến có thật.

:::note Hai nghĩa của ký tự `&`
Đứng trong khai báo (`int& r`) thì là tham chiếu. Đứng trước một biến trong biểu thức (`&a`) thì là lấy địa chỉ như trong C.
:::

## So sánh tham chiếu với con trỏ

Cùng một việc, cách viết bằng con trỏ và tham chiếu khác nhau như sau:

```cpp
int value = 10;

// Con trỏ
int* p = &value;   // phải lấy địa chỉ
*p = 20;           // phải giải tham chiếu bằng *

// Tham chiếu
int& r = value;    // gắn trực tiếp
r = 20;            // dùng như biến bình thường
```

| | Con trỏ | Tham chiếu |
|---|---|---|
| Có thể rỗng (`nullptr`) | Có | Không |
| Đổi sang đối tượng khác | Được | Không |
| Bắt buộc khởi tạo | Không | Có |
| Cách dùng | Phải viết `*`, `->` | Như biến bình thường |

Tham chiếu an toàn và gọn hơn nhưng con trỏ vẫn cần thiết khi đối tượng có thể không tồn tại (giá trị `nullptr`) hoặc khi cần đổi sang trỏ vào đối tượng khác.

## Truyền tham trị

Đây là cách truyền mặc định, giống hệt C: hàm nhận một bản sao của đối số. Thay đổi bên trong hàm không ảnh hưởng tới biến gốc.

```cpp
void resetCounter(int counter)
{
    counter = 0;          // chỉ sửa bản sao
}

int errors = 5;
resetCounter(errors);
std::cout << errors;      // in ra: 5 (không đổi)
```

## Truyền tham chiếu

Khi thêm `&` vào tham số, hàm nhận chính biến gốc thay vì bản sao. Thay đổi bên trong hàm sẽ tác động trực tiếp lên biến của nơi gọi.

```cpp
void resetCounter(int& counter)
{
    counter = 0;          // sửa trực tiếp biến gốc
}

int errors = 5;
resetCounter(errors);     // gọi như bình thường, không cần &errors
std::cout << errors;      // in ra: 0
```

So với cách làm trong C, nơi gọi không phải viết `&`, bên trong hàm không phải viết `*` và không cần kiểm tra `NULL`.

Cách dùng phổ biến trong code nhúng là tham số đầu ra: hàm trả về `bool` để báo thành công hay thất bại, còn giá trị đọc được đưa ra qua tham chiếu.

```cpp
bool readTemperature(float& result)
{
    bool sensorOk = true;       // giả sử đọc cảm biến thành công
    if (!sensorOk) {
        return false;
    }
    result = 27.5f;
    return true;
}

float temp;
if (readTemperature(temp)) {
    std::cout << "Temperature: " << temp;   // in ra: Temperature: 27.5
}
```

## Truyền theo tham chiếu hằng (const&)

Với dữ liệu lớn, truyền theo giá trị rất lãng phí vì phải sao chép toàn bộ:

```cpp
struct Frame {
    uint8_t data[256];
    uint16_t length;
};

std::cout << sizeof(Frame);     // in ra: 258

void printFrame(Frame frame);   // mỗi lần gọi sao chép 258 byte lên stack
```

Truyền theo tham chiếu thì không sao chép nhưng lại cho phép hàm sửa dữ liệu, trong khi hàm `printFrame` chỉ cần đọc. Giải pháp là **tham chiếu hằng** `const&`: không sao chép, đồng thời compiler cấm hàm thay đổi dữ liệu.

```cpp
void printFrame(const Frame& frame)
{
    std::cout << "Length: " << frame.length << "\n";
    frame.length = 0;   // lỗi: không được sửa qua tham chiếu hằng
}
```

Nhìn vào khai báo `const Frame&`, người đọc code biết ngay hai điều: dữ liệu không bị sao chép và hàm cam kết không thay đổi nó.

Ngoài ra, tham chiếu hằng còn nhận được giá trị tạm thời như chuỗi ký tự viết trực tiếp. Ví dụ với `std::string`, lớp chuỗi của thư viện chuẩn C++:

```cpp
#include <string>

void showMessage(const std::string& msg)
{
    std::cout << msg << "\n";
}

std::string status = "Ready";
showMessage(status);          // in ra: Ready
showMessage("Sensor error");  // in ra: Sensor error (vẫn hợp lệ)
```

## Khi nào dùng cách truyền nào

| Tình huống | Cách truyền | Ví dụ |
|---|---|---|
| Kiểu nhỏ: `int`, `float`, `bool`, `char`, con trỏ, enum | Theo giá trị | `void setPin(int pin)` |
| Đối tượng lớn, hàm chỉ đọc | `const&` | `void send(const Frame& f)` |
| Hàm cần sửa đối số | `&` | `bool read(float& out)` |
| Đối số có thể không tồn tại | Con trỏ | `void attach(Sensor* s)`, cho phép truyền `nullptr` |

Với kiểu nhỏ, truyền theo giá trị không chậm hơn truyền tham chiếu vì bản chất tham chiếu cũng được thực hiện bằng một địa chỉ có kích thước tương đương. Vì vậy ta không viết `const int&` mà chỉ viết `int`.

## Trả về tham chiếu

Hàm cũng có thể trả về tham chiếu, cho phép nơi gọi truy cập trực tiếp vào dữ liệu mà không sao chép:

```cpp
uint8_t registers[16] = {0};

uint8_t& reg(int index)
{
    return registers[index];
}

reg(3) = 0xFF;                  // gán trực tiếp vào registers[3]
std::cout << int(registers[3]); // in ra: 255
```

:::warning Đối tượng trả về phải còn tồn tại
Đối tượng được trả về qua tham chiếu phải còn tồn tại sau khi hàm kết thúc, như biến toàn cục, biến `static`, hoặc thành viên của đối tượng. Tuyệt đối không trả về tham chiếu tới biến cục bộ.
:::

## Tham chiếu trong vòng lặp for theo phạm vi

C++ có cú pháp vòng lặp duyệt lần lượt từng phần tử của mảng, gọi là **range-based for**:

```cpp
int readings[4] = {20, 21, 22, 23};

for (int value : readings) {
    std::cout << value << " ";   // in ra: 20 21 22 23
}
```

Biến `value` là bản sao của từng phần tử. Muốn sửa phần tử, ta dùng tham chiếu:

```cpp
for (int& value : readings) {
    value += 1;                  // sửa trực tiếp trong mảng
}
// readings: 21 22 23 24
```

Kết hợp với `auto`, ta có ba cách viết thường gặp:

```cpp
for (auto x : list)         // bản sao, dùng cho kiểu nhỏ
for (auto& x : list)        // tham chiếu, khi cần sửa phần tử
for (const auto& x : list)  // tham chiếu hằng, khi chỉ đọc phần tử lớn
```

Cần nhớ rằng `auto` đơn thuần luôn tạo bản sao, kể cả khi vế phải là một tham chiếu. Muốn có tham chiếu thì phải viết rõ `auto&`:

```cpp
int& r = reg(3);
auto a = r;       // a là int, một bản sao
auto& b = r;      // b là int&, gắn với registers[3]
```

## Lỗi thường gặp

**Trả về tham chiếu tới biến cục bộ: `reference to local variable 'value' returned`**

```cpp
int& getValue()
{
    int value = 42;
    return value;     // cảnh báo
}

int& v = getValue();
std::cout << v;       // kết quả không xác định, có thể in số rác hoặc crash
```

`value` bị hủy ngay khi hàm kết thúc, nên tham chiếu trả về gắn với vùng nhớ không còn hợp lệ (gọi là **dangling reference**). Cách sửa là trả về theo giá trị:

```cpp
int getValue()
{
    int value = 42;
    return value;
}
```

**Truyền giá trị tạm thời vào tham chiếu không hằng: `cannot bind non-const lvalue reference of type 'std::string&' to an rvalue`**

```cpp
void showMessage(std::string& msg);

showMessage("Hello");   // lỗi
```

`"Hello"` tạo ra một đối tượng tạm thời. C++ không cho phép tham chiếu không hằng gắn với đối tượng tạm, vì mọi thay đổi lên nó sẽ biến mất ngay. Nếu hàm chỉ đọc, hãy đổi thành `const std::string&`.

**Hàm không có tác dụng vì quên `&`**

```cpp
void calibrate(float offset, float value)   // thiếu &
{
    value += offset;
}

float temp = 25.0f;
calibrate(1.5f, temp);
std::cout << temp;   // in ra: 25 (mong đợi 26.5)
```

Lỗi này không có cảnh báo nào vì code hoàn toàn hợp lệ. Cách sửa là khai báo `float& value`. Khi thấy một hàm "không có tác dụng", hãy kiểm tra cách truyền tham số trước tiên.

**Vòng lặp không sửa được phần tử**

```cpp
for (auto value : readings) {
    value = 0;        // chỉ sửa bản sao, mảng không thay đổi
}
```

Cách sửa là viết `for (auto& value : readings)`.
