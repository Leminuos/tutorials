Người viết firmware bằng C thường tổ chức chương trình theo một trong hai cách. Cách thứ nhất là vòng lặp chính `while (1)` lần lượt kiểm tra mọi thứ: có byte UART mới không, nút có được bấm không, đã đến giờ đọc cảm biến chưa. Cách thứ hai là dùng RTOS, mỗi task một vòng lặp riêng, và các task trao đổi với nhau qua hàng đợi thông điệp. Cách thứ hai thực chất chính là **Active Object**: mỗi module có luồng riêng và hàng đợi riêng, chỉ giao tiếp với bên ngoài bằng thông điệp. Bài này cài đặt pattern đó bằng C++, dựa trên `BlockingQueue` của Bài C15 và máy trạng thái của Bài C14.

## Vấn đề thực tế

Ở Bài C14, ta đã viết máy trạng thái `Connection` quản lý kết nối UART. Trong ứng dụng thật, sự kiện đến với nó từ nhiều luồng khác nhau:

- Luồng giao diện gửi `Connect` khi người dùng bấm nút.
- Luồng đọc UART gửi `Success` hoặc `Failure` khi nhận được phản hồi.
- Luồng timer gửi `Timeout` khi quá lâu không có dữ liệu.

Nếu cả ba luồng cùng gọi `conn.handle()`, máy trạng thái bị truy cập đồng thời: hai luồng cùng đọc và sửa `state_`, `retries_`, dẫn đến trạng thái sai. Giải pháp đầu tiên ai cũng nghĩ tới là thêm mutex:

```cpp
void Connection::handle(Event e)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // ... xử lý máy trạng thái, có thể gọi sang module khác
}
```

Cách này giải quyết được race condition, nhưng kéo theo vấn đề mới:

- **Bên gọi bị chặn.** Nếu `handle()` mất thời gian (gửi bản tin, ghi log), luồng giao diện gọi `connect()` cũng phải chờ theo, và giao diện bị đơ.
- **Nguy cơ deadlock.** Trong `handle()`, máy trạng thái có thể gọi sang module khác, ví dụ báo cho giao diện cập nhật trạng thái. Nếu đúng lúc đó luồng giao diện đang giữ khóa của nó và gọi `conn.connect()`, hai luồng chờ nhau mãi mãi.
- **Mutex lan khắp nơi.** Mọi module bị gọi từ nhiều luồng đều cần mutex, và ta phải luôn nhớ thứ tự khóa giữa chúng để tránh deadlock. Khi hệ thống lớn dần, điều này gần như không thể kiểm soát.

## Ý tưởng của Active Object

Active Object đảo ngược cách tiếp cận. Thay vì để nhiều luồng cùng chạy vào bên trong đối tượng rồi dùng khóa để ngăn chúng va chạm, ta cho đối tượng có luồng riêng của nó, và chỉ luồng đó được chạm vào dữ liệu bên trong.

```
  Luồng giao diện --+
                    |  post(Connect)
  Luồng UART -------+----------------->  +------------------------------+
                    |  post(Success)     |  Hàng đợi sự kiện            |
  Luồng timer ------+                    |  [Connect][Success][Timeout] |
                       post(Timeout)     +--------------+---------------+
                                                        | lấy ra từng sự kiện
                                                        v
                                         +------------------------------+
                                         |  Luồng riêng của đối tượng   |
                                         |  xử lý tuần tự: handle(e)    |
                                         |  state_, retries_ chỉ được   |
                                         |  truy cập từ luồng này       |
                                         +------------------------------+
```

Active Object gồm ba thành phần:

- **Hàng đợi sự kiện**: nơi các luồng khác đặt sự kiện vào.
- **Luồng riêng** chạy một vòng lặp sự kiện (**event loop**): liên tục lấy sự kiện ra khỏi hàng đợi và xử lý từng cái một.
- **Các hàm công khai** không trực tiếp thực hiện logic, mà chỉ đóng gói yêu cầu thành sự kiện, đặt vào hàng đợi, rồi trả về ngay.

Nhờ vậy, mọi vấn đề ở phần trên đều được giải quyết. Bên gọi không bao giờ bị chặn lâu, vì nó chỉ đặt sự kiện vào hàng đợi. Dữ liệu bên trong chỉ có một luồng truy cập, nên không cần mutex cho máy trạng thái. Và vì các module không gọi trực tiếp vào nhau mà chỉ gửi thông điệp, không có chuỗi khóa nào để gây deadlock.

