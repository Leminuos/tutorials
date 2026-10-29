Ở [Bài C4](../04-raii/README.md), ta đã thấy RAII giải quyết memory leak và double free bằng cách gắn việc giải phóng vào destructor. Nhưng với mỗi đối tượng tạo bằng `new`, ta vẫn phải tự viết `delete` trong destructor của chủ sở hữu, tự cấm sao chép và chỉ cần quên một chỗ là lỗi quay trở lại.

Thư viện chuẩn có sẵn các class làm việc này thay ta, gọi là **smart pointer**. Bài này trình bày smart pointer là gì, các loại smart pointer, nó dùng để làm gì và khi nào chọn loại nào.

Để quan sát khi nào đối tượng được tạo và hủy, trong bài ta dùng một class nhỏ in ra thông báo:

```cpp
#include <iostream>
#include <string>

class Tracer {
public:
    Tracer(const std::string& name) : m_name(name) { std::cout << "Create " << m_name << "\n"; }
    ~Tracer() { std::cout << "Destroy " << m_name << "\n"; }

private:
    std::string m_name;
};
```

## Smart pointer là gì

**Smart pointer** là một class RAII bọc lấy con trỏ thô: nó giữ con trỏ tới đối tượng trên heap và tự `delete` đối tượng đó khi bị destructor. Bên ngoài, ta dùng nó giống hệt con trỏ thường với `->` và `*`.

Để thấy rõ cách hoạt động của smart pointer thì ta tự viết một phiên bản tối giản như sau:

```cpp
template <typename T>
class SimplePtr {
public:
    explicit SimplePtr(T* p) : m_ptr(p) {}
    ~SimplePtr() { delete m_ptr; }                 // destructor: tự delete

    SimplePtr(const SimplePtr&) = delete;          // cấm sao chép
    SimplePtr& operator=(const SimplePtr&) = delete;

    T* operator->() const { return m_ptr; }        // cho phép viết p->...
    T& operator*() const { return *m_ptr; }        // cho phép viết *p

private:
    T* m_ptr;
};
```

Đó chính là một class RAII, chỉ khác là dùng được cho mọi kiểu `T` nhờ template. Smart pointer của thư viện chuẩn hoàn thiện hơn nhiều nhưng ý tưởng cốt lõi là vậy.

So với con trỏ thô, smart pointer mang lại ba tác dụng:

| Tác dụng | Giải quyết vấn đề |
|---|---|
| Tự `delete` khi bị destructor, ở mọi đường thoát | Memory leak |
| Kiểu dữ liệu cho biết ai sở hữu đối tượng | Double free, không rõ ai phải `delete` |
| Biết đối tượng đã bị hủy hay chưa (`weak_ptr`) | Dangling pointer, use-after-free |

Các smart pointer nằm trong thư viện `<memory>`.

## Ba loại smart pointer

| Loại | Ý nghĩa | Sao chép được? | Tự `delete` khi |
|---|---|---|---|
| `std::unique_ptr<T>` | Sở hữu duy nhất: chỉ một chủ | Không, chỉ chuyển bằng `std::move` | `unique_ptr` bị hủy |
| `std::shared_ptr<T>` | Sở hữu chung: nhiều chủ | Có | `shared_ptr` cuối cùng bị hủy |
| `std::weak_ptr<T>` | Không sở hữu, chỉ quan sát đối tượng của `shared_ptr` | Có | Không bao giờ tự `delete` |

```
  unique_ptr:   [owner] ------> object

  shared_ptr:   [owner A] --+
                [owner B] --+--> object   (reference count = 3)
                [owner C] --+

  weak_ptr:     [observer] - - > object   (không tăng reference count)
```

## unique_ptr

`std::unique_ptr` dùng khi đối tượng có **đúng một chủ sở hữu**. Đây là loại dùng nhiều nhất và là lựa chọn mặc định.

### Tạo và sử dụng

```cpp
#include <memory>

void process(bool error)
{
    auto sensor = std::make_unique<Tracer>("sensor");   // tạo đối tượng trên heap

    if (error) {
        return;            // không cần delete
    }

}   // sensor bị hủy -> tự delete đối tượng Tracer

process(true);
// in ra:
// Create sensor
// Destroy sensor
```

