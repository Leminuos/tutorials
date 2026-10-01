Người làm nhúng bằng C hầu như ai cũng từng viết state machine: một biến `state` kiểu `enum`, một khối `switch-case` lớn trong vòng lặp chính, mỗi `case` xử lý một trạng thái. Đây là một trong những kỹ thuật quan trọng nhất của lập trình nhúng, vì thiết bị gần như luôn hoạt động theo trạng thái: đang khởi động, đang chờ, đang chạy, đang lỗi. Bài này đi từ cách viết `switch-case` quen thuộc, qua bảng chuyển trạng thái, tới State pattern dùng class, và chỉ ra khi nào nên chọn cách nào.

## Vấn đề thực tế

Xét module quản lý kết nối tới một bộ cảm biến qua UART. Người dùng bấm nút để kết nối, module gửi bản tin bắt tay và chờ phản hồi. Nếu thất bại thì thử lại, quá 3 lần thì báo lỗi. Đang kết nối mà mất dữ liệu quá lâu thì tự kết nối lại.

Cách viết thường gặp khi chưa nghĩ theo trạng thái là dùng các cờ `bool`:

```cpp
bool isConnecting = false;
bool isConnected  = false;
bool hasError     = false;
int  retries      = 0;

void onConnectButton()
{
    if (!isConnected && !isConnecting && !hasError) {
        isConnecting = true;
        sendHandshake();
    }
}

void onResponse(bool ok)
{
    if (isConnecting) {
        if (ok) {
            isConnecting = false;
            isConnected  = true;
        } else if (++retries >= 3) {
            isConnecting = false;
            hasError     = true;
        }
    }
}
```

Code này có nhiều vấn đề:

- **Tổ hợp trạng thái không hợp lệ.** Ba cờ `bool` tạo ra 8 tổ hợp, nhưng chỉ có 4 tổ hợp có nghĩa. Chỉ cần quên tắt một cờ là thiết bị rơi vào trạng thái vô lý như `isConnected && hasError`.
- **Logic của một trạng thái nằm rải rác.** Muốn biết thiết bị làm gì khi đang kết nối, ta phải đọc mọi hàm xử lý sự kiện.
- **Lỗi khó thấy.** Đoạn code trên có một lỗi: `retries` không bao giờ được đặt lại về 0, nên sau lần lỗi đầu tiên, các lần kết nối sau sẽ báo lỗi sớm hơn. Lỗi này rất khó phát hiện khi đọc code, vì không có chỗ nào thể hiện rõ "bắt đầu một lần kết nối mới".
- **Khó trả lời câu hỏi cơ bản**: "nếu sự kiện X xảy ra khi đang ở trạng thái Y thì sao?"

## Ý tưởng của State Machine

State machine (máy trạng thái) mô tả hệ thống bằng bốn thành phần:

- **Trạng thái** (state): hệ thống luôn ở đúng một trạng thái tại mỗi thời điểm.
- **Sự kiện** (event): điều xảy ra từ bên ngoài, như người dùng bấm nút, nhận được phản hồi, hết thời gian chờ.
- **Chuyển trạng thái** (transition): quy tắc "ở trạng thái A, nếu có sự kiện E thì sang trạng thái B".
- **Hành động** (action): việc cần làm khi chuyển trạng thái, như gửi bản tin, bật đèn báo lỗi.

Với module kết nối, ta có 4 trạng thái và 6 sự kiện. Sơ đồ chuyển trạng thái chính:

```
                    Connect                  Success
  +--------------+ --------> +------------+ --------> +-----------+
  | Disconnected |           | Connecting |           | Connected |
  +--------------+ <-------- +------------+ <-------- +-----------+
         ^          Disconnect      |          Timeout
         |                          | Failure (hết lượt thử)
         | Reset                    v
         |                    +------------+
         +--------------------|   Error    |
                              +------------+
```

Sơ đồ được giản lược cho dễ nhìn. Bảng dưới đây liệt kê đầy đủ mọi chuyển trạng thái:

| Trạng thái hiện tại | Sự kiện | Điều kiện | Trạng thái mới |
|---|---|---|---|
| Disconnected | Connect | | Connecting |
| Connecting | Success | | Connected |
| Connecting | Failure | còn lượt thử | Connecting (thử lại) |
| Connecting | Failure | hết lượt thử | Error |
| Connecting | Disconnect | | Disconnected |
| Connected | Timeout | | Connecting |
| Connected | Disconnect | | Disconnected |
| Error | Reset | | Disconnected |

Mọi tổ hợp không có trong bảng (ví dụ nhận `Success` khi đang `Disconnected`) đều bị bỏ qua.

Thiết kế sơ đồ và bảng này trước khi viết code là thói quen rất đáng có. Bảng trả lời được mọi câu hỏi "nếu... thì sao", và code chỉ còn là việc dịch bảng sang C++.

Ta khai báo trạng thái và sự kiện bằng `enum class`:

```cpp
enum class State { Disconnected, Connecting, Connected, Error };
enum class Event { Connect, Success, Failure, Timeout, Disconnect, Reset };

const int MAX_RETRIES = 3;
```

Chỉ một biến `State` thay cho ba cờ `bool`, nên các tổ hợp vô lý không thể tồn tại.

## Cách 1: switch-case

Cách cài đặt trực tiếp nhất là một hàm `handle()` nhận sự kiện, dùng `switch` theo trạng thái hiện tại:

```cpp
class Connection {
public:
    void  handle(Event e);
    State state() const { return state_; }

private:
    void transitionTo(State next);

    State state_   = State::Disconnected;
    int   retries_ = 0;
};
```

```cpp
void Connection::handle(Event e)
{
    switch (state_) {
    case State::Disconnected:
        if (e == Event::Connect)    { transitionTo(State::Connecting); return; }
        break;

    case State::Connecting:
        if (e == Event::Success)    { transitionTo(State::Connected); return; }
        if (e == Event::Disconnect) { transitionTo(State::Disconnected); return; }
        if (e == Event::Failure) {
            if (++retries_ < MAX_RETRIES) {
                std::printf("Retry %d\n", retries_);
            } else {
                transitionTo(State::Error);
            }
            return;
        }
        break;

    case State::Connected:
        if (e == Event::Timeout)    { transitionTo(State::Connecting); return; }
        if (e == Event::Disconnect) { transitionTo(State::Disconnected); return; }
        break;

    case State::Error:
        if (e == Event::Reset)      { transitionTo(State::Disconnected); return; }
        break;
    }

    std::printf("Event ignored\n");
}
```

Mọi thay đổi trạng thái đều đi qua một hàm duy nhất là `transitionTo()`. Tạm thời nó chỉ in ra và cập nhật trạng thái:

```cpp
const char* toString(State s)
{
    switch (s) {
    case State::Disconnected: return "Disconnected";
    case State::Connecting:   return "Connecting";
    case State::Connected:    return "Connected";
    case State::Error:        return "Error";
    }
    return "?";
}

void Connection::transitionTo(State next)
{
    std::printf("%s -> %s\n", toString(state_), toString(next));
    state_ = next;
}
```

:::tip Không dùng default khi switch trên trạng thái
Khi `switch` trên `enum class`, hãy liệt kê đủ mọi giá trị và không dùng `default`. Khi đó, nếu sau này ta thêm trạng thái mới mà quên xử lý, `g++ -Wall` sẽ cảnh báo ngay những `switch` còn thiếu. Dùng `default` sẽ làm mất cảnh báo hữu ích này.
:::

Với mỗi trạng thái, ta phải quyết định rõ xử lý thế nào với sự kiện không mong đợi: bỏ qua, ghi log, hay coi là lỗi. Ở đây ta in ra để dễ theo dõi. Trong sản phẩm thật, ghi log các sự kiện bị bỏ qua giúp phát hiện lỗi logic rất nhanh.