## Cài đặt vòng lặp sự kiện

Ta viết một lớp `EventLoop` dùng chung, gói hàng đợi và luồng xử lý lại với nhau. Lớp này nhận vào một hàm xử lý, và gọi hàm đó cho từng sự kiện lấy ra từ hàng đợi:

```cpp
#include <functional>
#include <thread>

template <typename EventT, size_t N = 32>
class EventLoop {
public:
    using Handler = std::function<void(const EventT&)>;

    EventLoop(Handler handler) : handler_(std::move(handler)) {}
    ~EventLoop() { stop(); }

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void start()
    {
        thread_ = std::thread([this] { run(); });
    }

    void stop()
    {
        queue_.close();                // không nhận thêm sự kiện
        if (thread_.joinable()) {
            thread_.join();            // chờ luồng xử lý hết sự kiện còn lại
        }
    }

    bool post(const EventT& event)
    {
        return queue_.push(event);
    }

private:
    void run()
    {
        while (auto event = queue_.pop()) {
            handler_(*event);
        }
    }

    Handler                  handler_;
    BlockingQueue<EventT, N> queue_;   // từ Bài C15
    std::thread              thread_;
};
```

Hàm `run()` chính là vòng lặp sự kiện. Nó ngủ khi hàng đợi rỗng, thức dậy khi có sự kiện, xử lý xong thì quay lại chờ. Khi `stop()` được gọi, hàng đợi bị đóng; nhờ cách cài đặt `pop()` ở Bài C15, vòng lặp vẫn xử lý hết các sự kiện còn tồn đọng rồi mới kết thúc.

`EventLoop` bị cấm sao chép (Bài C7) vì nó sở hữu một luồng. Gọi `stop()` nhiều lần vẫn an toàn: lần thứ hai, hàng đợi đã đóng sẵn và luồng không còn `joinable`.

## Biến máy trạng thái thành Active Object

Giờ ta bọc máy trạng thái của Bài C14 thành một Active Object. Phần logic `handle()` và `transitionTo()` giữ nguyên, chỉ thay đổi cách sự kiện đi vào:

```cpp
class ConnectionManager {
public:
    ConnectionManager()
        : loop_([this](const Event& e) { handle(e); })
    {
        loop_.start();
    }

    ~ConnectionManager()
    {
        loop_.stop();     // dừng luồng trước khi các thành viên khác bị hủy
    }

    // API công khai: chỉ gửi sự kiện rồi trả về ngay, gọi được từ bất kỳ luồng nào
    void connect()            { loop_.post(Event::Connect); }
    void disconnect()         { loop_.post(Event::Disconnect); }
    void onResponse(bool ok)  { loop_.post(ok ? Event::Success : Event::Failure); }
    void onTimeout()          { loop_.post(Event::Timeout); }

private:
    // máy trạng thái từ Bài C14: chỉ chạy trên luồng của loop_, không cần mutex
    void handle(Event e);
    void transitionTo(State next);
    void onEnter(State s);
    void onExit(State s);

    State state_   = State::Disconnected;
    int   retries_ = 0;

    EventLoop<Event> loop_;   // khai báo CUỐI CÙNG
};
```

Hai chi tiết quan trọng về vòng đời:

**`loop_` được khai báo sau cùng.** Các thành viên được khởi tạo theo thứ tự khai báo, nên khi `loop_` khởi tạo và luồng bắt đầu chạy, `state_` và `retries_` đã sẵn sàng. Ngược lại, các thành viên bị hủy theo thứ tự ngược lại, nên `loop_` bị hủy đầu tiên, dừng luồng trước khi `state_` biến mất.

**Destructor gọi `stop()` một cách tường minh.** Dù `loop_` tự dừng khi bị hủy, việc gọi rõ ràng trong destructor giúp người đọc thấy ngay thời điểm luồng kết thúc, và vẫn an toàn nếu sau này ai đó vô tình đổi thứ tự khai báo thành viên.

Sử dụng:

```cpp
{
    ConnectionManager conn;

    conn.connect();
    conn.onResponse(false);
    conn.onResponse(true);
    std::printf("[main] Posted 3 events\n");
}   // conn bị hủy: xử lý hết sự kiện còn lại rồi dừng luồng
```

Một kết quả có thể có:

```
[main] Posted 3 events
Disconnected -> Connecting
  Send handshake
Retry 1
Connecting -> Connected
  Start reading data
```

Dòng của `main` xuất hiện trước, vì ba lời gọi chỉ đặt sự kiện vào hàng đợi rồi trả về ngay. Các sự kiện được xử lý sau đó, trên luồng riêng của `ConnectionManager`, đúng thứ tự đã gửi. Kết quả xử lý giống hệt phiên bản ở Bài C14, nhưng giờ `connect()`, `onResponse()`, `onTimeout()` gọi được từ bất kỳ luồng nào mà không cần một mutex nào trong máy trạng thái.

Các nguồn sự kiện bên ngoài chỉ cần gọi đúng hàm công khai. Ví dụ một luồng timer đơn giản:

```cpp
std::thread timer([&conn] {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    conn.onTimeout();
});
```

### Sự kiện kèm dữ liệu

Sự kiện kiểu `enum` chỉ cho biết điều gì xảy ra, không mang theo dữ liệu. Trong thực tế, nhiều sự kiện cần dữ liệu đi kèm, ví dụ sự kiện "nhận được khung dữ liệu" phải mang theo chính khung đó. C++17 có `std::variant`, một kiểu có thể chứa một trong nhiều kiểu đã liệt kê, rất hợp để biểu diễn sự kiện:

```cpp
#include <variant>

struct Connect {};
struct Disconnect {};
struct DataReceived { Frame frame; };
struct ResponseResult { bool ok; };

using Message = std::variant<Connect, Disconnect, DataReceived, ResponseResult>;
```

Mỗi loại sự kiện là một struct riêng, mang đúng dữ liệu nó cần. Khi xử lý, ta kiểm tra loại sự kiện bằng `std::get_if`, hàm trả về con trỏ tới dữ liệu nếu đúng loại, hoặc `nullptr` nếu không:

```cpp
void ConnectionManager::handle(const Message& msg)
{
    if (std::get_if<Connect>(&msg)) {
        // xử lý kết nối
    } else if (auto* data = std::get_if<DataReceived>(&msg)) {
        processFrame(data->frame);
    } else if (auto* res = std::get_if<ResponseResult>(&msg)) {
        // dùng res->ok
    }
}
```

Hàm công khai tạo sự kiện tương ứng:

```cpp
void onFrame(const Frame& frame) { loop_.post(DataReceived{frame}); }
```

## Cách 2: Hàng đợi chứa lời gọi hàm

Với máy trạng thái, hàng đợi sự kiện là lựa chọn tự nhiên. Nhưng với module không phải máy trạng thái, việc khai báo một loại sự kiện cho mỗi hàm công khai khá cồng kềnh. Một cách khác là cho hàng đợi chứa lời gọi hàm dưới dạng `std::function<void()>`. Hàm công khai đóng gói việc cần làm vào một lambda và đặt vào hàng đợi; luồng xử lý chỉ việc lấy ra và gọi:

```cpp
class Heater {
private:
    using Task = std::function<void()>;

public:
    Heater() : loop_([](const Task& task) { task(); })
    {
        loop_.start();
    }

    ~Heater() { loop_.stop(); }

    void setTarget(float celsius)
    {
        loop_.post([this, celsius] {
            target_ = celsius;          // chạy trên luồng của Heater
            regulate();
        });
    }

    void onTemperature(float celsius)
    {
        loop_.post([this, celsius] {
            current_ = celsius;
            regulate();
        });
    }

private:
    void regulate();                    // bật/tắt bộ gia nhiệt theo target_ và current_

    float target_  = 0.0f;
    float current_ = 0.0f;
    EventLoop<Task> loop_;              // vẫn khai báo cuối cùng
};
```

Bên ngoài gọi `heater.setTarget(60.0f)` như một hàm bình thường, nhưng phần thân thực sự chạy trên luồng của `Heater`. Đây là cách "chuyển lời gọi sang luồng khác" (**marshalling**), và mọi hàm công khai viết theo cách này đều tự động an toàn khi gọi từ nhiều luồng.

