## Vì sao cần event loop

Chương trình console thường chạy từ trên xuống dưới rồi kết thúc. Ứng dụng giao diện thì khác: phần lớn thời gian nó **chờ**: Chờ người dùng chạm màn hình, chờ tới lúc đọc cảm biến, chờ dữ liệu từ UART...Khi có chuyện xảy ra, nó xử lý thật nhanh rồi quay lại chờ tiếp.

Vòng lặp "chờ, xử lý, chờ" đó gọi là **event loop**. Trong Qt, event loop chạy khi ta gọi `app.exec()` ở cuối hàm `main()`.

## Event loop hoạt động thế nào

Về mặt ý tưởng, event loop giống đoạn mã sau:

```
while (!quit) {
    chờ cho tới khi có sự kiện;       <- ngủ, không tốn CPU
    lấy sự kiện ra khỏi hàng đợi;
    gửi sự kiện tới đối tượng nhận;   <- gọi hàm xử lý và các slot liên quan
}
```

```
Chạm màn hình ---+
Timer hết giờ ---+      +--------------+      +---------------+      +-------------+
Dữ liệu UART ----+ ---> | Hàng đợi     | ───► | Vòng lặp      | ───► | Đối tượng   |
Cần vẽ lại ------+      | sự kiện      |      | (app.exec())  |      | nhận        |
deleteLater -----+      +--------------┘      +---------------+      +-------------+
```

Hai tính chất quan trọng rút ra từ vòng lặp này:
1. Khi không có sự kiện, chương trình ngủ và gần như không tốn CPU. Với thiết bị nhúng, điều này có nghĩa là ít nóng, ít tốn điện và CPU còn chỗ cho các tiến trình khác.
2. Vòng lặp chỉ xử lý một sự kiện tại một thời điểm. Trong lúc một hàm xử lý hay một slot đang chạy, không có sự kiện nào khác được xử lý: màn hình không được vẽ lại, lần chạm không được nhận, timer không phát timeout. Với người dùng, giao diện trông như bị đứng.

Tính chất thứ hai là nguồn gốc của phần lớn các lỗi giao diện bị treo. Quy tắc rút ra:

> Mọi hàm xử lý sự kiện và mọi slot chạy trong luồng giao diện phải kết thúc nhanh để trả quyền điều khiển về vòng lặp sự kiện. Do đó, không gọi `sleep()`, không chờ phần cứng bằng vòng lặp bận, không đọc file lớn trong slot,...Với việc lâu, có hai hướng: chia nhỏ việc và dùng timer chạy từng phần, hoặc đưa sang luồng khác.

Vẽ lại màn hình cũng là một sự kiện. Khi ta gọi `label->setText("...")`, Qt không vẽ ngay. Nó chỉ ghi nhận nhãn này cần vẽ lại và đặt yêu cầu vào hàng đợi. Việc vẽ thực sự chỉ diễn ra khi vòng lặp sự kiện đến lượt xử lý yêu cầu đó.

Nhờ vậy, nếu trong một slot ta gọi `setText` mười lần, Qt chỉ vẽ một lần với nội dung cuối cùng. Nhưng cũng vì vậy, nếu slot chạy lâu, người dùng sẽ không thấy bất kỳ thay đổi trung gian nào.

## Sự kiện được gửi tới đâu?

Mỗi sự kiện là một đối tượng `QEvent` hoặc lớp con như `QMouseEvent`, `QKeyEvent`. Khi sự kiện tới một widget, hàm `event()` của widget phân loại và gọi hàm xử lý tương ứng:

| Hàm xử lý | Được gọi khi |
| --- | --- |
| `mousePressEvent()` / `mouseReleaseEvent()` | Nhấn / nhả chuột hoặc chạm màn hình |
| `keyPressEvent()` | Nhấn phím |
| `paintEvent()` | Cần vẽ lại widget |
| `resizeEvent()` | Widget đổi kích thước |
| `showEvent()` / `hideEvent()` | Widget được hiện / ẩn |

Để xử lý theo ý mình, ta cần kế thừa widget và override hàm tương ứng. 

Với màn hình cảm ứng, một lần chạm đơn được Qt tự chuyển thành sự kiện chuột cho các widget. Vì vậy nút bấm, thanh trượt...hoạt động với cảm ứng mà ta không phải viết thêm gì.

Nếu một widget không xử lý sự kiện chuột, sự kiện được chuyển tiếp lên widget cha. Đây là lý do nhấn vào một `QLabel` nằm trong cửa sổ thì cửa sổ nhận được sự kiện đó.