`std::make_unique<Tracer>("sensor")` tương đương `new Tracer("sensor")` nhưng trả về loại `unique_ptr`. Ta luôn dùng `make_unique` thay vì `new` để trong code không còn lệnh `new` nào phải cần tìm lệnh `delete` tương ứng.

### Các thao tác

| Thao tác | Ý nghĩa |
|---|---|
| `std::make_unique<T>(args...)` | Tạo đối tượng `T` trên heap, `args` được chuyển cho constructor |
| `p->f()`, `*p` | Dùng đối tượng như con trỏ thường |
| `if (p)`, `p == nullptr` | Kiểm tra `p` có đang giữ đối tượng không |
| `p.get()` | Lấy con trỏ thô để dùng tạm, `p` vẫn là chủ sở hữu |
| `p.reset()` | `delete` đối tượng ngay, `p` trở thành rỗng |
| `p.reset(new T(...))` | `delete` đối tượng cũ, nhận giữ đối tượng mới |
| `p.release()` | Trả về con trỏ thô và từ bỏ quyền sở hữu, không `delete`. Người nhận phải tự `delete` |
| `std::move(p)` | Chuyển quyền sở hữu sang `unique_ptr` khác, `p` trở thành rỗng |

```cpp
auto p = std::make_unique<Tracer>("a");     // in ra: Create a

if (p) {
    std::cout << "p holds an object\n";      // in ra: p holds an object
}

Tracer* raw = p.get();          // raw chỉ để dùng tạm, không được delete raw

p.reset(new Tracer("b"));       // in ra: Create b
                                //        Destroy a   (đối tượng cũ bị delete)
p.reset();                      // in ra: Destroy b
std::cout << (p == nullptr);    // in ra: 1
```

`release()` hiếm khi cần, chỉ dùng khi phải trao con trỏ cho code cũ hoặc thư viện C sẽ tự giải phóng. Muốn hủy đối tượng sớm thì dùng `reset()`, không dùng `release()`: gọi `p.release();` mà bỏ qua giá trị trả về là memory leak.

Với mảng, `unique_ptr<T[]>` gọi `delete[]` thay vì `delete` và hỗ trợ truy cập phần tử bằng `[]`:

```cpp
auto buffer = std::make_unique<uint8_t[]>(256);   // mảng 256 byte, khởi tạo bằng 0
buffer[0] = 0xAA;
```

Thường thì `std::vector` tiện hơn nhưng `unique_ptr<T[]>` hữu ích khi cần một vùng nhớ kích thước cố định, không cần thay đổi kích thước sau khi tạo.

### Không sao chép được, chỉ chuyển được

Nếu `unique_ptr` sao chép được, ta sẽ có hai `unique_ptr` cùng giữ một đối tượng, và khi cả hai bị hủy, đối tượng bị `delete` hai lần (double free). Vì vậy compiler cấm sao chép:

```cpp
auto a = std::make_unique<Tracer>("sensor");
auto b = a;              // lỗi biên dịch: use of deleted function
```

Thay vào đó, ta **chuyển** (move) quyền sở hữu. Hình dung `unique_ptr` như chiếc chìa khóa duy nhất của một căn nhà: không thể đánh thêm chìa nhưng có thể trao chìa cho người khác. Sau khi trao, người cũ không còn chìa nữa:

```cpp
auto a = std::make_unique<Tracer>("sensor");
auto c = std::move(a);   // c nhận đối tượng, a trở thành rỗng

std::cout << (a == nullptr);   // in ra: 1
```

```
  trước:   a ----> [sensor]        c = (rỗng)
  sau:     a = (rỗng)              c ----> [sensor]
```

Đối tượng `sensor` không bị tạo lại hay sao chép, nó vẫn nằm nguyên trên heap, chỉ có người giữ nó thay đổi. Tại mọi thời điểm, đối tượng luôn có đúng một chủ.

Move là cơ chế chung của C++, không riêng gì smart pointer. Mọi class sở hữu tài nguyên trong thư viện chuẩn như `std::vector`, `std::string` đều hỗ trợ move và lợi ích ở đây là tránh sao chép dữ liệu lớn.

Ví dụ ghi âm 1 giây với tần số lấy mẫu 48 kHz, được 48000 mẫu 16-bit, khoảng 96 KB:

```cpp
std::vector<int16_t> samples(48000);

std::vector<int16_t> copied = samples;             // sao chép: cấp phát 96 KB mới và chép từng mẫu
std::vector<int16_t> moved  = std::move(samples);  // di chuyển: chỉ chuyển con trỏ tới vùng nhớ
```

