Trong C, ta cấp phát bộ nhớ động bằng `malloc()` và giải phóng bằng `free()`, và phải tự nhớ gọi `free()` ở đúng chỗ, đúng một lần. Quên `free()` gây rò rỉ bộ nhớ, gọi hai lần thì chương trình crash, còn dùng vùng nhớ sau khi đã `free()` thì chương trình chạy sai một cách khó đoán.

Với thiết bị nhúng, vấn đề này càng nghiêm trọng. Một ứng dụng HMI chạy liên tục nhiều tuần. Mỗi lần cập nhật màn hình rò rỉ vài chục byte thì sau vài ngày, board 512 MB RAM như BeagleBone Black cũng sẽ cạn bộ nhớ.

C++ bổ sung hai điều: constructor/destructor được gọi tự động theo vòng đời của đối tượng và kỹ thuật RAII gắn việc giải phóng tài nguyên vào vòng đời đó. Hiểu rõ bài này là điều kiện để viết code C++ không rò rỉ tài nguyên.

Trong bài, ta dùng một class nhỏ in ra thông báo mỗi khi đối tượng được tạo và bị hủy, để quan sát vòng đời:

```cpp
#include <iostream>
#include <string>

class Tracer {
public:
    Tracer(const std::string& name) : m_name(name)
    {
        std::cout << "Create " << m_name << "\n";
    }

    ~Tracer()
    {
        std::cout << "Destroy " << m_name << "\n";
    }

private:
    std::string m_name;
};
```

## Ba nơi một đối tượng có thể sống

Tùy cách khai báo, đối tượng được đặt ở một trong ba vùng nhớ và mỗi vùng có vòng đời khác nhau:

| Vùng nhớ | Cách tạo | Được tạo khi | Bị hủy khi |
|---|---|---|---|
| Stack | Biến cục bộ | Chương trình chạy tới dòng khai báo | Ra khỏi phạm vi `{ }` chứa nó |
| Heap | `new` | Gọi `new` | Gọi `delete` |
| Static | Biến toàn cục, biến `static` | Trước khi vào `main()` (biến `static` cục bộ: lần đầu chạy qua) | Sau khi `main()` kết thúc |

```cpp
Tracer g("global");

void counter()
{
    static Tracer s("local static");   // chỉ được tạo ở lần gọi đầu tiên
}

int main()
{
    std::cout << "Start of main\n";
    Tracer local("local");
    counter();
    counter();                           // không tạo lại
    std::cout << "End of main\n";
}

// in ra:
// Create global
// Start of main
// Create local
// Create local static
// End of main
// Destroy local
// Destroy local static
// Destroy global
```

Hai vùng ta làm việc nhiều nhất là stack và heap, được trình bày ở các mục tiếp theo.

## Đối tượng trên stack

Đối tượng khai báo như biến cục bộ nằm trên stack. Nó tự động bị hủy khi chương trình ra khỏi cặp ngoặc `{ }` chứa nó, bất kể ra bằng cách nào: chạy hết khối lệnh, `return` sớm hay `break` khỏi vòng lặp.

```cpp
void process(bool error)
{
    Tracer a("a");
    Tracer b("b");

    if (error) {
        return;        // a và b vẫn được hủy đúng cách
    }

    Tracer c("c");
}

process(true);
// in ra:
// Create a
// Create b
// Destroy b
// Destroy a
```

Để ý thứ tự: đối tượng tạo sau thì bị hủy trước, giống như chồng đĩa, đĩa đặt lên sau cùng được lấy ra đầu tiên. Quy tắc này đảm bảo đối tượng tạo sau (có thể phụ thuộc vào đối tượng tạo trước) luôn bị hủy khi những thứ nó phụ thuộc vẫn còn tồn tại.

Stack là lựa chọn mặc định vì nhanh và an toàn, nhưng có hai giới hạn:

- **Kích thước nhỏ.** Trên Linux, stack của luồng chính thường chỉ vài MB, stack của các luồng phụ còn nhỏ hơn. Không nên khai báo mảng lớn cục bộ như `uint8_t buffer[4 * 1024 * 1024];`.
- **Vòng đời gắn với phạm vi.** Đối tượng không thể sống lâu hơn hàm tạo ra nó. Nếu cần một đối tượng tồn tại suốt thời gian chạy của ứng dụng, được tạo trong một hàm và dùng ở hàm khác, ta phải dùng heap.