## QTimer

`QTimer` là cách chuẩn để làm việc theo thời gian mà không chặn event loop. Nó phát signal `timeout` sau một khoảng thời gian. Đây là công cụ ta dùng nhiều nhất trong HMI: đọc cảm biến theo chu kỳ, cập nhật đồng hồ, tự đóng thông báo sau vài giây...

Timer lặp lại:

```cpp
auto *timer = new QTimer(this);
connect(timer, &QTimer::timeout, this, &MainScreen::readSensor);
timer->start(500);          // phát timeout mỗi 500 ms
```

Timer chạy một lần:

```cpp
QTimer::singleShot(3000, this, [this]() {
    m_messageLabel->hide();    // ẩn thông báo sau 3 giây
});
```

`start()` lúc timer đang chạy sẽ đếm lại từ đầu. Đặc điểm này rất tiện để làm **timer chờ**: mỗi lần người dùng chạm, gọi `start()` lại; timer chỉ hết giờ khi không ai chạm đủ lâu. HMI hay dùng cách này để tắt đèn nền hoặc quay về màn hình chính.

:::note QTimer không chính xác tuyệt đối
Đây là điều rất quan trọng với ứng dụng nhúng:

- `timeout` chỉ được xử lý khi vòng lặp sự kiện đến lượt nó. Nếu vòng lặp đang bận 200 ms, timer bị trễ 200 ms.
- Nếu timer bị lỡ nhiều chu kỳ vì vòng lặp bận, Qt không bù lại các lần bị lỡ; nó chỉ phát một lần khi vòng lặp rảnh.
- Mặc định, timer thuộc loại `Qt::CoarseTimer`, cho phép lệch khoảng 5% so với khoảng thời gian đặt. Có thể đổi sang `Qt::PreciseTimer` bằng `setTimerType()` khi cần chính xác hơn.

Vì vậy, `QTimer` phù hợp cho giao diện và giám sát, không phù hợp cho điều khiển thời gian thực. Những việc cần độ chính xác cỡ micro giây (tạo xung PWM, lấy mẫu tín hiệu tốc độ cao) phải giao cho phần cứng, driver trong kernel hoặc một vi điều khiển riêng.
:::

## Đo thời gian bằng QElapsedTimer

Khi cần biết thực sự bao nhiêu thời gian đã trôi qua, ta dùng `QElapsedTimer`. Nó đọc đồng hồ monotonic của hệ thống, không bị ảnh hưởng khi đổi giờ hệ thống hay khi NTP chỉnh giờ.

```cpp
QElapsedTimer t;
t.start();
// ... làm việc ...
qint64 ms = t.elapsed(); // số mili giây đã trôi qua
```

## Event filter

Event filter cho phép một đối tượng xem trước các sự kiện gửi tới đối tượng khác:

```cpp
target->installEventFilter(filterObject);
```

Mỗi sự kiện gửi tới `target` sẽ đi qua hàm `eventFilter()` của `filterObject` trước. Hàm này trả về:
- `false`: cho sự kiện đi tiếp tới đích như bình thường.
- `true`: "nuốt" sự kiện, đích sẽ không nhận được.

Nếu cài filter lên chính đối tượng ứng dụng (`app.installEventFilter(...)`) thì ta theo dõi được mọi sự kiện của toàn bộ ứng dụng. Trên HMI, cách này thường dùng để phát hiện người dùng không thao tác trong một thời gian rồi chuyển sang màn hình chờ hoặc tắt đèn nền.

## Ví dụ

Màn hình dùng ba timer và `Counter`, một lớp bộ đếm nhỏ kế thừa `QObject` có `step()` tăng/giảm 1 và signal `valueChanged(int)`:

- Timer đếm: bấm "Run" thì `Counter` tự tăng mỗi 500 ms.
- Timer đồng hồ: mỗi giây cập nhật dòng "Tick: … | Thực tế: … s".
- Timer chờ: 10 giây không có thao tác thì dòng trạng thái đổi.

Nút "Block 3 s" cố ý gọi `sleep` để thấy giao diện đứng như thế nào.

```
+------------------------------+
|              12              |  <- giá trị Counter
|     Tick: 15 | Real: 15 s    |  <- đồng hồ
|            Active            |  <- trạng thái chờ
| +-------------++-----------+ |
| |     Run     || Block 3 s | |
| +-------------++-----------+ |
+------------------------------+
```

