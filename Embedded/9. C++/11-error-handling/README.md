Trong C, cách báo lỗi quen thuộc là trả về một giá trị đặc biệt (`-1`, `NULL`) kèm biến toàn cục `errno`:

```c
// C
int fd = open("/dev/ttyS1", O_RDWR);
if (fd < 0) {
    perror("open");
    return -1;
}
```

Cách này có ba nhược điểm. Rất dễ quên kiểm tra giá trị trả về, và compiler không nhắc gì. Giá trị báo lỗi lẫn với giá trị hợp lệ: một hàm đọc nhiệt độ không thể dùng `-1` để báo lỗi, vì -1 độ là nhiệt độ hợp lệ. Và khi lỗi xảy ra ở tầng sâu, mỗi tầng phía trên đều phải kiểm tra rồi chuyển tiếp lên, kèm theo việc dọn dẹp tài nguyên, thường dẫn tới các chuỗi `goto cleanup`.

C++ có **exception** để giải quyết các vấn đề này, nhưng exception lại có nhược điểm riêng, khiến nhiều dự án nhúng hạn chế dùng nó. Bài này trình bày cả exception lẫn các cách thay thế, để ta biết chọn cách phù hợp.

## Exception

Khi phát hiện lỗi, hàm ném (`throw`) một đối tượng exception. Chương trình lập tức dừng thực hiện các lệnh tiếp theo và nhảy tới khối bắt (`catch`) gần nhất phù hợp với loại exception đó.

```cpp
#include <stdexcept>

float readAdc(int channel)
{
    if (channel < 0 || channel > 6) {
        throw std::out_of_range("Invalid ADC channel");
    }
    return 0.9f;
}

try {
    float v = readAdc(9);
    std::cout << v;                   // không được thực hiện
} catch (const std::out_of_range& e) {
    std::cout << "Error: " << e.what() << "\n";
}
// in ra: Error: Invalid ADC channel
```

Khối `try` chứa đoạn code có thể phát sinh lỗi. Khối `catch` xử lý lỗi, với tham số là exception bị ném ra; hàm `what()` trả về thông báo lỗi. Exception luôn được bắt bằng tham chiếu hằng (`const ...&`), vì lý do sẽ nêu ở mục Lỗi thường gặp.

## Các loại exception chuẩn

Thư viện `<stdexcept>` định nghĩa sẵn các loại exception, tất cả đều kế thừa từ `std::exception`:

| Loại | Dùng khi |
|---|---|
| `std::runtime_error` | Lỗi chỉ phát hiện được lúc chạy (mất kết nối, đọc file thất bại) |
| `std::invalid_argument` | Tham số không hợp lệ |
| `std::out_of_range` | Chỉ số hoặc giá trị nằm ngoài phạm vi |
| `std::bad_alloc` | `new` không cấp phát được bộ nhớ (trong `<new>`) |

Một số hàm của thư viện chuẩn tự ném exception, và ta cần biết để xử lý:

```cpp
std::vector<int> v = {1, 2, 3};
v.at(10);                     // ném std::out_of_range (Bài C8)

int baud = std::stoi("abc");  // chuyển chuỗi thành số: ném std::invalid_argument
```

Có thể viết nhiều khối `catch` cho nhiều loại exception. Vì các exception kế thừa nhau, khối `catch` bắt lớp cha sẽ bắt được cả lớp con (nhờ đa hình ở Bài C6), nên ta đặt lớp con lên trước:

```cpp
try {
    // ...
} catch (const std::out_of_range& e) {
    // xử lý riêng lỗi vượt phạm vi
} catch (const std::exception& e) {
    // mọi exception chuẩn khác
} catch (...) {
    // bất kỳ thứ gì bị ném ra, kể cả không phải exception chuẩn
}
```

## Exception lan truyền qua nhiều tầng

Nếu hàm ném exception không tự bắt, exception lan lên hàm gọi nó, rồi tiếp tục lên trên cho tới khi gặp khối `catch` phù hợp. Trên đường đi, mọi đối tượng cục bộ ở các tầng đều bị hủy đúng thứ tự, tức là destructor của chúng được gọi. Quá trình này gọi là **stack unwinding**.

