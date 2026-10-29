Một chương trình nhúng không chỉ dùng bộ nhớ. Nó còn mở file trong `/sys` để điều khiển GPIO, mở cổng UART, tạo socket, khóa mutex. Tất cả đều là **tài nguyên** (resource): thứ phải lấy về trước khi dùng và phải trả lại sau khi dùng xong.

Trong C, việc trả lại hoàn toàn do người viết code tự nhớ: `malloc()` phải đi kèm `free()`, `fopen()` đi kèm `fclose()`, `pthread_mutex_lock()` đi kèm `pthread_mutex_unlock()`. Với thiết bị nhúng, chỉ cần quên một lần là đủ gây chuyện, vì ứng dụng thường chạy liên tục nhiều tuần và một lỗi nhỏ lặp lại hàng triệu lần sẽ làm cạn tài nguyên của board.

C++ giải quyết vấn đề này bằng kỹ thuật **RAII**. Bài này trình bày các lỗi thường gặp khi tự quản lý tài nguyên, RAII giải quyết chúng như thế nào, các quy tắc khi viết một class RAII và cách dùng nó cho từng loại tài nguyên.

## Các vấn đề khi tự quản lý tài nguyên

### Memory leak

**Memory leak** xảy ra khi vùng nhớ trên heap không còn con trỏ nào trỏ tới nhưng chưa được giải phóng. Bộ nhớ đó không thể giải phóng được nữa cho tới khi chương trình kết thúc.

```cpp
void updateScreen()
{
    uint8_t* frame = new uint8_t[1024];
    // ... vẽ màn hình ...
}   // quên delete[]: mỗi lần gọi mất thêm 1 KB
```

Nếu `updateScreen()` được gọi 30 lần mỗi giây, sau một giờ ứng dụng đã mất hơn 100 MB.

Trường hợp phổ biến hơn là hàm có nhiều đường thoát và lệnh giải phóng bị sót ở một đường:

```cpp
bool sendData()
{
    uint8_t* buffer = new uint8_t[256];

    if (!prepare(buffer)) {
        return false;       // memory leak: quên delete[] ở nhánh này
    }

    transmit(buffer);
    delete[] buffer;
    return true;
}
```

Lỗi chỉ xuất hiện khi `prepare()` thất bại nên chạy thử bình thường không phát hiện được.

### Resource leak

Lỗi tương tự xảy ra với mọi tài nguyên khác, gọi chung là **resource leak**. Ví dụ đọc ADC qua sysfs:

```cpp
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>

int readAdc()
{
    int fd = open("/sys/bus/iio/devices/iio:device0/in_voltage0_raw", O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    char text[16] = {};
    if (read(fd, text, sizeof(text) - 1) <= 0) {
        return -1;          // resource leak: quên close(fd)
    }

    close(fd);
    return std::atoi(text);
}
```

Mỗi tiến trình trên Linux chỉ được mở một số file giới hạn (thường là 1024). Mỗi lần `read()` lỗi, một file descriptor bị mất. Tới khi hết, mọi lệnh `open()`, kể cả mở socket hay cổng UART, đều thất bại với lỗi `Too many open files`.

### Deadlock

Khóa mutex cũng là tài nguyên: khóa rồi phải mở.

```cpp
#include <mutex>

std::mutex g_mutex;
int g_counter = 0;

bool increase()
{
    g_mutex.lock();

    if (g_counter >= 100) {
        return false;       // quên unlock()
    }

    ++g_counter;
    g_mutex.unlock();
    return true;
}
```

Sau khi `g_counter` đạt 100, lần gọi tiếp theo thoát ra mà vẫn giữ khóa. Mọi luồng khác gọi `increase()` sẽ chờ ở `lock()` mãi mãi. Đây là **deadlock**: chương trình không crash, không báo lỗi, chỉ đứng im.

### Double free

**Double free** xảy ra khi cùng một vùng nhớ bị giải phóng hai lần, thường vì có hai con trỏ trỏ tới nó và không rõ ai chịu trách nhiệm giải phóng:

```cpp
uint8_t* frame = new uint8_t[64];
uint8_t* current = frame;       // con trỏ thứ hai tới cùng vùng nhớ

delete[] frame;
delete[] current;               // crash: free(): double free detected
```

### Dangling pointer và use-after-free

Sau khi giải phóng, con trỏ vẫn giữ địa chỉ cũ nhưng vùng nhớ đó không còn hợp lệ. Con trỏ ở trạng thái này gọi là **dangling pointer**. Dùng nó là lỗi **use-after-free**:

