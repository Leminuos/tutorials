Trong C, khi làm việc với một cảm biến, ta thường gom dữ liệu vào một struct và viết các hàm riêng để thao tác trên struct đó:

```c
// C
struct TemperatureSensor {
    int channel;
    float lastValue;
    float threshold;
};

void sensor_init(struct TemperatureSensor* s, int channel);
float sensor_read(struct TemperatureSensor* s);
```

Cách làm này có ba điểm yếu. Dữ liệu và hàm tách rời nhau, chỉ gắn với nhau bằng quy ước đặt tên. Bất kỳ đoạn code nào cũng sửa được `threshold` thành một giá trị vô lý như `-1000`. Và nếu quên gọi `sensor_init()`, struct chứa giá trị rác mà không có gì cảnh báo.

Class trong C++ giải quyết cả ba vấn đề. Đây là nền tảng của lập trình hướng đối tượng, và của hầu hết thư viện, framework C++.

## Từ struct đến class

Class cho phép đặt hàm bên trong cùng với dữ liệu. Dữ liệu trong class gọi là **biến thành viên** (member variable), hàm gọi là **hàm thành viên** (member function hay method). Một biến được tạo từ class gọi là **đối tượng** (object).

```cpp
class TemperatureSensor {
public:
    int channel;
    float lastValue;

    float read()
    {
        lastValue = 25.0f + channel;   // giả lập giá trị đọc được
        return lastValue;
    }
};

TemperatureSensor sensor;       // tạo đối tượng
sensor.channel = 2;
std::cout << sensor.read();     // in ra: 27
```

So với C, có hai khác biệt. Hàm `read()` nằm trong class và được gọi qua dấu chấm: `sensor.read()` thay vì `sensor_read(&sensor)`. Bên trong hàm, ta dùng trực tiếp `channel` và `lastValue` mà không cần truyền con trỏ tới struct vì hàm thành viên luôn biết nó đang làm việc với đối tượng nào.

Nếu có con trỏ tới đối tượng, ta dùng `->` giống như với struct trong C:

```cpp
TemperatureSensor* p = &sensor;
p->read();
```

## Phạm vi truy cập: public và private

Class chia thành viên thành hai vùng:

- `public`: truy cập được từ bất kỳ đâu.
- `private`: chỉ các hàm thành viên của class được truy cập.

Ta đặt dữ liệu vào `private` và chỉ cho bên ngoài thao tác qua các hàm `public`. Nhờ vậy, class tự kiểm soát được dữ liệu của mình. Nguyên tắc này gọi là **đóng gói** (encapsulation).

```cpp
class TemperatureSensor {
public:
    float threshold() { return m_threshold; }

    void setThreshold(float value)
    {
        if (value < -40.0f || value > 125.0f) return;
        m_threshold = value;
    }

private:
    float m_threshold = 70.0f;
};

TemperatureSensor sensor;
sensor.setThreshold(80.0f);        // hợp lệ
sensor.setThreshold(-1000.0f);     // bị bỏ qua
std::cout << sensor.threshold();   // in ra: 80

sensor.m_threshold = -1000.0f;     // lỗi: 'm_threshold' is private
```

Hàm dùng để đọc dữ liệu gọi là **getter**, hàm dùng để ghi gọi là **setter**. Trong chuỗi bài này, getter mang tên của dữ liệu (`threshold()`), setter thêm tiền tố `set` (`setThreshold()`). Biến thành viên có tiền tố `m_` để dễ phân biệt với biến cục bộ.

Ở ví dụ trên, `m_threshold = 70.0f` là giá trị khởi tạo mặc định của biến thành viên. Mọi đối tượng mới tạo ra đều có ngưỡng 70 độ, trừ khi được thay đổi.

## struct và class trong C++

Trong C++, `struct` cũng có thể chứa hàm, có `public`/`private` giống hệt `class`. Khác biệt duy nhất là phạm vi mặc định:

```cpp
struct A {
    int x;     // mặc định public
};

class B {
    int x;     // mặc định private
};
```