```
  sao chép:   samples --> [96 KB]            di chuyển:   samples --> (rỗng)
              copied  --> [96 KB bản sao]                 moved   --> [96 KB]  (vẫn vùng nhớ cũ)
```

Bên trong, `std::vector` chỉ là một con trỏ tới vùng nhớ trên heap cùng với kích thước. Sao chép phải cấp phát vùng nhớ mới và chép toàn bộ dữ liệu; di chuyển chỉ trao con trỏ đó cho `vector` mới rồi đặt `vector` cũ về rỗng, nhanh như nhau dù dữ liệu lớn bao nhiêu.

### Ví dụ: thành viên class

Đây là cách dùng `unique_ptr` quan trọng nhất. Một gateway IoT sở hữu hai module: modem 4G và GPS. Nếu giữ bằng con trỏ thô `Tracer*`, class phải tự `delete` chúng trong destructor và tự cấm sao chép. Với `unique_ptr`:

```cpp
class Gateway {
public:
    Gateway()
        : m_modem(std::make_unique<Tracer>("modem")),
          m_gps(std::make_unique<Tracer>("gps"))
    {
    }

    // không cần destructor
    // không cần cấm sao chép: unique_ptr đã không sao chép được

private:
    std::unique_ptr<Tracer> m_modem;
    std::unique_ptr<Tracer> m_gps;
};

int main()
{
    Gateway gateway;
}
// in ra:
// Create modem
// Create gps
// Destroy gps
// Destroy modem
```

Class không có destructor nào. Các thành viên `unique_ptr` tự hủy theo thứ tự ngược với khai báo, mỗi cái tự `delete` đối tượng của mình. Nếu constructor thất bại khi tạo `m_gps`, `m_modem` đã tạo xong vẫn tự được giải phóng. Class tự quản lý tài nguyên mà không cần viết destructor, copy constructor hay toán tử gán nào, gọi là **quy tắc không** (rule of zero).

### Ví dụ: hàm tạo đối tượng đa hình

`unique_ptr` kiểu lớp cha giữ được đối tượng lớp con. Ví dụ thiết bị gửi dữ liệu qua UART hoặc qua TCP tùy cấu hình, hai cách gửi có chung một interface:

```cpp
class Transport {
public:
    virtual ~Transport() = default;
    virtual void send(const std::string& data) = 0;
};

class UartTransport : public Transport {
public:
    void send(const std::string& data) override { std::cout << "UART: " << data << "\n"; }
};

class TcpTransport : public Transport {
public:
    void send(const std::string& data) override { std::cout << "TCP: " << data << "\n"; }
};
```

Hàm tạo đối tượng trả về `unique_ptr<Transport>`, nơi gọi nhận luôn quyền sở hữu mà không cần biết lớp con cụ thể:

```cpp
std::unique_ptr<Transport> createTransport(bool useTcp)
{
    if (useTcp) {
        return std::make_unique<TcpTransport>();
    }
    return std::make_unique<UartTransport>();
}
```

Class `Reporter` nhận và sở hữu đối tượng `Transport`:

```cpp
class Reporter {
public:
    Reporter(std::unique_ptr<Transport> transport) : m_transport(std::move(transport)) {}

    void report(float value) { m_transport->send(std::to_string(value)); }

private:
    std::unique_ptr<Transport> m_transport;
};

Reporter reporter(createTransport(true));
reporter.report(25.5f);      // in ra: TCP: 25.500000
```

`Reporter` không thể vô tình sống lâu hơn đối tượng `Transport` của nó: khi `reporter` bị hủy, `Transport` bị hủy theo, và nhờ destructor ảo, destructor của đúng lớp con được gọi.

Kết hợp với `std::vector`, ta có một danh sách sở hữu nhiều loại đối tượng khác nhau:

```cpp
std::vector<std::unique_ptr<Transport>> links;

links.push_back(std::make_unique<UartTransport>());
links.push_back(std::make_unique<TcpTransport>());

for (const auto& link : links) {
    link->send("hello");
}
// in ra:
// UART: hello
// TCP: hello
// khi links bị hủy, mọi đối tượng đều được delete
```

### Ví dụ: tài nguyên kiểu C với custom deleter

