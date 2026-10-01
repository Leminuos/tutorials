Trong C, khi muốn viết code dùng được cho nhiều kiểu dữ liệu, ta chỉ có hai lựa chọn, và cả hai đều có nhược điểm.

Lựa chọn thứ nhất là macro. Macro không kiểm tra kiểu, và dễ gây lỗi khó thấy:

```c
// C
#define MAX(a, b) ((a) > (b) ? (a) : (b))

int i = 5, j = 3;
int m = MAX(i++, j);   // i++ bị thực hiện hai lần: m = 6, i = 7
```

Lựa chọn thứ hai là con trỏ `void*`, như hàm `qsort()` của thư viện chuẩn C. Hàm phải nhận thêm kích thước phần tử và một hàm so sánh tự ép kiểu, và compiler không thể phát hiện nếu ta truyền nhầm kiểu.

Ngoài ra, C không có mảng động sẵn có. Mỗi khi cần một danh sách có thể lớn dần, ta phải tự `malloc()`, `realloc()` và `free()`, với đầy đủ các rủi ro bộ nhớ đã nói ở Bài C4.

C++ giải quyết bằng **template**, cho phép viết code tổng quát mà vẫn được compiler kiểm tra kiểu đầy đủ. Dựa trên template, thư viện chuẩn (**STL**) cung cấp sẵn các cấu trúc dữ liệu như mảng động, bảng tra cứu, cùng các thuật toán sắp xếp, tìm kiếm.

## Template hàm

Template hàm là một "khuôn mẫu" để compiler tạo ra hàm. Ta viết hàm với một kiểu chưa xác định, ký hiệu là `T`; khi gọi hàm với kiểu cụ thể, compiler tự sinh ra phiên bản tương ứng.

```cpp
template <typename T>
T maxValue(T a, T b)
{
    return (a > b) ? a : b;
}

std::cout << maxValue(3, 7);         // T là int, in ra: 7
std::cout << maxValue(2.5f, 1.5f);   // T là float, in ra: 2.5
```

Dòng `template <typename T>` khai báo `T` là tham số kiểu. Compiler nhìn vào đối số để suy ra `T`, giống cách `auto` suy ra kiểu ở Bài C1. Khác với macro, đối số chỉ được tính một lần và kiểu được kiểm tra chặt chẽ.

Khi hai đối số khác kiểu, compiler không biết chọn `T` là gì. Ta có thể chỉ định rõ kiểu trong cặp ngoặc nhọn:

```cpp
maxValue(3, 2.5f);          // lỗi: deduced conflicting types for parameter 'T' ('int' and 'float')
maxValue<float>(3, 2.5f);   // T là float, in ra: 3
```

Thư viện chuẩn đã có sẵn `std::max` và `std::min` trong `<algorithm>`, hoạt động đúng như hàm trên. Ví dụ này chỉ để minh họa cách template làm việc.

:::tip Đọc thông báo lỗi template
Compiler chỉ kiểm tra `T` có dùng được hay không khi hàm thực sự được gọi. Nếu gọi `maxValue` với một kiểu không có toán tử `>`, compiler báo lỗi bên trong thân template, và thông báo lỗi có thể dài hàng chục dòng. Hãy tìm dòng `error:` đầu tiên và dòng `required from here`; dòng này chỉ ra chỗ gọi hàm trong code của ta.
:::

## Template class

Template cũng áp dụng được cho class. Ví dụ quen thuộc trong lập trình nhúng là **bộ đệm vòng** (ring buffer), dùng để lưu dữ liệu nhận từ UART trước khi xử lý. Trong C, ta thường phải viết lại bộ đệm vòng cho từng kiểu dữ liệu; với template, ta viết một lần:

```cpp
template <typename T, size_t N>
class RingBuffer {
public:
    bool push(const T& value)
    {
        if (m_count == N) {
            return false;            // bộ đệm đầy
        }
        m_data[m_head] = value;
        m_head = (m_head + 1) % N;
        ++m_count;
        return true;
    }

    bool pop(T& out)
    {
        if (m_count == 0) {
            return false;            // bộ đệm rỗng
        }
        out = m_data[m_tail];
        m_tail = (m_tail + 1) % N;
        --m_count;
        return true;
    }

    size_t size() const { return m_count; }
    static constexpr size_t capacity() { return N; }

private:
    T m_data[N];
    size_t m_head = 0;
    size_t m_tail = 0;
    size_t m_count = 0;
};
```

