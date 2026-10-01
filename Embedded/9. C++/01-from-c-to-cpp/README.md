C++ được xây dựng dựa trên C nên phần lớn code C mà ta đã viết vẫn biên dịch được bằng compiler C++. Tuy nhiên, khi dự án lớn dần, C bộc lộ nhiều bất tiện: tên hàm dễ trùng nhau nên phải thêm tiền tố dài dòng (`uart_read`, `i2c_read`, `sensor_read`...), hằng số khai báo bằng `#define` không có kiểu dữ liệu, mỗi kiểu tham số lại cần một hàm có tên khác nhau. Trong bài này, ta sẽ xem C++ giải quyết những vấn đề đó bằng các tính năng cơ bản nhất.

File C++ có đuôi `.cpp` và được biên dịch bằng `g++` thay cho `gcc`:

```bash
g++ -std=c++17 -Wall -Wextra main.cpp -o main
```

## In ra màn hình với iostream

Trong C, ta dùng `printf` và phải tự ghi định dạng (`%d`, `%f`...) khớp với kiểu dữ liệu. C++ cung cấp `std::cout`, nó tự nhận biết kiểu dữ liệu nên không cần chuỗi định dạng. Toán tử `<<` dùng để đẩy dữ liệu ra màn hình và có thể nối nhiều giá trị liên tiếp.

```cpp
#include <iostream>

int main()
{
    int temperature = 27;
    float voltage = 3.3f;

    std::cout << "Temperature: " << temperature << " degC\n";
    std::cout << "Voltage: " << voltage << " V\n";
    // in ra:
    // Temperature: 27 degC
    // Voltage: 3.3 V
}
```

`printf` vẫn dùng được trong C++ nhưng từ đây các ví dụ sẽ dùng `std::cout`.

## Namespace

Trong C, mọi hàm và biến toàn cục nằm chung một không gian tên. Hai thư viện cùng có hàm `init()` sẽ gây lỗi trùng tên khi link nên ta phải tự thêm tiền tố:

```c
// C: tự đặt tiền tố để tránh trùng tên
void uart_init(void);
void lcd_init(void);
```

C++ giải quyết bằng **namespace**: gom các hàm, biến, kiểu dữ liệu vào một vùng tên riêng. Hai namespace khác nhau có thể chứa hàm trùng tên mà không xung đột. Khi gọi, ta viết tên namespace, theo sau là toán tử `::`.

```cpp
namespace uart {
    void init() { std::cout << "UART init\n"; }
}

namespace lcd {
    void init() { std::cout << "LCD init\n"; }
}

uart::init();   // in ra: UART init
lcd::init();    // in ra: LCD init
```

Đây cũng là lý do ta viết `std::cout`: toàn bộ thư viện chuẩn của C++ nằm trong namespace `std`.

Để khỏi phải gõ tên namespace nhiều lần, ta có thể dùng `using`:

```cpp
using std::cout;         // chỉ đưa cout vào phạm vi hiện tại
cout << "Hello\n";

using namespace std;     // đưa toàn bộ namespace std vào
```

:::warning Không dùng using namespace trong header
Mọi file `#include` header đó sẽ bị kéo theo toàn bộ tên trong namespace, dễ gây trùng tên ngoài ý muốn. Trong file `.cpp` thì có thể dùng nhưng viết rõ `std::` vẫn là thói quen tốt.
:::

## Kiểu bool

Trong C (trước C99), không có kiểu logic riêng. Ta dùng `int` với quy ước 0 là sai, khác 0 là đúng. Từ C99 phải `#include <stdbool.h>` mới dùng được `bool`. Trong C++, `bool` là kiểu có sẵn với hai giá trị `true` và `false`.

```cpp
bool ledOn = false;
bool sensorReady = (temperature > -40);

if (!ledOn) {
    ledOn = true;
}
```

Khi in bằng `std::cout`, `bool` mặc định hiện thành `1` hoặc `0`. Muốn in chữ `true`/`false` thì dùng `std::boolalpha`:

```cpp
std::cout << ledOn << "\n";                    // in ra: 1
std::cout << std::boolalpha << ledOn << "\n";  // in ra: true
```

## nullptr thay cho NULL

Trong C, con trỏ rỗng được biểu diễn bằng `NULL`. Trong C++, `NULL` thường được định nghĩa là số `0`, tức là một số nguyên chứ không phải con trỏ. Điều này gây nhầm lẫn khi có hai hàm cùng tên, một nhận số nguyên, một nhận con trỏ (tính năng function overloading sẽ được trình bày ở phần sau của bài):

```cpp
void send(int value)         { std::cout << "Send integer\n"; }
void send(const char* text)  { std::cout << "Send string\n"; }

send(NULL);     // ý định gửi con trỏ rỗng, nhưng lại gọi send(int)
                // hoặc bị compiler báo lỗi gọi mơ hồ
```

C++11 đưa vào từ khóa `nullptr`, là giá trị con trỏ rỗng thực sự. Compiler luôn hiểu đúng đây là con trỏ.