Mặc định `unique_ptr` gọi `delete`, nhưng ta có thể chỉ định cách giải phóng khác, gọi là **deleter**. Rất tiện cho thư viện C, nơi tài nguyên được giải phóng bằng hàm riêng như `fclose()`.

Deleter là một struct có `operator()`, nghĩa là đối tượng của nó gọi được như một hàm:

```cpp
#include <cstdio>
#include <memory>

struct FileCloser {
    void operator()(FILE* f) const
    {
        std::fclose(f);
        std::cout << "File closed\n";
    }
};

using FilePtr = std::unique_ptr<FILE, FileCloser>;   // đặt tên ngắn, như typedef trong C
```

Ví dụ đọc thời gian hệ thống đã chạy từ `/proc/uptime`. File luôn được đóng ở mọi đường thoát mà không cần tự viết class bọc `FILE*`:

```cpp
double readUptime()
{
    FilePtr file(std::fopen("/proc/uptime", "r"));
    if (!file) {
        return -1.0;
    }

    double seconds = 0.0;
    if (std::fscanf(file.get(), "%lf", &seconds) != 1) {
        return -1.0;        // file tự đóng
    }
    return seconds;         // file tự đóng
}

std::cout << readUptime() << "\n";
// in ra:
// File closed
// 12345.67
```

`unique_ptr` không gọi deleter khi đang rỗng, nên nếu `fopen()` thất bại, `fclose()` không bị gọi với `nullptr`. Cách này chỉ áp dụng cho tài nguyên biểu diễn bằng con trỏ; tài nguyên dạng số nguyên như file descriptor của `open()` vẫn cần một class RAII tự viết.

### Chi phí

`unique_ptr` với deleter mặc định có kích thước đúng bằng một con trỏ thô, và compiler tối ưu mọi thao tác như con trỏ thường. Ta có được an toàn bộ nhớ mà không tốn thêm RAM hay CPU, điều rất có giá trị trên thiết bị nhúng.

## shared_ptr

`std::shared_ptr` dùng khi đối tượng **thực sự có nhiều chủ**, và không chủ nào biết chắc mình là người dùng cuối cùng.

Nó hoạt động nhờ **reference count** (bộ đếm tham chiếu): mỗi bản sao của `shared_ptr` làm bộ đếm tăng 1, mỗi bản sao bị hủy làm bộ đếm giảm 1. Khi bộ đếm về 0, đối tượng bị `delete`.

```cpp
auto config = std::make_shared<Tracer>("config");
std::cout << config.use_count() << "\n";      // in ra: 1

{
    auto copy = config;                       // sao chép: bộ đếm tăng
    std::cout << config.use_count() << "\n";  // in ra: 2
}                                             // copy bị hủy: bộ đếm giảm

std::cout << config.use_count() << "\n";      // in ra: 1
config.reset();                               // bộ đếm về 0 -> delete

// in ra:
// Create config
// ...
// Destroy config
```

### Các thao tác

| Thao tác | Ý nghĩa |
|---|---|
| `std::make_shared<T>(args...)` | Tạo đối tượng và bộ đếm trong cùng một lần cấp phát |
| `auto q = p;` | Sao chép: thêm một chủ sở hữu, bộ đếm tăng 1 |
| `p->f()`, `*p`, `if (p)`, `p.get()` | Giống `unique_ptr` |
| `p.use_count()` | Số `shared_ptr` đang cùng giữ đối tượng, chủ yếu để debug |
| `p.reset()` | `p` thôi giữ đối tượng, bộ đếm giảm 1; về 0 thì `delete` |
| `std::move(p)` | Chuyển sang `shared_ptr` khác mà không tăng bộ đếm, `p` trở thành rỗng |
| `std::shared_ptr<T> s = std::move(u);` | Chuyển một `unique_ptr` thành `shared_ptr` |

```cpp
auto a = std::make_shared<Tracer>("config");   // in ra: Create config
auto b = a;                                    // count = 2
auto c = std::move(b);                         // count vẫn = 2, b trở thành rỗng

std::cout << a.use_count() << "\n";            // in ra: 2
a.reset();                                     // count = 1, đối tượng vẫn sống
c.reset();                                     // count = 0, in ra: Destroy config

auto u = std::make_unique<Tracer>("sensor");   // in ra: Create sensor
std::shared_ptr<Tracer> s = std::move(u);      // u rỗng, s sở hữu, count = 1
```

