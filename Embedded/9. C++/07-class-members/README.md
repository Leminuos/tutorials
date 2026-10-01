Bài này gom ba chủ đề nhỏ, đều là những chỗ C++ sửa các điểm yếu quen thuộc của C.

Thứ nhất, trong C, khi cần dữ liệu dùng chung cho mọi đối tượng cùng loại như đếm số cảm biến đang mở, ta dùng biến toàn cục và biến này tách rời khỏi struct mà nó phục vụ. Thứ hai, `enum` của C đưa mọi tên hằng ra phạm vi chung nên hai enum không thể cùng có giá trị `OFF` và giá trị enum tự chuyển thành số nguyên mà không cảnh báo. Thứ ba, khi gán struct cho nhau, C sao chép từng byte và nếu struct chứa con trỏ thì hai struct cùng trỏ tới một vùng nhớ, đúng như lỗi xóa hai lần ta đã gặp ở Bài C4.

## Biến thành viên static

Biến thành viên thông thường thuộc về từng đối tượng: mỗi cảm biến có `m_name` riêng. **Biến thành viên static** thì thuộc về cả class: chỉ có đúng một bản duy nhất, dùng chung cho mọi đối tượng.

```cpp
class Sensor {
public:
    Sensor()  { ++s_count; }
    ~Sensor() { --s_count; }

    static int count() { return s_count; }

private:
    inline static int s_count = 0;   // một bản duy nhất cho cả class
};

Sensor a, b;
{
    Sensor c;
    std::cout << Sensor::count();   // in ra: 3
}
std::cout << Sensor::count();       // in ra: 2
```

Ta đặt tiền tố `s_` cho biến static để phân biệt với biến thành viên thường có tiền tố `m_`.

Từ khóa `inline` trong khai báo là cú pháp của C++17, cho phép khởi tạo biến static ngay trong class. Trong code cũ hơn, ta sẽ gặp cách viết tách làm hai nơi: khai báo trong file `.h` và định nghĩa trong file `.cpp`:

```cpp
// sensor.h
class Sensor {
    static int s_count;           // chỉ khai báo
};

// sensor.cpp
int Sensor::s_count = 0;          // định nghĩa, cấp phát bộ nhớ thật
```

Hằng số dùng chung cho class được khai báo bằng `static constexpr` và dùng được làm kích thước mảng như hằng số:

```cpp
class Adc {
public:
    static constexpr int CHANNEL_COUNT = 7;    // ADC của BBB có 7 kênh

private:
    uint16_t m_lastRaw[CHANNEL_COUNT] = {};
};

std::cout << Adc::CHANNEL_COUNT;   // in ra: 7
```

## Hàm thành viên static

**Hàm thành viên static** cũng thuộc về class chứ không thuộc về đối tượng nào. Nó được gọi qua tên class bằng toán tử `::`, không cần tạo đối tượng.

Vì không gắn với đối tượng nào, hàm static không có con trỏ `this` nên không truy cập được biến thành viên thường. Hàm static phù hợp với các chức năng tiện ích liên quan tới class nhưng không phụ thuộc vào trạng thái của một đối tượng cụ thể.

```cpp
class Adc {
public:
    static constexpr int MAX_RAW = 4095;       // ADC 12 bit

    // Đổi giá trị thô sang điện áp. ADC của BBB dùng điện áp tham chiếu 1.8 V
    static float toVoltage(uint16_t raw, float vref = 1.8f)
    {
        return raw * vref / MAX_RAW;
    }
};

std::cout << Adc::toVoltage(2048);   // in ra: 0.90022
```

## So sánh static trong C và trong class

Từ khóa `static` có nhiều nghĩa tùy vị trí, dễ gây nhầm lẫn cho người từ C sang:

| Vị trí | Ý nghĩa |
|---|---|
| Biến/hàm toàn cục trong file | Chỉ dùng được trong file đó (giống C) |
| Biến cục bộ trong hàm | Giữ giá trị giữa các lần gọi hàm (giống C) |
| Thành viên trong class | Thuộc về class, dùng chung cho mọi đối tượng (chỉ có trong C++) |

## enum class

`enum` của C có ba vấn đề:

```c
// C
enum LedState   { OFF, ON };
enum MotorState { OFF, RUNNING };   // lỗi: redeclaration of 'OFF'

enum LedState led = ON;
if (led == 1) { }                   // so sánh với số, compiler không phàn nàn
int x = RUNNING + ON;               // vô nghĩa nhưng hợp lệ
```

Các tên hằng tràn ra phạm vi chung nên dễ trùng tên, buộc ta phải thêm tiền tố như `LED_OFF`, `MOTOR_OFF`. Giá trị enum tự chuyển thành số nguyên nên có thể so sánh, cộng trừ lẫn lộn giữa các enum khác nhau.

C++11 đưa vào `enum class`, giải quyết cả hai vấn đề: tên hằng nằm trong phạm vi của enum và giá trị enum không tự chuyển thành số.

```cpp
enum class LedState   { Off, On, Blinking };
enum class MotorState { Off, Running };   // không trùng tên với LedState::Off

LedState led = LedState::On;

if (led == LedState::On) { }   // hợp lệ
if (led == 1) { }              // lỗi: no match for 'operator==' (LedState và int)
int x = led;                   // lỗi: cannot convert 'LedState' to 'int'
```

### Chỉ định kiểu dữ liệu nền

Với code nhúng, ta thường cần enum có kích thước chính xác, ví dụ để ghi vào khung dữ liệu truyền thông. `enum class` cho phép chỉ định kiểu nền sau dấu `:`

```cpp
enum class ModbusFunction : uint8_t {
    ReadHoldingRegisters = 0x03,
    WriteSingleRegister  = 0x06,
};

std::cout << sizeof(ModbusFunction);   // in ra: 1
```

### Chuyển đổi giữa enum class và số nguyên

Khi thực sự cần chuyển đổi, ta dùng `static_cast`, là cách ép kiểu của C++:

```cpp
uint8_t frame[8];
frame[1] = static_cast<uint8_t>(ModbusFunction::ReadHoldingRegisters);   // enum → số

auto func = static_cast<ModbusFunction>(frame[1]);                        // số → enum
```

`static_cast<T>(x)` có tác dụng giống ép kiểu kiểu C `(T)x`, nhưng compiler kiểm tra chặt hơn và từ chối những chuyển đổi vô lý (như ép một con trỏ thành `float`). Ngoài ra, cú pháp này dễ tìm kiếm trong code hơn. Từ đây ta dùng `static_cast` thay cho ép kiểu kiểu C.

### Cảnh báo khi switch thiếu trường hợp

Với `-Wall`, compiler kiểm tra `switch` trên enum có xử lý đủ mọi giá trị không. Khi thêm giá trị mới vào enum, compiler chỉ ra mọi chỗ cần cập nhật:

```cpp
void apply(LedState state)
{
    switch (state) {
    case LedState::Off: std::cout << "Off\n"; break;
    case LedState::On:  std::cout << "On\n"; break;
    }
    // cảnh báo: enumeration value 'Blinking' not handled in switch
}
```

## Copy constructor

Copy constructor là constructor tạo đối tượng mới từ một đối tượng có sẵn cùng kiểu. Nó nhận tham số là tham chiếu hằng tới đối tượng nguồn.

```cpp
class Config {
public:
    Config(const std::string& name) : m_name(name) {}

    Config(const Config& other) : m_name(other.m_name)
    {
        std::cout << "Copy constructor\n";
    }

private:
    std::string m_name;
};

void apply(Config cfg) { }

Config a("default");
Config b = a;       // in ra: Copy constructor
Config c(a);        // in ra: Copy constructor
apply(a);           // in ra: Copy constructor (truyền tham trị)
```

Dòng `Config b = a;` có dấu `=` nhưng không phải phép gán mà là tạo `b` từ `a` nên gọi copy constructor.

Nếu ta không tự viết, compiler tự tạo copy constructor mặc định, sao chép lần lượt từng thành viên. Với các thành viên như `int`, `float`, `std::string`, cách làm này hoàn toàn đúng nên phần lớn class không cần tự viết copy constructor.

## Copy assignment operator

