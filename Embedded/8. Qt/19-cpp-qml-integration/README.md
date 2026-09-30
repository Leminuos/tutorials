## Vì sao cần kết nối

QML giỏi mô tả giao diện, nhưng không nên chứa logic nặng hay truy cập phần cứng. Truy cập phần cứng, đọc cảm biến, logic cảnh báo, cấu hình vẫn nên viết bằng C++. Việc còn lại là **đưa các đối tượng C++ vào QML** để giao diện hiển thị và điều khiển chúng.

```
+--------------- QML (giao diện) ---------------+
| Label { text: Sensor.temperature }            |
| Slider { onMoved: Sensor.limit = value }      |
| Button { onClicked: Sensor.refresh() }        |
+----------^-------------------------+----------+
  NOTIFY   | property, signal        | WRITE, Q_INVOKABLE
+----------+-------------------------v----------+
| SensorBackend (C++)                           |
|   AlarmMonitor -> Hardware (cảm biến, rơ-le)  |
+-----------------------------------------------+
```

Mọi thứ đi qua **meta-object system**: moc đọc các macro `Q_OBJECT`, `Q_PROPERTY`, `Q_INVOKABLE`, `Q_ENUM` và sinh bảng thông tin lớp lúc chạy. QML chỉ thấy những gì có trong bảng đó.

| Phía C++ | Phía QML |
|---|---|
| `Q_PROPERTY(... READ ... NOTIFY ...)` | Thuộc tính đọc được, dùng trong binding, tự cập nhật khi signal NOTIFY phát |
| `Q_PROPERTY(... WRITE ...)` | Gán được: `Sensor.limit = 25` |
| `Q_PROPERTY(... CONSTANT)` | Thuộc tính không bao giờ đổi, không cần NOTIFY |
| `Q_INVOKABLE` hoặc `public slots` | Gọi được như hàm: `Sensor.refresh()` |
| `signals` | Handler `onTênSignal`, hoặc `Connections` |
| `Q_ENUM` | Hằng số: `AlarmMonitor.Alarm` |

## Ba cách đưa C++ vào QML

| Cách | Hàm | Dùng khi |
|---|---|---|
| Đối tượng có sẵn, dùng chung | `qmlRegisterSingletonInstance(uri, major, minor, tên, con-trỏ)` | Một đối tượng duy nhất do C++ tạo và quản lý: backend cảm biến, cấu hình |
| Kiểu để QML tự tạo | `qmlRegisterType<Lớp>(uri, major, minor, tên)` | Nhiều đối tượng, mỗi màn hình có thể tạo riêng |
| Kiểu chỉ để đọc enum | `qmlRegisterUncreatableType<Lớp>(..., lý do)` | QML cần hằng số của lớp nhưng không được tạo đối tượng |

Các kiểu được đăng ký dưới một **module** do ta đặt tên, ví dụ `"Hmi"` phiên bản 1.0. QML dùng bằng `import Hmi 1.0`. Mọi lệnh đăng ký phải chạy **trước** `engine.load()`.

Qt còn có `setContextProperty()` để đưa một đối tượng vào QML dưới dạng biến toàn cục. Cách này vẫn dùng được trong Qt 5 nhưng QML engine không biết trước kiểu của nó, nên kém hiệu quả hơn và công cụ không kiểm tra được; `qmlRegisterSingletonInstance()` (có từ Qt 5.14) là lựa chọn tốt hơn cho cùng mục đích.

:::warning Property không có NOTIFY thì binding không cập nhật
Binding chỉ tự tính lại khi thuộc tính phát signal NOTIFY. Khai báo `Q_PROPERTY(double temperature READ temperature)` mà thiếu `NOTIFY` thì QML đọc giá trị một lần lúc tạo rồi không bao giờ đổi, và log có cảnh báo `depends on non-NOTIFYable properties`. Mọi thuộc tính thay đổi lúc chạy phải có NOTIFY, và code C++ phải `emit` signal đó mỗi khi giá trị đổi.
:::