Theo quy ước chung, ta dùng `struct` cho dữ liệu đơn giản không cần bảo vệ (như khung dữ liệu `Frame` ở Bài C2), và dùng `class` khi có dữ liệu cần đóng gói cùng các hàm xử lý.

## Constructor

**Constructor** là hàm đặc biệt, tự động được gọi khi đối tượng được tạo. Constructor có tên trùng tên class và không có kiểu trả về. Nhờ constructor, ta không thể quên việc khởi tạo đối tượng như khi quên gọi `sensor_init()` trong C.

```cpp
class TemperatureSensor {
public:
    TemperatureSensor(int channel)
    {
        m_channel = channel;
        std::cout << "Init sensor channel " << channel << "\n";
    }

private:
    int m_channel;
};

TemperatureSensor sensor(3);   // in ra: Init sensor channel 3
TemperatureSensor other;       // lỗi: phải truyền channel
```

Constructor có thể được overloading như hàm thường để cho phép nhiều cách tạo đối tượng:

```cpp
class TemperatureSensor {
public:
    TemperatureSensor()                  { m_channel = 0; }
    TemperatureSensor(int channel)       { m_channel = channel; }

private:
    int m_channel;
};

TemperatureSensor a;       // gọi constructor không tham số, kênh 0
TemperatureSensor b(5);    // gọi constructor có tham số, kênh 5
```

Constructor không có tham số được gọi là **constructor mặc định**. Nếu class không khai báo constructor nào, compiler tự tạo một constructor mặc định rỗng. Nhưng khi ta đã tự viết bất kỳ constructor nào, compiler sẽ không tạo nữa.

## Danh sách khởi tạo thành viên

Ở các ví dụ trên, ta gán giá trị trong thân constructor. C++ có cách tốt hơn là **danh sách khởi tạo** (member initializer list), viết sau dấu `:` ngay trước thân constructor:

```cpp
class TemperatureSensor {
public:
    TemperatureSensor(int channel, float threshold)
        : m_channel(channel), m_threshold(threshold)
    {
        // thân constructor có thể để trống
    }

private:
    int m_channel;
    float m_threshold;
};
```

Khác biệt giữa hai cách: với danh sách khởi tạo, biến thành viên được khởi tạo ngay với giá trị đúng. Còn khi gán trong thân constructor, biến được tạo ra trước với giá trị chưa xác định, rồi mới bị gán giá trị mới.

Với kiểu `int` hay `float`, hai cách cho kết quả như nhau. Nhưng có những thành viên bắt buộc phải dùng danh sách khởi tạo vì chúng không thể gán sau khi tạo: thành viên `const` và thành viên tham chiếu.

```cpp
class Led {
public:
    Led(int pin) { m_pin = pin; }    // lỗi: assignment of read-only member 'Led::m_pin'
    Led(int pin) : m_pin(pin) {}     // đúng

private:
    const int m_pin;                 // chân GPIO không đổi sau khi tạo
};
```

:::warning Thứ tự khởi tạo theo thứ tự khai báo
Các thành viên luôn được khởi tạo theo thứ tự khai báo trong class, không phải thứ tự viết trong danh sách khởi tạo. Để tránh nhầm lẫn, ta luôn viết danh sách khởi tạo theo đúng thứ tự khai báo.
:::

## Destructor

**Destructor** là hàm tự động được gọi khi đối tượng bị hủy, ví dụ khi biến cục bộ ra khỏi phạm vi của nó. Destructor có tên là `~` cộng tên class, không có tham số và không có kiểu trả về.

Destructor dùng để giải phóng tài nguyên mà đối tượng đang giữ: đóng file thiết bị, tắt LED, giải phóng bộ nhớ.

```cpp
class Led {
public:
    Led(int pin) : m_pin(pin)
    {
        std::cout << "LED on " << m_pin << "\n";
    }

    ~Led()
    {
        std::cout << "LED off " << m_pin << "\n";
    }

private:
    const int m_pin;
};

int main()
{
    Led led1(60);
    {
        Led led2(61);
    }
    std::cout << "End of main\n";
}

// in ra:
// LED on 60
// LED on 61
// LED off 61
// End of main
// LED off 60
```