Khi gán một đối tượng đã tồn tại bằng giá trị của đối tượng khác, C++ gọi **toán tử gán sao chép** (`operator=`):

```cpp
class Config {
public:
    // ... như trên ...

    Config& operator=(const Config& other)
    {
        m_name = other.m_name;
        std::cout << "Copy assignment\n";
        return *this;
    }
};

Config a("default");
Config d("other");
d = a;              // d đã tồn tại → gọi copy assignment, không phải copy constructor
```

Toán tử gán trả về `*this`, tức chính đối tượng vừa được gán (`this` là con trỏ, `*this` là đối tượng nó trỏ tới). Việc này cho phép viết gán liên tiếp như `a = b = c;`, giống với kiểu số nguyên.

Cách phân biệt đơn giản: đối tượng đang được tạo thì gọi copy constructor. Đối tượng đã có sẵn thì gọi toán tử gán.

## Sao chép nông và sao chép sâu

Quay lại lỗi ở Bài C4. Class `Buffer` sở hữu một vùng nhớ cấp phát bằng `new[]`. Copy constructor mặc định chỉ sao chép giá trị con trỏ, gọi là **sao chép nông** (shallow copy), khiến hai đối tượng cùng trỏ tới một vùng nhớ:

```
Sao chép nông:                    Sao chép sâu:
  a.m_data --+                      a.m_data --> [dữ liệu]
             +--> [dữ liệu]         b.m_data --> [bản sao dữ liệu]
  b.m_data --+
```

Để mỗi đối tượng có dữ liệu riêng, ta tự viết copy constructor và toán tử gán thực hiện **sao chép sâu** (deep copy): cấp phát vùng nhớ mới rồi chép dữ liệu sang.

```cpp
#include <cstring>

class Buffer {
public:
    Buffer(size_t size) : m_size(size), m_data(new uint8_t[size]()) {}

    ~Buffer() { delete[] m_data; }

    // Copy constructor: cấp phát vùng nhớ riêng rồi chép dữ liệu
    Buffer(const Buffer& other)
        : m_size(other.m_size), m_data(new uint8_t[other.m_size])
    {
        std::memcpy(m_data, other.m_data, m_size);
    }

    // Toán tử gán: phải giải phóng dữ liệu cũ trước khi nhận dữ liệu mới
    Buffer& operator=(const Buffer& other)
    {
        if (this == &other) {
            return *this;              // tự gán cho chính mình: a = a
        }

        uint8_t* newData = new uint8_t[other.m_size];
        std::memcpy(newData, other.m_data, other.m_size);

        delete[] m_data;
        m_data = newData;
        m_size = other.m_size;
        return *this;
    }

    uint8_t* data() { return m_data; }

private:
    size_t m_size;
    uint8_t* m_data;
};

Buffer a(4);
a.data()[0] = 0xAA;

Buffer b = a;           // b có vùng nhớ riêng
b.data()[0] = 0x55;

std::cout << std::hex << int(a.data()[0]);   // in ra: aa (a không bị ảnh hưởng)
// ra khỏi phạm vi: mỗi đối tượng xóa vùng nhớ của riêng mình, không còn double free
```

Toán tử gán phức tạp hơn copy constructor ở hai điểm. Nó phải kiểm tra trường hợp tự gán (`a = a`): nếu không, ta sẽ xóa dữ liệu của chính mình trước khi kịp chép. Và nó phải giải phóng dữ liệu cũ, vì đối tượng đích đã có sẵn vùng nhớ của nó. Việc cấp phát vùng nhớ mới trước rồi mới xóa vùng cũ giúp đối tượng vẫn giữ nguyên trạng thái nếu cấp phát thất bại.

## Move constructor và move assignment

Sao chép sâu an toàn nhưng tốn kém: mỗi lần sao chép là một lần cấp phát và chép toàn bộ dữ liệu. Điều này hợp lý khi đối tượng nguồn còn được dùng tiếp. Nhưng nếu đối tượng nguồn sắp bị bỏ đi thì việc chép là thừa:

```cpp
Buffer makeFrame()
{
    Buffer frame(1024);
    // ... điền dữ liệu ...
    return frame;   // frame sắp bị hủy, chép 1024 byte rồi xóa bản gốc là lãng phí
}
```