Template này có hai tham số: `T` là kiểu phần tử, còn `N` là một giá trị (kích thước bộ đệm) chứ không phải kiểu. Khi dùng, ta phải ghi rõ cả hai trong cặp ngoặc nhọn:

```cpp
RingBuffer<uint8_t, 64> uartRx;     // bộ đệm 64 byte cho UART
RingBuffer<float, 10> history;      // 10 giá trị nhiệt độ gần nhất

uartRx.push(0xAA);
uartRx.push(0x01);

uint8_t byte;
uartRx.pop(byte);
std::cout << std::hex << static_cast<int>(byte);   // in ra: aa
std::cout << std::dec << uartRx.size();            // in ra: 1
```

Vì `N` được xác định lúc biên dịch, mảng `m_data` có kích thước cố định và nằm ngay trong đối tượng, không cấp phát heap. Đây là tính chất rất được ưa chuộng trong code nhúng: bộ nhớ dùng bao nhiêu biết trước được, không có rủi ro phân mảnh heap.

## std::array: mảng cố định an toàn hơn

`std::array` là phiên bản C++ của mảng C thông thường. Nó cũng có kích thước cố định, không cấp phát heap, nhưng bổ sung thêm những gì mảng C còn thiếu:

```cpp
#include <array>

std::array<uint8_t, 4> frame = {0xAA, 0x01, 0x02, 0x55};

std::cout << frame.size();          // in ra: 4 (mảng C phải tự tính bằng sizeof)
frame.at(10);                       // ném lỗi std::out_of_range thay vì ghi đè bộ nhớ

std::array<uint8_t, 4> copy = frame;   // sao chép được, mảng C thì không gán được
```

Khi truyền vào hàm, `std::array` không bị biến thành con trỏ và mất thông tin kích thước như mảng C. Với mảng kích thước cố định, ta nên dùng `std::array` thay cho mảng C.

## std::vector: mảng động

`std::vector` là mảng có thể thay đổi kích thước lúc chạy chương trình. Nó tự cấp phát, tự mở rộng và tự giải phóng bộ nhớ theo RAII (Bài C4), nên ta không bao giờ phải gọi `new` hay `delete`.

```cpp
#include <vector>

std::vector<float> readings;

readings.push_back(25.1f);     // thêm vào cuối
readings.push_back(27.4f);
readings.push_back(24.8f);

std::cout << readings.size();  // in ra: 3
std::cout << readings[0];      // in ra: 25.1
std::cout << readings.back();  // in ra: 24.8 (phần tử cuối)

for (float r : readings) {     // duyệt bằng range-based for (Bài C2)
    std::cout << r << " ";
}

readings.clear();              // xóa hết phần tử
```

Có thể khởi tạo vector với danh sách giá trị ban đầu:

```cpp
std::vector<int> ledPins = {60, 61, 62, 63};
```

### Cách vector quản lý bộ nhớ

`vector` cấp phát trước một vùng nhớ có sức chứa (**capacity**) lớn hơn hoặc bằng số phần tử hiện có (**size**). Khi thêm phần tử mà hết chỗ, nó cấp phát vùng nhớ mới lớn hơn, chép toàn bộ dữ liệu sang, rồi giải phóng vùng cũ:

```cpp
std::vector<int> v;
for (int i = 0; i < 5; ++i) {
    v.push_back(i);
    std::cout << v.size() << "/" << v.capacity() << "\n";
}
// in ra (với GCC):
// 1/1
// 2/2
// 3/4
// 4/4
// 5/8
```

Mỗi lần mở rộng là một lần cấp phát và sao chép. Nếu biết trước số phần tử tối đa, ta dùng `reserve()` để cấp phát một lần duy nhất ngay từ đầu:

```cpp
std::vector<float> history;
history.reserve(1000);          // cấp phát sẵn chỗ cho 1000 phần tử
// 1000 lần push_back tiếp theo không cấp phát thêm lần nào
```

:::tip Gọi reserve() lúc khởi động
Trên thiết bị nhúng, gọi `reserve()` lúc khởi động giúp tránh cấp phát liên tục trong khi chạy, giảm phân mảnh heap.
:::

## Iterator: duyệt qua các phần tử

