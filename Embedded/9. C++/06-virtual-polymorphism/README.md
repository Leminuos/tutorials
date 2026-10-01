Ở cuối Bài C5, ta gặp một hạn chế: khi gọi `print()` qua tham chiếu `Sensor&` trỏ tới một `TemperatureSensor`, compiler vẫn gọi `Sensor::print()` vì nó chỉ nhìn vào kiểu của tham chiếu, không nhìn vào đối tượng thật.

Người làm nhúng bằng C thường giải quyết vấn đề này bằng con trỏ hàm đặt trong struct. Mỗi loại thiết bị gán các con trỏ hàm tới cài đặt riêng của mình và code dùng chung chỉ việc gọi qua con trỏ. Nhân Linux dùng đúng kỹ thuật này cho driver, với các struct như `file_operations`:

```c
// C
struct SensorOps {
    float (*read)(struct Sensor* s);
    void  (*print)(struct Sensor* s);
};

struct Sensor {
    const struct SensorOps* ops;   // mỗi loại cảm biến trỏ tới bảng hàm riêng
    char name[16];
};

s->ops->print(s);   // gọi đúng hàm của loại cảm biến thực tế
```

Cách này hiệu quả nhưng phải tự khai báo bảng hàm, tự gán con trỏ và compiler không kiểm tra được việc gán nhầm hay thiếu hàm. C++ tự động hóa toàn bộ bằng **hàm ảo** (virtual function).

## Hàm ảo

Thêm từ khóa `virtual` trước hàm ở lớp cha. Khi đó, lời gọi qua con trỏ hoặc tham chiếu lớp cha sẽ chọn hàm theo kiểu thật của đối tượng, không theo kiểu của con trỏ hay tham chiếu.

```cpp
class Sensor {
public:
    Sensor(const std::string& name) : m_name(name) {}
    virtual ~Sensor() = default;   // destructor ảo

    virtual void print() const
    {
        std::cout << "Sensor: " << m_name << "\n";
    }

    const std::string& name() const { return m_name; }

private:
    std::string m_name;
};

class TemperatureSensor : public Sensor {
public:
    TemperatureSensor(const std::string& name, float threshold)
        : Sensor(name), m_threshold(threshold) {}

    void print() const override
    {
        Sensor::print();
        std::cout << "  Overheat threshold: " << m_threshold << "\n";
    }

private:
    float m_threshold;
};

TemperatureSensor temp("Engine", 85.0f);
Sensor& r = temp;
r.print();
// in ra:
// Sensor: Engine
//   Overheat threshold: 85
```

So với Bài C5, ta chỉ thêm `virtual` ở lớp cha và `override` ở lớp con nhưng giờ `r.print()` đã gọi đúng `TemperatureSensor::print()`.

Việc định nghĩa lại một hàm ảo ở lớp con gọi là **ghi đè** (override). Còn việc định nghĩa lại hàm không ảo như ở Bài C5 chỉ là **che khuất** (hide): hàm lớp con chỉ được gọi khi gọi trực tiếp qua đối tượng hoặc con trỏ lớp con.

## Đa hình

Sức mạnh thực sự của hàm ảo thể hiện khi ta xử lý nhiều loại đối tượng qua cùng một kiểu lớp cha. Khả năng này gọi là **đa hình** (polymorphism): cùng một lời gọi hàm, mỗi đối tượng phản ứng theo cách riêng.

```cpp
class Sensor {
public:
    Sensor(const std::string& name) : m_name(name) {}
    virtual ~Sensor() = default;

    virtual float read() { return 0.0f; }
    const std::string& name() const { return m_name; }

private:
    std::string m_name;
};

class TemperatureSensor : public Sensor {
public:
    using Sensor::Sensor;           // dùng lại constructor của lớp cha
    float read() override { return 27.5f; }
};

class HumiditySensor : public Sensor {
public:
    using Sensor::Sensor;
    float read() override { return 64.0f; }
};

TemperatureSensor temp("Temperature");
HumiditySensor humi("Humidity");

Sensor* sensors[] = { &temp, &humi };

for (Sensor* s : sensors) {
    std::cout << s->name() << ": " << s->read() << "\n";
}
// in ra:
// Temperature: 27.5
// Humidity: 64
```

Dòng `using Sensor::Sensor;` là cú pháp của C++11 cho phép lớp con dùng lại constructor của lớp cha, tránh phải viết lại constructor chỉ để chuyển tham số lên lớp cha.

Vòng lặp trên không cần biết có những loại cảm biến nào. Khi thêm một loại cảm biến mới, như cảm biến áp suất, ta chỉ viết thêm class mới; vòng lặp giữ nguyên, không sửa một dòng nào. Đây là lý do đa hình là nền tảng của hầu hết các framework và driver lớn.