```cpp
send(nullptr);  // in ra: Send string

int* buffer = nullptr;
if (buffer == nullptr) {
    std::cout << "Not allocated yet\n";
}
```

Trong code C++, ta luôn dùng `nullptr` thay cho `NULL`.

## Suy luận kiểu với auto

Khi khai báo biến có giá trị khởi tạo, ta có thể viết `auto` thay cho tên kiểu. Compiler sẽ tự suy ra kiểu từ giá trị ở vế phải.

```cpp
auto count = 10;        // int
auto voltage = 3.3f;    // float
auto ratio = 0.5;       // double
auto name = "BBB";      // const char*
```

`auto` hữu ích nhất khi tên kiểu quá dài. Ở các bài sau, ta sẽ gặp những kiểu như `std::map<std::string, std::vector<int>>::iterator`. Khi đó viết `auto` giúp code dễ đọc hơn nhiều.

Tuy vậy, không nên lạm dụng `auto` cho các kiểu đơn giản. Với code nhúng, kiểu dữ liệu chính xác rất quan trọng (ví dụ `uint8_t` hay `int`) nên viết rõ kiểu giúp người đọc không phải đoán:

```cpp
auto reg = 0x1F;        // int, dù ta có thể muốn uint8_t
uint8_t reg2 = 0x1F;    // rõ ràng hơn
```

:::note Biến auto phải có giá trị khởi tạo
Compiler cần giá trị khởi tạo để suy ra kiểu nên `auto x;` sẽ báo lỗi.
:::

## const và constexpr thay cho macro #define

Trong C, hằng số thường được khai báo bằng `#define`. Bộ tiền xử lý chỉ thay thế văn bản trước khi biên dịch, nên hằng số không có kiểu, không thuộc namespace nào và không hiện tên khi debug.

```c
// C
#define LED_PIN      60
#define BUFFER_SIZE  64
```

C++ khuyến khích dùng `const` hoặc `constexpr`:

```cpp
constexpr int LED_PIN = 60;
constexpr int BUFFER_SIZE = 64;

uint8_t rxBuffer[BUFFER_SIZE];   // dùng làm kích thước mảng được
```

Hai từ khóa này khác nhau như sau:

- `const`: giá trị không được thay đổi sau khi khởi tạo. Giá trị có thể được xác định lúc chạy chương trình.
- `constexpr`: giá trị phải được tính ra ngay lúc biên dịch. Dùng cho hằng số cố định như chân GPIO, kích thước bộ đệm, địa chỉ thanh ghi.

```cpp
int readConfig() { return 115200; }

const int baudRate = readConfig();       // hợp lệ: const cho phép giá trị lúc chạy
constexpr int baud2 = readConfig();      // lỗi: không tính được lúc biên dịch
constexpr int MAX_RETRY = 3;             // hợp lệ
```

Ngoài ra, hằng số `const`/`constexpr` có thể đặt trong namespace để tránh trùng tên:

```cpp
namespace config {
    constexpr int LED_PIN = 60;
    constexpr int UART_BAUD = 115200;
}

std::cout << config::UART_BAUD;   // in ra: 115200
```

## Function overloading

Trong C, mỗi hàm phải có tên duy nhất. Muốn xử lý nhiều kiểu tham số, ta phải đặt nhiều tên khác nhau, như thư viện chuẩn C có `abs`, `labs`, `fabs`:

```c
// C
void uart_send_byte(uint8_t b);
void uart_send_string(const char* s);
void uart_send_buffer(const uint8_t* data, size_t len);
```

C++ cho phép nhiều hàm trùng tên nếu danh sách tham số khác nhau (khác số lượng hoặc khác kiểu). Compiler dựa vào tham số khi gọi để chọn đúng hàm.

```cpp
void send(uint8_t b)                       { std::cout << "Send 1 byte\n"; }
void send(const char* s)                   { std::cout << "Send string\n"; }
void send(const uint8_t* data, size_t len) { std::cout << "Send " << len << " byte\n"; }

uint8_t frame[4] = {0xAA, 0x01, 0x02, 0x55};

send(uint8_t(0x41));   // in ra: Send 1 byte
send("Hello");         // in ra: Send string
send(frame, 4);        // in ra: Send 4 byte
```

Chỉ khác kiểu trả về thì không đủ để overloading vì khi gọi hàm, compiler không dựa vào kiểu trả về để phân biệt:

```cpp
int   readValue();
float readValue();   // lỗi: chỉ khác kiểu trả về
```

### Gọi thư viện C từ C++

Để phân biệt các hàm trùng tên, compiler C++ mã hóa thêm thông tin tham số vào tên hàm khi biên dịch (gọi là **name mangling**). Compiler C không làm vậy. Vì thế, khi gọi hàm từ một thư viện viết bằng C, ta phải bọc phần khai báo trong `extern "C"` để linker tìm đúng tên:

```cpp
extern "C" {
    #include "gpio_driver.h"   // header của thư viện viết bằng C
}
```