## Cấp phát trên heap với new và delete

Toán tử `new` tạo đối tượng trên heap và trả về con trỏ tới nó. Đối tượng tồn tại cho tới khi ta gọi `delete`.

```cpp
Tracer* createSensor()
{
    Tracer* p = new Tracer("sensor");   // cấp phát bộ nhớ và gọi constructor
    return p;                           // đối tượng vẫn sống sau khi hàm kết thúc
}

Tracer* sensor = createSensor();        // in ra: Create sensor
// ... dùng sensor ...
delete sensor;                          // gọi destructor rồi giải phóng bộ nhớ
                                        // in ra: Destroy sensor
```

:::warning Không dùng malloc/free cho đối tượng C++
`malloc()` chỉ cấp phát một vùng nhớ thô, không gọi constructor; `free()` không gọi destructor. Với đối tượng C++, ta luôn dùng `new`/`delete`, và không bao giờ trộn lẫn hai cặp này.
:::

```cpp
Tracer* a = (Tracer*)malloc(sizeof(Tracer));   // sai: constructor không được gọi,
                                               // m_name chưa được khởi tạo
```

Với mảng, ta dùng cặp `new[]` và `delete[]`:

```cpp
uint8_t* buffer = new uint8_t[1024];
// ...
delete[] buffer;     // phải có [] khi giải phóng mảng
```

## Memory leak

**Rò rỉ bộ nhớ** (memory leak) xảy ra khi đối tượng trên heap không còn con trỏ nào trỏ tới nhưng chưa được `delete`. Bộ nhớ đó không thể giải phóng được nữa cho tới khi chương trình kết thúc.

```cpp
void updateScreen()
{
    Tracer* frame = new Tracer("frame");
    // ... vẽ màn hình ...
}   // quên delete: mỗi lần gọi hàm mất thêm một đối tượng

for (int i = 0; i < 3; ++i) {
    updateScreen();
}
// in ra: Create frame (3 lần), không có dòng "Destroy frame" nào
```

Nguyên nhân phổ biến hơn cả việc "quên" là có nhiều đường thoát khỏi hàm, và `delete` bị sót ở một đường:

```cpp
bool sendData()
{
    uint8_t* buffer = new uint8_t[256];

    if (!prepare(buffer)) {
        return false;       // rò rỉ: quên delete[] ở nhánh này
    }

    transmit(buffer);
    delete[] buffer;
    return true;
}
```

Để phát hiện rò rỉ, khi chạy trên máy tính, ta có thể biên dịch với **AddressSanitizer**. Chương trình sẽ báo các vùng nhớ bị rò rỉ khi kết thúc:

```bash
g++ -std=c++17 -g -fsanitize=address main.cpp -o main
./main
# ERROR: LeakSanitizer: detected memory leaks
# Direct leak of 256 byte(s) ... in sendData() main.cpp:5
```

:::tip Kiểm tra rò rỉ trên board
Trên board, có thể dùng `valgrind --leak-check=full ./app` cho mục đích tương tự.
:::

## Con trỏ treo và xóa hai lần

Sau khi `delete`, con trỏ vẫn giữ nguyên địa chỉ cũ, nhưng vùng nhớ đó không còn hợp lệ. Con trỏ ở trạng thái này gọi là **con trỏ treo** (dangling pointer). Dùng nó hoặc `delete` nó thêm lần nữa đều gây lỗi.

```cpp
Tracer* p = new Tracer("sensor");
delete p;

delete p;    // xóa hai lần: crash với "free(): double free detected"
```

Một thói quen phòng tránh là gán `nullptr` ngay sau khi `delete`. Gọi `delete` trên `nullptr` là hợp lệ và không làm gì cả:

```cpp
delete p;
p = nullptr;

delete p;    // an toàn, không có tác dụng
if (p) {     // kiểm tra được trước khi dùng
    // ...
}
```

Tuy vậy, cách này chỉ bảo vệ được con trỏ `p`. Nếu có con trỏ khác cũng trỏ tới cùng đối tượng, con trỏ đó vẫn bị treo. Giải pháp triệt để là xác định rõ ai sở hữu đối tượng, như các mục sau.

## Thứ tự tạo và hủy của thành viên trong class