```
example/
+-- CMakeLists.txt
+-- counter.h           <- lớp Counter kế thừa QObject
+-- counter.cpp
+-- timerpanel.h
+-- timerpanel.cpp
+-- main.cpp
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.16)
project(event-loop-timer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(event-loop-timer
    counter.h
    counter.cpp
    timerpanel.h
    timerpanel.cpp
    main.cpp
)

target_link_libraries(event-loop-timer PRIVATE Qt5::Widgets)
```
::: explain [Giải thích chi tiết]
- `set(CMAKE_AUTOMOC ON)`: tự chạy moc cho các lớp có `Q_OBJECT`.
- `COMPONENTS Widgets` và `Qt5::Widgets`: ứng dụng có giao diện nên cần Qt Widgets; Qt Core và Qt Gui được kéo theo tự động.
- `counter.h`, `counter.cpp`: lớp `Counter` được biên dịch cùng các file của màn hình.
:::

```cpp [timerpanel.h]
#ifndef TIMERPANEL_H
#define TIMERPANEL_H

#include <QElapsedTimer>
#include <QWidget>

class QLabel;
class QPushButton;
class QTimer;
class Counter;

class TimerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TimerPanel(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void onClockTick();
    void toggleRunning();
    void blockEventLoop();

    Counter *m_counter;
    QTimer *m_stepTimer;   // đếm tự động
    QTimer *m_clockTimer;  // cập nhật đồng hồ mỗi giây
    QTimer *m_idleTimer;   // phát hiện không có thao tác
    QElapsedTimer m_uptime;
    int m_tickCount = 0;

    QLabel *m_valueLabel;
    QLabel *m_clockLabel;
    QLabel *m_statusLabel;
    QPushButton *m_runButton;
};

#endif // TIMERPANEL_H
```
::: explain [Giải thích chi tiết]
- `eventFilter()`: hàm virtual của `QObject`, được gọi cho mọi sự kiện đi qua đối tượng mà ta đã gắn filter.
- Ba con trỏ `QTimer *`: mỗi timer một việc. Tách riêng giúp dừng, đổi interval từng timer mà không ảnh hưởng timer khác.
- `QElapsedTimer m_uptime`: đối tượng nhỏ, không phải `QObject`, nên để làm thành viên trực tiếp thay vì dùng `new`.
:::