Nhiều thao tác trên container, như xóa phần tử hay dùng thuật toán, làm việc với **iterator**. Iterator hoạt động giống con trỏ: trỏ tới một phần tử, dùng `*` để lấy giá trị, dùng `++` để sang phần tử kế tiếp.

```cpp
std::vector<int> v = {10, 20, 30};

for (auto it = v.begin(); it != v.end(); ++it) {
    std::cout << *it << " ";      // in ra: 10 20 30
}
```

`begin()` trỏ tới phần tử đầu tiên, còn `end()` trỏ tới vị trí ngay sau phần tử cuối cùng, không phải phần tử cuối. Đây là quy ước chung của mọi container trong thư viện chuẩn.

```
   begin()                 end()
     |                       |
     v                       v
   +----+----+----+
   | 10 | 20 | 30 |
   +----+----+----+
```

Iterator được dùng khi xóa phần tử:

```cpp
v.erase(v.begin() + 1);           // xóa phần tử thứ hai
// v: 10 30
```

Vòng lặp range-based for thực chất được compiler dịch thành vòng lặp dùng iterator như trên. Trong hầu hết trường hợp, ta dùng range-based for cho gọn và chỉ viết iterator khi cần.

## std::map: bảng tra cứu theo khóa

`std::map` lưu các cặp khóa → giá trị, cho phép tra cứu nhanh giá trị theo khóa. Các phần tử luôn được sắp xếp theo khóa.

```cpp
#include <map>

std::map<std::string, float> thresholds;

thresholds["Engine"] = 85.0f;               // thêm hoặc sửa bằng []
thresholds["Server room"] = 35.0f;
thresholds.insert({"Power supply", 70.0f});      // thêm bằng insert

std::cout << thresholds["Engine"];          // in ra: 85
std::cout << thresholds.size();              // in ra: 3
```

Để kiểm tra một khóa có tồn tại hay không, ta dùng `find()`. Hàm này trả về iterator tới phần tử tìm thấy, hoặc `end()` nếu không có:

```cpp
auto it = thresholds.find("Fan");
if (it == thresholds.end()) {
    std::cout << "No threshold for fan\n";
} else {
    std::cout << it->second;   // it->first là khóa, it->second là giá trị
}
```

Khi duyệt map, mỗi phần tử là một cặp gồm `first` (khóa) và `second` (giá trị). C++17 cho phép tách cặp này thành hai biến có tên, gọi là **structured binding**:

```cpp
for (const auto& [name, value] : thresholds) {
    std::cout << name << ": " << value << "\n";
}
// in ra (sắp xếp theo khóa):
// Engine: 85
// Power supply: 70
// Server room: 35
```

Trong code nhúng, `map` phù hợp để lưu cấu hình theo tên, hay bảng thanh ghi Modbus theo địa chỉ, ví dụ `std::map<uint16_t, uint16_t> registers;`.

## Thuật toán có sẵn

Thư viện `<algorithm>` cung cấp nhiều thuật toán làm việc trên mọi container thông qua cặp iterator `begin()` và `end()`:

```cpp
#include <algorithm>
#include <numeric>

std::vector<float> r = {25.1f, 27.4f, 24.8f, 26.0f};

auto maxIt = std::max_element(r.begin(), r.end());
std::cout << *maxIt;                                        // in ra: 27.4

float sum = std::accumulate(r.begin(), r.end(), 0.0f);      // trong <numeric>
std::cout << sum / r.size();                                // in ra: 25.825

std::sort(r.begin(), r.end());
// r: 24.8 25.1 26 27.4

auto found = std::find(r.begin(), r.end(), 26.0f);
if (found != r.end()) {
    std::cout << "Found value 26";
}
```

Các thuật toán này đã được kiểm chứng kỹ và thường nhanh hơn vòng lặp tự viết. Khi kết hợp với lambda ở Bài C9, ta còn tùy biến được cách so sánh, cách lọc phần tử.

## Chọn container nào

| Container | Kích thước | Cấp phát heap | Dùng khi |
|---|---|---|---|
| `std::array` | Cố định lúc biên dịch | Không | Khung dữ liệu, bảng hằng số, bộ đệm cố định |
| `std::vector` | Thay đổi được | Có | Lựa chọn mặc định cho danh sách |
| `std::map` | Thay đổi được | Có | Tra cứu theo khóa, cần thứ tự sắp xếp |