Khi truyền `shared_ptr` vào hàm, mỗi lần truyền theo giá trị là một lần tăng giảm bộ đếm. Nếu hàm chỉ dùng đối tượng mà không cần giữ lại, hãy nhận `const T&` hoặc `T*` thay vì `shared_ptr<T>`:

```cpp
void print(const Tracer& t);                   // chỉ dùng: không đụng tới bộ đếm
void store(std::shared_ptr<Tracer> t);         // giữ lại lâu dài: trở thành một chủ sở hữu

print(*s);
store(s);
```

### Ví dụ: một frame dùng chung cho nhiều luồng

Camera chụp một frame, frame đó được gửi tới hai nơi: luồng hiển thị và luồng gửi qua mạng. Hai luồng xử lý xong vào thời điểm khác nhau, và không ai biết bên nào xong trước. Nếu dùng `unique_ptr`, ta không biết giao quyền sở hữu cho bên nào; nếu dùng con trỏ thô, bên xong trước `delete` sẽ khiến bên kia dùng phải dangling pointer.

Với `shared_ptr`, frame tự được giải phóng khi bên cuối cùng dùng xong:

```cpp
std::vector<std::shared_ptr<Tracer>> displayQueue;
std::vector<std::shared_ptr<Tracer>> networkQueue;

void capture()
{
    auto frame = std::make_shared<Tracer>("frame");   // count = 1
    displayQueue.push_back(frame);                    // count = 2
    networkQueue.push_back(frame);                    // count = 3
}                                                     // frame cục bộ bị hủy: count = 2

capture();
displayQueue.clear();    // hiển thị xong: count = 1
networkQueue.clear();    // gửi xong: count = 0 -> delete

// in ra:
// Create frame
// Destroy frame
```

Bộ đếm của `shared_ptr` được tăng giảm an toàn khi nhiều luồng cùng sao chép hoặc hủy `shared_ptr`. Tuy vậy, việc đọc ghi chính đối tượng `frame` từ nhiều luồng vẫn cần mutex như bình thường.

### Chi phí

`shared_ptr` không miễn phí như `unique_ptr`:

- Kích thước gấp đôi con trỏ thô, và cần thêm một vùng nhớ trên heap cho bộ đếm.
- Mỗi lần sao chép hay hủy đều phải tăng giảm bộ đếm theo cách an toàn đa luồng, chậm hơn thao tác thường.
- Khó biết chính xác khi nào đối tượng bị hủy, vì nó phụ thuộc vào việc nhiều nơi khác nhau thả ra.

:::tip Mặc định dùng unique_ptr
Chỉ dùng `shared_ptr` khi thật sự không xác định được một chủ sở hữu duy nhất. Bắt đầu với `unique_ptr` cũng không mất gì: khi cần, nó chuyển được sang `shared_ptr` bằng `std::move`, còn chiều ngược lại thì không.
:::

## weak_ptr

`std::weak_ptr` trỏ tới một đối tượng do `shared_ptr` quản lý nhưng **không tăng reference count** nên không giữ đối tượng sống. Điểm quan trọng là nó **biết được đối tượng đã bị hủy hay chưa**, điều mà con trỏ thô không làm được.

Muốn dùng đối tượng, ta gọi `lock()`: nếu đối tượng còn sống, nó trả về một `shared_ptr` tạm thời; nếu đã bị hủy, nó trả về rỗng.

### Các thao tác

| Thao tác | Ý nghĩa |
|---|---|
| `std::weak_ptr<T> w = sp;` | Tạo từ một `shared_ptr`, bộ đếm không tăng |
| `w.lock()` | Trả về `shared_ptr` nếu đối tượng còn sống, rỗng nếu đã bị hủy |
| `w.expired()` | `true` nếu đối tượng đã bị hủy |
| `w.reset()` | Thôi quan sát đối tượng |

`weak_ptr` không có `->` hay `*`: không thể dùng trực tiếp đối tượng qua nó, bắt buộc phải `lock()` trước. Nhờ vậy không có cách nào vô tình dùng một đối tượng đã bị hủy.

```cpp
auto sp = std::make_shared<Tracer>("display");   // in ra: Create display
std::weak_ptr<Tracer> w = sp;

std::cout << sp.use_count() << "\n";              // in ra: 1 (weak_ptr không được tính)

if (auto locked = w.lock()) {                    // đối tượng còn sống
    // dùng locked->...; trong khối này đối tượng chắc chắn không bị hủy
}

sp.reset();                                      // in ra: Destroy display
std::cout << w.expired() << "\n";                 // in ra: 1
std::cout << (w.lock() == nullptr) << "\n";       // in ra: 1
```