```cpp
uint8_t* frame = new uint8_t[64];
uint8_t* current = frame;

delete[] frame;
frame = nullptr;                // chỉ bảo vệ được frame

current[0] = 0xFF;              // use-after-free: current là dangling pointer
```

Đây là lỗi khó nhất trong nhóm. Vùng nhớ đã giải phóng có thể đang được cấp cho đối tượng khác, nên lệnh ghi trên làm hỏng dữ liệu của một phần code hoàn toàn không liên quan. Chương trình có thể chạy bình thường nhiều giờ rồi mới crash.

### Kết luận

Mọi lỗi trên có chung một nguyên nhân: **lệnh lấy và lệnh trả tài nguyên nằm tách rời nhau** và không có gì bắt buộc lệnh trả được gọi đúng một lần, đúng lúc. Hàm càng nhiều nhánh `return`, càng nhiều con trỏ cùng trỏ tới một tài nguyên thì càng dễ sai.

:::tip Phát hiện lỗi bộ nhớ
Khi chạy trên máy tính, biên dịch với `-fsanitize=address` (AddressSanitizer). Nó phát hiện memory leak, double free, use-after-free và chỉ ra dòng code gây lỗi. Trên board có thể dùng `valgrind --leak-check=full ./app`.

```bash
g++ -std=c++17 -g -fsanitize=address main.cpp -o main
./main
# ERROR: LeakSanitizer: detected memory leaks
# Direct leak of 256 byte(s) ... in sendData() main.cpp:5
```
:::

## RAII là gì

**RAII** (Resource Acquisition Is Initialization) là kỹ thuật gắn vòng đời của tài nguyên vào vòng đời của một đối tượng:

- **Constructor lấy tài nguyên** (cấp phát, mở file, khóa mutex...).
- **Destructor trả tài nguyên** (giải phóng, đóng file, mở khóa...).

RAII dựa trên một đảm bảo của C++: đối tượng khai báo như biến cục bộ tự bị hủy khi chương trình ra khỏi cặp ngoặc `{ }` chứa nó và destructor chắc chắn được gọi, bất kể thoát bằng cách nào: chạy hết khối lệnh, `return` sớm, `break` hay exception. Compiler tự chèn lời gọi destructor vào mọi lối thoát đó. Đây là thứ C hoàn toàn không có.

Ví dụ: bật tắt LED qua sysfs. Trong C, ta phải `fclose()` ở mọi đường thoát:

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

Với RAII, ta viết một class bọc lấy `FILE*`:

```cpp
#include <cstdio>
#include <iostream>

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

Hàm sử dụng không còn lệnh đóng file nào:

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

Lệnh lấy (`fopen`) và lệnh trả (`fclose`) giờ nằm cạnh nhau trong một class, được viết và kiểm tra đúng một lần. Thêm bao nhiêu nhánh `return` vào `blink()` sau này cũng không thể gây resource leak.

## Quy tắc khi viết class RAII

| Quy tắc | Lý do |
|---|---|
| 1. Constructor lấy tài nguyên | Đối tượng tồn tại đồng nghĩa với tài nguyên đã được lấy |
| 2. Destructor trả tài nguyên, kiểm tra trước khi trả | Tài nguyên có thể lấy thất bại (`fopen` trả `nullptr`) |
| 3. Cấm sao chép | Tránh hai đối tượng cùng trả một tài nguyên (double free) |
| 4. Mỗi class chỉ quản lý một tài nguyên | Tránh leak khi constructor thất bại giữa chừng |
| 5. Tạo đối tượng RAII trên stack hoặc làm thành viên class | Để việc hủy và việc trả tài nguyên xảy ra tự động |

**Quy tắc 1 và 2.** Nếu lấy tài nguyên thất bại, đối tượng phải ở một trạng thái rõ ràng mà người dùng kiểm tra được như `isOpen()` của `LedFile` hoặc constructor báo lỗi bằng exception. Destructor phải xử lý được cả trường hợp này nên luôn có `if (m_file)` trước `fclose()`. Destructor cũng không được ném exception.

**Quy tắc 3.** Khi sao chép đối tượng, C++ mặc định sao chép từng thành viên. Với `LedFile`, chỉ con trỏ `FILE*` được sao chép nên hai đối tượng cùng giữ một file và cả hai đều gọi `fclose()` khi bị hủy. Hai dòng `= delete` biến lỗi này thành lỗi biên dịch.

**Quy tắc 4.** Destructor chỉ được gọi nếu constructor đã chạy xong. Nếu một class tự giữ hai tài nguyên và constructor thất bại khi lấy tài nguyên thứ hai, destructor không chạy và tài nguyên thứ nhất bị leak:

```cpp
class Logger {
public:
    Logger()
        : m_log(std::fopen("/var/log/app.log", "a")),
          m_buffer(new char[4096])    // nếu new thất bại, m_log không bao giờ được đóng
    {
    }