Code đã chạy được, nhưng còn thiếu hai việc: gửi bản tin bắt tay và đặt lại bộ đếm `retries_` mỗi khi bắt đầu kết nối. Mục tiếp theo giải quyết việc này.

### Hành động khi vào và ra trạng thái

Có hai đường đi vào trạng thái `Connecting`: từ `Disconnected` khi người dùng bấm kết nối, và từ `Connected` khi mất dữ liệu. Cả hai đều cần đặt lại bộ đếm và gửi bản tin bắt tay. Nếu viết hành động này ở từng chỗ chuyển trạng thái, ta sẽ lặp code và dễ quên ở một chỗ, đúng như lỗi `retries` trong cách dùng cờ `bool`.

Giải pháp là gắn hành động vào trạng thái thay vì vào chuyển trạng thái:

- **Hành động khi vào** (entry action): chạy mỗi khi vào trạng thái, dù đến từ đâu.
- **Hành động khi ra** (exit action): chạy mỗi khi rời trạng thái, dù đi đâu.

Vì mọi chuyển trạng thái đều đi qua `transitionTo()`, ta chỉ cần thêm hai lời gọi vào đó:

```cpp
void Connection::transitionTo(State next)
{
    onExit(state_);
    std::printf("%s -> %s\n", toString(state_), toString(next));
    state_ = next;
    onEnter(state_);
}

void Connection::onEnter(State s)
{
    switch (s) {
    case State::Connecting:
        retries_ = 0;
        std::printf("  Send handshake\n");
        break;
    case State::Connected:
        std::printf("  Start reading data\n");
        break;
    case State::Error:
        std::printf("  Error LED on\n");
        break;
    case State::Disconnected:
        break;
    }
}

void Connection::onExit(State s)
{
    if (s == State::Error) {
        std::printf("  Error LED off\n");
    }
}
```

Việc bật đèn lỗi khi vào `Error` và tắt khi ra khỏi `Error` là ví dụ điển hình cho giá trị của entry/exit: dù sau này có thêm bao nhiêu đường thoát khỏi trạng thái lỗi, đèn luôn được tắt đúng lúc.

Chạy thử một chuỗi sự kiện:

```cpp
Connection conn;
conn.handle(Event::Connect);
conn.handle(Event::Failure);
conn.handle(Event::Success);
conn.handle(Event::Timeout);    // mất dữ liệu, kết nối lại
conn.handle(Event::Failure);
conn.handle(Event::Failure);
conn.handle(Event::Failure);    // hết lượt thử
conn.handle(Event::Connect);    // đang lỗi, không được kết nối
conn.handle(Event::Reset);
```

Kết quả:

```
Disconnected -> Connecting
  Send handshake
Retry 1
Connecting -> Connected
  Start reading data
Connected -> Connecting
  Send handshake
Retry 1
Retry 2
Connecting -> Error
  Error LED on
Event ignored
  Error LED off
Error -> Disconnected
```

Chú ý dòng `Retry 1` xuất hiện lại sau khi kết nối lại: bộ đếm đã được đặt về 0 trong `onEnter`, nên lỗi `retries` của phiên bản dùng cờ `bool` không còn xảy ra.

Cách `switch-case` có ưu điểm là đơn giản, dễ hiểu, không cần kiến thức gì đặc biệt. Nhược điểm là khi số trạng thái và sự kiện tăng lên, hàm `handle()` phình to rất nhanh, và khó nhìn ra toàn bộ cấu trúc của máy trạng thái.

## Cách 2: Bảng chuyển trạng thái

Nhìn lại hàm `handle()`, ta thấy nó thực chất chỉ là bảng chuyển trạng thái ở phần đầu bài được viết lại dưới dạng `if`. Vậy sao không viết thẳng bảng đó vào code?

Mỗi dòng của bảng là một struct gồm trạng thái nguồn, sự kiện, điều kiện (**guard**), hành động và trạng thái đích:

```cpp
struct Transition {
    State from;
    Event event;
    bool  (*guard)(const Connection&);   // điều kiện; nullptr = luôn đúng
    void  (*action)(Connection&);        // hành động; nullptr = không có
    State to;
};
```

