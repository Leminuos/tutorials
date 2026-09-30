## Vấn đề

Ở Bài 5, ta đã thấy vòng lặp sự kiện chỉ xử lý một việc tại một thời điểm và đã khắc phục bằng cách chia công việc thành các bước nhỏ. Nhưng có những việc không thể chia nhỏ, vì chính một lời gọi hàm đã mất nhiều thời gian:
- Đọc cảm biến nhiệt độ DS18B20 qua 1-Wire trên BBB: chỉ một lệnh đọc file `/sys/bus/w1/devices/28-.../w1_slave` đã chặn khoảng 750 ms để cảm biến chuyển đổi nhiệt độ.
- Gửi lệnh qua UART rồi chờ thiết bị trả lời theo kiểu đồng bộ.
- Ghi một file log lớn xuống thẻ nhớ rồi gọi fsync để chắc chắn dữ liệu đã được lưu.
- Truy vấn cơ sở dữ liệu SQLite, tính toán thống kê trên tập dữ liệu lớn.

Nếu gọi những hàm này trong luồng giao diện, giao diện sẽ đứng suốt thời gian chờ. Giải pháp là chạy chúng trong một thread riêng.

BeagleBone Black chỉ có một nhân CPU. Hệ điều hành chia thời gian cho các luồng thay phiên nhau chạy, nên thêm luồng không làm tổng khối lượng tính toán nhanh hơn. Một phép tính mất 2 giây vẫn mất khoảng 2 giây dù chạy ở luồng nào.

Lợi ích thật sự của luồng trên BBB là giữ cho giao diện luôn phản hồi:
- Khi luồng phụ đang chờ phần cứng (đọc cảm biến, chờ UART), nó ngủ và không tốn CPU. Luồng giao diện được chạy thoải mái.
- Khi luồng phụ đang tính toán, hệ điều hành vẫn xen kẽ cho luồng giao diện chạy, nên chạm vào màn hình vẫn có phản hồi, chỉ hơi chậm hơn bình thường.

Vì vậy trên BBB, luồng chủ yếu dùng cho các việc chờ phần cứng. Đó cũng là trường hợp phổ biến nhất trong ứng dụng HMI.

Luồng chạy `main()` và `app.exec()` gọi là luồng giao diện (GUI thread hay main thread). Qt quy định:

> Mọi widget chỉ được tạo, đọc, sửa trong luồng giao diện.

Luồng phụ không bao giờ được gọi `label->setText()` hay bất kỳ hàm nào của widget. Vi phạm quy tắc này không gây lỗi ngay mà gây crash ngẫu nhiên, có khi sau vài giờ, có khi sau vài ngày và rất khó tìm nguyên nhân.

Cách đúng: luồng phụ phát signal mang theo kết quả và luồng giao diện nhận signal đó để cập nhật widget.

## Thread affinity

Mỗi `QObject` thuộc về một luồng, gọi là thread affinity. Mặc định đối tượng thuộc luồng đã tạo ra nó. Ta kiểm tra bằng hàm `thread()` và đổi luồng sở hữu bằng `moveToThread()`.

Luồng mà đối tượng thuộc về quyết định:

- Slot của đối tượng chạy ở luồng nào khi nhận signal từ luồng khác.
- Timer của đối tượng kêu ở luồng nào.

Quy tắc đi kèm:

- Đối tượng có parent không chuyển luồng được; con phải cùng luồng với cha. Vì vậy worker được tạo **không có parent**.
- Timer phải được tạo và khởi động trong luồng mà nó thuộc về; khởi động từ luồng khác chỉ nhận được cảnh báo và timer không chạy. Worker tạo timer trong `start()`, lúc đó hàm đang chạy ở luồng đọc.

## Signal/slot giữa các luồng

Ở Bài 3 ta đã hẹn sẽ nói kỹ phần này. Tham số cuối cùng của `connect` là kiểu kết nối:

| Kiểu | Cách hoạt động |
| --- | --- |
| `Qt::DirectConnection` | Slot được gọi ngay lập tức, trong luồng đang phát signal, giống gọi hàm thường |
| `Qt::QueuedConnection` | Tham số được sao chép, lời gọi được đặt vào hàng đợi sự kiện của luồng sở hữu người nhận. Slot chạy sau, trong luồng đó |
| `Qt::AutoConnection (default)` | Khi phát signal, Qt kiểm tra: người nhận cùng luồng thì dùng Direct, khác luồng thì dùng Queued |
| `Qt::BlockingQueuedConnection` | Như Queued, nhưng luồng phát đứng chờ tới khi slot chạy xong |

Nhờ `AutoConnection`, ta gần như không phải chọn kiểu kết nối: cứ `connect` như bình thường, Qt tự chuyển dữ liệu an toàn sang đúng luồng.

```
Luồng SensorThread                          Luồng GUI
------------------                          ---------
emit temperatureRead(27.3)
        |   (khác luồng → Queued)
        +--> sao chép 27.3, đặt vào ------> hàng đợi sự kiện
             hàng đợi của luồng GUI            |
                                               v
                                        onTemperatureRead(27.3)
```

Vì tham số của kết nối Queued được sao chép, chúng phải là kiểu sao chép được. Các kiểu có sẵn như `int`, `double`, `QString`, `QByteArray`, `QList<double>` đều dùng được ngay. Với struct tự định nghĩa, nếu gặp lỗi `Cannot queue arguments of type ...`, ta cần khai báo kiểu đó với hệ thống meta của Qt.

`BlockingQueuedConnection` gây treo vĩnh viễn (deadlock) nếu người phát và người nhận cùng luồng nên chỉ dùng khi thật sự hiểu rõ.

## Luồng trong Qt: QThread

`QThread` quản lý một luồng của hệ điều hành. Khi gọi `start()`, luồng mới chạy hàm `run()`; mặc định `run()` chạy một event loop riêng cho luồng đó. Nhờ event loop, đối tượng sống trong luồng này nhận được signal, dùng được `QTimer`.

Có vài cách dùng luồng trong Qt:

| Cách | Khi nào dùng |
|---|---|
| Worker + `moveToThread()` | Công việc lặp lại, có trạng thái, cần timer hoặc nhận lệnh: đọc cảm biến, cổng serial |
| Kế thừa `QThread`, override `run()` | Vòng lặp tự quản lý, không cần signal gửi vào luồng |
| `QtConcurrent::run()` | Chạy một hàm một lần rồi lấy kết quả |

Trong ứng dụng HMI, worker + `moveToThread` là cách dùng nhiều nhất, và cũng là cách được tài liệu Qt khuyến nghị.

### Worker + moveToThread

Ý tưởng: tách hai vai trò.
- `QThread` là đối tượng quản lý một luồng: khởi động, dừng, chờ. Bản thân nó không chứa công việc.
- Worker là một `QObject` bình thường chứa công việc dưới dạng các slot. Ta chuyển worker sang luồng mới bằng `moveToThread` và từ đó mọi slot của worker (khi được gọi qua signal) chạy trong luồng mới.

Mẫu code chuẩn:

```cpp
auto *worker = new SensorWorker;              // không có cha
worker->moveToThread(&m_thread);

connect(&m_thread, &QThread::started,  worker, &SensorWorker::start);
connect(&m_thread, &QThread::finished, worker, &QObject::deleteLater);

m_thread.start();                             // luồng chạy, vòng lặp sự kiện bắt đầu
```

- Worker không được có cha vì Qt không cho chuyển một đối tượng có cha sang luồng khác (cha và con phải cùng luồng).
- `started` → `start`: khi luồng bắt đầu, slot `start()` của worker chạy trong luồng mới.
- `finished` → `deleteLater`: khi luồng kết thúc, worker tự được hủy. Đây là cách worker được giải phóng vì nó không có cha.
- Mặc định, `QThread::run()` gọi `exec()` nên luồng mới có sẵn vòng lặp sự kiện.