Thư viện chuẩn còn nhiều container khác như `std::unordered_map` (tra cứu nhanh hơn nhưng không sắp xếp), `std::list`, `std::deque`. Tuy nhiên với hầu hết nhu cầu, ba container trên là đủ, và `vector` nên là lựa chọn đầu tiên.

## Lỗi thường gặp

**Tách phần cài đặt template sang file `.cpp`: `undefined reference to 'int maxValue<int>(int, int)'`**

```cpp
// utils.h
template <typename T>
T maxValue(T a, T b);

// utils.cpp
template <typename T>
T maxValue(T a, T b) { return (a > b) ? a : b; }

// main.cpp
maxValue(3, 7);   // lỗi khi link
```

Compiler chỉ sinh ra phiên bản cụ thể của template tại nơi sử dụng, và lúc đó nó cần thấy toàn bộ phần cài đặt. Khi biên dịch `main.cpp`, compiler chỉ thấy khai báo nên không sinh được code. Cách sửa là đặt toàn bộ template (cả khai báo lẫn cài đặt) trong file header.

**In ra giá trị rác hoặc crash (`heap-use-after-free`) khi dùng tham chiếu sau khi vector thay đổi kích thước**

```cpp
std::vector<int> v = {1, 2, 3};
int& first = v[0];

v.push_back(4);          // vector hết chỗ, chuyển dữ liệu sang vùng nhớ mới
std::cout << first;      // first trỏ vào vùng nhớ cũ đã bị giải phóng
```

Khi vector mở rộng, mọi tham chiếu, con trỏ và iterator tới phần tử cũ đều trở nên không hợp lệ. Cách phòng tránh là không giữ tham chiếu tới phần tử qua các lần thêm phần tử, hoặc giữ chỉ số thay cho tham chiếu, hoặc `reserve()` đủ chỗ từ trước.

**Map tự thêm phần tử khi dùng `[]` để kiểm tra khóa**

```cpp
if (thresholds["Fan"] > 0) { }   // mục đích chỉ để kiểm tra
std::cout << thresholds.size();   // in ra: 4 (tăng thêm một phần tử)
```

Với map, nếu khóa chưa tồn tại, toán tử `[]` tự tạo phần tử mới với giá trị mặc định (0 với số). Cách sửa là dùng `find()` hoặc `count()` để kiểm tra:

```cpp
if (thresholds.count("Fan") > 0) { }
```

**Truy cập vượt chỉ số bằng `[]` mà không có lỗi nào**

```cpp
std::vector<int> v = {1, 2, 3};
v[5] = 10;          // không có lỗi hay cảnh báo, ghi đè vùng nhớ bất kỳ
v.at(5) = 10;       // ném lỗi std::out_of_range
```

Toán tử `[]` không kiểm tra chỉ số để đạt tốc độ tối đa, giống mảng C. Khi chỉ số đến từ dữ liệu bên ngoài (như khung dữ liệu UART), hãy dùng `at()` hoặc tự kiểm tra trước. Khi thử nghiệm trên máy tính, AddressSanitizer cũng phát hiện được lỗi này.

**Xóa phần tử trong vòng lặp range-based for**

```cpp
for (auto& r : readings) {
    if (r < 0) {
        // không thể xóa r ở đây: xóa làm hỏng iterator mà vòng lặp đang dùng
    }
}
```

Cách đúng là dùng iterator và lấy iterator mới do `erase()` trả về:

```cpp
for (auto it = readings.begin(); it != readings.end(); ) {
    if (*it < 0) {
        it = readings.erase(it);   // erase trả về iterator tới phần tử kế tiếp
    } else {
        ++it;
    }
}
```

Bài C9 sẽ giới thiệu cách viết gọn hơn bằng `std::remove_if` kết hợp lambda.

**Cảnh báo `comparison of integer expressions of different signedness`**

```cpp
for (int i = 0; i < readings.size(); ++i) { }
// cảnh báo: comparison of integer expressions of different signedness:
//           'int' and 'std::vector<float>::size_type'
```

`size()` trả về `size_t`, kiểu không dấu. So sánh số âm với số không dấu cho kết quả sai (số âm bị hiểu thành số rất lớn). Cách sửa là dùng `size_t i`, hoặc tốt hơn là dùng range-based for khi không cần chỉ số.