Hình dung như chuyển nhà: sao chép là xây một ngôi nhà mới giống hệt rồi phá nhà cũ, còn **di chuyển** (move) chỉ là trao chìa khóa. Với `Buffer`, trao chìa khóa nghĩa là đối tượng mới lấy luôn con trỏ `m_data` của đối tượng nguồn, còn đối tượng nguồn được đặt về rỗng:

```
Trước khi move:                 Sau khi move b từ a:
  a.m_data --> [dữ liệu]          a.m_data --> nullptr
                                  b.m_data --> [dữ liệu]   (vẫn vùng nhớ cũ)
```

Không cấp phát, không chép dữ liệu, chỉ gán lại hai con trỏ.

### Khi nào được phép move

Chỉ được lấy dữ liệu của một đối tượng khi chắc chắn không ai dùng nó nữa. C++ coi hai loại đối tượng sau là không ai dùng nữa:

- **Đối tượng tạm**: đối tượng không có tên, tồn tại trong một câu lệnh như `Buffer(1024)` hay giá trị trả về của hàm.
- **Đối tượng được đánh dấu bằng `std::move`** (trong `<utility>`): ta tự khẳng định với compiler rằng biến này không dùng nữa.

Để phân biệt hai trường hợp, C++ dùng hai kiểu tham số khác nhau:

```cpp
Buffer(const Buffer& other);   // copy constructor: other còn được dùng, phải chép
Buffer(Buffer&& other);        // move constructor: other sắp bỏ đi, được lấy dữ liệu
```

`Buffer&&` (hai dấu `&`) gọi là **tham chiếu rvalue**, có thể hiểu đơn giản là tham chiếu tới đối tượng sắp bỏ đi. Giống như nạp chồng hàm, compiler nhìn vào đối số để chọn hàm: biến bình thường thì chọn bản `const Buffer&`, đối tượng tạm hoặc `std::move(a)` thì chọn bản `Buffer&&`.

`std::move` không tự di chuyển gì cả. Nó chỉ dán nhãn sắp bỏ đi lên biến để compiler chọn hàm move. Việc di chuyển thật sự do move constructor hoặc move assignment thực hiện.

### Move constructor

Move constructor tạo đối tượng mới bằng cách lấy dữ liệu của `other`:

```cpp
Buffer(Buffer&& other) noexcept
    : m_size(other.m_size), m_data(other.m_data)   // 1. lấy dữ liệu của other
{
    other.m_size = 0;                              // 2. đặt other về rỗng
    other.m_data = nullptr;
    std::cout << "Move constructor\n";
}
```

Bước 2 là bắt buộc. Sau khi move, `other` vẫn tồn tại và vẫn bị hủy như bình thường. Nếu `other.m_data` còn trỏ vào vùng nhớ cũ, destructor của `other` sẽ `delete[]` vùng nhớ mà đối tượng mới đang dùng, gây double free. Đặt về `nullptr` thì an toàn vì `delete[] nullptr` không làm gì.

### Move assignment

Move assignment dùng khi đối tượng đích đã tồn tại. Nó giống move constructor, chỉ thêm hai bước như ở copy assignment: kiểm tra tự gán và giải phóng dữ liệu cũ:

```cpp
Buffer& operator=(Buffer&& other) noexcept
{
    if (this == &other) {          // 1. tự gán: a = std::move(a)
        return *this;
    }

    delete[] m_data;               // 2. giải phóng dữ liệu cũ của đối tượng đích

    m_data = other.m_data;         // 3. lấy dữ liệu của other
    m_size = other.m_size;

    other.m_data = nullptr;        // 4. đặt other về rỗng
    other.m_size = 0;

    std::cout << "Move assignment\n";
    return *this;
}
```

### Khi nào hàm nào được gọi

Thêm hai hàm trên cùng hàm `size()` vào class `Buffer`, rồi thử:

```cpp
Buffer a(4);
Buffer b = std::move(a);   // in ra: Move constructor
std::cout << a.size() << " " << b.size() << "\n";   // in ra: 0 4

Buffer c(8);
c = std::move(b);          // in ra: Move assignment
c = Buffer(16);            // in ra: Move assignment (Buffer(16) là đối tượng tạm)
```