:::warning Không capture tham chiếu tới biến cục bộ của bên gọi
Lambda được đặt vào hàng đợi sẽ chạy sau khi hàm công khai đã trả về. Vì vậy, tuyệt đối không capture tham chiếu tới biến cục bộ của bên gọi.
:::

```cpp
void setProfile(const Profile& profile)
{
    loop_.post([this, &profile] { apply(profile); });   // sai: profile có thể đã bị hủy
    loop_.post([this, profile]  { apply(profile); });   // đúng: lambda giữ bản sao
}
```

Capture `this` vẫn an toàn, vì luồng xử lý luôn dừng trước khi đối tượng bị hủy.

So sánh hai cách:

| | Hàng đợi sự kiện | Hàng đợi lời gọi hàm |
|---|---|---|
| Phù hợp với | Máy trạng thái | Module dịch vụ thông thường |
| Thêm chức năng mới | Thêm loại sự kiện và nhánh xử lý | Chỉ cần viết thêm hàm công khai |
| Ghi log, debug | Dễ: in ra tên sự kiện | Khó: lambda không có tên |
| Chi phí | Thấp, kích thước sự kiện cố định | `std::function` có thể cấp phát heap |

## Nhận kết quả từ Active Object

Vì hàm công khai trả về ngay, bên gọi không nhận được kết quả trực tiếp. Có hai cách để lấy kết quả.

**Cách thứ nhất: thông báo ngược lại bằng Observer** (Bài C12). Active Object phát thông báo khi có kết quả, ví dụ `ConnectionManager` thông báo mỗi khi trạng thái thay đổi.

:::note Callback chạy trên luồng của Active Object
Callback của Observer chạy trên luồng của Active Object, không phải luồng của bên nhận. Nếu bên nhận cũng là một Active Object, callback chỉ nên làm một việc là đặt sự kiện vào hàng đợi của chính bên nhận.
:::

```cpp
conn.subscribe([&ui](State s) {
    ui.post(StateChanged{s});    // chuyển sang luồng giao diện, không xử lý tại chỗ
});
```

**Cách thứ hai: dùng `std::future`** khi bên gọi thực sự cần chờ câu trả lời. Active Object nhận một `std::promise`, điền giá trị vào đó khi xử lý xong; bên gọi chờ trên `std::future` tương ứng:

```cpp
#include <future>
#include <memory>

std::future<float> Heater::currentTarget()
{
    auto promise = std::make_shared<std::promise<float>>();
    auto future  = promise->get_future();

    loop_.post([this, promise] {
        promise->set_value(target_);   // đọc target_ trên đúng luồng của Heater
    });
    return future;
}

float t = heater.currentTarget().get();   // chờ tới khi luồng Heater trả lời
```

`std::promise` chỉ di chuyển được mà không sao chép được, trong khi `std::function` yêu cầu lambda sao chép được, nên ta bọc `promise` trong `std::shared_ptr` (Bài C10).

Cách chờ đồng bộ này phải dùng hết sức hạn chế. Nếu chính luồng của `Heater` gọi `currentTarget().get()`, nó sẽ chờ một sự kiện mà chỉ chính nó mới xử lý được, và treo vĩnh viễn. Hai Active Object chờ đồng bộ lẫn nhau cũng gây deadlock tương tự. Nguyên tắc chung là ưu tiên giao tiếp bất đồng bộ cả hai chiều.

## Hệ thống gồm nhiều Active Object

Khi áp dụng cho cả ứng dụng, ta chia hệ thống thành một số Active Object, mỗi cái phụ trách một mảng, và chúng chỉ giao tiếp qua thông điệp:

```
                +--------------------+
 Luồng đọc ---> | ConnectionManager  | ---+--> +--------------+
 UART           | (máy trạng thái)   |    |    |  Giao diện   |
                +--------------------+    |    +--------------+
                          ^               |
                          | lệnh          +--> +--------------+
                +--------------------+         |  Nhật ký     |
 Nút bấm -----> |   Bộ điều khiển    | ------> | (ghi thẻ nhớ)|
                +--------------------+         +--------------+
```