Ta không bao giờ phải tự gọi destructor. Chỉ cần đối tượng hết phạm vi, việc dọn dẹp tự diễn ra.

## Con trỏ this

Bên trong hàm thành viên, từ khóa `this` là con trỏ trỏ tới chính đối tượng đang gọi hàm. Khi gọi `sensor.read()`, bên trong `read()` thì `this` chính là `&sensor`.

Trường hợp hay dùng `this` nhất là khi tên tham số trùng với tên biến thành viên:

```cpp
class Uart {
public:
    void setBaudRate(int baudRate)
    {
        this->baudRate = baudRate;   // vế trái là thành viên, vế phải là tham số
    }

private:
    int baudRate;
};
```

Nhờ quy ước đặt tiền tố `m_` cho biến thành viên, ta hiếm khi gặp trường hợp trùng tên này. Tuy vậy, `this` vẫn xuất hiện thường xuyên khi một hàm cần truyền chính đối tượng này cho hàm khác, ví dụ khi đăng ký đối tượng làm nơi nhận callback.

## Hàm thành viên const

Ở Bài 2, ta đã thấy tham chiếu hằng `const&` là cách truyền đối tượng lớn hiệu quả. Nhưng khi truyền đối tượng class theo `const&`, ta gặp vấn đề:

```cpp
void printSensor(const TemperatureSensor& s)
{
    std::cout << s.threshold();
    // lỗi: passing 'const TemperatureSensor' as 'this' argument discards qualifiers
}
```

Compiler không biết hàm `threshold()` có thay đổi đối tượng hay không nên cấm gọi trên đối tượng hằng. Để báo cho compiler biết một hàm không thay đổi đối tượng, ta thêm `const` vào sau danh sách tham số:

```cpp
class TemperatureSensor {
public:
    float threshold() const { return m_threshold; }   // hàm chỉ đọc

    void setThreshold(float value) { m_threshold = value; }   // hàm thay đổi dữ liệu

private:
    float m_threshold = 70.0f;
};
```

Bên trong hàm `const`, compiler cấm sửa bất kỳ biến thành viên nào. Quy tắc đơn giản: mọi getter và mọi hàm chỉ đọc dữ liệu đều nên khai báo `const`.

## Tách class ra file .h và .cpp

Trong dự án thật, mỗi class thường được chia làm hai file. File header (`.h`) chứa khai báo class, cho biết class có những gì. File source (`.cpp`) chứa phần cài đặt các hàm.

:::: code-group
```cpp [sensor.h]
#pragma once   // đảm bảo file chỉ được include một lần

class TemperatureSensor {
public:
    TemperatureSensor(int channel, float threshold = 70.0f);
    ~TemperatureSensor();

    float read();
    bool isOverheat() const;

private:
    int m_channel;
    float m_threshold;
    float m_lastValue = 0.0f;
};
```
::: explain [Giải thích chi tiết]
- `threshold = 70.0f`: tham số mặc định chỉ ghi ở file `.h`, như quy tắc ở Bài C1.
- `bool isOverheat() const`: từ khóa `const` của hàm thành viên phải ghi ở cả hai file.
:::

```cpp [sensor.cpp]
#include "sensor.h"
#include <iostream>

TemperatureSensor::TemperatureSensor(int channel, float threshold)
    : m_channel(channel), m_threshold(threshold)
{
    std::cout << "Open sensor channel " << m_channel << "\n";
}

TemperatureSensor::~TemperatureSensor()
{
    std::cout << "Close sensor channel " << m_channel << "\n";
}

float TemperatureSensor::read()
{
    m_lastValue = 25.0f + m_channel * 20.0f;   // giả lập
    return m_lastValue;
}

bool TemperatureSensor::isOverheat() const
{
    return m_lastValue > m_threshold;
}
```
::: explain [Giải thích chi tiết]
- `TemperatureSensor::`: khi cài đặt hàm bên ngoài class, tên hàm phải có tiền tố này để compiler biết hàm thuộc class nào. Toán tử `::` chính là toán tử ta đã dùng với namespace ở Bài C1.
- `: m_channel(channel), m_threshold(threshold)`: danh sách khởi tạo chỉ ghi ở file `.cpp`.
:::