Tóm lại:

| Câu lệnh | Hàm được gọi | Lý do |
|---|---|---|
| `Buffer b = a;` | Copy constructor | `a` còn được dùng tiếp |
| `Buffer b = std::move(a);` | Move constructor | Ta đánh dấu `a` không dùng nữa |
| `c = a;` | Copy assignment | `c` đã tồn tại, `a` còn được dùng tiếp |
| `c = std::move(a);` | Move assignment | `c` đã tồn tại, `a` không dùng nữa |
| `c = Buffer(16);` | Move assignment | Nguồn là đối tượng tạm |

Riêng câu lệnh `return frame;` trong hàm `makeFrame()` ở đầu mục, compiler tự move (hoặc bỏ hẳn bước sao chép), nên không cần viết `return std::move(frame);`.

:::warning Không dùng đối tượng sau khi đã move
Sau `Buffer b = std::move(a);`, `a` vẫn tồn tại nhưng đã rỗng. Ta chỉ nên hủy nó hoặc gán giá trị mới cho nó, không đọc dữ liệu của nó. Lỗi này compiler không cảnh báo.
:::

Cả hai hàm move đều được đánh dấu `noexcept`, nghĩa là không bao giờ ném exception. Điều này quan trọng với `std::vector` (Bài C8): khi mở rộng, `vector` chỉ move các phần tử sang vùng nhớ mới nếu move constructor là `noexcept`, nếu không nó sẽ sao chép từng phần tử. Hàm move chỉ gán lại con trỏ nên luôn có thể đánh dấu `noexcept`.

## Quy tắc năm và quy tắc không

Nếu một class cần tự viết một trong các hàm sau thì gần như chắc chắn cần viết đủ:

- Destructor
- Copy constructor
- Toán tử gán sao chép
- Move constructor
- Toán tử gán di chuyển

Lý do: class cần destructor riêng thường là vì nó sở hữu tài nguyên, và khi đó các hàm sao chép, di chuyển mặc định đều sai. Ba hàm đầu được gọi là **quy tắc ba** (rule of three) từ trước C++11; thêm hai hàm move thành **quy tắc năm** (rule of five).

Compiler chỉ tự tạo move constructor và move assignment khi class không tự khai báo destructor, copy constructor hay copy assignment. Vì vậy class `Buffer` có destructor riêng mà không viết hàm move thì không bị lỗi, nhưng mọi lần "di chuyển" thực chất đều là sao chép.

Cách tốt hơn nữa là tránh phải viết cả năm hàm, bằng cách để các thành viên tự quản lý tài nguyên theo RAII. Ví dụ nếu `Buffer` dùng `std::vector<uint8_t>` (Bài C8) thay vì con trỏ thô, sao chép sâu và di chuyển đều diễn ra tự động, và ta không cần viết hàm nào. Đây gọi là **quy tắc không** (rule of zero), và là cách ta nên ưu tiên.

## Cấm sao chép với = delete

Có những đối tượng mà việc sao chép không có ý nghĩa: một cổng UART, một file thiết bị GPIO, một kết nối mạng. Hai đối tượng cùng điều khiển `/dev/ttyS1` sẽ gây xung đột, và khi một đối tượng bị hủy, nó đóng cổng mà đối tượng kia vẫn đang dùng.

Với các class này, ta cấm sao chép bằng cách đánh dấu hai hàm sao chép là `= delete`, đúng như đã làm tạm ở Bài C4:

```cpp
class SerialPort {
public:
    SerialPort(const std::string& device);
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;
};

SerialPort port("/dev/ttyS1");
SerialPort other = port;
// lỗi: use of deleted function 'SerialPort::SerialPort(const SerialPort&)'

void send(SerialPort p);    // truyền theo giá trị cũng bị cấm
void send(SerialPort& p);   // phải truyền theo tham chiếu
```

Nhờ vậy, mọi ý định sao chép đều bị phát hiện lúc biên dịch thay vì gây crash lúc chạy.