Dừng luồng:

```cpp
m_thread.quit();   // yêu cầu vòng lặp sự kiện của luồng kết thúc
m_thread.wait();   // chờ tới khi luồng dừng hẳn
```

`quit()` chỉ có tác dụng khi vòng lặp sự kiện có cơ hội chạy. Nếu worker đang ở giữa một lệnh đọc chặn 750 ms, luồng chỉ dừng sau khi lệnh đó xong. Việc dừng luồng thường đặt trong destructor của đối tượng sở hữu QThread.

Lưu ý về đối tượng con của worker: khi worker được chuyển luồng, các đối tượng con của nó được chuyển theo. Nhưng đối tượng không có cha thì không. Vì vậy, các `QTimer`, `QSerialPort`... bên trong worker phải hoặc có cha là worker hoặc được tạo trong slot `start()` (lúc đó đã chạy trong luồng mới).

### Kế thừa QThread

```cpp
class LoggerThread : public QThread
{
protected:
    void run() override
    {
        while (!isInterruptionRequested()) {
            // làm việc...
        }
    }
};
```

Chỉ phần code bên trong `run()` chạy ở luồng mới. Bản thân đối tượng `LoggerThread` vẫn thuộc luồng đã tạo nó (thường là luồng giao diện), nên các slot khai báo trong lớp này chạy ở luồng giao diện, không phải luồng mới. Đây là nguồn nhầm lẫn rất phổ biến.

Vòng lặp trong `run()` không có vòng lặp sự kiện nên `quit()` không dừng được nó. Ta dừng bằng `requestInterruption()` từ bên ngoài và kiểm tra `isInterruptionRequested()` bên trong vòng lặp.

### QtConcurrent::run

Với việc chạy một lần, `QtConcurrent::run` là cách gọn nhất: nó lấy một luồng từ thread pool, chạy hàm, rồi trả về một `QFuture` chứa kết quả.

```cpp
QFuture<double> future = QtConcurrent::run([data]() {
    return tinhTrungBinh(data);       // chạy ở luồng khác
});
```

Để nhận kết quả mà không chặn giao diện, ta dùng `QFutureWatcher`: nó phát signal finished trong luồng giao diện khi việc hoàn tất.

```cpp
auto *watcher = new QFutureWatcher<double>(this);
connect(watcher, &QFutureWatcher<double>::finished, this, [watcher]() {
    double result = watcher->result();
    // cập nhật giao diện...
    watcher->deleteLater();
});
watcher->setFuture(future);
```

`QtConcurrent` nằm trong module riêng, cần khai báo `Concurrent` trong `CMakeLists.txt`.

## Chia sẻ dữ liệu giữa các luồng

Khi hai luồng cùng đọc/ghi một biến, cần khóa bằng `QMutex` (thường qua `QMutexLocker`). Nhưng khóa dễ gây deadlock và khó gỡ lỗi. Với ứng dụng HMI, cách an toàn hơn là không dùng chung: mỗi luồng giữ dữ liệu của mình, trao đổi bằng cách gửi bản sao qua signal, như `SensorSample` trong ví dụ.

## Ví dụ

Màn hình có đồng hồ đếm 0,1 giây, giá trị nhiệt độ và hai nút:

- **Start**: worker ở luồng riêng đọc cảm biến mỗi 2 giây. Đồng hồ chạy đều.
- **Read in GUI thread**: gọi đúng hàm đọc đó nhưng trong luồng GUI. Đồng hồ đứng 750 ms.

Trên máy tính, cảm biến được giả lập. Trên board có DS18B20, truyền đường dẫn file `w1_slave` để đọc thật.

```
+------------------------------+
|       GUI clock: 12.3 s      |
|                              |
|           26.4 °C            |
|                              |
|      Read took 750 ms        |
|[    Start    ][Read GUI thr.]|
+------------------------------+
```