```cpp [timerpanel.cpp]
#include "timerpanel.h"
#include "counter.h"

#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <chrono>
#include <thread>

TimerPanel::TimerPanel(QWidget *parent)
    : QWidget(parent)
    , m_counter(new Counter(this))
    , m_stepTimer(new QTimer(this))
    , m_clockTimer(new QTimer(this))
    , m_idleTimer(new QTimer(this))
    , m_valueLabel(new QLabel("0"))
    , m_clockLabel(new QLabel)
    , m_statusLabel(new QLabel("Active"))
    , m_runButton(new QPushButton("Run"))
{
    setFixedSize(320, 240);

    QFont bigFont = m_valueLabel->font();
    bigFont.setPointSize(40);
    m_valueLabel->setFont(bigFont);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_clockLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    auto *blockButton = new QPushButton("Block 3 s");
    m_runButton->setMinimumHeight(48);
    blockButton->setMinimumHeight(48);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_runButton);
    buttons->addWidget(blockButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_valueLabel);
    layout->addWidget(m_clockLabel);
    layout->addWidget(m_statusLabel);
    layout->addLayout(buttons);

    // Timer đếm: mỗi 500 ms gọi step(), chỉ chạy khi bấm "Run"
    m_stepTimer->setInterval(500);
    connect(m_stepTimer, &QTimer::timeout, m_counter, &Counter::step);
    connect(m_counter, &Counter::valueChanged,
            m_valueLabel, qOverload<int>(&QLabel::setNum));

    // Timer đồng hồ: chạy suốt, mỗi giây một lần
    connect(m_clockTimer, &QTimer::timeout, this, &TimerPanel::onClockTick);
    m_clockTimer->start(1000);
    m_uptime.start();
    onClockTick();

    // Timer chờ: chạy một lần, 10 s sau lần chạm cuối
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(10000);
    connect(m_idleTimer, &QTimer::timeout, this, [this]() {
        m_statusLabel->setText("Idle for 10 s");
    });
    m_idleTimer->start();
    qApp->installEventFilter(this);

    connect(m_runButton, &QPushButton::clicked, this, &TimerPanel::toggleRunning);
    connect(blockButton, &QPushButton::clicked, this, &TimerPanel::blockEventLoop);
}

bool TimerPanel::eventFilter(QObject *watched, QEvent *event)
{
    // Chạm màn hình hoặc bấm chuột trên máy tính thì đếm lại từ đầu
    if (event->type() == QEvent::MouseButtonPress) {
        m_idleTimer->start();
        m_statusLabel->setText("Active");
    }
    return QWidget::eventFilter(watched, event); // không chặn sự kiện
}

void TimerPanel::onClockTick()
{
    const qint64 seconds = m_uptime.elapsed() / 1000;
    m_clockLabel->setText(QString("Tick: %1 | Real: %2 s")
                              .arg(m_tickCount++)
                              .arg(seconds));
}

void TimerPanel::toggleRunning()
{
    if (m_stepTimer->isActive()) {
        m_stepTimer->stop();
        m_runButton->setText("Run");
    } else {
        m_stepTimer->start();
        m_runButton->setText("Stop");
    }
}

void TimerPanel::blockEventLoop()
{
    // Cố ý làm sai: chặn event loop 3 giây, giao diện sẽ đứng
    std::this_thread::sleep_for(std::chrono::seconds(3));
}
```
::: explain [Giải thích chi tiết]
- `new QTimer(this)`: timer có parent là màn hình, nên bị xóa cùng màn hình, không cần `delete`.
- `m_stepTimer->setInterval(500)` chỉ đặt chu kỳ, chưa chạy. Timer chỉ chạy khi `toggleRunning()` gọi `start()`.
- `m_clockTimer->start(1000)`: chạy ngay, lặp mãi. Gọi `onClockTick()` một lần để dòng đồng hồ có nội dung ngay khi mở màn hình, không phải đợi 1 giây.
- `m_idleTimer`: chạy một lần. `eventFilter()` gọi `start()` lại mỗi khi có `MouseButtonPress`, nên timer chỉ hết giờ khi 10 giây liền không ai chạm.
- `qApp->installEventFilter(this)`: gắn filter lên cả ứng dụng. Khi `TimerPanel` bị xóa, Qt tự gỡ filter.
- `eventFilter()` trả về kết quả của lớp cha, tức `false`: sự kiện vẫn đi tiếp tới widget đích. Trả về `true` sẽ chặn sự kiện, nút bấm không nhận được chạm.
- `QString("...%1...%2").arg(...).arg(...)`: ghép số vào chuỗi: `%1` được thay bằng giá trị của `arg()` đầu tiên, `%2` bằng giá trị thứ hai.
- `blockEventLoop()`: dùng `std::this_thread::sleep_for` để chặn event loop. Đây là ví dụ về điều **không được làm**.

Trên board, chạm màn hình cảm ứng cũng sinh ra `MouseButtonPress`, vì với Widgets, Qt mặc định chuyển sự kiện chạm chưa được xử lý thành sự kiện chuột. Vì vậy filter hoạt động giống nhau trên máy tính và trên board.
:::

```cpp [main.cpp]
#include <QApplication>

#include "timerpanel.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    TimerPanel panel;
    panel.show();

    return app.exec();
}
```
::: explain [Giải thích chi tiết]
- `TimerPanel panel`: màn hình chứa cả ba timer. Timer chỉ chạy khi event loop chạy, tức là sau khi gọi `app.exec()`.
- `QApplication app(argc, argv)`: tạo trước mọi widget.
- `app.exec()`: chạy event loop, chỉ trả về khi ứng dụng thoát.
:::
::::

### Build và chạy

```bash
cd example
cmake -S . -B build
cmake --build build
./build/event-loop-timer
```

Thử các thao tác sau:

1. Bấm "Run": số tăng mỗi nửa giây. Bấm lại để dừng.
2. Để yên 10 giây: trạng thái đổi thành "Idle for 10 s". Bấm vào bất kỳ đâu: trạng thái trở lại "Active".
3. Bấm "Run" rồi bấm "Block 3 s": trong 3 giây số không tăng, đồng hồ không chạy, nút không phản hồi. Sau đó "Tick" chậm hơn "Real" vài đơn vị, vì các timeout bị lỡ không được phát bù.

:::tip Chia nhỏ việc dài bằng timer 0 ms
`QTimer::singleShot(0, this, ...)` xếp một việc vào cuối hàng đợi của event loop. Khi phải xử lý một danh sách dài (ví dụ ghi 10000 mẫu ra file), ta xử lý từng phần vài trăm mẫu rồi dùng timer 0 ms gọi lần tiếp theo. Giữa các phần, event loop vẫn kịp vẽ lại và nhận chạm.
:::