Bảng được khai báo ngay trong hàm `handle()`. Các lambda không capture gì có thể chuyển thành con trỏ hàm, và vì được viết bên trong hàm thành viên nên chúng truy cập được biến `private` như `retries_`:

```cpp
void Connection::handle(Event e)
{
    static const Transition table[] = {
        {State::Disconnected, Event::Connect,    nullptr, nullptr, State::Connecting},
        {State::Connecting,   Event::Success,    nullptr, nullptr, State::Connected},
        {State::Connecting,   Event::Failure,
            [](const Connection& c) { return c.retries_ + 1 < MAX_RETRIES; },
            [](Connection& c) { c.retries_++; std::printf("Retry %d\n", c.retries_); },
            State::Connecting},
        {State::Connecting,   Event::Failure,    nullptr, nullptr, State::Error},
        {State::Connecting,   Event::Disconnect, nullptr, nullptr, State::Disconnected},
        {State::Connected,    Event::Timeout,    nullptr, nullptr, State::Connecting},
        {State::Connected,    Event::Disconnect, nullptr, nullptr, State::Disconnected},
        {State::Error,        Event::Reset,      nullptr, nullptr, State::Disconnected},
    };

    for (const auto& t : table) {
        if (t.from == state_ && t.event == e &&
            (t.guard == nullptr || t.guard(*this))) {
            if (t.action != nullptr) {
                t.action(*this);
            }
            if (t.to != state_) {
                transitionTo(t.to);     // vẫn dùng onEnter/onExit như trước
            }
            return;
        }
    }

    std::printf("Event ignored\n");
}
```

Hàm này cho kết quả giống hệt phiên bản `switch-case`. Có hai quy tắc trong cách duyệt bảng cần nắm:

- **Các dòng được xét từ trên xuống, dòng đầu tiên khớp sẽ được dùng.** Vì vậy dòng `Failure` có điều kiện "còn lượt thử" phải đặt trước dòng `Failure` không điều kiện. Đảo thứ tự hai dòng này, thiết bị sẽ báo lỗi ngay lần thất bại đầu tiên.
- **Khi trạng thái đích trùng trạng thái hiện tại, ta chỉ chạy hành động mà không gọi `transitionTo()`.** Đây gọi là chuyển trạng thái nội bộ. Nếu gọi `transitionTo()`, hàm `onEnter` sẽ đặt lại `retries_` về 0 và máy trạng thái thử lại mãi mãi.

Ưu điểm lớn nhất của cách này: toàn bộ máy trạng thái nằm gọn trong một bảng, đối chiếu từng dòng với bảng thiết kế được. Thêm một chuyển trạng thái chỉ là thêm một dòng. Trên vi điều khiển, bảng `static const` còn được đặt trong bộ nhớ flash, không tốn RAM.

Nhược điểm là hành động phức tạp viết trong bảng rất khó đọc. Cách này hợp với máy trạng thái có nhiều chuyển trạng thái nhưng hành động đơn giản, như bộ phân tích giao thức.

## Cách 3: State pattern

Khi mỗi trạng thái có nhiều logic riêng, như cách xử lý dữ liệu khác nhau, bộ đếm riêng, cấu hình riêng, thì gom hết vào một class `Connection` sẽ khiến class này quá lớn. **State pattern** tách mỗi trạng thái thành một class riêng, dùng đa hình (Bài C6) để mỗi class tự xử lý sự kiện của mình.

Trước hết là lớp cơ sở cho mọi trạng thái:

```cpp
class Connection;   // khai báo trước

class ConnectionState {
public:
    virtual ~ConnectionState() = default;
    virtual const char* name() const = 0;
    virtual void onEnter(Connection&) {}
    virtual void onExit(Connection&) {}
    virtual void handle(Connection&, Event) {}   // mặc định: bỏ qua sự kiện
};
```

Các hàm có cài đặt mặc định rỗng, nên mỗi trạng thái chỉ cần viết lại những gì nó quan tâm. Mỗi trạng thái là một class con:

```cpp
class DisconnectedState : public ConnectionState {
public:
    const char* name() const override { return "Disconnected"; }
    void handle(Connection& conn, Event e) override;
};

class ConnectingState : public ConnectionState {
public:
    const char* name() const override { return "Connecting"; }
    void onEnter(Connection& conn) override;
    void handle(Connection& conn, Event e) override;

private:
    int retries_ = 0;     // dữ liệu chỉ thuộc về trạng thái này
};

// ConnectedState và ErrorState viết tương tự
```

Chú ý `retries_` giờ là thành viên của `ConnectingState`, không còn nằm trong `Connection`. Dữ liệu chỉ dùng trong một trạng thái được đặt đúng chỗ của nó, là một lợi ích rõ ràng của cách tách class.

Lớp `Connection` giữ các đối tượng trạng thái và con trỏ tới trạng thái hiện tại. Sự kiện được chuyển thẳng cho trạng thái hiện tại xử lý:

```cpp
class Connection {
public:
    Connection() : current_(&disconnected_) {}

    void handle(Event e) { current_->handle(*this, e); }

    void transitionTo(ConnectionState& next)
    {
        current_->onExit(*this);
        std::printf("%s -> %s\n", current_->name(), next.name());
        current_ = &next;
        current_->onEnter(*this);
    }

    ConnectionState& disconnected() { return disconnected_; }
    ConnectionState& connecting()   { return connecting_; }
    ConnectionState& connected()    { return connected_; }
    ConnectionState& error()        { return error_; }

private:
    DisconnectedState disconnected_;
    ConnectingState   connecting_;
    ConnectedState    connected_;
    ErrorState        error_;
    ConnectionState*  current_;
};
```

Các đối tượng trạng thái là thành viên của `Connection`, nên được tạo một lần duy nhất và không cần cấp phát động khi chuyển trạng thái.

Phần xử lý của từng trạng thái được viết sau khi `Connection` đã được khai báo đầy đủ:

```cpp
void DisconnectedState::handle(Connection& conn, Event e)
{
    if (e == Event::Connect) {
        conn.transitionTo(conn.connecting());
    }
}

void ConnectingState::onEnter(Connection&)
{
    retries_ = 0;
    std::printf("  Send handshake\n");
}

void ConnectingState::handle(Connection& conn, Event e)
{
    switch (e) {
    case Event::Success:
        conn.transitionTo(conn.connected());
        break;
    case Event::Disconnect:
        conn.transitionTo(conn.disconnected());
        break;
    case Event::Failure:
        if (++retries_ < MAX_RETRIES) {
            std::printf("Retry %d\n", retries_);
        } else {
            conn.transitionTo(conn.error());
        }
        break;
    default:
        break;     // các sự kiện khác: bỏ qua
    }
}
```

Muốn biết thiết bị làm gì khi đang kết nối, ta chỉ cần mở `ConnectingState`. Thêm trạng thái mới là thêm một class, không phải sửa các trạng thái cũ.

:::warning transitionTo() là lệnh cuối cùng
Sau khi gọi `conn.transitionTo()` bên trong `handle()`, trạng thái hiện tại đã đổi. Không nên viết thêm code thao tác trên dữ liệu của trạng thái cũ sau lời gọi này, vì logic lúc đó thuộc về trạng thái đã rời đi.
:::

Ở đây, ta dùng `default` trong `switch (e)` vì mục đích là bỏ qua mọi sự kiện mà trạng thái không quan tâm, khác với `switch` trên trạng thái ở cách 1.

## So sánh ba cách cài đặt

