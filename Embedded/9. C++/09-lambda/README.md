Trong C, khi cần truyền một đoạn xử lý vào hàm khác (callback), ta dùng con trỏ hàm. Ví dụ muốn sắp xếp mảng giảm dần bằng `qsort()`:

```c
// C
int compare_desc(const void* a, const void* b)
{
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa < fb) - (fa > fb);
}

qsort(values, count, sizeof(float), compare_desc);
```

Cách này có hai bất tiện. Hàm so sánh phải viết riêng ở một chỗ khác, xa nơi sử dụng, dù chỉ dùng đúng một lần. Và nếu callback cần dữ liệu từ bên ngoài (chẳng hạn một ngưỡng nhiệt độ), ta phải thêm tham số `void* user_data` rồi ép kiểu qua lại, và compiler không kiểm tra được gì.

**Lambda** cho phép viết hàm ngay tại chỗ cần dùng, và lấy trực tiếp các biến xung quanh mà không cần `void*`. Lambda là cách phổ biến để viết callback cho sự kiện như nhận dữ liệu UART hay hết giờ timer.

## Lambda là gì

Lambda là một hàm không tên, được viết ngay trong biểu thức. Cấu trúc đầy đủ gồm bốn phần:

```
[capture](tham số) -> kiểu trả về { thân hàm }
```

Phần `[capture]` sẽ được giải thích ở mục sau. Phần `-> kiểu trả về` thường được bỏ qua vì compiler tự suy ra. Ta có thể gán lambda cho một biến `auto` rồi gọi như hàm bình thường:

```cpp
auto square = [](int x) { return x * x; };
std::cout << square(5);    // in ra: 25

auto greet = []() { std::cout << "Hello\n"; };
greet();                   // in ra: Hello
```

Tuy nhiên, lambda hiếm khi được gán vào biến như vậy. Cách dùng phổ biến nhất là truyền trực tiếp vào một hàm khác.

## Dùng lambda với thuật toán

Các thuật toán trong `<algorithm>` ở Bài C8 đều nhận được lambda để tùy biến cách xử lý. Ví dụ sắp xếp giảm dần, tương đương đoạn `qsort()` ở phần mở đầu:

```cpp
std::vector<float> r = {25.1f, 82.4f, 24.8f, 86.0f};

std::sort(r.begin(), r.end(), [](float a, float b) { return a > b; });
// r: 86 82.4 25.1 24.8
```

Lambda nhận hai phần tử và trả về `true` nếu `a` phải đứng trước `b`. Đoạn code ngắn, nằm ngay tại chỗ dùng, và kiểu dữ liệu được kiểm tra đầy đủ.

Đếm số giá trị vượt ngưỡng bằng `std::count_if`:

```cpp
auto over = std::count_if(r.begin(), r.end(), [](float v) { return v > 80.0f; });
std::cout << over;    // in ra: 2
```

Tìm phần tử đầu tiên thỏa điều kiện bằng `std::find_if`:

```cpp
struct Reading {
    std::string name;
    float value;
};

std::vector<Reading> list = {{"Server room", 28.0f}, {"Engine", 82.4f}};

auto it = std::find_if(list.begin(), list.end(),
                       [](const Reading& x) { return x.name == "Engine"; });
if (it != list.end()) {
    std::cout << it->value;    // in ra: 82.4
}
```

Và đây là cách viết gọn đã hứa ở Bài C8 để xóa các phần tử thỏa điều kiện, ví dụ loại bỏ các giá trị lỗi `-999` do cảm biến trả về khi mất kết nối:

```cpp
std::vector<float> data = {25.1f, -999.0f, 24.8f, -999.0f, 26.0f};

data.erase(std::remove_if(data.begin(), data.end(),
                          [](float v) { return v < -40.0f; }),
           data.end());
// data: 25.1 24.8 26
```

`std::remove_if` dồn các phần tử được giữ lại lên đầu và trả về iterator tới vị trí kết thúc mới; sau đó `erase` cắt bỏ phần thừa phía sau. Cách viết kết hợp hai hàm này rất phổ biến trong C++.

## Capture: lấy biến từ bên ngoài

Mặc định, lambda không dùng được biến cục bộ bên ngoài nó:

```cpp
float threshold = 80.0f;
auto isOver = [](float v) { return v > threshold; };
// lỗi: 'threshold' is not captured
```

Để dùng, ta phải liệt kê biến trong cặp ngoặc vuông, gọi là **capture**. Có hai cách capture:

- **Capture theo giá trị** (`[threshold]`): lambda lưu một bản sao của biến tại thời điểm tạo lambda.
- **Capture theo tham chiếu** (`[&threshold]`): lambda truy cập chính biến gốc, giống tham chiếu ở Bài C2.

