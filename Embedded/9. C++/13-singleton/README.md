Trong C, khi cần một thứ dùng chung cho cả chương trình, như bộ ghi nhật ký hay cấu hình hệ thống, ta thường khai báo biến toàn cục, hoặc viết một module `.c` với biến `static` bên trong và các hàm truy cập bên ngoài. Cách làm này tự nhiên trong C nhưng có nhiều rủi ro: bất kỳ ai cũng có thể sửa biến toàn cục, thứ tự khởi tạo giữa các file không xác định, và không có gì ngăn ai đó vô tình tạo thêm một bản thứ hai. **Singleton** là pattern đảm bảo một class chỉ có đúng một đối tượng, kèm một điểm truy cập chung cho toàn chương trình.

Đây cũng là pattern gây tranh cãi nhiều nhất. Bài này trình bày cách cài đặt đúng, nhưng phần quan trọng không kém là hiểu vì sao nên hạn chế dùng nó.

## Vấn đề thực tế

Xét bộ ghi nhật ký `Logger` ghi vào file `/var/log/app.log`. Gần như module nào cũng cần ghi nhật ký: driver UART, bộ xử lý dữ liệu, module mạng, giao diện. Nếu mỗi module tự tạo `Logger` riêng:

```cpp
// uart_driver.cpp
Logger logger("/var/log/app.log");

// network.cpp
Logger logger("/var/log/app.log");
```

Ta có nhiều đối tượng cùng mở một file. Các dòng nhật ký có thể ghi chồng lên nhau, mỗi đối tượng có cấu hình riêng (mức log, định dạng thời gian) không đồng bộ, và thay đổi cấu hình ở một nơi không ảnh hưởng nơi khác.

Điều ta muốn là: cả chương trình chỉ có một `Logger`, và mọi module đều truy cập được nó.

## Ý tưởng của Singleton

Singleton đạt được hai mục tiêu bằng hai kỹ thuật:

- **Chỉ có một đối tượng**: đặt constructor ở phần `private`, nên bên ngoài không thể tự tạo đối tượng. Đồng thời cấm sao chép để không ai nhân bản được.
- **Truy cập từ mọi nơi**: cung cấp một hàm `static` tên là `instance()`, trả về đối tượng duy nhất đó.

```
   uart_driver.cpp --+
   network.cpp ------+--> Logger::instance() --> [ đối tượng Logger duy nhất ]
   display.cpp ------+
```

## Cách cài đặt cổ điển

Cách cài đặt thường thấy trong tài liệu cũ dùng một con trỏ `static`, tạo đối tượng ở lần gọi đầu tiên:

```cpp
class Logger {
public:
    static Logger* instance()
    {
        if (instance_ == nullptr) {
            instance_ = new Logger();
        }
        return instance_;
    }

private:
    Logger() {}
    static Logger* instance_;
};

Logger* Logger::instance_ = nullptr;
```

Cách này có hai lỗi:

- **Không an toàn khi đa luồng.** Nếu hai luồng gọi `instance()` cùng lúc, cả hai có thể cùng thấy `instance_ == nullptr` và cùng tạo đối tượng. Kết quả là có hai `Logger`, và một cái bị rò rỉ bộ nhớ.
- **Destructor không bao giờ được gọi.** Đối tượng tạo bằng `new` nhưng không ai `delete`. Với `Logger`, điều này nghĩa là dữ liệu còn trong bộ đệm có thể không được ghi xuống file khi chương trình kết thúc.

## Cách cài đặt chuẩn

Cách cài đặt được khuyên dùng trong C++ hiện đại dựa vào biến `static` cục bộ trong hàm:

```cpp
class Logger {
public:
    static Logger& instance()
    {
        static Logger logger;    // tạo ở lần gọi đầu tiên, chỉ một lần duy nhất
        return logger;
    }

    // cấm sao chép và di chuyển (Bài C7)
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(const char* message)
    {
        std::printf("[%u] %s\n", ++count_, message);
    }

private:
    Logger()  { std::printf("[Logger] Created\n"); }
    ~Logger() { std::printf("[Logger] Destroyed\n"); }

    unsigned count_ = 0;
};
```

Biến `static` cục bộ có ba tính chất khiến nó hoàn hảo cho Singleton:

- Chỉ được khởi tạo một lần, ở lần đầu tiên chương trình chạy qua dòng khai báo.
- An toàn khi đa luồng. Từ C++11, chuẩn ngôn ngữ đảm bảo nếu nhiều luồng cùng gọi lần đầu, chỉ một luồng khởi tạo, các luồng khác chờ cho tới khi khởi tạo xong.
- Tự động bị hủy khi chương trình kết thúc bình thường, nên destructor được gọi.

Khi khai báo `delete` cho copy constructor, compiler cũng không tự tạo move constructor (Bài C7), nên `Logger` vừa không sao chép được vừa không di chuyển được.