## Lớp cầu nối (backend)

Không nên đưa thẳng mọi lớp C++ vào QML. `AlarmMonitor` có hàm `poll()` và signal `valueRead(double)` phù hợp cho C++, nhưng QML cần **thuộc tính** để dùng binding. Lớp `SensorBackend` làm cầu nối:

- Sở hữu phần cứng (`Hardware`) và `AlarmMonitor`, tự hỏi cảm biến theo timer.
- Lưu giá trị mới nhất và đưa ra dưới dạng `Q_PROPERTY` có NOTIFY.
- Đưa ra đúng những thao tác giao diện cần: đặt ngưỡng, đọc ngay.

Cùng một backend dùng được cho cả giao diện Widgets và QML; các lớp phần cứng và logic bên dưới không phải sửa gì.

## Luồng và QML

Mọi đối tượng QML sống trong luồng GUI. Nếu backend nhận dữ liệu từ một luồng đọc phần cứng riêng (`QThread`), để worker ở luồng đó `emit` signal; kết nối kiểu queued đưa dữ liệu về luồng GUI, backend cập nhật property và phát NOTIFY ở đó. Không bao giờ phát NOTIFY hay sửa property được QML dùng từ luồng phụ.

## Ví dụ

Màn hình nhiệt độ dùng lại ba lớp C++ có sẵn, không sửa dòng nào:

| Lớp | Vai trò |
|---|---|
| `Hardware`, `createHardware(bool simulate)` | Cảm biến nhiệt độ và rơ-le, bản thật (LM75 qua I2C, GPIO) hoặc giả lập; xây dựng ở Bài 17 |
| `AlarmMonitor` | `poll()` đọc cảm biến, phát `valueRead(double)` và `stateChanged(State)`; `State` là `Normal`, `Alarm`, `SensorError`; ngưỡng có trễ |
| `Counter` | Bộ đếm kế thừa `QObject`, property `value` đọc/ghi được, signal `valueChanged(int)` |

Giao diện gồm:

- Nhiệt độ hiện lớn, chuyển đỏ khi cảnh báo, dựa trên binding với `Sensor.temperature` và `Sensor.state`.
- Slider đặt ngưỡng 20–40 °C, ghi thẳng vào property `limit` của C++.
- Nút "Read" gọi hàm `Q_INVOKABLE` `refresh()`.
- Một `Counter` tạo trong QML đếm số lần vào cảnh báo, thông qua `Connections`.

```
+------------------------------+
| Sensor: simulated            |
|                              |
|           24.5 °C            |
|                              |
|    Over limit (count 1)      |
| Limit 20 °C =o======= [Read] |
+------------------------------+
```

```
example/
+-- CMakeLists.txt
+-- qml.qrc
+-- main.qml
+-- src/
    +-- hal.h, hardware.*, linuxhardware.*, simhardware.*   <- phần cứng, dùng lại
    +-- alarmmonitor.*                                      <- logic cảnh báo, dùng lại
    +-- counter.*                                           <- bộ đếm, dùng lại
    +-- sensorbackend.h                                     <- mới
    +-- sensorbackend.cpp                                   <- mới
    +-- main.cpp
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.16)
project(cpp-qml VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Quick QuickControls2)

add_executable(cpp-qml
    src/hal.h
    src/hardware.h src/hardware.cpp
    src/linuxhardware.h src/linuxhardware.cpp
    src/simhardware.h src/simhardware.cpp
    src/alarmmonitor.h src/alarmmonitor.cpp
    src/counter.h src/counter.cpp
    src/sensorbackend.h src/sensorbackend.cpp
    src/main.cpp
    qml.qrc
)

target_include_directories(cpp-qml PRIVATE src)
target_link_libraries(cpp-qml PRIVATE Qt5::Quick Qt5::QuickControls2)
install(TARGETS cpp-qml RUNTIME DESTINATION bin)
```
::: explain [Giải thích chi tiết]
- `CMAKE_AUTOMOC ON`: bắt buộc, vì các lớp đưa vào QML đều dùng `Q_OBJECT`.
:::