Khi một class có thành viên là đối tượng của class khác, thứ tự tạo và hủy tuân theo quy tắc:

- **Khi tạo**: các thành viên được tạo trước, theo thứ tự khai báo rồi mới chạy thân constructor.
- **Khi hủy**: thân destructor chạy trước rồi các thành viên bị hủy theo thứ tự ngược với lúc tạo.

```cpp
class Device {
public:
    Device() : m_uart("uart"), m_led("led")
    {
        std::cout << "Device constructor body\n";
    }

    ~Device()
    {
        std::cout << "Device destructor body\n";
    }

private:
    Tracer m_uart;
    Tracer m_led;
};

int main()
{
    Device device;
}

// in ra:
// Create uart
// Create led
// Device constructor body
// Device destructor body
// Destroy led
// Destroy uart
```

Có thể hình dung như lắp ráp một thiết bị: linh kiện phải có trước thì mới lắp được thiết bị; khi tháo, ta tháo vỏ thiết bị trước rồi mới lấy từng linh kiện ra. Nhờ vậy, trong thân constructor và destructor của `Device`, ta luôn dùng được `m_uart` và `m_led` một cách an toàn.

## Quyền sở hữu: đối tượng quản lý đối tượng khác

Khi một đối tượng tạo ra đối tượng khác bằng `new`, ta nói nó **sở hữu** đối tượng đó, và có trách nhiệm `delete` nó. Nơi tự nhiên nhất để `delete` chính là destructor:

```cpp
class Controller {
public:
    Controller()
        : m_sensor(new Tracer("sensor")),
          m_display(new Tracer("display"))
    {
    }

    ~Controller()
    {
        delete m_display;
        delete m_sensor;
    }

private:
    Tracer* m_sensor;
    Tracer* m_display;
};

int main()
{
    Controller controller;   // đối tượng trên stack
}   // controller bị hủy → destructor tự delete sensor và display

// in ra:
// Create sensor
// Create display
// Destroy display
// Destroy sensor
```

Điều đáng chú ý là ở `main()`, ta không viết `delete` nào. Chỉ cần đối tượng gốc `controller` nằm trên stack, khi nó bị hủy, destructor của nó dọn dẹp những gì nó sở hữu. Nếu `m_sensor` cũng sở hữu các đối tượng khác, destructor của `m_sensor` lại tiếp tục dọn dẹp, và cứ thế lan xuống. Ta có một **cây sở hữu** được giải phóng hoàn toàn tự động từ gốc.

:::note Quy tắc một chủ sở hữu
Mỗi đối tượng trên heap chỉ có đúng một chủ sở hữu, và chỉ chủ sở hữu được `delete` nó. Các nơi khác có thể giữ con trỏ để sử dụng, nhưng không bao giờ `delete`.
:::

## RAII

Ý tưởng ở mục trước áp dụng được cho mọi loại tài nguyên, không riêng bộ nhớ: file, cổng UART, socket mạng, khóa mutex. Kỹ thuật này gọi là **RAII** (Resource Acquisition Is Initialization): constructor lấy tài nguyên, destructor trả tài nguyên. Chỉ cần đối tượng bị hủy, tài nguyên chắc chắn được trả lại.

Trong C, khi làm việc với file, ta phải `fclose()` ở mọi đường thoát:

```c
// C
bool blink(void)
{
    FILE* f = fopen("/sys/class/leds/beaglebone:green:usr0/brightness", "w");
    if (!f) return false;

    if (fputs("1", f) < 0) {
        fclose(f);          // phải nhớ đóng ở đây
        return false;
    }

    fputs("0", f);
    fclose(f);              // và ở đây
    return true;
}
```

Với RAII, ta viết một class bọc lấy file:

```cpp
#include <cstdio>

class LedFile {
public:
    LedFile(const char* path) : m_file(std::fopen(path, "w")) {}

    ~LedFile()
    {
        if (m_file) {
            std::fclose(m_file);
            std::cout << "File closed\n";
        }
    }

    // Không cho sao chép đối tượng
    LedFile(const LedFile&) = delete;
    LedFile& operator=(const LedFile&) = delete;

    bool isOpen() const { return m_file != nullptr; }

    bool write(const char* value)
    {
        return std::fputs(value, m_file) >= 0 && std::fflush(m_file) == 0;
    }

private:
    FILE* m_file;
};
```