Cách sử dụng:

```cpp
int main()
{
    Logger::instance().log("Application started");
    Logger::instance().log("UART opened");
    Logger::instance().log("Connected to server");
    return 0;
}
```

Kết quả:

```
[Logger] Created
[1] Application started
[2] UART opened
[3] Connected to server
[Logger] Destroyed
```

Constructor chỉ chạy một lần, và bộ đếm tăng liên tục qua các lần gọi, chứng tỏ tất cả đều dùng chung một đối tượng.

Nếu cần gọi nhiều lần trong một hàm, ta có thể giữ tham chiếu cho gọn:

```cpp
Logger& logger = Logger::instance();
logger.log("Line 1");
logger.log("Line 2");
```

:::warning Chỉ việc khởi tạo là an toàn đa luồng
Chuẩn C++ chỉ đảm bảo việc khởi tạo là an toàn khi đa luồng. Các hàm thành viên như `log()` vẫn có thể bị nhiều luồng gọi cùng lúc, nên nếu chúng sửa dữ liệu bên trong (như `count_`, hoặc ghi file), ta vẫn phải tự bảo vệ bằng mutex (Bài C18).
:::

:::note Destructor không chạy khi chương trình crash
Destructor của đối tượng `static` chỉ chạy khi chương trình kết thúc bình thường (trả về từ `main()` hoặc gọi `exit()`). Nếu chương trình bị crash, bị `kill -9`, hoặc thoát bằng `_exit()`, destructor không chạy. Với `Logger`, ta nên ghi dữ liệu xuống file ngay sau mỗi dòng quan trọng thay vì trông chờ destructor.
:::

## Vấn đề thứ tự khởi tạo giữa các file

Để thấy vì sao cách dùng biến `static` cục bộ tốt hơn biến toàn cục, xét trường hợp hai đối tượng toàn cục ở hai file khác nhau:

```cpp
// logger.cpp
Logger gLogger;

// config.cpp
Config gConfig;     // constructor của Config gọi gLogger.log("Loading config")
```

Chuẩn C++ không quy định đối tượng toàn cục ở file nào được khởi tạo trước. Nếu `gConfig` được tạo trước, nó sẽ gọi hàm trên `gLogger` chưa được khởi tạo. Lỗi này có thể xuất hiện hay biến mất chỉ vì đổi thứ tự file trong `CMakeLists.txt`.

Với cách dùng `Logger::instance()`, `Logger` được tạo đúng lúc lần đầu được cần đến, nên vấn đề này không xảy ra.

## Vấn đề thứ tự hủy

Thứ tự khởi tạo đã được giải quyết, nhưng thứ tự hủy vẫn có thể gây lỗi. Các đối tượng `static` bị hủy theo thứ tự ngược với thứ tự chúng khởi tạo xong. Xét `Config` cũng là Singleton, và destructor của nó ghi nhật ký:

```cpp
Config::~Config()
{
    Logger::instance().log("Saving config");
}
```

Nếu chương trình gọi `Config::instance()` trước rồi mới dùng `Logger` lần đầu, thì `Logger` khởi tạo xong sau `Config`, nên bị hủy trước `Config`. Khi destructor của `Config` chạy, nó gọi `log()` trên một `Logger` đã bị hủy.

Có một số cách tránh:

- **Không dùng Singleton khác bên trong destructor của Singleton.** Đây là cách an toàn nhất.
- **Dùng Singleton phụ thuộc ngay trong constructor.** Nếu constructor của `Config` gọi `Logger::instance()`, thì `Logger` chắc chắn khởi tạo xong trước `Config`, và do đó bị hủy sau.

```cpp
Config::Config()
{
    Logger::instance().log("Loading config");   // Logger khởi tạo xong trước Config
}
```

Chính sự phức tạp này là một trong những lý do Singleton bị xem là pattern "có vấn đề" khi dùng nhiều.

## Nhược điểm của Singleton

Singleton giải quyết được bài toán "chỉ một đối tượng", nhưng kéo theo những nhược điểm của biến toàn cục:

**Phụ thuộc bị che giấu.** Nhìn vào khai báo hàm, ta không biết nó dùng những gì:

```cpp
float readTemperature(int channel);   // hàm này có dùng Logger, Config, I2cBus không?
```

Muốn biết, ta phải đọc toàn bộ phần cài đặt. Khi chương trình lớn, rất khó nắm được module nào phụ thuộc vào module nào.

**Khó test.** Nếu hàm bên trong gọi `Logger::instance()` và `I2cBus::instance()`, ta không có cách nào thay chúng bằng bản giả lập khi chạy test trên máy tính. Hơn nữa, trạng thái của Singleton còn lưu lại giữa các test, khiến kết quả test này ảnh hưởng tới test sau.