Khác biệt thể hiện rõ khi biến gốc thay đổi sau khi tạo lambda:

```cpp
float threshold = 80.0f;

auto byValue = [threshold](float v)  { return v > threshold; };
auto byRef   = [&threshold](float v) { return v > threshold; };

threshold = 90.0f;

std::cout << byValue(85.0f);   // in ra: 1 (vẫn so với 80, bản sao lúc tạo)
std::cout << byRef(85.0f);     // in ra: 0 (so với 90, giá trị hiện tại)
```

Capture theo tham chiếu cũng cho phép lambda sửa biến bên ngoài:

```cpp
int overCount = 0;

std::for_each(r.begin(), r.end(), [&overCount](float v) {
    if (v > 80.0f) {
        ++overCount;
    }
});

std::cout << overCount;   // in ra: 2
```

Có thể capture nhiều biến, kết hợp cả hai cách:

```cpp
[threshold, &overCount]    // threshold theo giá trị, overCount theo tham chiếu
[=]                        // mọi biến được dùng đều theo giá trị
[&]                        // mọi biến được dùng đều theo tham chiếu
[=, &overCount]            // tất cả theo giá trị, riêng overCount theo tham chiếu
```

Cách viết `[=]` và `[&]` tiện nhưng khiến người đọc khó biết lambda đang dùng những biến nào. Ta nên liệt kê rõ từng biến, nhất là khi lambda được lưu lại để gọi sau (xem mục "Vòng đời của biến được capture").

### Capture this

Bên trong hàm thành viên, muốn lambda truy cập biến thành viên của class, ta capture `this`:

```cpp
class Monitor {
public:
    long countOverheat(const std::vector<float>& values) const
    {
        return std::count_if(values.begin(), values.end(),
                             [this](float v) { return v > m_threshold; });
    }

private:
    float m_threshold = 80.0f;
};
```

:::warning [this] không sao chép đối tượng
`[this]` chỉ capture con trỏ `this`. Lambda truy cập `m_threshold` qua con trỏ này. Nếu lambda được gọi sau khi đối tượng `Monitor` đã bị hủy, nó sẽ truy cập vùng nhớ không còn hợp lệ. Đây là nguồn gốc của một lỗi rất hay gặp với callback (xem mục "Vòng đời của biến được capture").
:::

### Sửa bản sao với mutable

Biến capture theo giá trị mặc định là hằng bên trong lambda. Muốn sửa bản sao đó, ta thêm từ khóa `mutable`:

```cpp
int calls = 0;

auto counter = [calls]() mutable { return ++calls; };

counter();
counter();
std::cout << counter();   // in ra: 3 (bản sao bên trong lambda được tăng dần)
std::cout << calls;       // in ra: 0 (biến gốc không đổi)
```

Trường hợp này ít gặp. Phần lớn thời gian, nếu cần sửa biến bên ngoài thì ta capture theo tham chiếu.

## Kiểu trả về

Compiler tự suy ra kiểu trả về từ lệnh `return`. Nếu có nhiều lệnh `return` với kiểu khác nhau, compiler báo lỗi:

```cpp
auto toVoltage = [](int raw) {
    if (raw < 0) {
        return 0;                  // int
    }
    return raw * 1.8f / 4095;      // float
};
// lỗi: inconsistent deduction for auto return type: 'int' and then 'float'
```

Khi đó ta chỉ định kiểu trả về bằng `->`:

```cpp
auto toVoltage = [](int raw) -> float {
    if (raw < 0) {
        return 0;                  // được chuyển thành float
    }
    return raw * 1.8f / 4095;
};
```

## Lưu trữ callback với std::function

Để một class nhận và lưu callback từ bên ngoài, ta dùng `std::function` trong `<functional>`. Đây là kiểu có thể chứa bất kỳ thứ gì gọi được như hàm: hàm thường, con trỏ hàm, hay lambda có capture.

Cú pháp `std::function<void(uint8_t)>` nghĩa là "một thứ gọi được, nhận `uint8_t`, trả về `void`".

```cpp
#include <functional>

class UartReceiver {
public:
    void setOnByteReceived(std::function<void(uint8_t)> callback)
    {
        m_onByte = callback;
    }

    void simulateReceive(uint8_t byte)   // giả lập nhận một byte
    {
        if (m_onByte) {                  // kiểm tra đã có callback chưa
            m_onByte(byte);
        }
    }

private:
    std::function<void(uint8_t)> m_onByte;
};
```

Nơi sử dụng gắn xử lý bằng lambda, và có thể capture bất kỳ dữ liệu nào cần thiết. Ví dụ đưa byte nhận được vào bộ đệm vòng `RingBuffer` từ Bài C8:

```cpp
RingBuffer<uint8_t, 64> rxBuffer;
UartReceiver uart;

uart.setOnByteReceived([&rxBuffer](uint8_t b) {
    rxBuffer.push(b);
});

uart.simulateReceive(0xAA);
uart.simulateReceive(0x01);
std::cout << rxBuffer.size();   // in ra: 2
```

So với cách làm trong C bằng `void (*callback)(uint8_t byte, void* user_data)`, ta không cần tham số `void*` và không cần ép kiểu. Dữ liệu cần thiết được lambda mang theo qua capture.

`std::function` có chi phí nhỏ khi gọi, và có thể cấp phát heap nếu lambda capture nhiều dữ liệu. Nó phù hợp cho callback sự kiện; với đoạn code được gọi liên tục hàng triệu lần, nên cân nhắc template như các hàm trong `<algorithm>`.

:::note Lambda và API kiểu C
Lambda không có capture tự chuyển được thành con trỏ hàm thông thường, nên có thể truyền cho các API kiểu C. Lambda có capture thì không.
:::

```cpp
void registerHandler(void (*handler)(int code));   // API kiểu C

registerHandler([](int code) { std::cout << code; });            // hợp lệ
registerHandler([threshold](int code) { /* ... */ });            // lỗi: không chuyển được
```

## Vòng đời của biến được capture

Khi lambda được gọi ngay (như trong `std::sort`), capture theo tham chiếu luôn an toàn. Nhưng khi lambda được lưu lại để gọi sau (như callback trong `std::function`), ta phải đảm bảo mọi thứ nó tham chiếu tới vẫn còn tồn tại vào lúc gọi.

```cpp
UartReceiver uart;   // biến toàn cục, sống suốt chương trình

void setup()
{
    int received = 0;
    uart.setOnByteReceived([&received](uint8_t) {
        ++received;
    });
}   // received bị hủy tại đây, nhưng lambda vẫn nằm trong uart

setup();
uart.simulateReceive(0x01);   // lambda ghi vào biến đã bị hủy: hành vi không xác định
```

Đây chính là lỗi tham chiếu treo ở Bài C2, chỉ là khó phát hiện hơn vì lỗi xảy ra ở nơi khác, vào thời điểm khác. Quy tắc: lambda được lưu lại để gọi sau thì không capture biến cục bộ theo tham chiếu. Hoặc capture theo giá trị, hoặc đảm bảo đối tượng được tham chiếu sống lâu hơn lambda.

## Lỗi thường gặp

**Giá trị lạ hoặc crash do capture tham chiếu tới biến đã bị hủy: AddressSanitizer báo `stack-use-after-scope` hoặc `stack-use-after-return`**

Đã trình bày ở mục "Vòng đời của biến được capture". Chương trình thường chạy đúng một thời gian rồi mới cho giá trị lạ hoặc crash, vì vùng nhớ cũ chưa bị ghi đè ngay. Cách sửa là capture theo giá trị, hoặc chuyển biến thành thành viên của một đối tượng sống đủ lâu.

**Lambda không thấy giá trị mới của biến capture theo giá trị**

```cpp
float threshold = 80.0f;
auto isOver = [threshold](float v) { return v > threshold; };

threshold = 90.0f;         // người dùng đổi ngưỡng
isOver(85.0f);             // vẫn trả về true, vì lambda giữ bản sao 80
```

Capture theo giá trị là ảnh chụp tại thời điểm tạo lambda. Nếu cần giá trị hiện tại, hãy capture theo tham chiếu (với điều kiện biến sống đủ lâu), hoặc lưu ngưỡng làm biến thành viên và capture `this`.

**Sửa biến capture theo giá trị khi thiếu `mutable`: `increment of read-only variable 'calls'`**

```cpp
int calls = 0;
auto counter = [calls]() { return ++calls; };   // lỗi
```

Nếu ý định là sửa biến gốc, hãy capture theo tham chiếu `[&calls]`. Nếu chỉ muốn lambda có bộ đếm riêng, thêm `mutable`.

**Kích thước container không đổi sau `std::remove_if`**

```cpp
std::remove_if(data.begin(), data.end(), [](float v) { return v < -40.0f; });
std::cout << data.size();   // in ra: 5, kích thước không đổi
```

`remove_if` chỉ dồn phần tử, không thay đổi kích thước container, vì nó chỉ làm việc với iterator và không biết container là gì. Phần đuôi vẫn còn các giá trị không xác định. Cách sửa là luôn kết hợp với `erase` như ở mục "Dùng lambda với thuật toán".