Nhiều thư viện C đã tự thêm `extern "C"` trong header của chúng, khi đó ta không cần bọc lại.

## Tham số mặc định

C++ cho phép gán giá trị mặc định cho tham số. Khi gọi hàm, có thể bỏ qua các tham số này và giá trị mặc định sẽ được dùng.

```cpp
void blinkLed(int pin, int times = 3, int delayMs = 500)
{
    std::cout << "Pin " << pin << ": blink " << times
              << " times, " << delayMs << " ms each\n";
}

blinkLed(60);            // in ra: Pin 60: blink 3 times, 500 ms
blinkLed(60, 5);         // in ra: Pin 60: blink 5 times, 500 ms
blinkLed(60, 5, 100);    // in ra: Pin 60: blink 5 times, 100 ms
```

Có hai quy tắc cần nhớ.

**Thứ nhất,** tham số mặc định phải nằm liên tiếp ở cuối danh sách tham số. Ta không thể bỏ qua một tham số ở giữa:

```cpp
void blinkLed(int pin = 60, int times);    // lỗi: tham số mặc định không ở cuối
blinkLed(60, , 100);                       // lỗi: không bỏ trống được tham số giữa
```

**Thứ hai,** khi tách khai báo và định nghĩa hàm ra file `.h` và `.cpp`, giá trị mặc định chỉ ghi ở khai báo trong file header:

```cpp
// led.h
void blinkLed(int pin, int times = 3, int delayMs = 500);

// led.cpp
void blinkLed(int pin, int times, int delayMs)   // không ghi lại giá trị mặc định
{
    // ...
}
```

## Ép kiểu kiểu C++

Trong C, phép ép kiểu `(type)value` làm được mọi thứ, từ đổi `float` sang `int` đến biến một con trỏ thành con trỏ kiểu khác hoàn toàn và compiler không phân biệt các trường hợp này. C++ tách thành các phép ép kiểu riêng, mỗi loại cho một mục đích:

```cpp
float voltage = 3.7f;
int milliVolt = static_cast<int>(voltage * 1000);   // milliVolt = 3700
```

`static_cast` dùng cho các chuyển đổi thông thường và an toàn như giữa các kiểu số.

```cpp
struct Frame {
    uint8_t id;
    uint8_t data[8];
};

Frame frame{};
const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&frame);   // xem struct như dãy byte
```

`reinterpret_cast` dùng để diễn giải lại vùng nhớ theo kiểu khác, thường gặp khi gửi struct qua đường truyền. Phép này nguy hiểm, và tên dài, dễ nhận ra khiến người đọc code biết ngay đây là chỗ cần chú ý.

Phép ép kiểu kiểu C vẫn biên dịch được trong C++, nhưng nên tránh vì nó che giấu ý định thực sự. Tùy chọn `-Wold-style-cast` của g++ sẽ cảnh báo mọi chỗ còn dùng cách cũ.

## Lỗi thường gặp

**Gọi hàm nạp chồng bị mơ hồ: `call of overloaded 'setThreshold(double)' is ambiguous`**

```cpp
void setThreshold(int value);
void setThreshold(float value);

setThreshold(25.5);   // lỗi
```

`25.5` có kiểu `double`, không khớp chính xác với hàm nào. Chuyển `double` sang `int` hay sang `float` đều hợp lệ như nhau, nên compiler không biết chọn hàm nào. Cách sửa là viết giá trị đúng kiểu:

```cpp
setThreshold(25.5f);  // gọi bản float
```

**Ghi lại giá trị mặc định ở phần định nghĩa hàm: `default argument given for parameter 2`**

```cpp
// led.h
void blinkLed(int pin, int times = 3);

// led.cpp
void blinkLed(int pin, int times = 3) { }   // lỗi
```

Giá trị mặc định chỉ được khai báo một lần. Cách sửa là xóa `= 3` trong file `.cpp`.

**Lỗi khi link `undefined reference to 'gpio_write(int, int)'` khi gọi hàm từ thư viện C**

```cpp
#include "gpio_driver.h"   // thư viện viết bằng C, header không có extern "C"

gpio_write(60, 1);          // lỗi khi link
```

Compiler C++ tìm hàm với tên đã được mã hóa kèm tham số, trong khi thư viện C chỉ có tên gốc `gpio_write`. Cách sửa là bọc header trong `extern "C"`:

```cpp
extern "C" {
    #include "gpio_driver.h"
}
```

**Hằng số `#define` cho kết quả sai**

```cpp
#define BUFFER_SIZE 32 + 32

int total = BUFFER_SIZE * 2;   // mong đợi 128, thực tế là 96
```

`#define` chỉ thay thế văn bản, nên biểu thức trở thành `32 + 32 * 2`. Dùng `constexpr` thì giá trị được tính đúng như một biến:

```cpp
constexpr int BUFFER_SIZE = 32 + 32;
int total = BUFFER_SIZE * 2;   // 128
```