## Từ khóa override

Từ khóa `override` đặt sau khai báo hàm ở lớp con, báo cho compiler biết hàm này phải ghi đè một hàm ảo của lớp cha. Nếu không khớp, compiler báo lỗi ngay.

Điều này quan trọng vì hàm ghi đè phải khớp hoàn toàn với hàm lớp cha: cùng tên, cùng tham số, cùng `const`. Chỉ cần lệch một chi tiết, hàm của lớp con trở thành một hàm mới, và lời gọi qua lớp cha âm thầm dùng hàm cũ.

```cpp
class Sensor {
public:
    virtual void print() const;
};

class TemperatureSensor : public Sensor {
public:
    void print() override;
    // lỗi: 'void TemperatureSensor::print()' marked 'override', but does not override
    // (thiếu const nên không khớp với hàm lớp cha)
};
```

:::tip Luôn viết override
Luôn viết `override` khi ghi đè hàm ảo. Không cần viết lại `virtual` ở lớp con, vì hàm ghi đè một hàm ảo thì tự động cũng là hàm ảo.
:::

## Hàm ảo hoạt động thế nào

Phía sau, compiler làm đúng những gì ta tự làm bằng con trỏ hàm trong C:

- Mỗi class có hàm ảo được compiler tạo một **bảng hàm ảo** (vtable), chứa địa chỉ các hàm ảo của class đó. Đây chính là `struct SensorOps` ở phần mở đầu.
- Mỗi đối tượng của class đó có thêm một con trỏ ẩn trỏ tới bảng hàm ảo của class mình. Đây chính là thành viên `ops`.
- Lời gọi hàm ảo được dịch thành: lấy con trỏ ẩn, tra bảng, gọi hàm theo địa chỉ tìm được.

```
  Đối tượng TemperatureSensor          vtable của TemperatureSensor
 +---------------------------+        +------------------------------+
 | con trỏ ẩn (vptr)  -------+------> | &TemperatureSensor::~...     |
 | m_name                    |        | &TemperatureSensor::print    |
 | m_threshold               |        +------------------------------+
 +---------------------------+
```

Ta có thể thấy con trỏ ẩn này qua kích thước đối tượng:

```cpp
class Plain   { int x; void f(); };
class Virtual { int x; virtual void f(); };

std::cout << sizeof(Plain);    // in ra: 4
std::cout << sizeof(Virtual);  // in ra: 16 trên máy tính 64-bit, 8 trên BBB (32-bit)
```

Chi phí của hàm ảo là thêm một con trỏ cho mỗi đối tượng và mỗi lời gọi hàm phải tra bảng thay vì gọi trực tiếp. Với hầu hết ứng dụng, kể cả trên BeagleBone Black, chi phí này không đáng kể. Chỉ nên cân nhắc với những hàm được gọi hàng triệu lần mỗi giây, như hàm xử lý từng điểm ảnh.

## Destructor ảo

Khi đối tượng lớp con được tạo bằng `new` và bị `delete` qua con trỏ lớp cha, destructor nào được gọi? Nếu destructor của lớp cha không ảo, chỉ destructor lớp cha được gọi và phần lớp con không bao giờ được dọn dẹp.

Dùng lại class `Tracer` từ Bài C4:

```cpp
class Sensor {
public:
    ~Sensor() { std::cout << "Destructor Sensor\n"; }   // không ảo
    virtual float read() { return 0.0f; }
};

class TemperatureSensor : public Sensor {
public:
    TemperatureSensor() : m_filter("filter") {}
    ~TemperatureSensor() { std::cout << "Destructor TemperatureSensor\n"; }

private:
    Tracer m_filter;
};

Sensor* s = new TemperatureSensor();   // in ra: Create filter
delete s;
// cảnh báo: deleting object of polymorphic class type 'Sensor'
//           which has non-virtual destructor might cause undefined behavior
// in ra: Destructor Sensor
// (không có "Destructor TemperatureSensor" và "Destroy filter")
```

Nếu `m_filter` giữ bộ nhớ hay một file thiết bị, tài nguyên đó bị rò rỉ. Cách sửa là khai báo destructor lớp cha là `virtual`:

```cpp
class Sensor {
public:
    virtual ~Sensor() { std::cout << "Destructor Sensor\n"; }
    virtual float read() { return 0.0f; }
};

// delete s giờ in ra:
// Destructor TemperatureSensor
// Destroy filter
// Destructor Sensor
```

Khi destructor không cần làm gì, ta viết gọn `virtual ~Sensor() = default;`, nghĩa là dùng destructor mặc định do compiler tạo nhưng đánh dấu là ảo.

:::warning Lớp cha phải có destructor ảo
Class nào có hàm ảo hoặc được thiết kế để làm lớp cha thì destructor phải là `virtual`.
:::