```cpp [main.cpp]
#include "sensor.h"
#include <iostream>

int main()
{
    TemperatureSensor sensor(3, 80.0f);
    sensor.read();
    std::cout << "Overheat: " << std::boolalpha << sensor.isOverheat() << "\n";
}
```
::: explain [Giải thích chi tiết]
`sensor` được tạo trên stack nên constructor chạy ngay dòng khai báo, và destructor tự chạy khi `main()` kết thúc.
:::
::::

Khi biên dịch, ta liệt kê tất cả file `.cpp`:

```bash
g++ -std=c++17 -Wall -Wextra main.cpp sensor.cpp -o main
./main
```

```
Open sensor channel 3
Overheat: true
Close sensor channel 3
```

## Lỗi thường gặp

**Quên dấu chấm phẩy sau khai báo class: `expected ';' after class definition`**

```cpp
class Led {
    int m_pin;
}             // thiếu ;
```

Compiler báo lỗi ở dòng tiếp theo hoặc thậm chí ở file khác include header này, với thông báo khó hiểu như `expected ';' after class definition` hoặc `new types may not be defined in a return type`. Khi gặp lỗi lạ ngay sau một header, hãy kiểm tra dấu `;` cuối class trước tiên.

**Khai báo đối tượng với cặp ngoặc rỗng: `request for member 'read' in 'sensor', which is of non-class type 'TemperatureSensor()'`**

```cpp
TemperatureSensor sensor();   // tưởng là tạo đối tượng
sensor.read();                // lỗi
```

C++ hiểu dòng đầu là khai báo một hàm tên `sensor`, không có tham số, trả về `TemperatureSensor`. Muốn dùng constructor mặc định thì bỏ cặp ngoặc, hoặc dùng ngoặc nhọn:

```cpp
TemperatureSensor sensor;
TemperatureSensor sensor2{};
```

**Danh sách khởi tạo sai thứ tự: `'Uart::m_baud' will be initialized after 'int Uart::m_timeout'`**

```cpp
class Uart {
public:
    Uart(int baud) : m_baud(baud), m_timeout(100000 / m_baud) {}   // cảnh báo

private:
    int m_timeout;   // khai báo trước, nên được khởi tạo trước
    int m_baud;
};
```

`m_timeout` được khởi tạo trước, khi `m_baud` chưa có giá trị, nên phép chia dùng giá trị rác, có thể gây chia cho 0. Cách sửa là viết danh sách khởi tạo theo đúng thứ tự khai báo, và tránh để thành viên này phụ thuộc vào thành viên khác khi khởi tạo.

**Quên tiền tố tên class khi cài đặt hàm: `'m_lastValue' was not declared in this scope`**

```cpp
// sensor.cpp
float read()        // thiếu TemperatureSensor::
{
    return m_lastValue;   // lỗi
}
```

Không có tiền tố, compiler hiểu đây là một hàm tự do bình thường, không thuộc class nào. Nếu hàm không dùng thành viên nào, compiler sẽ không báo lỗi ở đây mà báo lỗi `undefined reference to TemperatureSensor::read()` khi link. Cách sửa là viết `float TemperatureSensor::read()`.

**Thiếu `const` ở một trong hai file: `no declaration matches 'bool TemperatureSensor::isOverheat()'`**

```cpp
// sensor.h
bool isOverheat() const;

// sensor.cpp
bool TemperatureSensor::isOverheat() { ... }   // lỗi
```

Với compiler, hàm có `const` và không có `const` là hai hàm khác nhau. Cách sửa là thêm `const` ở file `.cpp`.