```
example/
+-- CMakeLists.txt
+-- sensorsample.h
+-- sensorworker.h
+-- sensorworker.cpp
+-- threadscreen.h
+-- threadscreen.cpp
+-- main.cpp
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.16)
project(threads VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(threads
    sensorsample.h
    sensorworker.h
    sensorworker.cpp
    threadscreen.h
    threadscreen.cpp
    main.cpp
)

target_link_libraries(threads PRIVATE Qt5::Widgets)
```
::: explain [Giải thích chi tiết]
- `set(CMAKE_AUTOMOC ON)`: tự chạy moc cho các lớp có `Q_OBJECT`.
- `COMPONENTS Widgets` và `Qt5::Widgets`: ứng dụng có giao diện nên cần Qt Widgets; Qt Core và Qt Gui được kéo theo tự động.
- `sensorsample.h` chỉ có header, không có file `.cpp` đi kèm.
:::

```cpp [sensorsample.h]
#ifndef SENSORSAMPLE_H
#define SENSORSAMPLE_H

#include <QMetaType>

// Một lần đọc cảm biến, gửi từ luồng đọc sang luồng giao diện
struct SensorSample
{
    double value = 0.0;       // °C
    qint64 durationMs = 0;    // thời gian của lần đọc
    bool valid = false;
};
Q_DECLARE_METATYPE(SensorSample)

#endif // SENSORSAMPLE_H
```
::: explain [Giải thích chi tiết]
- `valid`: lần đọc có thể lỗi (CRC sai, cảm biến bị rút). Giao diện cần phân biệt "0 °C" với "không đọc được".
- `Q_DECLARE_METATYPE(SensorSample)`: bước đầu tiên để gửi kiểu này qua signal giữa hai luồng.
:::

```cpp [sensorworker.h]
#ifndef SENSORWORKER_H
#define SENSORWORKER_H

#include <QObject>

#include "sensorsample.h"

class QTimer;

// Đọc cảm biến theo chu kỳ; được chuyển sang luồng riêng bằng moveToThread()
class SensorWorker : public QObject
{
    Q_OBJECT

public:
    // path rỗng: giả lập; ngược lại: đường dẫn file w1_slave của DS18B20
    explicit SensorWorker(const QString &path, QObject *parent = nullptr);

    // Đọc một lần, chặn cho tới khi xong. Gọi được từ bất kỳ luồng nào
    SensorSample readOnce();

    void start(int periodMs);
    void stop();

signals:
    void sampleReady(const SensorSample &sample);

private:
    void readAndEmit();

    QString m_path;
    QTimer *m_timer = nullptr;
};

#endif // SENSORWORKER_H
```
::: explain [Giải thích chi tiết]
- `SensorWorker` kế thừa `QObject` để chuyển được sang luồng khác bằng `moveToThread()` và phát được signal.
- `readOnce()`: đọc đồng bộ, dùng cho nút đọc trực tiếp ngay trong luồng GUI để so sánh.
- `start()`, `stop()`: hàm thường, không cần nằm trong `slots:`. Cú pháp connect bằng con trỏ hàm nối được signal vào bất kỳ hàm thành viên nào.
- `m_timer`: bắt đầu là `nullptr`, được tạo trong `start()`. Nhờ vậy timer được tạo trong luồng worker, không phải luồng GUI.
:::

