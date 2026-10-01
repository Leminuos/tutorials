Trong một thiết bị thường có nhiều loại cảm biến: nhiệt độ, độ ẩm, áp suất. Các loại này có nhiều điểm chung như tên, giá trị đọc gần nhất, trạng thái lỗi,...nhưng cũng có phần riêng như cảm biến nhiệt độ có ngưỡng quá nhiệt, cảm biến áp suất có hệ số hiệu chỉnh...Trong C, ta thường giải quyết bằng một trong hai cách: sao chép phần chung sang từng struct hoặc đặt một struct chung làm thành viên đầu tiên rồi ép kiểu con trỏ qua lại:

```c
// C
struct Sensor {
    char name[16];
    float lastValue;
};

struct TemperatureSensor {
    struct Sensor base;      // phải là thành viên đầu tiên
    float threshold;
};

struct TemperatureSensor t;
struct Sensor* s = (struct Sensor*)&t;   // ép kiểu thủ công, compiler không kiểm tra
```

Cách thứ hai khá phổ biến trong code nhúng và cả trong nhân Linux, nhưng hoàn toàn dựa vào quy ước: đặt sai vị trí thành viên hay ép nhầm kiểu thì compiler không báo gì. C++ đưa ý tưởng này vào ngôn ngữ dưới tên **kế thừa** (inheritance) với sự kiểm tra đầy đủ của compiler.

## Kế thừa là gì

Kế thừa cho phép tạo một class mới dựa trên class có sẵn. Class có sẵn gọi là **lớp cha** (base class), class mới gọi là **lớp con** (derived class). Lớp con tự động có mọi thành viên của lớp cha và có thể thêm thành viên riêng.

```cpp
class Sensor {
public:
    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

private:
    std::string m_name;
};

class TemperatureSensor : public Sensor {   // TemperatureSensor kế thừa Sensor
public:
    float threshold() const { return m_threshold; }
    void setThreshold(float value) { m_threshold = value; }

private:
    float m_threshold = 70.0f;
};

TemperatureSensor temp;
temp.setName("Engine temperature"); // hàm của lớp cha
temp.setThreshold(85.0f);           // hàm của lớp con
std::cout << temp.name();           // in ra: Engine temperature
```

Cú pháp `class TemperatureSensor : public Sensor` đọc là `TemperatureSensor` kế thừa công khai từ `Sensor`. Từ khóa `public` ở đây nghĩa là các thành viên `public` của lớp cha vẫn là `public` khi truy cập qua lớp con. Trong thực tế, ta hầu như luôn dùng kế thừa `public`.

Kế thừa thể hiện quan hệ "is a": cảm biến nhiệt độ là một cảm biến. Đây là cách kiểm tra đơn giản để quyết định có nên dùng kế thừa hay không.

## Lớp con truy cập được gì của lớp cha

Lớp con chứa tất cả thành viên của lớp cha, kể cả thành viên `private`. Tuy nhiên, thành viên `private` của lớp cha chỉ các hàm của chính lớp cha được truy cập, lớp con cũng không ngoại lệ.

```cpp
class TemperatureSensor : public Sensor {
public:
    void printName() const
    {
        std::cout << m_name;    // lỗi: 'std::string Sensor::m_name' is private
        std::cout << name();    // đúng: dùng hàm public của lớp cha
    }
};
```

Quy tắc này có chủ đích: lớp cha tự bảo vệ dữ liệu của mình, kể cả trước lớp con. Nếu lớp cha đổi cách lưu trữ bên trong, các lớp con không bị ảnh hưởng miễn là các hàm `public` giữ nguyên.

## Phạm vi truy cập protected

Đôi khi lớp cha muốn cho lớp con truy cập trực tiếp một thành viên, nhưng vẫn giấu nó với bên ngoài. Khi đó ta dùng `protected`:

| Phạm vi | Lớp cha | Lớp con | Bên ngoài |
|---|---|---|---|
| `public` | Được | Được | Được |
| `protected` | Được | Được | Không |
| `private` | Được | Không | Không |

```cpp
class Sensor {
public:
    float lastValue() const { return m_lastValue; }

protected:
    float m_lastValue = 0.0f;   // lớp con được ghi trực tiếp giá trị đọc được
};

class TemperatureSensor : public Sensor {
public:
    float read()
    {
        m_lastValue = 27.5f;    // hợp lệ: m_lastValue là protected
        return m_lastValue;
    }
};

TemperatureSensor temp;
temp.read();
std::cout << temp.lastValue();  // in ra: 27.5
temp.m_lastValue = 0.0f;        // lỗi: 'float Sensor::m_lastValue' is protected
```

Không nên đặt mọi thứ thành `protected` cho tiện. Mỗi thành viên `protected` là một cam kết rằng mọi lớp con đều có thể phụ thuộc vào nó. Nguyên tắc chung là để dữ liệu ở `private` và chỉ dùng `protected` cho những gì lớp con thực sự cần.

## Gọi constructor của lớp cha

Khi tạo đối tượng lớp con, phần lớp cha bên trong nó phải được khởi tạo trước, bằng constructor của lớp cha. Ta gọi constructor lớp cha trong danh sách khởi tạo của lớp con, đặt trước các thành viên:

```cpp
class Sensor {
public:
    Sensor(const std::string& name) : m_name(name) {}
    const std::string& name() const { return m_name; }

private:
    std::string m_name;
};

class TemperatureSensor : public Sensor {
public:
    TemperatureSensor(const std::string& name, float threshold)
        : Sensor(name),              // khởi tạo phần lớp cha
          m_threshold(threshold)     // khởi tạo thành viên của lớp con
    {
    }

private:
    float m_threshold;
};

TemperatureSensor temp("Engine temperature", 85.0f);
std::cout << temp.name();   // in ra: Engine temperature
```

Nếu lớp cha có constructor mặc định, ta có thể không gọi, compiler sẽ tự gọi constructor mặc định. Còn nếu lớp cha chỉ có constructor cần tham số như `Sensor` ở trên, lớp con bắt buộc phải gọi nó.

## Thứ tự tạo và hủy trong kế thừa

Ở Bài 4, ta đã thấy thành viên được tạo trước thân constructor. Với kế thừa, lớp cha còn được tạo trước cả thành viên. Thứ tự đầy đủ là:

- **Khi tạo**: lớp cha → các thành viên của lớp con → thân constructor lớp con.
- **Khi hủy**: ngược lại hoàn toàn: thân destructor lớp con → các thành viên → lớp cha.

Dùng lại class `Tracer` từ Bài 4:

```cpp
class Sensor {
public:
    Sensor()  { std::cout << "Constructor Sensor\n"; }
    ~Sensor() { std::cout << "Destructor Sensor\n"; }
};

class TemperatureSensor : public Sensor {
public:
    TemperatureSensor() : m_filter("filter")
    {
        std::cout << "Constructor TemperatureSensor\n";
    }

    ~TemperatureSensor()
    {
        std::cout << "Destructor TemperatureSensor\n";
    }

private:
    Tracer m_filter;
};

int main()
{
    TemperatureSensor temp;
}

// in ra:
// Constructor Sensor
// Create filter
// Constructor TemperatureSensor
// Destructor TemperatureSensor
// Destroy filter
// Destructor Sensor
```

Vẫn là hình ảnh lắp ráp ở Bài C4: nền móng (lớp cha) có trước rồi đến linh kiện (thành viên), cuối cùng là phần hoàn thiện (thân constructor). Nhờ vậy, trong constructor và destructor của lớp con, ta luôn dùng được phần lớp cha một cách an toàn.

## Định nghĩa lại hàm của lớp cha

Lớp con có thể viết một hàm trùng tên với hàm của lớp cha để thay đổi hành vi. Khi gọi qua đối tượng lớp con, hàm của lớp con được dùng. Muốn gọi phiên bản của lớp cha, ta viết tên lớp cha cùng toán tử `::`.

```cpp
class Sensor {
public:
    Sensor(const std::string& name) : m_name(name) {}

    void print() const
    {
        std::cout << "Sensor: " << m_name << "\n";
    }

private:
    std::string m_name;
};

class TemperatureSensor : public Sensor {
public:
    TemperatureSensor(const std::string& name, float threshold)
        : Sensor(name), m_threshold(threshold) {}

    void print() const
    {
        Sensor::print();    // tái sử dụng phần in của lớp cha
        std::cout << "  Overheat threshold: " << m_threshold << "\n";
    }

private:
    float m_threshold;
};

TemperatureSensor temp("Engine", 85.0f);
temp.print();
// in ra:
// Sensor: Engine
//   Overheat threshold: 85
```

Cách viết `Sensor::print()` giúp lớp con mở rộng hành vi của lớp cha thay vì viết lại toàn bộ.

## Dùng con trỏ và tham chiếu lớp cha

Vì cảm biến nhiệt độ là một cảm biến, C++ cho phép con trỏ hoặc tham chiếu kiểu lớp cha trỏ tới đối tượng lớp con, không cần ép kiểu:

```cpp
TemperatureSensor temp("Engine", 85.0f);

Sensor* p = &temp;    // hợp lệ
Sensor& r = temp;     // hợp lệ
```

Điều này rất hữu ích: một hàm nhận `const Sensor&` dùng được cho mọi loại cảm biến, kể cả các loại được viết thêm sau này.

```cpp
void logSensor(const Sensor& s)
{
    std::cout << "[LOG] " << s.name() << "\n";
}

TemperatureSensor temp("Engine", 85.0f);
HumiditySensor humi("Server room");     // giả sử cũng kế thừa Sensor

logSensor(temp);   // in ra: [LOG] Engine
logSensor(humi);   // in ra: [LOG] Server room
```

Tuy nhiên, qua con trỏ hoặc tham chiếu lớp cha, ta chỉ thấy được phần lớp cha:

```cpp
Sensor& r = temp;

r.name();          // hợp lệ
r.threshold();     // lỗi: 'class Sensor' has no member named 'threshold'
r.print();         // hợp lệ, nhưng gọi Sensor::print(), không phải TemperatureSensor::print()
// in ra: Sensor: Engine   (mất dòng "Overheat threshold")
```