**Trạng thái toàn cục khó kiểm soát.** Bất kỳ module nào cũng có thể sửa `Config::instance()`. Khi cấu hình bị sai, ta phải lục khắp chương trình để tìm nơi đã sửa.

**Giả định "chỉ có một" bị phá vỡ.** Đây là vấn đề rất hay gặp trong nhúng. Ta viết `I2cBus` dạng Singleton vì board hiện tại chỉ dùng một bus I2C. Phiên bản phần cứng sau có thêm bus thứ hai, và mọi chỗ gọi `I2cBus::instance()` trong toàn bộ dự án phải sửa lại.

## Cách thay thế: tạo một lần, truyền đi

Phần lớn trường hợp, thứ ta thực sự cần là "chương trình chỉ tạo một đối tượng", chứ không phải "class không cho phép tạo hơn một đối tượng". Hai điều này khác nhau. Ta có thể đạt được điều đầu bằng cách tạo đối tượng một lần trong `main()` rồi truyền tham chiếu cho những ai cần:

```cpp
class SensorService {
public:
    SensorService(Logger& logger, I2cBus& bus) : logger_(logger), bus_(bus) {}

    float readTemperature(int channel)
    {
        logger_.log("Reading temperature");
        // đọc qua bus_...
        return 25.0f;
    }

private:
    Logger& logger_;
    I2cBus& bus_;
};

int main()
{
    Logger logger("/var/log/app.log");
    I2cBus bus("/dev/i2c-1");

    SensorService sensors(logger, bus);
    // ...
}
```

Cách này giải quyết các nhược điểm ở trên:

- Nhìn vào constructor của `SensorService` là biết ngay nó phụ thuộc vào `Logger` và `I2cBus`.
- Thứ tự khởi tạo và hủy rõ ràng theo thứ tự khai báo trong `main()`.
- Khi test, ta truyền vào bản giả lập thay cho bus thật.
- Board có hai bus I2C thì tạo hai đối tượng `I2cBus` và truyền mỗi cái cho đúng nơi dùng.

Kỹ thuật này gọi là **Dependency Injection**, sẽ được trình bày đầy đủ ở Bài C29 cùng với Factory.

## Biến thể: điểm truy cập toàn cục có thể thay thế

Với một số thứ như nhật ký, việc truyền tham chiếu đi khắp nơi quá cồng kềnh, vì gần như hàm nào cũng cần ghi log. Một cách dung hòa là giữ điểm truy cập toàn cục, nhưng cho phép thay đối tượng phía sau khi khởi động hoặc khi test:

```cpp
class ILogger {
public:
    virtual ~ILogger() = default;
    virtual void log(const char* message) = 0;
};

class Log {
public:
    static ILogger& get()           { return *current(); }
    static void install(ILogger& l) { current() = &l; }

private:
    static ILogger*& current()
    {
        static ConsoleLogger defaultLogger;     // mặc định in ra console
        static ILogger* logger = &defaultLogger;
        return logger;
    }
};
```

Code trong chương trình vẫn gọi gọn như Singleton:

```cpp
Log::get().log("UART opened");
```

Nhưng khi chạy trên board, `main()` cài đặt bộ ghi vào file; khi test, ta cài đặt bộ ghi giả lập để kiểm tra nội dung log:

```cpp
FileLogger fileLogger("/var/log/app.log");
Log::install(fileLogger);
```

Biến thể này chỉ nên dùng cho số ít dịch vụ thực sự có mặt ở khắp nơi, như nhật ký. Nó vẫn mang nhược điểm phụ thuộc bị che giấu, và đối tượng được `install` phải sống lâu hơn mọi chỗ gọi `Log::get()`.

## Khi nào nên và không nên dùng

Singleton có thể chấp nhận được khi:

- **Dịch vụ cắt ngang toàn chương trình** như ghi nhật ký, nơi việc truyền tham chiếu đi khắp nơi gây cồng kềnh hơn lợi ích nó mang lại.
- **Dữ liệu chỉ đọc sau khi khởi động**, như thông tin phiên bản firmware hay cấu hình nạp một lần lúc khởi động và không bao giờ thay đổi.

Không nên dùng Singleton khi:

- **Chỉ để khỏi phải truyền tham số.** Đây là lý do phổ biến nhất nhưng cũng sai nhất.
- **Tài nguyên phần cứng có thể có nhiều bản** như bus I2C, cổng UART, đường GPIO. Hôm nay có một, nhưng phiên bản phần cứng sau có thể có hai.
- **Trạng thái thay đổi liên tục và được nhiều module sửa.** Lúc đó Singleton chỉ là biến toàn cục mang tên khác.
- **Class cần được test với bản giả lập.**

Một câu hỏi hữu ích trước khi dùng Singleton: nếu hệ thống có hai đối tượng này thì có sai về mặt logic không? Nếu câu trả lời là "không sai, chỉ là hiện tại chưa cần", thì đừng dùng Singleton.