Hàm sử dụng trở nên gọn gàng, không còn một lệnh đóng file nào:

```cpp
bool blink()
{
    LedFile led("/sys/class/leds/beaglebone:green:usr0/brightness");
    if (!led.isOpen()) {
        return false;
    }

    if (!led.write("1")) {
        return false;       // file tự đóng
    }

    led.write("0");
    return true;            // file tự đóng
}
```

Dù hàm thoát ở đâu, destructor của `led` cũng được gọi và file được đóng. Thêm bao nhiêu nhánh `return` sau này cũng không thể quên đóng file.

Thư viện chuẩn C++ được xây dựng hoàn toàn theo RAII. `std::string` mà ta dùng từ Bài C2 tự cấp phát bộ nhớ cho chuỗi và tự giải phóng khi bị hủy; đó là lý do ta chưa bao giờ phải `delete` một `std::string`. Bài C10 sẽ giới thiệu smart pointer, là cách áp dụng RAII cho bất kỳ đối tượng nào tạo bằng `new`.

## Lỗi thường gặp

**Dùng `delete` cho mảng cấp phát bằng `new[]`: AddressSanitizer báo `alloc-dealloc-mismatch`**

```cpp
uint8_t* buffer = new uint8_t[256];
delete buffer;      // sai: phải là delete[]
```

Đây là hành vi không xác định. Với kiểu đơn giản như `uint8_t`, chương trình thường vẫn chạy bình thường nên lỗi dễ bị bỏ qua; với mảng đối tượng, chỉ phần tử đầu được gọi destructor, hoặc chương trình crash. Quy tắc: `new` đi với `delete`, `new[]` đi với `delete[]`.

**Trả về con trỏ tới đối tượng trên stack: `address of local variable 'sensor' returned`**

```cpp
Tracer* createSensor()
{
    Tracer sensor("sensor");
    return &sensor;   // cảnh báo
}

Tracer* p = createSensor();   // p trỏ tới đối tượng đã bị hủy
```

Đây là lỗi tương tự trả về tham chiếu tới biến cục bộ ở Bài C2. Nếu đối tượng cần sống lâu hơn hàm, hãy tạo bằng `new`.

**Sao chép đối tượng đang sở hữu con trỏ: crash với `free(): double free detected`**

```cpp
class Buffer {
public:
    Buffer(int size) : m_data(new uint8_t[size]) {}
    ~Buffer() { delete[] m_data; }

private:
    uint8_t* m_data;
};

Buffer a(64);
Buffer b = a;    // b sao chép giá trị con trỏ, cả a và b cùng trỏ tới một vùng nhớ
// khi ra khỏi phạm vi: b xóa vùng nhớ, rồi a xóa lại lần nữa
```

Khi sao chép, C++ mặc định chỉ sao chép giá trị từng thành viên, nên chỉ con trỏ được sao chép chứ không phải dữ liệu nó trỏ tới. Hai đối tượng cùng nghĩ mình là chủ sở hữu. Cách sửa nhanh là cấm sao chép như đã làm với `LedFile`:

```cpp
Buffer(const Buffer&) = delete;
Buffer& operator=(const Buffer&) = delete;
```

Khi đó dòng `Buffer b = a;` sẽ báo lỗi ngay lúc biên dịch thay vì crash lúc chạy. Bài C7 sẽ giải thích đầy đủ về sao chép đối tượng.

**Chương trình crash ngẫu nhiên do dùng đối tượng sau khi đã `delete`: AddressSanitizer báo `heap-use-after-free`**

```cpp
Tracer* sensor = new Tracer("sensor");
Tracer* display = sensor;    // con trỏ thứ hai, chỉ để sử dụng

delete sensor;
sensor = nullptr;

// display vẫn trỏ tới đối tượng đã bị hủy
```

Chương trình có thể vẫn chạy bình thường một thời gian rồi crash ở một chỗ không liên quan, khiến lỗi rất khó truy vết. AddressSanitizer chỉ đúng dòng gây lỗi. Cách phòng tránh là tuân thủ quy tắc một chủ sở hữu, và đảm bảo các con trỏ "chỉ để sử dụng" không bao giờ sống lâu hơn chủ sở hữu. Bài C10 sẽ giới thiệu `std::weak_ptr`, loại con trỏ biết được đối tượng đã bị hủy hay chưa.