```cpp [sensorworker.cpp]
#include "sensorworker.h"

#include <QElapsedTimer>
#include <QFile>
#include <QRandomGenerator>
#include <QThread>
#include <QTimer>

SensorWorker::SensorWorker(const QString &path, QObject *parent)
    : QObject(parent)
    , m_path(path)
{
}

SensorSample SensorWorker::readOnce()
{
    SensorSample sample;
    QElapsedTimer timer;
    timer.start();

    if (m_path.isEmpty()) {
        // Giả lập: DS18B20 ở độ phân giải 12 bit cần khoảng 750 ms để đo
        QThread::msleep(750);
        sample.value = 25.0 + QRandomGenerator::global()->generateDouble() * 2.0;
        sample.valid = true;
    } else {
        // File w1_slave có 2 dòng; dòng 1 kết thúc bằng YES nếu CRC đúng,
        // dòng 2 kết thúc bằng t=<nhiệt độ x 1000>
        QFile file(m_path);
        if (file.open(QIODevice::ReadOnly)) {
            const QList<QByteArray> lines = file.readAll().split('\n');
            if (lines.size() >= 2 && lines.at(0).trimmed().endsWith("YES")) {
                const int pos = lines.at(1).indexOf("t=");
                bool ok = false;
                const int milli = lines.at(1).mid(pos + 2).trimmed().toInt(&ok);
                if (pos >= 0 && ok) {
                    sample.value = milli / 1000.0;
                    sample.valid = true;
                }
            }
        }
    }

    sample.durationMs = timer.elapsed();
    return sample;
}

void SensorWorker::start(int periodMs)
{
    // Tạo timer ở đây (đã ở luồng đọc), không tạo trong constructor
    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &SensorWorker::readAndEmit);
    }
    m_timer->start(periodMs);
    readAndEmit();   // đọc ngay lần đầu
}

void SensorWorker::stop()
{
    if (m_timer)
        m_timer->stop();
}

void SensorWorker::readAndEmit()
{
    emit sampleReady(readOnce());
}
```
::: explain [Giải thích chi tiết]
- `readOnce()`: hàm chặn. Ở chế độ giả lập, `QThread::msleep(750)` mô phỏng thời gian đo của DS18B20.
- Đọc file `w1_slave`: dòng 1 kết thúc bằng `YES` khi CRC đúng; dòng 2 chứa `t=23125`, nghĩa là 23,125 °C. Mọi trường hợp sai định dạng đều trả về `valid = false`.
- `start()`: timer được tạo ở đây với parent là worker, nên timer thuộc luồng đọc. Lần gọi đầu tiên đọc ngay, không đợi hết chu kỳ.
:::

```cpp [threadscreen.h]
#ifndef THREADSCREEN_H
#define THREADSCREEN_H

#include <QElapsedTimer>
#include <QWidget>

#include "sensorsample.h"

class QLabel;
class QPushButton;
class QThread;
class SensorWorker;

class ThreadScreen : public QWidget
{
    Q_OBJECT

public:
    explicit ThreadScreen(const QString &sensorPath, QWidget *parent = nullptr);
    ~ThreadScreen() override;

signals:
    // Gửi lệnh sang luồng đọc qua signal (kết nối kiểu queued)
    void startRequested(int periodMs);
    void stopRequested();

private:
    void showSample(const SensorSample &sample);
    void toggleWorker();
    void readInGuiThread();

    QThread *m_thread;
    SensorWorker *m_worker;
    bool m_running = false;

    QElapsedTimer m_clock;
    QLabel *m_clockLabel;
    QLabel *m_valueLabel;
    QLabel *m_infoLabel;
    QPushButton *m_startButton;
};

#endif // THREADSCREEN_H
```
::: explain [Giải thích chi tiết]
- `startRequested` và `stopRequested`: màn hình ra lệnh cho worker bằng signal, không gọi thẳng `m_worker->start()`. Gọi thẳng sẽ chạy hàm trong luồng GUI.
:::