```cpp
void level2()
{
    Tracer t("level 2");
    throw std::runtime_error("Sensor disconnected");
}

void level1()
{
    Tracer t("level 1");
    level2();
    std::cout << "Never reached\n";
}

try {
    level1();
} catch (const std::exception& e) {
    std::cout << "Caught: " << e.what() << "\n";
}

// in ra:
// Create level 1
// Create level 2
// Destroy level 2
// Destroy level 1
// Caught: Sensor disconnected
```

Đây là lý do RAII (Bài C4) và exception đi đôi với nhau: nếu mọi tài nguyên đều được giữ bởi đối tượng có destructor (smart pointer, class bọc file...), thì dù exception bay qua bao nhiêu tầng, tài nguyên vẫn được giải phóng đúng. Không cần `goto cleanup`.

Nếu exception lan tới tận `main()` mà không có `catch` nào, chương trình bị dừng ngay:

```
terminate called after throwing an instance of 'std::runtime_error'
  what():  Sensor disconnected
Aborted (core dumped)
```

## noexcept

Từ khóa `noexcept` khai báo rằng hàm không bao giờ ném exception. Nếu hàm vẫn ném, chương trình bị dừng ngay thay vì lan truyền exception.

```cpp
float toVoltage(uint16_t raw) noexcept
{
    return raw * 1.8f / 4095;
}
```

`noexcept` giúp người đọc và compiler biết hàm an toàn, đồng thời cho phép compiler tối ưu tốt hơn.

:::warning Destructor không được ném exception
Destructor mặc định là `noexcept`. Nếu destructor ném exception trong lúc stack unwinding đang xử lý một exception khác, chương trình bị dừng ngay lập tức.
:::

## Vì sao exception bị hạn chế trong nhúng

Exception là cơ chế mạnh, nhưng trong bối cảnh nhúng có nhiều lý do để tránh:

- **Tăng kích thước chương trình.** Compiler phải sinh thêm bảng thông tin để thực hiện stack unwinding.
- **Thời gian xử lý khó đoán trước.** Việc ném và bắt exception chậm hơn nhiều so với trả về mã lỗi, và thời gian phụ thuộc vào số tầng phải đi qua. Với code cần đáp ứng thời gian thực, đây là điểm trừ lớn.
- **Luồng điều khiển bị ẩn.** Nhìn vào một dòng gọi hàm, ta không biết nó có thể ném exception hay không, nên khó biết chương trình có thể thoát ra ở những điểm nào.

Vì vậy nhiều dự án nhúng biên dịch với tùy chọn `-fno-exceptions`, tắt hẳn exception. Khi đó lệnh `throw` gây lỗi biên dịch, và các hàm thư viện chuẩn lẽ ra ném exception sẽ dừng chương trình thay vào đó.

Trong chuỗi bài này, ta báo lỗi bằng mã lỗi, `std::optional` hoặc `bool`, như các mục tiếp theo. Exception chỉ xuất hiện khi dùng các hàm thư viện chuẩn có ném exception, và ta bắt nó ngay tại chỗ.

## Mã lỗi với enum class và [[nodiscard]]

Cách thay thế phổ biến nhất là trả về mã lỗi. Kết hợp `enum class` (Bài C7) để có danh sách lỗi rõ ràng, và tham số tham chiếu (Bài C2) để đưa giá trị ra ngoài:

```cpp
enum class [[nodiscard]] Error {
    None,
    NotConnected,
    Timeout,
    InvalidData,
};

Error readTemperature(float& out)
{
    bool connected = true;          // giả lập
    if (!connected) {
        return Error::NotConnected;
    }
    out = 27.5f;
    return Error::None;
}
```

Thuộc tính `[[nodiscard]]` (C++17) đặt trên kiểu `Error` giải quyết nhược điểm lớn nhất của mã lỗi kiểu C: mọi hàm trả về `Error` mà bị bỏ qua giá trị trả về đều bị compiler cảnh báo.

```cpp
float t;
readTemperature(t);
// cảnh báo: ignoring returned value of type 'Error', declared with attribute 'nodiscard'

if (readTemperature(t) != Error::None) {    // đúng: có kiểm tra
    std::cout << "Failed to read temperature\n";
}
```

`[[nodiscard]]` cũng có thể đặt trên từng hàm riêng lẻ, ví dụ hàm trả về `bool` báo thành công hay thất bại: `[[nodiscard]] bool open();`.