```xml [qml.qrc]
<RCC>
    <qresource prefix="/">
        <file>main.qml</file>
    </qresource>
</RCC>
```
::: explain [Giải thích chi tiết]
- `prefix="/"`: file `main.qml` trong resource có đường dẫn `qrc:/main.qml`, chính là đường dẫn `main.cpp` truyền cho `engine.load()`.
- Mỗi file QML phải được liệt kê ở đây; quên một file thì QML báo không tìm thấy lúc chạy.
:::

```cpp [src/sensorbackend.h]
#ifndef SENSORBACKEND_H
#define SENSORBACKEND_H

#include <QObject>

#include "alarmmonitor.h"
#include "hardware.h"

class QTimer;

// Cầu nối giữa phần cứng/logic C++ và giao diện QML.
// QML chỉ thấy các property, signal và hàm Q_INVOKABLE khai báo ở đây.
class SensorBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    Q_PROPERTY(double limit READ limit WRITE setLimit NOTIFY limitChanged)
    Q_PROPERTY(AlarmMonitor::State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool simulated READ simulated CONSTANT)

public:
    explicit SensorBackend(bool simulate, QObject *parent = nullptr);

    double temperature() const;
    double limit() const;
    void setLimit(double limit);
    AlarmMonitor::State state() const;
    bool simulated() const;

    // Gọi được từ QML: đọc cảm biến ngay, không chờ chu kỳ
    Q_INVOKABLE void refresh();

signals:
    void temperatureChanged();
    void limitChanged();
    void stateChanged();

private:
    Hardware m_hw;
    AlarmMonitor *m_monitor;
    QTimer *m_timer;
    double m_temperature = 0.0;
};

#endif // SENSORBACKEND_H
```
::: explain [Giải thích chi tiết]
- `Q_PROPERTY(AlarmMonitor::State state ...)`: kiểu enum của lớp khác dùng được làm property vì `AlarmMonitor` đã đăng ký enum đó bằng `Q_ENUM`.
- `Q_PROPERTY(bool simulated READ simulated CONSTANT)`: không đổi suốt chương trình, không cần NOTIFY.
- `Hardware m_hw`: backend sở hữu phần cứng, phần cứng sống cùng backend.
:::

```cpp [src/sensorbackend.cpp]
#include "sensorbackend.h"

#include <QTimer>

SensorBackend::SensorBackend(bool simulate, QObject *parent)
    : QObject(parent)
    , m_hw(createHardware(simulate))
    , m_monitor(new AlarmMonitor(m_hw.temperature.get(), this))
    , m_timer(new QTimer(this))
{
    m_monitor->setLimit(30.0);

    // Chuyển signal của AlarmMonitor thành signal NOTIFY của property
    connect(m_monitor, &AlarmMonitor::valueRead, this, [this](double value) {
        if (qFuzzyCompare(value, m_temperature))
            return;
        m_temperature = value;
        emit temperatureChanged();
    });
    connect(m_monitor, &AlarmMonitor::stateChanged, this, [this]() {
        m_hw.relay->setValue(m_monitor->state() == AlarmMonitor::State::Alarm);
        emit stateChanged();
    });

    connect(m_timer, &QTimer::timeout, m_monitor, &AlarmMonitor::poll);
    m_timer->start(1000);
    m_monitor->poll();
}

double SensorBackend::temperature() const
{
    return m_temperature;
}

double SensorBackend::limit() const
{
    return m_monitor->limit();
}

void SensorBackend::setLimit(double limit)
{
    if (qFuzzyCompare(limit, m_monitor->limit()))
        return;
    m_monitor->setLimit(limit);
    emit limitChanged();
}

AlarmMonitor::State SensorBackend::state() const
{
    return m_monitor->state();
}

bool SensorBackend::simulated() const
{
    return m_hw.simulated;
}

void SensorBackend::refresh()
{
    m_monitor->poll();
}
```
::: explain [Giải thích chi tiết]
- `m_hw(createHardware(simulate))` được khởi tạo trước `m_monitor` vì khai báo trước trong lớp; `AlarmMonitor` nhận con trỏ tới cảm biến bên trong `m_hw`.
- Lambda nối `valueRead` chỉ phát `temperatureChanged` khi giá trị thật sự đổi, tránh QML tính lại binding và vẽ lại vô ích, vốn tốn CPU trên board không có GPU.
- `stateChanged` còn điều khiển rơ-le: bật khi cảnh báo, tắt khi trở lại bình thường.
- `setLimit()`: hàm WRITE kiểm tra giá trị mới trước khi phát `limitChanged`, tránh vòng lặp binding giữa slider và property.
:::