```cpp [threadscreen.cpp]
#include "threadscreen.h"
#include "sensorworker.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

ThreadScreen::ThreadScreen(const QString &sensorPath, QWidget *parent)
    : QWidget(parent)
    , m_thread(new QThread(this))
    , m_worker(new SensorWorker(sensorPath))   // không có parent: sẽ chuyển luồng
    , m_clockLabel(new QLabel)
    , m_valueLabel(new QLabel("--.- °C"))
    , m_infoLabel(new QLabel("Not read yet"))
    , m_startButton(new QPushButton("Start"))
{
    setFixedSize(320, 240);

    QFont big = m_valueLabel->font();
    big.setPixelSize(40);
    big.setBold(true);
    m_valueLabel->setFont(big);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_clockLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setAlignment(Qt::AlignCenter);

    auto *blockButton = new QPushButton("Read in GUI thread");
    m_startButton->setMinimumHeight(44);
    blockButton->setMinimumHeight(44);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_startButton);
    buttons->addWidget(blockButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(m_clockLabel);
    layout->addWidget(m_valueLabel, 1);
    layout->addWidget(m_infoLabel);
    layout->addLayout(buttons);

    // Đồng hồ 0.1 s: đứng lại nghĩa là luồng GUI đang bị chặn
    m_clock.start();
    auto *clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, [this]() {
        m_clockLabel->setText(QString("GUI clock: %1 s")
                                  .arg(m_clock.elapsed() / 1000.0, 0, 'f', 1));
    });
    clockTimer->start(100);

    // Chuẩn bị luồng đọc
    qRegisterMetaType<SensorSample>();          // để gửi SensorSample qua luồng
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &ThreadScreen::startRequested, m_worker, &SensorWorker::start);
    connect(this, &ThreadScreen::stopRequested, m_worker, &SensorWorker::stop);
    connect(m_worker, &SensorWorker::sampleReady, this, &ThreadScreen::showSample);
    m_thread->start();

    connect(m_startButton, &QPushButton::clicked, this, &ThreadScreen::toggleWorker);
    connect(blockButton, &QPushButton::clicked, this, &ThreadScreen::readInGuiThread);
}

ThreadScreen::~ThreadScreen()
{
    // Dừng event loop của luồng đọc và chờ luồng kết thúc hẳn
    m_thread->quit();
    m_thread->wait();
}

void ThreadScreen::showSample(const SensorSample &sample)
{
    if (!sample.valid) {
        m_valueLabel->setText("Error");
        return;
    }
    m_valueLabel->setText(QString("%1 °C").arg(sample.value, 0, 'f', 1));
    m_infoLabel->setText(QString("Read took %1 ms").arg(sample.durationMs));
}

void ThreadScreen::toggleWorker()
{
    m_running = !m_running;
    if (m_running)
        emit startRequested(2000);
    else
        emit stopRequested();
    m_startButton->setText(m_running ? "Stop" : "Start");
}

void ThreadScreen::readInGuiThread()
{
    // Cố ý làm sai: gọi hàm chặn ngay trong luồng GUI, đồng hồ sẽ đứng
    SensorWorker direct(QString{});
    showSample(direct.readOnce());
}
```
::: explain [Giải thích chi tiết]
- `new QThread(this)`: đối tượng `QThread` thuộc luồng GUI và là con của màn hình. Chỉ **worker** mới được chuyển sang luồng mới.
- `new SensorWorker(sensorPath)` không có parent, rồi `moveToThread(m_thread)`.
- `qRegisterMetaType<SensorSample>()`: gọi trước khi có signal nào mang `SensorSample` đi qua luồng.
- `connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater)`: worker tự xóa khi luồng kết thúc.
- Các `connect` giữa màn hình và worker không ghi kiểu kết nối. `AutoConnection` tự chọn kiểu queued vì hai đối tượng ở hai luồng khác nhau.
- `~ThreadScreen()`: `quit()` rồi `wait()`, bắt buộc trước khi `m_thread` bị xóa cùng màn hình.
- `readInGuiThread()`: tạo một worker tạm và gọi `readOnce()` ngay trong luồng GUI để thấy tác hại.
:::

```cpp [main.cpp]
#include <QApplication>

#include "threadscreen.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Tham số 1 (tùy chọn): đường dẫn w1_slave của DS18B20 trên board
    const QString sensorPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();

    ThreadScreen screen(sensorPath);
    if (QGuiApplication::platformName() == "linuxfb") {
        screen.setWindowFlags(Qt::FramelessWindowHint);
        screen.showFullScreen();
    } else {
        screen.show();
    }

    return app.exec();
}
```
::: explain [Giải thích chi tiết]
- `argc > 1 ? ... : QString()`: không truyền tham số thì dùng đường dẫn rỗng, tức chế độ giả lập.
- `QGuiApplication::platformName() == "linuxfb"`: đang chạy trên board. Khi đó cửa sổ bỏ viền (`Qt::FramelessWindowHint`) và phủ toàn màn hình; trên máy tính thì hiện như cửa sổ thường.
- `setWindowFlags()` phải gọi trước `showFullScreen()` hoặc `show()`.
:::
::::