Nên dùng `lock()` thay vì kiểm tra `expired()` rồi mới dùng: trong môi trường đa luồng, đối tượng có thể bị hủy ngay sau khi `expired()` trả về `false`. `shared_ptr` do `lock()` trả về giữ đối tượng sống cho tới khi dùng xong.

### Ví dụ: listener có thể bị hủy bất cứ lúc nào

Cảm biến thông báo giá trị mới cho màn hình hiển thị. Màn hình có thể bị đóng bất cứ lúc nào, và cảm biến không nên là thứ giữ màn hình sống:

```cpp
class Display {
public:
    void show(float value) { std::cout << "Display: " << value << "\n"; }
};

class Sensor {
public:
    void setListener(std::weak_ptr<Display> display) { m_listener = display; }

    void update(float value)
    {
        if (auto display = m_listener.lock()) {   // còn sống thì lấy shared_ptr tạm
            display->show(value);
        } else {
            std::cout << "No listener\n";
        }
    }

private:
    std::weak_ptr<Display> m_listener;
};

Sensor sensor;
auto display = std::make_shared<Display>();
sensor.setListener(display);

sensor.update(25.5f);    // in ra: Display: 25.5
display.reset();         // màn hình bị đóng, đối tượng Display bị hủy
sensor.update(26.0f);    // in ra: No listener
```

So sánh với hai cách khác:

| `Sensor` giữ | Khi `Display` bị hủy |
|---|---|
| `Display*` | Con trỏ thành dangling pointer, gọi `show()` là use-after-free |
| `std::shared_ptr<Display>` | `Display` không bị hủy được vì `Sensor` vẫn giữ |
| `std::weak_ptr<Display>` | `lock()` trả về rỗng, `Sensor` biết để bỏ qua |

### Ví dụ: phá vòng tham chiếu

Nếu hai đối tượng giữ `shared_ptr` trỏ tới nhau, bộ đếm của cả hai không bao giờ về 0, và cả hai bị memory leak:

```cpp
struct Node {
    std::shared_ptr<Node> other;
    ~Node() { std::cout << "Destroy Node\n"; }
};

{
    auto a = std::make_shared<Node>();
    auto b = std::make_shared<Node>();
    a->other = b;
    b->other = a;
}   // không có dòng "Destroy Node" nào: memory leak
```

```
  a ----> [Node A] --other--> [Node B] <---- b
            ^                    |
            +------other---------+

  khi a, b bị hủy: A vẫn được B giữ, B vẫn được A giữ -> bộ đếm không về 0
```

Đổi một phía thành `weak_ptr` là đủ phá vòng:

```cpp
struct Node {
    std::weak_ptr<Node> other;    // không tham gia sở hữu
    ~Node() { std::cout << "Destroy Node\n"; }
};

// gán như trên, khi ra khỏi phạm vi in ra "Destroy Node" hai lần
```

Trong thực tế, nếu thấy cần `weak_ptr` để phá vòng tham chiếu, đó thường là dấu hiệu nên xem lại thiết kế: phần lớn quan hệ có thể biểu diễn bằng một chủ sở hữu rõ ràng với `unique_ptr`, phía còn lại chỉ giữ tham chiếu hoặc con trỏ thô.

## Chọn loại nào

Trả lời lần lượt các câu hỏi sau:

```
  Đối tượng có cần tạo trên heap không?
    |
    +-- không --> biến thường / thành viên class (không cần con trỏ)
    |
    +-- có --> Nơi này có sở hữu đối tượng không?
                |
                +-- không, chỉ dùng --> T& hoặc T*
                |                       (hoặc weak_ptr nếu đối tượng do shared_ptr quản lý
                |                        và có thể bị hủy bất cứ lúc nào)
                |
                +-- có --> Có đúng một chủ sở hữu không?
                            |
                            +-- có    --> unique_ptr
                            +-- không --> shared_ptr
```