    ~Logger()
    {
        delete[] m_buffer;
        std::fclose(m_log);
    }

private:
    FILE* m_log;
    char* m_buffer;
};
```

Nếu mỗi tài nguyên được bọc trong class RAII riêng và class lớn chỉ chứa các đối tượng đó làm thành viên, các thành viên đã tạo xong sẽ tự bị hủy khi constructor thất bại. Class lớn thậm chí không cần viết destructor.

**Quy tắc 5.** RAII chỉ có tác dụng khi đối tượng bị hủy tự động. Viết `LedFile* led = new LedFile(...)` là quay về vấn đề cũ: phải nhớ `delete led`.

## Quản lý tài nguyên bằng RAII

### Bộ nhớ

Sửa hàm `sendData()` bằng cách bọc vùng nhớ động vào một class:

```cpp
class Buffer {
public:
    Buffer(size_t size) : m_data(new uint8_t[size]), m_size(size) {}
    ~Buffer() { delete[] m_data; }

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    uint8_t* data() { return m_data; }
    size_t size() const { return m_size; }

private:
    uint8_t* m_data;
    size_t m_size;
};

bool sendData()
{
    Buffer buffer(256);

    if (!prepare(buffer.data())) {
        return false;       // buffer tự giải phóng
    }

    transmit(buffer.data());
    return true;            // buffer tự giải phóng
}
```

Thực tế ta hiếm khi phải tự viết class như `Buffer`, vì thư viện chuẩn đã có sẵn `std::vector<uint8_t> buffer(256);`. Với một đối tượng đơn lẻ tạo bằng `new`, [Bài C10](../10-smart-pointer/README.md) sẽ giới thiệu smart pointer.

### File descriptor

Hàm `readAdc()` dùng `open()`/`close()` của POSIX, thư viện chuẩn C++ không có class bọc sẵn, nên ta tự viết:

```cpp
class FileDescriptor {
public:
    FileDescriptor(const char* path, int flags) : m_fd(open(path, flags)) {}

    ~FileDescriptor()
    {
        if (m_fd >= 0) {
            close(m_fd);
        }
    }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;

    bool isValid() const { return m_fd >= 0; }
    int get() const { return m_fd; }

private:
    int m_fd;
};

int readAdc()
{
    FileDescriptor fd("/sys/bus/iio/devices/iio:device0/in_voltage0_raw", O_RDONLY);
    if (!fd.isValid()) {
        return -1;
    }

    char text[16] = {};
    if (read(fd.get(), text, sizeof(text) - 1) <= 0) {
        return -1;          // fd tự đóng
    }

    return std::atoi(text); // fd tự đóng
}
```

Cùng một mẫu này áp dụng được cho socket, cổng UART hay handle của bất kỳ thư viện C nào.

### Mutex

Thư viện chuẩn có sẵn `std::lock_guard`, khóa mutex trong constructor và mở khóa trong destructor. Sửa hàm `increase()`:

```cpp
bool increase()
{
    std::lock_guard<std::mutex> lock(g_mutex);   // khóa

    if (g_counter >= 100) {
        return false;       // tự mở khóa
    }

    ++g_counter;
    return true;            // tự mở khóa
}
```

Không còn nhánh nào thoát khỏi hàm mà quên mở khóa.

### Quyền sở hữu: đối tượng quản lý đối tượng khác

Khi một class tạo đối tượng khác bằng `new`, ta nói nó **sở hữu** (owns) đối tượng đó và phải `delete` nó trong destructor. Để quan sát, ta dùng class `Tracer` in ra thông báo khi được tạo và bị hủy:

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

    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

private:
    Tracer* m_sensor;
    Tracer* m_display;
};

int main()
{
    Controller controller;
}   // controller bị hủy -> destructor tự delete sensor và display

// in ra:
// Create sensor
// Create display
// Destroy display
// Destroy sensor
```

Trong `main()` không có lệnh `delete` nào. Đối tượng gốc `controller` nằm trên stack; khi nó bị hủy, destructor của nó dọn dẹp những gì nó sở hữu, và nếu những đối tượng đó lại sở hữu đối tượng khác, việc dọn dẹp tiếp tục lan xuống. Cả ứng dụng là một **cây sở hữu** được giải phóng tự động từ gốc:

```
  main()
    |
    +-- controller        (stack, tự hủy khi ra khỏi main)
          |
          +-- sensor      (heap, do controller delete)
          +-- display     (heap, do controller delete)
```

:::note Quy tắc một chủ sở hữu
Mỗi tài nguyên chỉ có đúng một chủ sở hữu (owner), và chỉ chủ sở hữu được giải phóng nó. Các nơi khác có thể giữ con trỏ để sử dụng, nhưng không bao giờ `delete`, và không được dùng con trỏ đó lâu hơn thời gian sống của chủ sở hữu. Quy tắc này loại bỏ double free và hạn chế dangling pointer.
:::

Khi một class có thành viên là đối tượng, thứ tự tạo và hủy như sau:

- **Khi tạo:** các thành viên được tạo theo thứ tự khai báo, rồi mới chạy thân constructor.
- **Khi hủy:** thân destructor chạy trước, rồi các thành viên bị hủy theo thứ tự ngược với khai báo.

Nhờ vậy, trong thân constructor và destructor, mọi thành viên luôn còn dùng được. `Controller` ở trên vẫn vi phạm quy tắc 4 vì giữ hai con trỏ thô.

### RAII trong thư viện chuẩn

Thư viện chuẩn C++ được xây dựng theo RAII. Đó là lý do ta chưa bao giờ phải giải phóng một `std::string`:

| Tài nguyên | Class RAII |
|---|---|
| Chuỗi ký tự | `std::string` |
| Mảng động | `std::vector` |
| File | `std::ifstream`, `std::ofstream` |
| Khóa mutex | `std::lock_guard`, `std::unique_lock` |
| Đối tượng trên heap | `std::unique_ptr`, `std::shared_ptr` |

Quy tắc thực hành: dùng các class có sẵn này trước; chỉ tự viết class RAII cho tài nguyên mà thư viện chuẩn không có, như file descriptor, socket hay handle của thư viện C.

## Lỗi thường gặp

**Quên cấm sao chép class sở hữu tài nguyên: crash với `free(): double free detected`**

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

Hai đối tượng cùng nghĩ mình là chủ sở hữu. Thêm hai dòng `= delete` cho copy constructor và toán tử gán, dòng `Buffer b = a;` sẽ báo lỗi lúc biên dịch thay vì crash lúc chạy.

**Dùng `delete` cho mảng cấp phát bằng `new[]`: AddressSanitizer báo `alloc-dealloc-mismatch`**

```cpp
uint8_t* buffer = new uint8_t[256];
delete buffer;      // sai: phải là delete[]
```

Đây là hành vi không xác định. Với kiểu đơn giản như `uint8_t`, chương trình thường vẫn chạy nên lỗi dễ bị bỏ qua; với mảng đối tượng, chỉ phần tử đầu được gọi destructor hoặc chương trình crash. Dùng `std::vector` thì không còn phải chọn giữa `delete` và `delete[]`.

**Dùng `malloc()`/`free()` cho đối tượng C++**

```cpp
Tracer* t = (Tracer*)malloc(sizeof(Tracer));   // constructor không được gọi
free(t);                                       // destructor không được gọi
```

`malloc()` chỉ cấp phát vùng nhớ thô, `free()` không gọi destructor, nên tài nguyên mà đối tượng giữ bị leak. Với đối tượng C++, luôn dùng `new`/`delete`, hoặc tốt hơn là class RAII.

**Trả về con trỏ tới đối tượng cục bộ: `address of local variable 'sensor' returned`**

```cpp
Tracer* createSensor()
{
    Tracer sensor("sensor");
    return &sensor;   // sensor bị hủy ngay khi hàm kết thúc
}
```

Con trỏ nhận được là dangling pointer ngay từ đầu. Nếu đối tượng cần sống lâu hơn hàm, hãy trả về chính đối tượng hoặc một smart pointer.

**Con trỏ không sở hữu sống lâu hơn chủ sở hữu: AddressSanitizer báo `heap-use-after-free`**

```cpp
Tracer* display = nullptr;
{
    Controller controller;
    display = controller.display();   // giả sử Controller có hàm trả về m_display
}   // controller hủy -> display bị delete

// display giờ là dangling pointer
```

Cách phòng tránh là tuân thủ quy tắc một chủ sở hữu: con trỏ không sở hữu không bao giờ được sống lâu hơn chủ sở hữu.