| | switch-case | Bảng chuyển trạng thái | State pattern |
|---|---|---|---|
| Phù hợp với | Máy nhỏ, dưới khoảng 5 trạng thái | Nhiều chuyển trạng thái, hành động đơn giản | Mỗi trạng thái có nhiều logic riêng |
| Nhìn được toàn bộ máy | Khó khi máy lớn | Rất dễ: một bảng | Phải mở nhiều class |
| Dữ liệu riêng từng trạng thái | Dồn chung một class | Dồn chung một class | Nằm trong class của trạng thái |
| Thêm trạng thái mới | Sửa hàm `handle` | Thêm dòng vào bảng | Thêm class mới |
| Chi phí | Thấp nhất | Thấp, bảng có thể nằm trong flash | Một lần gọi hàm ảo mỗi sự kiện |

Không có cách nào tốt nhất cho mọi trường hợp. Trong thực tế, một dự án thường dùng cả ba: `switch-case` cho máy trạng thái nhỏ trong driver, bảng cho bộ phân tích giao thức, State pattern cho logic điều khiển chính của thiết bị.

## Nguyên tắc khi làm việc với state machine

**Hàm xử lý sự kiện không được chờ.** `handle()` phải xử lý xong và trả về ngay, không được `sleep` hay vòng lặp chờ phản hồi. Máy trạng thái không tự chờ thời gian; thay vào đó, một timer bên ngoài sẽ gửi sự kiện `Timeout` khi hết giờ, và driver UART gửi sự kiện `Success` hoặc `Failure` khi có phản hồi. Nhờ vậy máy trạng thái không bao giờ bị kẹt, và luôn phản hồi được các sự kiện khác như người dùng bấm hủy.

**Mọi thay đổi trạng thái đi qua một hàm duy nhất.** Không bao giờ gán trực tiếp `state_ = ...` ở chỗ khác ngoài `transitionTo()`. Đây là điều kiện để entry/exit action luôn chạy đúng, và là chỗ duy nhất cần đặt log để theo dõi mọi chuyển trạng thái khi debug.

**Không gọi `handle()` từ bên trong hành động.** Nếu `onEnter` của trạng thái A lại gọi `handle()` để chuyển sang B ngay lập tức, ta có các chuyển trạng thái lồng nhau, rất khó theo dõi và dễ sai. Khi cần, hãy đưa sự kiện vào hàng đợi và xử lý sau khi lần chuyển hiện tại kết thúc (Bài C16).

**Sự kiện từ nhiều luồng phải đi qua hàng đợi.** Nếu timer chạy ở một luồng, driver UART ở luồng khác, cả hai cùng gọi `handle()` thì máy trạng thái bị truy cập đồng thời. Cách an toàn là các luồng chỉ đẩy sự kiện vào hàng đợi, còn máy trạng thái lấy ra xử lý tuần tự trên một luồng duy nhất. Đây chính là mô hình Active Object ở Bài C16.

## Khi nào không nên dùng

- **Hệ thống chỉ có hai trạng thái đơn giản**, như đèn bật/tắt. Một biến `bool` là đủ.
- **Máy trạng thái phân cấp phức tạp.** Khi có trạng thái lồng trong trạng thái (ví dụ `Connected` chứa các trạng thái con `Idle`, `Reading`, `Writing`, và mọi trạng thái con đều cần xử lý `Disconnect` giống nhau), ba cách trong bài bắt đầu lặp code nhiều. Lúc đó nên tìm hiểu máy trạng thái phân cấp (hierarchical state machine) hoặc dùng thư viện có sẵn như Boost.SML, thay vì tự viết.

## Ứng dụng trong nhúng

State machine có mặt ở hầu hết mọi tầng của hệ thống nhúng. Quản lý kết nối như trong bài là một ví dụ. Chế độ hoạt động của thiết bị (khởi động, tự kiểm tra, hoạt động, bảo trì, lỗi) là một máy trạng thái lớn điều khiển toàn bộ ứng dụng. Bộ phân tích khung dữ liệu nhận từng byte từ UART cũng là một máy trạng thái với các trạng thái chờ byte đầu khung, đọc độ dài, đọc dữ liệu, kiểm tra CRC; đây là nơi bảng chuyển trạng thái phát huy tác dụng. Giao diện HMI với nhiều màn hình cũng vậy: mỗi màn hình là một trạng thái, nút bấm là sự kiện.
