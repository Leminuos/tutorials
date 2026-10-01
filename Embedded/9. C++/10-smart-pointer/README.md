Ở Bài C4, ta đã rút ra quy tắc: mỗi đối tượng trên heap có đúng một chủ sở hữu, và chủ sở hữu `delete` nó trong destructor. Nhưng việc tuân thủ quy tắc này vẫn hoàn toàn dựa vào sự cẩn thận của người viết code. Chỉ cần quên một dòng `delete` trong destructor, quên cấm sao chép theo quy tắc ba (Bài C7), hay thêm một nhánh `return` sớm trước khi kịp `delete`, là lỗi bộ nhớ quay trở lại.

Nhìn lại cách `std::string` và `std::vector` tự quản lý bộ nhớ theo RAII, câu hỏi tự nhiên là: tại sao không có một class làm điều tương tự cho bất kỳ đối tượng nào tạo bằng `new`? Đó chính là **smart pointer**: một đối tượng nhỏ giữ con trỏ tới đối tượng trên heap, và tự `delete` đối tượng đó khi bản thân nó bị hủy.

Các smart pointer nằm trong thư viện `<memory>`. Trong bài, ta tiếp tục dùng class `Tracer` từ Bài C4 để quan sát khi nào đối tượng được tạo và hủy.

## unique_ptr

`std::unique_ptr` biểu diễn quyền sở hữu duy nhất: tại mỗi thời điểm, chỉ có một `unique_ptr` giữ đối tượng. Khi `unique_ptr` bị hủy, đối tượng bị `delete` theo.

```cpp
#include <memory>

void process()
{
    std::unique_ptr<Tracer> sensor = std::make_unique<Tracer>("sensor");

    // dùng giống hệt con trỏ thường
    // sensor->..., *sensor

    if (true) {
        return;        // không cần delete
    }
}   // sensor bị hủy → tự động delete đối tượng Tracer

process();
// in ra:
// Create sensor
// Destroy sensor
```

`std::make_unique<Tracer>("sensor")` cấp phát đối tượng trên heap và chuyển tham số cho constructor, tương đương `new Tracer("sensor")` nhưng trả về luôn `unique_ptr`. Ta dùng `make_unique` thay vì viết `new` trực tiếp, để trong code không còn lệnh `new` nào cần tìm lệnh `delete` tương ứng. Kết hợp với `auto`, cách viết gọn hơn nữa:

```cpp
auto sensor = std::make_unique<Tracer>("sensor");
```

Một số thao tác thường dùng:

```cpp
auto p = std::make_unique<Tracer>("a");

if (p) { }                  // kiểm tra có đang giữ đối tượng không, như con trỏ thường
Tracer* raw = p.get();      // lấy con trỏ thô để dùng (không chuyển quyền sở hữu)
p.reset();                  // delete đối tượng ngay bây giờ, p trở thành rỗng
                            // in ra: Destroy a
```

`unique_ptr` với cách xóa mặc định có kích thước đúng bằng một con trỏ thô, và mọi thao tác đều được compiler tối ưu như con trỏ thường. Nghĩa là ta có được an toàn bộ nhớ mà không tốn thêm chi phí, điều rất có giá trị trên thiết bị nhúng.

## Chuyển quyền sở hữu với std::move

Vì chỉ có một chủ sở hữu, `unique_ptr` không sao chép được. Nó đã được khai báo `= delete` cho copy constructor và toán tử gán sao chép, đúng như cách ta làm ở Bài C7:

```cpp
auto a = std::make_unique<Tracer>("sensor");
auto b = a;
// lỗi: use of deleted function 'std::unique_ptr<...>::unique_ptr(const std::unique_ptr<...>&)'
```

Tuy nhiên, ta có thể chuyển quyền sở hữu sang `unique_ptr` khác bằng `std::move` (trong `<utility>`). Sau khi chuyển, `unique_ptr` nguồn trở thành rỗng:

```cpp
auto a = std::make_unique<Tracer>("sensor");
auto b = std::move(a);      // b sở hữu đối tượng, a trở thành rỗng

std::cout << (a == nullptr);   // in ra: 1
std::cout << (b != nullptr);   // in ra: 1
```

`std::move(a)` đánh dấu rằng ta cho phép lấy tài nguyên đi khỏi `a`. Nó không tự di chuyển gì cả; chính `unique_ptr` nhận được sẽ lấy con trỏ và đặt `a` về rỗng.

Nhờ vậy, khai báo hàm cho biết rõ ý định về quyền sở hữu:

```cpp
void use(Tracer& t);                        // chỉ dùng, không sở hữu
void take(std::unique_ptr<Tracer> t);       // nhận quyền sở hữu
std::unique_ptr<Tracer> create();           // trao quyền sở hữu cho nơi gọi

auto p = create();       // nhận từ hàm: không cần std::move
use(*p);                 // cho mượn
take(std::move(p));      // trao hẳn, sau dòng này p rỗng
```

Chỉ nhìn vào khai báo, người đọc biết ngay ai chịu trách nhiệm hủy đối tượng. Với con trỏ thô `Tracer*`, không có cách nào biết điều đó nếu không đọc tài liệu hoặc đọc code bên trong.

## unique_ptr làm thành viên class

Đây là cách dùng `unique_ptr` quan trọng nhất. Viết lại class `Controller` ở Bài C4:

```cpp
class Controller {
public:
    Controller()
        : m_sensor(std::make_unique<Tracer>("sensor")),
          m_display(std::make_unique<Tracer>("display"))
    {
    }

    // không cần destructor
    // không cần cấm sao chép: unique_ptr đã không sao chép được

private:
    std::unique_ptr<Tracer> m_sensor;
    std::unique_ptr<Tracer> m_display;
};

int main()
{
    Controller controller;
}
// in ra:
// Create sensor
// Create display
// Destroy display
// Destroy sensor
```

Kết quả giống hệt phiên bản ở Bài C4, nhưng class không còn destructor. Các thành viên `unique_ptr` tự hủy theo thứ tự ngược với khai báo (Bài C4), và mỗi `unique_ptr` tự `delete` đối tượng của mình. Class cũng tự động không sao chép được, vì thành viên của nó không sao chép được. Đây chính là quy tắc không (rule of zero) đã nhắc ở Bài C7.

## Kết hợp với đa hình

`unique_ptr` kiểu lớp cha giữ được đối tượng lớp con, giống con trỏ thô. Áp dụng vào lớp trừu tượng phần cứng ở Bài C6, ta viết một hàm tạo nguồn dữ liệu phù hợp với môi trường:

```cpp
std::unique_ptr<TemperatureSource> createSource(bool simulate)
{
    if (simulate) {
        return std::make_unique<SimulatedSource>();
    }
    return std::make_unique<Ds18b20Source>("/sys/bus/w1/devices/28-.../w1_slave");
}
```

Và `Monitor` giờ sở hữu nguồn dữ liệu, thay vì chỉ giữ tham chiếu như ở Bài C6:

```cpp
class Monitor {
public:
    Monitor(std::unique_ptr<TemperatureSource> source, float threshold)
        : m_source(std::move(source)), m_threshold(threshold)
    {
    }

    void update() { /* dùng m_source->readCelsius() như Bài C6 */ }

private:
    std::unique_ptr<TemperatureSource> m_source;
    float m_threshold;
};

Monitor monitor(createSource(true), 80.0f);
```

Cách này an toàn hơn phiên bản dùng tham chiếu ở Bài C6: `Monitor` không thể vô tình sống lâu hơn nguồn dữ liệu của nó. Khi `monitor` bị hủy, nguồn dữ liệu bị hủy theo, và nhờ destructor ảo của `TemperatureSource`, destructor của đúng lớp con được gọi.

### Danh sách đối tượng đa hình

Kết hợp với `std::vector` ở Bài C8, ta có một danh sách sở hữu nhiều loại cảm biến khác nhau:

```cpp
std::vector<std::unique_ptr<Sensor>> sensors;

sensors.push_back(std::make_unique<TemperatureSensor>("Temperature"));
sensors.push_back(std::make_unique<HumiditySensor>("Humidity"));

for (const auto& s : sensors) {
    std::cout << s->name() << ": " << s->read() << "\n";
}
// khi sensors bị hủy, mọi cảm biến đều được delete
```

## Custom deleter cho tài nguyên kiểu C

Mặc định, `unique_ptr` gọi `delete` khi hủy. Nhưng ta có thể chỉ định cách giải phóng khác, gọi là **deleter**. Điều này rất tiện khi làm việc với thư viện C, nơi tài nguyên được giải phóng bằng hàm riêng như `fclose()`.

Deleter là một struct có toán tử gọi hàm `operator()`, nghĩa là đối tượng của struct này có thể được gọi như một hàm (lambda ở Bài C9 thực chất cũng được compiler tạo thành một đối tượng như vậy):

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

using FilePtr = std::unique_ptr<FILE, FileCloser>;
```

Dòng `using FilePtr = ...` tạo một tên gọi khác cho kiểu dữ liệu dài, tương tự `typedef` trong C. Giờ ta có thể viết lại hàm `blink()` ở Bài C4 mà không cần tự viết class `LedFile`:

```cpp
bool blink()
{
    FilePtr led(std::fopen("/sys/class/leds/beaglebone:green:usr0/brightness", "w"));
    if (!led) {
        return false;
    }

    std::fputs("1", led.get());
    std::fflush(led.get());
    std::fputs("0", led.get());
    return true;
}   // file tự đóng, in ra: File closed
```

`unique_ptr` không gọi deleter khi đang rỗng, nên nếu `fopen()` thất bại, `fclose()` không bị gọi với `nullptr`.

Cách này chỉ áp dụng cho tài nguyên được biểu diễn bằng con trỏ. Tài nguyên dạng số nguyên như file descriptor của `open()` thì vẫn cần một class RAII tự viết như `LedFile`.

## shared_ptr

Đôi khi một đối tượng thực sự được nhiều nơi cùng sở hữu, và không nơi nào biết chắc mình là nơi dùng cuối cùng. Khi đó ta dùng `std::shared_ptr`. Nó giữ một **bộ đếm tham chiếu**: mỗi bản sao của `shared_ptr` làm bộ đếm tăng lên, mỗi bản sao bị hủy làm bộ đếm giảm xuống. Khi bộ đếm về 0, đối tượng bị `delete`.

```cpp
auto config = std::make_shared<Tracer>("config");
std::cout << config.use_count();      // in ra: 1

{
    auto copy = config;               // shared_ptr sao chép được
    std::cout << config.use_count();  // in ra: 2
}   // copy bị hủy, bộ đếm giảm

std::cout << config.use_count();      // in ra: 1
config.reset();                       // bộ đếm về 0
// in ra:
// Create config
// Destroy config
```

`shared_ptr` tiện lợi nhưng có chi phí: kích thước gấp đôi con trỏ thường, thêm một vùng nhớ cho bộ đếm, và việc tăng giảm bộ đếm phải an toàn đa luồng nên chậm hơn. Quan trọng hơn, khi đối tượng có nhiều chủ sở hữu, ta khó biết chính xác khi nào nó bị hủy.

:::tip Mặc định dùng unique_ptr
Chỉ dùng `shared_ptr` khi quyền sở hữu chung là thực sự cần thiết.
:::

## Vòng tham chiếu và weak_ptr

`shared_ptr` có một điểm yếu: nếu hai đối tượng giữ `shared_ptr` trỏ tới nhau, bộ đếm của cả hai không bao giờ về 0, và cả hai bị rò rỉ.

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
}   // không có dòng "Destroy Node" nào được in ra: rò rỉ bộ nhớ
```

Giải pháp là `std::weak_ptr`: một con trỏ quan sát đối tượng do `shared_ptr` quản lý nhưng không tăng bộ đếm. Muốn dùng đối tượng, ta gọi `lock()` để lấy một `shared_ptr` tạm thời; nếu đối tượng đã bị hủy, `lock()` trả về rỗng.