```qml [main.qml]
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Hmi 1.0

ApplicationWindow {
    width: 320
    height: 240
    visible: true
    title: "C++ and QML"
    font.pixelSize: 14

    // Kiểu C++ tạo trong QML: đếm số lần vào cảnh báo
    Counter {
        id: alarmCounter
    }

    // Nhận signal của đối tượng C++ không nằm trong cây QML
    Connections {
        target: Sensor
        function onStateChanged() {
            if (Sensor.state === AlarmMonitor.Alarm)
                alarmCounter.value += 1
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6

        Label {
            text: Sensor.simulated ? "Sensor: simulated" : "Sensor: LM75"
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: Sensor.temperature.toFixed(1) + " °C"    // property C++ có NOTIFY: tự cập nhật
            font.pixelSize: 44
            font.bold: true
            color: Sensor.state === AlarmMonitor.Alarm ? "#d00000" : "black"
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: {
                switch (Sensor.state) {
                case AlarmMonitor.Alarm: return "Over limit (count " + alarmCounter.value + ")"
                case AlarmMonitor.SensorError: return "Sensor error"
                default: return "Normal"
                }
            }
        }

        RowLayout {
            Label { text: "Limit " + Sensor.limit.toFixed(0) + " °C" }
            Slider {
                Layout.fillWidth: true
                from: 20
                to: 40
                stepSize: 1
                value: Sensor.limit
                onMoved: Sensor.limit = value              // gọi WRITE của Q_PROPERTY
            }
            Button {
                text: "Read"
                onClicked: Sensor.refresh()                // gọi hàm Q_INVOKABLE
            }
        }
    }
}
```
::: explain [Giải thích chi tiết]
- `import Hmi 1.0`: module do C++ đăng ký trong `src/main.cpp`, trước khi nạp QML.
- `Counter { id: alarmCounter }`: QML tạo đối tượng C++ như mọi kiểu QML khác. `Counter::step()` không có `Q_INVOKABLE` nên QML không gọi được; thay vào đó, QML ghi thẳng vào property `value`, vì `value` có `WRITE`.
- `Connections { target: Sensor; function onStateChanged() {...} }`: nhận signal của một đối tượng không nằm trong cây QML. Cú pháp `function on...()` là cách viết của Qt 5.15.
- `Sensor.temperature.toFixed(1)`: `double` của C++ thành số JavaScript, `toFixed` định dạng 1 chữ số lẻ.
- `Sensor.state === AlarmMonitor.Alarm`: so sánh với hằng enum đăng ký qua `qmlRegisterUncreatableType`.
- `text: { switch (...) { ... } }`: binding có thể là một khối JavaScript trả về giá trị.
:::