## Hàm thuần ảo và lớp trừu tượng

Trong các ví dụ trên, `Sensor::read()` trả về `0.0f`, một giá trị vô nghĩa, chỉ để có thân hàm. Thực tế, một cảm biến chung chung không có cách đọc nào cả, chỉ các loại cảm biến cụ thể mới biết cách đọc.

C++ cho phép khai báo hàm ảo không có thân bằng cách thêm `= 0`. Hàm này gọi là **hàm thuần ảo** (pure virtual function). Class có ít nhất một hàm thuần ảo gọi là **lớp trừu tượng** (abstract class).

```cpp
class Sensor {
public:
    virtual ~Sensor() = default;
    virtual float read() = 0;           // hàm thuần ảo: lớp con bắt buộc phải cài đặt
    virtual const char* unit() const = 0;
};

class TemperatureSensor : public Sensor {
public:
    float read() override { return 27.5f; }
    const char* unit() const override { return "degC"; }
};

Sensor s;              // lỗi: cannot declare variable 's' to be of abstract type 'Sensor'
TemperatureSensor t;   // hợp lệ
Sensor& r = t;         // hợp lệ: con trỏ và tham chiếu kiểu lớp trừu tượng vẫn dùng được
std::cout << r.read() << " " << r.unit();   // in ra: 27.5 degC
```

Lớp trừu tượng có hai đặc điểm: không thể tạo đối tượng từ nó, và lớp con phải cài đặt tất cả hàm thuần ảo thì mới tạo được đối tượng. Nếu thiếu dù chỉ một hàm, lớp con cũng thành lớp trừu tượng.

Một lớp trừu tượng chỉ gồm các hàm thuần ảo đóng vai trò như một **giao diện** (interface): nó mô tả một đối tượng làm được gì, mà không quy định làm như thế nào.

## Ứng dụng: lớp trừu tượng phần cứng

Đây là ứng dụng quan trọng nhất của bài này đối với lập trình nhúng. Quy trình thường gặp là phát triển trên máy tính trước, chạy trên board sau. Nhưng máy tính không có cảm biến thật, vậy làm sao chạy thử phần đọc cảm biến? Câu trả lời là tách phần phần cứng ra sau một giao diện trừu tượng.

```
                 +-----------------------+
                 |        Monitor        |   logic ứng dụng
                 +-----------+-----------+
                             | dùng TemperatureSource&
                 +-----------v-----------+
                 |   TemperatureSource   |   giao diện trừu tượng
                 +-----------+-----------+
                             |
             +---------------+---------------+
             |                               |
   +---------v---------+          +----------v----------+
   |   Ds18b20Source   |          |   SimulatedSource   |
   |   (trên board)    |          |   (trên máy tính)   |
   +-------------------+          +---------------------+
```

Trước hết là giao diện mô tả "một nguồn cung cấp nhiệt độ":

```cpp
class TemperatureSource {
public:
    virtual ~TemperatureSource() = default;
    virtual float readCelsius() = 0;
    virtual bool isConnected() const = 0;
};
```

Cài đặt thứ nhất dùng trên BeagleBone Black, đọc cảm biến DS18B20 qua giao tiếp 1-Wire. Driver của Linux đưa giá trị đo ra file `w1_slave`, trong đó dòng thứ hai kết thúc bằng `t=23125`, nghĩa là 23,125 °C:

```cpp
#include <cstdio>
#include <cstring>
#include <cstdlib>

class Ds18b20Source : public TemperatureSource {
public:
    Ds18b20Source(const std::string& path) : m_path(path) {}

    float readCelsius() override
    {
        FILE* f = std::fopen(m_path.c_str(), "r");
        if (!f) {
            m_connected = false;
            return 0.0f;
        }

        char line[128];
        long milliCelsius = 0;
        while (std::fgets(line, sizeof(line), f)) {
            const char* t = std::strstr(line, "t=");
            if (t) {
                milliCelsius = std::strtol(t + 2, nullptr, 10);
            }
        }
        std::fclose(f);

        m_connected = true;
        return milliCelsius / 1000.0f;
    }

    bool isConnected() const override { return m_connected; }

private:
    std::string m_path;
    bool m_connected = false;
};
```

Cài đặt thứ hai dùng trên máy tính, sinh dữ liệu giả lập tăng dần để thử cả trường hợp quá nhiệt:

```cpp
class SimulatedSource : public TemperatureSource {
public:
    float readCelsius() override
    {
        m_value += 4.0f;
        if (m_value > 90.0f) {
            m_value = 70.0f;
        }
        return m_value;
    }

    bool isConnected() const override { return true; }

private:
    float m_value = 70.0f;
};
```

Phần logic của ứng dụng chỉ làm việc với `TemperatureSource&`, hoàn toàn không biết phía sau là cảm biến thật hay giả lập:

```cpp
class Monitor {
public:
    Monitor(TemperatureSource& source, float threshold)
        : m_source(source), m_threshold(threshold) {}

    void update()
    {
        if (!m_source.isConnected() && m_checked) {
            std::cout << "Sensor disconnected\n";
            return;
        }
        m_checked = true;

        float t = m_source.readCelsius();
        std::cout << "Temperature: " << t;
        if (t > m_threshold) {
            std::cout << "  -> OVERHEAT";
        }
        std::cout << "\n";
    }

private:
    TemperatureSource& m_source;   // thành viên tham chiếu: bắt buộc khởi tạo trong danh sách (Bài C3)
    float m_threshold;
    bool m_checked = false;
};
```

Khi chạy, ta chỉ cần chọn cài đặt ở một chỗ duy nhất:

```cpp
int main()
{
    SimulatedSource source;     // trên board: Ds18b20Source source("/sys/bus/w1/devices/28-.../w1_slave");
    Monitor monitor(source, 80.0f);

    for (int i = 0; i < 3; ++i) {
        monitor.update();
    }
}

// in ra:
// Temperature: 74
// Temperature: 78
// Temperature: 82  -> OVERHEAT
```

Lớp `Monitor` được viết và thử hoàn toàn trên máy tính. Khi lên board, ta chỉ đổi dòng tạo `source`. Bài C14 sẽ hướng dẫn dùng `#ifdef` để việc chọn này diễn ra tự động theo nơi biên dịch. Cách tổ chức này áp dụng được cho mọi ngoại vi như GPIO, I2C, SPI.

## Lỗi thường gặp

**Hàm lớp con không khớp chữ ký nên không ghi đè, chương trình chạy sai âm thầm**

```cpp
class Sensor {
public:
    virtual float read() const;
};

class TemperatureSensor : public Sensor {
public:
    float read();    // thiếu const, không có override
};

Sensor& r = temp;
r.read();            // gọi Sensor::read(), không có lỗi hay cảnh báo nào
```

Chương trình biên dịch bình thường nhưng gọi nhầm hàm. Cách phòng tránh là luôn viết `override`; khi đó compiler báo lỗi ngay như ở mục "Từ khóa override".

**Cảnh báo `deleting object of polymorphic class type ... which has non-virtual destructor`**

Lớp cha thiếu destructor ảo. Như đã trình bày ở mục "Destructor ảo", `delete` qua con trỏ lớp cha chỉ gọi destructor lớp cha, phần lớp con bị rò rỉ. Đừng bỏ qua cảnh báo này; thêm `virtual ~Sensor() = default;` vào lớp cha.

**Quên cài đặt một hàm thuần ảo: `cannot declare variable 'source' to be of abstract type 'SimulatedSource'`**

```cpp
class SimulatedSource : public TemperatureSource {
public:
    float readCelsius() override { return 25.0f; }
    // quên isConnected()
};

SimulatedSource source;
// lỗi: cannot declare variable 'source' to be of abstract type 'SimulatedSource'
// note: because the following virtual functions are pure within 'SimulatedSource':
//       'virtual bool TemperatureSource::isConnected() const'
```

Thông báo lỗi liệt kê chính xác những hàm còn thiếu. Cách sửa là cài đặt đủ các hàm đó.

**Gọi hàm ảo trong constructor nhưng hàm của lớp cha được chạy, hoặc crash với `pure virtual method called`**

```cpp
class Sensor {
public:
    Sensor() { init(); }
    virtual ~Sensor() = default;
    virtual void init() { std::cout << "Sensor::init\n"; }
};

class TemperatureSensor : public Sensor {
public:
    void init() override { std::cout << "TemperatureSensor::init\n"; }
};

TemperatureSensor t;   // in ra: Sensor::init
```

Theo thứ tự tạo ở Bài C5, khi constructor của `Sensor` chạy, phần `TemperatureSensor` chưa được tạo, nên C++ không cho gọi xuống hàm của lớp con. Destructor cũng tương tự theo chiều ngược lại. Nếu `init()` là hàm thuần ảo, chương trình thậm chí crash với thông báo `pure virtual method called`. Cách sửa là không gọi hàm ảo trong constructor/destructor; nếu cần bước khởi tạo riêng cho từng loại, hãy làm nó trong constructor của chính lớp con.

**Truyền đối tượng theo giá trị làm mất tính đa hình**

```cpp
void printSensor(Sensor s) { s.print(); }   // với Sensor trừu tượng: lỗi biên dịch
                                            // với Sensor thường: object slicing (Bài C5)
```

Đa hình chỉ hoạt động qua con trỏ hoặc tham chiếu. Cách sửa là dùng `const Sensor&` hoặc `Sensor*`.