| Ý định | Kiểu dùng |
|---|---|
| Sở hữu duy nhất | `std::unique_ptr<T>` |
| Sở hữu chung, không biết ai dùng cuối | `std::shared_ptr<T>` |
| Quan sát đối tượng của `shared_ptr`, có thể đã bị hủy | `std::weak_ptr<T>` |
| Chỉ sử dụng, đối tượng chắc chắn còn sống | `T&` hoặc `const T&` |
| Chỉ sử dụng, có thể không có đối tượng | `T*` (có thể là `nullptr`) |

Smart pointer biểu diễn **quyền sở hữu**, không phải để thay thế mọi con trỏ. Quy ước trong C++ hiện đại: con trỏ thô và tham chiếu không bao giờ sở hữu đối tượng, nên không ai được `delete` qua chúng. Việc `delete` chỉ do smart pointer thực hiện.

:::note Smart pointer trong nhúng
Smart pointer chỉ quản lý đối tượng trên heap. Nhiều hệ thống nhúng hạn chế cấp phát động lúc đang chạy vì sợ phân mảnh bộ nhớ, và chỉ cấp phát một lần khi khởi động. Khi đó `unique_ptr` vẫn rất hữu ích để giữ các đối tượng tạo lúc khởi động, còn `shared_ptr` nên được dùng hạn chế vì chi phí bộ đếm và vùng nhớ phụ.
:::

## Lỗi thường gặp

**Double free do hai smart pointer cùng nhận một con trỏ thô: crash với `free(): double free detected`**

```cpp
Tracer* raw = new Tracer("sensor");
std::unique_ptr<Tracer> a(raw);
std::unique_ptr<Tracer> b(raw);    // compiler không phát hiện được
// khi ra khỏi phạm vi: b delete, rồi a delete lần nữa
```

Mỗi `unique_ptr` đều tin mình là chủ sở hữu duy nhất. Lỗi tương tự xảy ra với hai `shared_ptr` tạo riêng từ cùng một con trỏ thô: chúng có hai bộ đếm riêng. Cách phòng tránh là luôn tạo bằng `std::make_unique` / `std::make_shared`, rồi sao chép `shared_ptr` hoặc `std::move` `unique_ptr` khi cần.

**`Segmentation fault` khi dùng `unique_ptr` sau khi đã chuyển quyền sở hữu**

```cpp
auto sensor = std::make_unique<Tracer>("sensor");
take(std::move(sensor));

sensor->...;    // sensor đã rỗng: truy cập nullptr
```

Sau `std::move`, biến nguồn trở thành rỗng, và compiler không cảnh báo. Quy tắc: sau khi `std::move` một biến, coi như biến đó không còn dùng được nữa.

**Memory leak âm thầm do vòng tham chiếu giữa các `shared_ptr`: AddressSanitizer báo `detected memory leaks`**

Đã trình bày ở mục "Phá vòng tham chiếu". Không có lỗi hay cảnh báo nào lúc biên dịch, và destructor không bao giờ được gọi. Cách sửa là đổi một phía thành `weak_ptr`, hoặc thiết kế lại để có một chủ sở hữu rõ ràng.

**Giữ con trỏ thô lấy từ `get()` lâu hơn smart pointer: AddressSanitizer báo `heap-use-after-free`**

```cpp
Tracer* raw = nullptr;
{
    auto sensor = std::make_unique<Tracer>("sensor");
    raw = sensor.get();
}   // sensor bị hủy -> đối tượng bị delete

// raw giờ là dangling pointer
```

`get()` chỉ để cho mượn tạm thời, ví dụ truyền vào hàm C cần `FILE*`. Nếu nơi khác cần giữ đối tượng lâu dài, hoặc trao quyền sở hữu bằng `std::move`, hoặc chuyển sang `shared_ptr` / `weak_ptr`.

**Destructor lớp con không được gọi khi `unique_ptr` lớp cha giữ lớp con**

```cpp
class Transport {
public:
    ~Transport() {}            // không ảo
    virtual void send(const std::string& data) = 0;
};

std::unique_ptr<Transport> t = std::make_unique<TcpTransport>();
// khi t bị hủy: chỉ destructor của Transport được gọi
```

`unique_ptr<Transport>` gọi `delete` qua con trỏ kiểu `Transport*`. Nếu destructor của lớp cha không ảo, chỉ destructor lớp cha chạy và phần lớp con, như socket mà `TcpTransport` đang giữ, không được dọn dẹp. Smart pointer không tự khắc phục được lỗi này. Lớp cha dùng cho đa hình luôn cần `virtual ~Transport() = default;`.