Nguyên tắc cốt lõi của kiến trúc này: **không chia sẻ dữ liệu, chỉ gửi thông điệp**. Mỗi Active Object sở hữu dữ liệu của riêng nó; muốn biết hay thay đổi dữ liệu của module khác, ta gửi thông điệp cho module đó. Nhờ vậy, mutex chỉ còn tồn tại bên trong `BlockingQueue`, một chỗ duy nhất đã được viết cẩn thận và kiểm tra kỹ.

Nếu từng làm việc với RTOS, ta sẽ nhận ra đây chính là mô hình task kết hợp message queue: mỗi task là một vòng lặp `xQueueReceive()` rồi xử lý thông điệp. Active Object chỉ là cách tổ chức mô hình đó thành class trong C++.

## Khi nào không nên dùng

- **Chương trình nhỏ, ít luồng.** Với một ứng dụng chỉ có một hai luồng đơn giản, một mutex đặt đúng chỗ dễ hiểu hơn cả một hệ thống thông điệp.
- **Cần kết quả ngay lập tức và thường xuyên.** Nếu bên gọi gần như luôn phải chờ kết quả, Active Object chỉ thêm độ trễ và độ phức tạp.
- **Quá nhiều Active Object.** Mỗi Active Object là một luồng, và mỗi luồng tốn bộ nhớ cho stack riêng. Trên board có RAM hạn chế như BBB, nên gom các module liên quan vào chung một Active Object, thay vì mỗi class một luồng.
- **Luồng xử lý trở nên khó theo dõi.** Khi mọi thứ đều bất đồng bộ, việc lần theo "sự kiện này dẫn tới đâu" khó hơn so với gọi hàm trực tiếp. Ghi log tên sự kiện ở vòng lặp sự kiện giúp giảm bớt khó khăn này.

## Lỗi thường gặp

**Mọi sự kiện khác bị trễ do hàm xử lý sự kiện bị chặn**

```cpp
void handle(Event e)
{
    sendHandshake();
    std::this_thread::sleep_for(std::chrono::seconds(2));   // sai: chờ phản hồi
}
```

Trong 2 giây này, mọi sự kiện khác, kể cả lệnh `Disconnect` của người dùng, đều nằm chờ trong hàng đợi. Đây chính là nguyên tắc "hàm xử lý sự kiện không được chờ" của Bài C14: việc chờ phải được biến thành sự kiện, như một timer gửi `Timeout` sau 2 giây.

**Exception `Resource deadlock avoided` do gọi `stop()` từ bên trong hàm xử lý**

```cpp
void handle(Event e)
{
    if (e == Event::Shutdown) {
        loop_.stop();    // sai: luồng xử lý tự chờ chính nó kết thúc
    }
}
```

`stop()` gọi `join()` trên chính luồng đang chạy, và `std::thread` sẽ ném exception báo deadlock. Việc dừng Active Object phải được thực hiện từ luồng bên ngoài.

**Crash khi hủy đối tượng do vòng lặp sự kiện không phải thành viên cuối cùng**

```cpp
class ConnectionManager {
    EventLoop<Event> loop_;    // sai: bị hủy sau cùng
    State state_;
};
```

Khi đối tượng bị hủy, `state_` bị hủy trước trong khi luồng của `loop_` có thể vẫn đang chạy `handle()` và truy cập `state_`. Nếu không thể đặt `loop_` cuối cùng, phải gọi `loop_.stop()` ngay đầu destructor.

**Capture tham chiếu trong lambda đặt vào hàng đợi**

Đã nêu ở phần hàng đợi lời gọi hàm: lambda chạy sau khi hàm gọi đã kết thúc, nên mọi thứ capture theo tham chiếu có thể đã không còn tồn tại.

## Ứng dụng trong nhúng

Active Object là kiến trúc phổ biến cho phần mềm nhúng hướng sự kiện, từ firmware chạy RTOS tới ứng dụng trên Linux nhúng. Máy trạng thái quản lý kết nối, bộ điều khiển chính của thiết bị, bộ ghi nhật ký, module giao tiếp mạng đều có thể là Active Object, nhận sự kiện từ driver, timer và giao diện mà không cần quan tâm sự kiện đến từ luồng nào. Kết hợp với máy trạng thái (Bài C14) và hàng đợi (Bài C15), đây là bộ khung vững chắc cho phần lớn ứng dụng điều khiển và giám sát.