```cpp [src/main.cpp]
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml>

#include "counter.h"
#include "sensorbackend.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Default");

    // --hw: phần cứng thật trên board; mặc định giả lập
    const bool simulate = !app.arguments().contains("--hw");
    SensorBackend sensor(simulate);

    // 1. Một đối tượng C++ có sẵn, dùng trong QML dưới tên Sensor
    qmlRegisterSingletonInstance("Hmi", 1, 0, "Sensor", &sensor);
    // 2. Một kiểu C++ mà QML tự tạo đối tượng: Counter { }
    qmlRegisterType<Counter>("Hmi", 1, 0, "Counter");
    // 3. Chỉ để QML dùng enum AlarmMonitor::State, không cho tạo đối tượng
    qmlRegisterUncreatableType<AlarmMonitor>("Hmi", 1, 0, "AlarmMonitor",
                                             "AlarmMonitor is only used to read enums");

    QQmlApplicationEngine engine;
    engine.load(QUrl("qrc:/main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
```
::: explain [Giải thích chi tiết]
- `SensorBackend sensor` là biến cục bộ của `main()`, sống lâu hơn engine QML. Với `qmlRegisterSingletonInstance`, C++ giữ quyền sở hữu đối tượng.
- Ba lệnh đăng ký đều đặt trước `engine.load()`.
- `#include <QtQml>`: header chứa các hàm `qmlRegister...`.
:::
::::

### Build và chạy

```bash
cd example
cmake -S . -B build
cmake --build build
./build/cpp-qml
```

Đã kiểm tra tự động: kéo ngưỡng xuống 20 °C thì màn hình chuyển "Over limit (count 1)" và giá trị `limit` trong C++ là 20; kéo lên 40 °C thì về "Normal"; đặt lại 20 °C và bấm "Read" thì chuyển ngay "Over limit (count 2)".

### Chạy trên board

```bash
QT_QUICK_BACKEND=software ./cpp-qml --hw -platform linuxfb:fb=/dev/fb1
```

`--hw` dùng LM75 và GPIO thật thay cho bản giả lập; giao diện QML không đổi.

:::tip Thử backend mà không cần giao diện
Vì mọi logic nằm trong C++, có thể viết unit test cho `SensorBackend` bằng Qt Test; `QSignalSpy` ghi lại các lần một signal được phát, để kiểm tra `temperatureChanged`, `stateChanged` mà không nạp QML nào.
:::

## Lỗi thường gặp

**Log hiện `module "Hmi" is not installed`**

Module chưa được đăng ký khi QML được nạp: lệnh `qmlRegister...` nằm sau `engine.load()`, sai tên module, hoặc phiên bản trong `import` khác phiên bản đăng ký.

**Log hiện `QQmlExpression: Expression ... depends on non-NOTIFYable properties`, giá trị trên màn hình không đổi**

Property được dùng trong binding không có `NOTIFY`. Thêm signal NOTIFY và `emit` nó khi giá trị đổi.

**Log hiện `TypeError: Property 'step' of object Counter(0x...) is not a function`**

Hàm C++ không được đánh dấu `Q_INVOKABLE` và cũng không phải slot, nên QML không thấy. Thêm `Q_INVOKABLE`, hoặc để QML thao tác qua property có `WRITE`.

**Log hiện `TypeError: Cannot assign to read-only property "temperature"`**

QML gán vào một property C++ không có `WRITE`. Thuộc tính chỉ đọc chỉ dùng được ở vế phải của binding; muốn QML thay đổi được, thêm hàm `WRITE`, hoặc một hàm `Q_INVOKABLE` riêng.

**Chương trình có thể crash khi thoát**

Đối tượng đăng ký bằng `qmlRegisterSingletonInstance` phải sống lâu hơn engine. Nếu nó được khai báo sau `QQmlApplicationEngine` trong `main()`, nó bị hủy trước engine, và engine có thể còn dùng tới nó trong lúc dọn dẹp. Khai báo backend trước engine như trong ví dụ.

**Enum so sánh luôn sai trong QML**

So sánh với số cứng hoặc chuỗi thay vì hằng enum, hoặc quên đăng ký lớp chứa enum. Đăng ký bằng `qmlRegisterUncreatableType` và so sánh với `TênLớp.GiáTrị`.