Dòng cuối là một hạn chế lớn. Ta muốn gọi `r.print()` thì mỗi loại cảm biến tự in theo cách của nó, nhưng compiler chỉ nhìn kiểu của `r` là `Sensor&` nên gọi `Sensor::print()`. Bài C6 sẽ giải quyết đúng vấn đề này bằng hàm ảo.

:::warning Object slicing
Chỉ con trỏ và tham chiếu mới giữ được đối tượng lớp con nguyên vẹn. Nếu sao chép đối tượng lớp con vào một biến kiểu lớp cha, phần riêng của lớp con bị cắt bỏ. Hiện tượng này gọi là **object slicing**.
:::

```cpp
Sensor copy = temp;    // chỉ sao chép phần Sensor, mất m_threshold
```

Vì vậy, hàm nhận đối tượng lớp cha nên khai báo tham số là `const Sensor&` hoặc `Sensor*`, không phải `Sensor`.

## Kế thừa hay thành phần

Ở Bài C4, class `Device` chứa `m_uart` và `m_led` làm thành viên. Đó là quan hệ "has a" (**thành phần**, composition): thiết bị có một cổng UART. Còn kế thừa là quan hệ "is a".

| Câu hỏi | Trả lời | Dùng |
|---|---|---|
| Cảm biến nhiệt độ là một cảm biến? | Đúng | Kế thừa |
| Thiết bị là một cổng UART? | Sai, thiết bị có một cổng UART | Thành phần |

Một lỗi thiết kế hay gặp là kế thừa chỉ để dùng lại code:

```cpp
class Device : public Uart { ... };   // sai về ý nghĩa: Device không phải là Uart
```

Khi đó mọi hàm `public` của `Uart` đều lộ ra qua `Device`, và `Device` không thể có hai cổng UART. Nguyên tắc chung: ưu tiên thành phần, chỉ dùng kế thừa khi quan hệ "là một" thực sự đúng.

## Lỗi thường gặp

**Quên từ khóa `public` khi kế thừa: `'Sensor' is an inaccessible base of 'TemperatureSensor'`**

```cpp
class TemperatureSensor : Sensor {   // thiếu public
    // ...
};

TemperatureSensor temp("Engine", 85.0f);
temp.name();
// lỗi: 'const std::string& Sensor::name() const' is inaccessible within this context
Sensor& r = temp;
// lỗi: 'Sensor' is an inaccessible base of 'TemperatureSensor'
```

Với `class`, kế thừa mặc định là `private`, biến mọi thành viên của lớp cha thành `private` trong lớp con. Cách sửa là luôn viết `: public Sensor`.

**Không gọi constructor của lớp cha: `no matching function for call to 'Sensor::Sensor()'`**

```cpp
class TemperatureSensor : public Sensor {
public:
    TemperatureSensor(float threshold) : m_threshold(threshold) {}   // lỗi
};
```

Không thấy lời gọi constructor lớp cha, compiler cố gọi `Sensor()` nhưng `Sensor` chỉ có constructor nhận `name`. Cách sửa là thêm `Sensor(...)` vào đầu danh sách khởi tạo.

**Hàm của lớp con che mất hàm cùng tên của lớp cha, chương trình chạy sai âm thầm**

```cpp
class Sensor {
public:
    void setOffset(float value) { m_offset = value; }
protected:
    float m_offset = 0.0f;
};

class PressureSensor : public Sensor {
public:
    void setOffset(int rawValue) { m_offset = rawValue * 0.1f; }
};

PressureSensor p;
p.setOffset(0.5f);   // mong gọi bản float của lớp cha
                     // thực tế gọi bản int của lớp con: 0.5f bị cắt thành 0
```

Không có lỗi hay cảnh báo nào với các tùy chọn `-Wall -Wextra`. Khi lớp con khai báo một hàm tên `setOffset`, mọi hàm `setOffset` của lớp cha bị che khuất, dù khác tham số. Nạp chồng hàm (Bài C1) không hoạt động xuyên qua lớp cha và lớp con. Cách sửa là đưa các hàm của lớp cha vào phạm vi lớp con bằng `using`:

```cpp
class PressureSensor : public Sensor {
public:
    using Sensor::setOffset;              // giữ lại bản float của lớp cha
    void setOffset(int rawValue) { ... }  // thêm bản int
};
```

Tốt hơn nữa là đặt tên khác đi, như `setRawOffset(int)`, để tránh nhầm lẫn ngay từ đầu.

**Truyền đối tượng lớp con theo giá trị vào hàm nhận lớp cha, mất phần riêng của lớp con**

```cpp
void printSensor(Sensor s)   // truyền theo giá trị
{
    s.print();
}

printSensor(temp);   // temp bị cắt thành Sensor, mất phần riêng của TemperatureSensor
```

Đây là object slicing đã nhắc ở trên. Cách sửa là khai báo tham số `const Sensor& s`. Kết hợp với hàm ảo ở Bài C6, khi đó `s.print()` mới gọi đúng phiên bản của từng loại cảm biến.