```cpp
struct Node {
    std::weak_ptr<Node> other;    // không tham gia sở hữu
    ~Node() { std::cout << "Destroy Node\n"; }
};

// ... gán như trên, khi ra khỏi phạm vi in ra "Destroy Node" hai lần

if (auto p = a->other.lock()) {   // lấy shared_ptr tạm, kiểm tra đối tượng còn tồn tại
    // dùng p
}
```

Trong thực tế, nếu thấy cần `weak_ptr` để phá vòng tham chiếu, đó thường là dấu hiệu nên xem lại thiết kế: phần lớn quan hệ có thể biểu diễn bằng một chủ sở hữu rõ ràng với `unique_ptr`.

## Con trỏ thô vẫn có chỗ dùng

Smart pointer dùng để biểu diễn quyền sở hữu. Khi một hàm hoặc class chỉ sử dụng đối tượng mà không sở hữu nó, ta vẫn dùng tham chiếu hoặc con trỏ thô:

| Ý định | Kiểu dùng |
|---|---|
| Sở hữu duy nhất | `std::unique_ptr<T>` |
| Sở hữu chung | `std::shared_ptr<T>` |
| Chỉ sử dụng, đối tượng chắc chắn tồn tại | `T&` hoặc `const T&` |
| Chỉ sử dụng, đối tượng có thể không có | `T*` (có thể là `nullptr`) |

Quy ước ngầm: trong code C++ hiện đại, một con trỏ thô không bao giờ sở hữu đối tượng nó trỏ tới, nên không ai được `delete` qua nó. Việc `delete` chỉ do smart pointer thực hiện.

## Lỗi thường gặp

**Crash `free(): double free detected` do hai smart pointer cùng sở hữu một đối tượng**

```cpp
Tracer* raw = new Tracer("sensor");
std::unique_ptr<Tracer> a(raw);
std::unique_ptr<Tracer> b(raw);    // compiler không phát hiện được
// khi ra khỏi phạm vi: b delete, rồi a delete lần nữa
```

Mỗi `unique_ptr` đều tin mình là chủ sở hữu duy nhất. Cách phòng tránh là luôn tạo bằng `std::make_unique` / `std::make_shared` thay vì từ con trỏ thô, và chuyển quyền sở hữu bằng `std::move`.

**`Segmentation fault` khi dùng `unique_ptr` sau khi đã chuyển quyền sở hữu**

```cpp
auto sensor = std::make_unique<Tracer>("sensor");
Monitor monitor(std::move(sensor), 80.0f);

sensor->...;    // sensor đã rỗng: truy cập nullptr
```

Sau `std::move`, biến nguồn trở thành rỗng. Lỗi này compiler không cảnh báo. Quy tắc: sau khi `std::move` một biến, coi như biến đó không còn dùng được nữa.

**Rò rỉ bộ nhớ âm thầm do vòng tham chiếu giữa các `shared_ptr`: AddressSanitizer báo `detected memory leaks`**

Đã trình bày ở mục "Vòng tham chiếu và weak_ptr". Không có lỗi hay cảnh báo nào lúc biên dịch, và destructor không bao giờ được gọi. Cách sửa là đổi một phía thành `weak_ptr`, hoặc thiết kế lại để có một chủ sở hữu rõ ràng.

**Destructor lớp con không được gọi khi `unique_ptr` lớp cha giữ lớp con**

```cpp
class Sensor {
public:
    ~Sensor() {}               // không ảo
    virtual float read() = 0;
};

std::unique_ptr<Sensor> s = std::make_unique<TemperatureSensor>();
// khi s bị hủy: chỉ destructor của Sensor được gọi
```

Đây chính là lỗi ở Bài C6, và smart pointer không tự khắc phục được, vì `unique_ptr<Sensor>` gọi `delete` qua con trỏ kiểu `Sensor*`. Lớp cha dùng cho đa hình luôn cần `virtual ~Sensor() = default;`.