Không sao chép được không có nghĩa là không di chuyển được. Một cổng UART không thể có hai chủ, nhưng hoàn toàn có thể chuyển từ chủ này sang chủ khác, ví dụ trả về từ một hàm mở cổng. Khi đó ta cấm sao chép và tự viết thêm hai hàm move, theo đúng cách đã làm với `Buffer`. Các class sở hữu tài nguyên trong thư viện chuẩn như `std::unique_ptr` (Bài C10) hay `std::thread` đều được thiết kế theo cách này.

Ngược lại với `= delete`, ta có `= default` để yêu cầu compiler tạo hàm mặc định một cách tường minh, như destructor ảo `virtual ~Sensor() = default;` ở Bài C6.

## Lỗi thường gặp

**Thiếu định nghĩa cho biến thành viên static: `undefined reference to 'Sensor::s_count'`**

```cpp
// sensor.h
class Sensor {
    static int s_count;     // chỉ khai báo, không có inline
};

// không có dòng "int Sensor::s_count = 0;" trong sensor.cpp
// lỗi khi link
```

Không có `inline`, dòng trong class chỉ là khai báo, chưa cấp phát bộ nhớ. Cách sửa là thêm định nghĩa vào file `.cpp`, hoặc dùng `inline static` (C++17).

**Dùng biến thành viên thường trong hàm static: `invalid use of member 'Adc::m_lastRaw' in static member function`**

```cpp
class Adc {
public:
    static float lastVoltage() { return m_lastRaw * 1.8f / 4095; }   // lỗi

private:
    uint16_t m_lastRaw = 0;
};
```

Hàm static không có `this`, nên không biết lấy `m_lastRaw` của đối tượng nào. Cách sửa là bỏ `static` nếu hàm cần dữ liệu của đối tượng.

**In giá trị kiểu `uint8_t` ra thành ký tự lạ**

```cpp
auto code = static_cast<uint8_t>(ModbusFunction::ReadHoldingRegisters);
std::cout << code;       // không in ra 3, mà in ký tự có mã 0x03 (không nhìn thấy được)
```

`uint8_t` thực chất là `unsigned char`, nên `std::cout` in nó như một ký tự. Lỗi này hay gặp khi in dữ liệu từ khung truyền thông. Cách sửa là ép sang `int` trước khi in:

```cpp
std::cout << static_cast<int>(code);   // in ra: 3
```

**Gói tin bị bỏ qua âm thầm do chuyển số từ bên ngoài sang enum mà không kiểm tra**

```cpp
auto func = static_cast<ModbusFunction>(frame[1]);   // frame[1] có thể là 0x7F do nhiễu

switch (func) {
case ModbusFunction::ReadHoldingRegisters: /* ... */ break;
case ModbusFunction::WriteSingleRegister:  /* ... */ break;
}
// với giá trị 0x7F: không trường hợp nào khớp, gói tin bị bỏ qua âm thầm
```

`static_cast` không kiểm tra giá trị có nằm trong enum hay không. Với dữ liệu từ UART, CAN hay mạng, luôn thêm nhánh `default` để xử lý giá trị không hợp lệ:

```cpp
switch (func) {
case ModbusFunction::ReadHoldingRegisters: /* ... */ break;
case ModbusFunction::WriteSingleRegister:  /* ... */ break;
default:
    std::cout << "Invalid function code\n";
    break;
}
```

**Quên `return *this` trong toán tử gán: `no return statement in function returning non-void`**

```cpp
Buffer& operator=(const Buffer& other)
{
    // ... sao chép ...
}   // cảnh báo
```

Chương trình có thể chạy bình thường với `a = b;`, nhưng crash hoặc cho kết quả sai với `a = b = c;`. Đừng bỏ qua cảnh báo này; hãy thêm `return *this;` ở cuối hàm.

**Crash `double free` do class sở hữu tài nguyên nhưng chỉ viết destructor**

Đây chính là lỗi double free ở Bài C4, và là vi phạm quy tắc ba. Khi viết destructor có `delete`, `fclose()` hay `close()`, hãy tự hỏi ngay: đối tượng này có nên sao chép được không? Nếu không, thêm hai dòng `= delete`. Nếu có, viết đủ copy constructor và toán tử gán sao chép sâu.