### Build và chạy

```bash
cd example
cmake -S . -B build
cmake --build build
./build/threads
```

Bấm "Start": giá trị cập nhật mỗi 2 giây, đồng hồ chạy đều. Bấm "Read in GUI thread": đồng hồ và mọi nút đứng yên trong 750 ms.

### Chạy trên board

Phần này giả định board đã bật driver 1-Wire (overlay `w1-gpio` với một chân GPIO nối DS18B20 và điện trở kéo lên). Tên overlay và chân cụ thể phụ thuộc image, cần kiểm tra theo tài liệu của image đang dùng. Khi driver hoạt động, mỗi cảm biến xuất hiện dưới dạng thư mục `28-xxxxxxxxxxxx`:

```bash
ls /sys/bus/w1/devices/
cat /sys/bus/w1/devices/28-*/w1_slave
./threads /sys/bus/w1/devices/28-<mã-cảm-biến>/w1_slave -platform linuxfb:fb=/dev/fb1
```

## Lỗi thường gặp

**Log hiện `QObject::connect: Cannot queue arguments of type 'SensorSample'` kèm `(Make sure 'SensorSample' is registered using qRegisterMetaType().)`**

Kiểu tự định nghĩa được gửi qua kết nối queued nhưng chưa đăng ký lúc chạy. Slot không bao giờ được gọi. Thêm `qRegisterMetaType<SensorSample>()` trước khi kết nối được dùng.

**Log hiện `QObject::startTimer: Timers cannot be started from another thread`**

Timer thuộc luồng này nhưng được khởi động từ luồng khác. Thường gặp khi tạo timer trong constructor của worker (lúc đó worker còn ở luồng GUI), hoặc gọi thẳng `worker->start()` từ luồng GUI. Tạo timer trong slot chạy ở luồng đọc và ra lệnh bằng signal.

**Log hiện `QObject::moveToThread: Cannot move objects with a parent`**

Worker được tạo với parent. Tạo worker không có parent, và dùng `deleteLater` nối với `QThread::finished` để giải phóng.

**Chương trình abort khi thoát, log hiện `QThread: Destroyed while thread is still running`**

`QThread` bị xóa trong khi luồng còn chạy. Gọi `quit()` rồi `wait()` trước khi xóa, như destructor của `ThreadScreen`.

**Crash ngẫu nhiên, hoặc log hiện `QPixmap: It is not safe to use pixmaps outside the GUI thread`**

Code ở luồng phụ đụng tới widget hay pixmap. Chuyển mọi việc cập nhật giao diện về luồng GUI bằng signal.

**Giao diện vẫn đứng dù đã dùng QThread**

Hàm chặn vẫn đang chạy ở luồng GUI. Nguyên nhân hay gặp: gọi thẳng hàm của worker thay vì qua signal, hoặc chỉ kế thừa `QThread` mà đặt code trong slot thay vì trong `run()`. Slot của một đối tượng `QThread` chạy ở luồng đã tạo ra đối tượng `QThread` đó, không phải luồng mới. In `QThread::currentThread()` ở đầu hàm để kiểm tra hàm đang chạy ở luồng nào.

**Ứng dụng đóng chậm gần một giây**

`wait()` phải chờ lần đọc đang dở hoàn tất. Đây là hành vi bình thường với phần cứng chặn. Nếu cần thoát nhanh hơn, chia hàm đọc thành các bước ngắn và kiểm tra `QThread::currentThread()->isInterruptionRequested()` giữa các bước, sau khi gọi `requestInterruption()` từ luồng GUI.