Để hiện thông báo lỗi dễ đọc, ta viết thêm một hàm chuyển mã lỗi thành chuỗi. Nhờ cảnh báo thiếu trường hợp của `switch` (Bài C7), khi thêm mã lỗi mới, compiler nhắc ta cập nhật hàm này:

```cpp
const char* toString(Error e)
{
    switch (e) {
    case Error::None:         return "No error";
    case Error::NotConnected: return "Disconnected";
    case Error::Timeout:      return "Timeout";
    case Error::InvalidData:  return "Invalid data";
    }
    return "Unknown error";
}
```

## std::optional: có giá trị hoặc không

Nhiều hàm chỉ cần báo "có kết quả" hoặc "không có kết quả", không cần biết lý do chi tiết. `std::optional<T>` (C++17, trong `<optional>`) biểu diễn đúng điều này: nó hoặc chứa một giá trị kiểu `T`, hoặc rỗng.

Ví dụ phân tích dòng dữ liệu của cảm biến DS18B20 ở Bài C6:

```cpp
#include <optional>
#include <cstdlib>

std::optional<float> parseTemperature(const std::string& line)
{
    auto pos = line.find("t=");
    if (pos == std::string::npos) {
        return std::nullopt;                      // không có giá trị
    }
    long milli = std::strtol(line.c_str() + pos + 2, nullptr, 10);
    return milli / 1000.0f;                       // có giá trị
}
```

Nơi gọi kiểm tra như con trỏ, và lấy giá trị bằng `*`:

```cpp
auto t = parseTemperature("4b 01 4b 46 7f ff 05 10 e1 t=23125");
if (t) {
    std::cout << *t;                    // in ra: 23.125
}

auto bad = parseTemperature("crc=e1 NO");
std::cout << bad.has_value();           // in ra: 0
std::cout << bad.value_or(-999.0f);     // in ra: -999 (giá trị thay thế khi rỗng)
```

So với cách trả về `bool` kèm tham số đầu ra, `optional` gọn hơn: kết quả và trạng thái nằm trong cùng một giá trị trả về, nên không thể dùng nhầm giá trị khi hàm thất bại mà không kiểm tra.

## Kết hợp giá trị và mã lỗi

Khi vừa cần giá trị vừa cần biết lý do lỗi, ta gom cả hai vào một struct:

```cpp
struct TempResult {
    Error error = Error::None;
    float value = 0.0f;

    bool ok() const { return error == Error::None; }
};

[[nodiscard]] TempResult readTemperature();

auto r = readTemperature();
if (!r.ok()) {
    std::cout << "Error: " << toString(r.error) << "\n";
    return;
}
std::cout << r.value;
```

C++23 có sẵn `std::expected<T, E>` làm đúng việc này một cách tổng quát. Vì chuỗi bài dùng C++17, ta tự viết struct như trên.

## assert: bắt lỗi lập trình

Cần phân biệt hai loại lỗi:

- **Lỗi lúc chạy**: có thể xảy ra ngay cả khi code đúng, như cảm biến bị rút ra, UART hết thời gian chờ, người dùng nhập sai. Loại này phải được xử lý bằng mã lỗi hoặc `optional` như các mục trên.
- **Lỗi lập trình**: chỉ xảy ra khi code có bug, như truyền `nullptr` vào hàm yêu cầu con trỏ hợp lệ, hay gọi hàm với tham số mà logic chương trình đảm bảo không bao giờ sai.

Với loại thứ hai, ta dùng `assert` (trong `<cassert>`). Nếu điều kiện sai, chương trình dừng ngay và in ra vị trí lỗi:

```cpp
#include <cassert>

void setDutyCycle(int percent)
{
    assert(percent >= 0 && percent <= 100);
    // ...
}

setDutyCycle(150);
// main: main.cpp:5: void setDutyCycle(int): Assertion `percent >= 0 && percent <= 100' failed.
// Aborted (core dumped)
```

:::note assert bị loại bỏ ở bản release
Khi biên dịch bản release với `-DNDEBUG`, mọi `assert` bị loại bỏ hoàn toàn, không tốn chi phí. Vì vậy `assert` giúp phát hiện bug sớm trong lúc phát triển, nhưng không thay thế được việc kiểm tra lỗi lúc chạy.
:::

Nếu giá trị `percent` đến từ thanh trượt trên giao diện hay từ khung dữ liệu UART, ta phải kiểm tra bằng `if` và xử lý, không dùng `assert`.

## Chọn cách xử lý lỗi nào

| Tình huống | Cách xử lý |
|---|---|
| Hàm có thể không có kết quả, không cần biết lý do | `std::optional<T>` |
| Cần biết lý do lỗi | Mã lỗi `enum class` với `[[nodiscard]]`, hoặc struct kết quả |
| Chỉ cần thành công/thất bại | `[[nodiscard]] bool` |
| Lỗi do bug, không bao giờ xảy ra nếu code đúng | `assert` |
| Gọi hàm thư viện chuẩn có ném exception | Bắt bằng `try`/`catch` ngay tại chỗ, hoặc dùng hàm thay thế không ném |

## Lỗi thường gặp

**Thất bại khó hiểu ở một chỗ cách xa nguyên nhân do bỏ qua giá trị trả về báo lỗi**

```cpp
bool openPort();

openPort();          // không kiểm tra, chương trình tiếp tục như thể đã mở được cổng
sendCommand();       // thất bại khó hiểu ở đây, cách xa nguyên nhân thật
```

Cách phòng tránh là đánh dấu `[[nodiscard]]` cho hàm hoặc cho kiểu mã lỗi, để compiler cảnh báo mọi chỗ bỏ qua kết quả.

**Chương trình dừng với `terminate called after throwing an instance of 'std::invalid_argument'`**

```cpp
std::string input = receiveFromUart();   // nhận được "11520O" do nhiễu
int baud = std::stoi(input);             // "11520" hợp lệ, nhưng...
int value = std::stoi("abc");
// terminate called after throwing an instance of 'std::invalid_argument'
//   what():  stoi
```

Với dữ liệu đến từ bên ngoài, lỗi định dạng là chuyện bình thường, không nên để chúng làm dừng ứng dụng. Hoặc bắt exception ngay tại chỗ, hoặc dùng hàm không ném exception như `std::strtol` (kiểm tra con trỏ kết thúc) hay `std::from_chars` (C++17, trong `<charconv>`).

**`what()` chỉ in ra `std::exception` do bắt exception theo giá trị**

```cpp
try {
    throw std::out_of_range("Invalid ADC channel");
} catch (std::exception e) {       // bắt theo giá trị
    std::cout << e.what();         // in ra: std::exception (mất thông báo gốc)
}
```

Đây là object slicing ở Bài C5. Exception bị sao chép vào biến kiểu lớp cha, mất phần của lớp con. Luôn bắt bằng `const std::exception& e`.

**Dùng giá trị của `optional` rỗng: ném `std::bad_optional_access` hoặc hành vi không xác định**

```cpp
auto t = parseTemperature("crc=e1 NO");
std::cout << *t;          // hành vi không xác định: đọc giá trị không tồn tại
std::cout << t.value();   // ném std::bad_optional_access
```

Luôn kiểm tra `if (t)` trước khi dùng `*t`, hoặc dùng `value_or()` để có giá trị thay thế.

**Chạy đúng khi debug nhưng hỏng ở bản release do đặt code có tác dụng phụ bên trong `assert`**

```cpp
assert(port.open());     // bản debug: mở cổng và kiểm tra
                         // bản release: cả dòng bị xóa, cổng không bao giờ được mở
```

Lỗi này rất khó tìm ra nguyên nhân vì chỉ xuất hiện ở bản release trên board. Cách sửa là tách lời gọi hàm ra khỏi `assert`:

```cpp
bool opened = port.open();
assert(opened);
```

Tuy nhiên, việc mở cổng thất bại là lỗi lúc chạy (cổng có thể đang bị chương trình khác chiếm), nên cách đúng hơn nữa là kiểm tra bằng `if` và xử lý.

**Chương trình dừng ngay do ném exception từ destructor**

```cpp
SerialPort::~SerialPort()
{
    if (!close()) {
        throw std::runtime_error("Failed to close port");   // chương trình bị dừng ngay
    }
}
```

Destructor là `noexcept` mặc định, nên exception ném ra từ đây làm chương trình dừng lập tức. Trong destructor, nếu thao tác dọn dẹp thất bại, chỉ nên ghi log và bỏ qua.
