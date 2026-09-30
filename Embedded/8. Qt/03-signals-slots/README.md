## Vì sao cần signal và slot

Trong ứng dụng HMI, các đối tượng liên tục cần thông báo cho nhau:
- Cảm biến đọc được giá trị mới → nhãn trên màn hình cần hiển thị lại.
- Người dùng nhấn nút → cần gửi lệnh xuống phần cứng.
- Nhiệt độ vượt ngưỡng → cần hiện cảnh báo, ghi log, bật còi.

Cách đơn giản nhất là cho cảm biến giữ con trỏ tới nhãn và gọi thẳng `label->setText(...)`. Nhưng khi đó lớp cảm biến bị ràng buộc chặt với giao diện: muốn thêm chức năng ghi log, ta phải sửa lớp cảm biến; muốn chạy cảm biến mà không có giao diện (ví dụ khi viết test), ta không làm được.

Qt giải quyết bằng cơ chế **signal** và **slot**. Đây là cơ chế giao tiếp giữa các đối tượng trong Qt, ý tưởng của nó là:

- **Signal**: đối tượng phát thông báo khi có chuyện xảy ra. Nó chỉ thông báo "Dữ liệu vừa thay đổi", không cần biết ai đang nghe.
- **Slot**: hàm được gọi tự động khi signal mà nó được nối phát ra.
- **connect**: nối một signal với một slot. Việc nối do một bên thứ ba làm, thường là lớp giao diện.

```
  TemperatureSensor                        SensorPanel
+----------------+ temperatureChanged(33) +---------------------+
| readSample()   |-----------+----------->| updateDisplay(33)   |
|  emit ...      |           |            +---------------------+
+----------------+           |            +---------------------+
                             +----------->| lambda ghi log      |
                                          +---------------------+
   TemperatureSensor không biết có bao nhiêu nơi đang lắng nghe
```

## Signal

Signal được khai báo trong mục `signals:` của lớp. Ta chỉ khai báo, không cần viết thân hàm vì moc sẽ tự sinh ra phần đó.

```cpp
signals:
    void temperatureChanged(double value);
```

Để phát signal, ta dùng từ khóa emit:

```cpp
emit temperatureChanged(m_temperature);
```

Các lớp có sẵn của Qt cũng có rất nhiều signal, ví dụ `QPushButton::clicked` khi nút được nhấn, `QTimer::timeout` khi hết thời gian hẹn giờ, `QSerialPort::readyRead` khi có dữ liệu UART tới.

## Slot

Slot là một hàm thành viên bình thường. Ta thường khai báo slot trong mục `public slots:` để làm rõ ý đồ:

```cpp
public slots:
    void reset();
```

Slot có thân hàm như mọi hàm khác và vẫn gọi trực tiếp được như hàm thường.

Với cú pháp connect hiện đại, Slot có thể là:
- Hàm khai báo trong phần `public slots:` hoặc `private slots:`.
- Hàm thành viên thường như `TemperatureSensor::readSample()`.
- Lambda, hữu ích khi xử lý ngắn và chỉ dùng một chỗ.

Không bắt buộc nằm trong `slots:`. Tuy vậy, ta vẫn nên đặt các hàm dự định dùng làm slot vào mục `slots:` vì một số tính năng như gọi hàm từ QML cần điều đó.

## Hàm connect

Cú pháp chuẩn:

```cpp
connect(sender, &Sender::signalName, receiver, &Receiver::slotName);
```

| Tham số | Ý nghĩa |
|---|---|
| `sender` | Con trỏ tới đối tượng phát signal |
| `&Sender::signalName` | Con trỏ tới signal |
| `receiver` | Đối tượng nhận, kết nối tự cancel khi đối tượng này bị xóa |
| `&Receiver::slotName` | Hàm được gọi |

Cú pháp này dùng con trỏ hàm nên compiler kiểm tra được tên signal, tên slot và kiểu tham số ngay lúc biên dịch. Sai là báo lỗi ngay, không phải đợi tới lúc chạy.

Khi gọi `connect` bên trong một lớp kế thừa `QObject`, ta viết `connect(...)`. Khi gọi ở nơi khác như trong `main()`, ta viết `QObject::connect(...)`.

Ví dụ:

```cpp
connect(quitButton, &QPushButton::clicked, &app, &QApplication::quit);
```

Đọc là: khi `quitButton` phát `clicked` thì gọi `app.quit()`.

:::note Cú pháp cũ SIGNAL()/SLOT()
Trong code cũ hoặc tài liệu cũ, ta sẽ gặp cách viết:

```cpp
connect(button, SIGNAL(clicked()), &app, SLOT(quit()));
```

Cú pháp này dùng chuỗi ký tự nên gõ sai tên thì compiler không phát hiện được, chỉ khi chạy mới có cảnh báo trong console. Ta chỉ cần nhận ra nó khi đọc code cũ còn khi viết code mới thì luôn dùng cú pháp con trỏ hàm.
:::

## Các kiểu connect

- Một signal → nhiều slot: tất cả slot đều được gọi, theo thứ tự đã connect.
- Nhiều signal → một slot: slot được gọi khi bất kỳ signal nào phát ra.
- Signal → signal: nối signal của đối tượng này thẳng vào signal của đối tượng khác, để chuyển tiếp thông báo mà không cần viết hàm trung gian.

```cpp
connect(&sensor, &TemperatureSensor::temperatureChanged, this, &MainScreen::displayValueChanged);   // chuyển tiếp signal
```

## Tham số giữa signal và slot

Qt truyền tham số của signal sang slot theo thứ tự:
- Kiểu tham số phải tương thích: signal gửi `double` thì slot phải nhận `double` hoặc kiểu mà `double` tự chuyển sang được.
- Slot được phép nhận ít tham số hơn signal, các tham số thừa bị bỏ qua. Ví dụ `QPushButton::clicked(bool checked)` vẫn nối được với `QApplication::quit()` không có tham số.

Chiều ngược lại thì không được: slot không thể đòi nhiều tham số hơn những gì signal gửi.

Ví dụ:

| Signal | Slot | Kết quả |
|---|---|---|
| `temperatureChanged(double)` | `updateDisplay(double)` | Hợp lệ |
| `temperatureChanged(double)` | `reset()` | Hợp lệ, bỏ qua `double` |
| `clicked(bool)` | `readSample()` | Hợp lệ, bỏ qua `bool` |
| `clicked(bool)` | `setText(const QString &)` | Lỗi biên dịch: `bool` không đổi được sang `QString` |

## Lambda và context object

Lambda có thể nối trực tiếp:

```cpp
connect(button, &QPushButton::clicked, this, [this]() { /* ... */ });
```

Tham số thứ ba (`this`) là **context object**. Khi context object bị xóa, kết nối tự hủy, lambda không bao giờ được gọi với con trỏ `this` đã chết.

:::warning Luôn truyền context object cho lambda
Qt cho phép bỏ tham số thứ ba: `connect(button, &QPushButton::clicked, [this]() {...})`. Khi đó kết nối chỉ hủy theo `button`. Nếu `this` bị xóa trước mà `button` vẫn sống, lambda chạy trên vùng nhớ đã giải phóng và chương trình crash. Lỗi kiểu này thường chỉ xuất hiện sau nhiều giờ chạy trên thiết bị.
:::

## Khi signal được phát, slot chạy lúc nào

Khi sender và receiver cùng nằm trong một luồng, emit gọi ngay lập tức tất cả các slot theo thứ tự connect. Chỉ khi các slot chạy xong thì chương trình mới đi tiếp dòng sau emit.

```cpp
m_temperature = value;
emit temperatureChanged(value);   // ← các slot chạy xong hết ở đây
qDebug() << "Dòng này chạy sau tất cả các slot";
```

Điều này có hai hệ quả quan trọng: slot chạy chậm sẽ làm người phát bị chậm theo, và nếu slot chạy lâu thì giao diện bị đứng.

Khi sender và receiver ở hai luồng khác nhau, Qt tự chuyển sang cách gửi khác: đặt lời gọi vào hàng đợi của luồng nhận và slot sẽ chạy sau khi luồng đó rảnh. Nhờ vậy dữ liệu từ luồng đọc phần cứng được chuyển an toàn sang luồng giao diện.

## Ngắt kết nối

`connect()` trả về một `QMetaObject::Connection`. Giữ lại giá trị này để ngắt sau:

```cpp
QMetaObject::Connection c = connect(...);
disconnect(c);
```

Phần lớn trường hợp không cần tự ngắt vì kết nối tự hủy khi sender hoặc receiver bị xóa.

## Ví dụ

Ta làm một màn hình 320×240 hiển thị cảm biến nhiệt độ giả lập `TemperatureSensor`. Nút "Read" đọc một mẫu mới (mỗi lần nóng thêm 8 độ), nút "Reset" đưa nhiệt độ về 25 độ. Khi nhiệt độ đạt ngưỡng 60 độ, dòng trạng thái đổi thành "Overheat!".

```
example/
+-- CMakeLists.txt
+-- temperaturesensor.h
+-- temperaturesensor.cpp
+-- sensorpanel.h
+-- sensorpanel.cpp
+-- main.cpp
```

```
+------------------------------+
|                              |
|           33.0 °C            |  <- m_valueLabel
|            Normal            |  <- m_statusLabel
| +------------++------------+ |
| |    Read    ||   Reset    | |
| +------------++------------+ |
+------------------------------+
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.16)
project(signals-slots VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(signals-slots
    temperaturesensor.h
    temperaturesensor.cpp
    sensorpanel.h
    sensorpanel.cpp
    main.cpp
)

target_link_libraries(signals-slots PRIVATE Qt5::Widgets)
```
::: explain [Giải thích chi tiết]
Ví dụ có giao diện nên cần module `Widgets` (tự kéo theo `Core` và `Gui`). `CMAKE_AUTOMOC` bật moc, công cụ sinh code cho các lớp có macro `Q_OBJECT` như `TemperatureSensor` và `SensorPanel`.
:::

```cpp [temperaturesensor.h]
#ifndef TEMPERATURESENSOR_H
#define TEMPERATURESENSOR_H

#include <QObject>

class TemperatureSensor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double temperature READ temperature NOTIFY temperatureChanged)
    Q_PROPERTY(double threshold READ threshold WRITE setThreshold)

public:
    enum class Status { Normal, Overheat };
    Q_ENUM(Status)

    explicit TemperatureSensor(QObject *parent = nullptr);

    double temperature() const;
    double threshold() const;
    void setThreshold(double threshold);
    Status status() const;

    void readSample();

    Q_INVOKABLE void reset();

signals:
    void temperatureChanged(double temperature);

private:
    void setTemperature(double temperature);

    double m_temperature = 25.0;
    double m_threshold = 60.0;
};

#endif // TEMPERATURESENSOR_H
```
::: explain [Giải thích chi tiết]
Lớp cảm biến, kế thừa `QObject`. Phần liên quan tới bài này:
- `void temperatureChanged(double temperature)` trong `signals:`: signal phát ra mỗi khi nhiệt độ đổi, kèm giá trị mới.
- `readSample()` và `reset()`: hàm thường, sẽ được nối trực tiếp với nút bấm mà không cần khai báo là slot.
- `status()`: trả về `Status::Normal` hoặc `Status::Overheat` khi so với ngưỡng (mặc định 60 độ).
:::

```cpp [temperaturesensor.cpp]
#include "temperaturesensor.h"

TemperatureSensor::TemperatureSensor(QObject *parent)
    : QObject(parent)
{
}

double TemperatureSensor::temperature() const
{
    return m_temperature;
}

double TemperatureSensor::threshold() const
{
    return m_threshold;
}

void TemperatureSensor::setThreshold(double threshold)
{
    m_threshold = threshold;
}

TemperatureSensor::Status TemperatureSensor::status() const
{
    return m_temperature >= m_threshold ? Status::Overheat : Status::Normal;
}

void TemperatureSensor::readSample()
{
    setTemperature(m_temperature + 8.0);
}

void TemperatureSensor::reset()
{
    setTemperature(25.0);
}

void TemperatureSensor::setTemperature(double temperature)
{
    if (qFuzzyCompare(m_temperature, temperature)) return;
    m_temperature = temperature;
    emit temperatureChanged(m_temperature);
}
```
::: explain [Giải thích chi tiết]
`setTemperature()` là nơi duy nhất đổi nhiệt độ và phát `temperatureChanged`. Nó chỉ `emit` khi giá trị thật sự đổi nên bấm "Reset" lúc đang ở 25 độ sẽ không phát signal.
:::

```cpp [sensorpanel.h]
#ifndef SENSORPANEL_H
#define SENSORPANEL_H

#include <QWidget>

class QLabel;
class TemperatureSensor;

class SensorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SensorPanel(QWidget *parent = nullptr);

private slots:
    void updateDisplay(double temperature);

private:
    TemperatureSensor *m_sensor;
    QLabel *m_valueLabel;
    QLabel *m_statusLabel;
};

#endif // SENSORPANEL_H
```
::: explain [Giải thích chi tiết]
- `class QLabel;`: khai báo trước (forward declaration) thay cho `#include`. Header chỉ dùng con trỏ nên không cần định nghĩa đầy đủ, giúp build nhanh hơn.
- `private slots:`: `updateDisplay` là slot. `slots` là từ khóa của Qt, moc dùng nó để ghi hàm vào bảng meta-object.
:::

```cpp [sensorpanel.cpp]
#include "sensorpanel.h"
#include "temperaturesensor.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SensorPanel::SensorPanel(QWidget *parent)
    : QWidget(parent)
    , m_sensor(new TemperatureSensor(this))
    , m_valueLabel(new QLabel)
    , m_statusLabel(new QLabel)
{
    setFixedSize(320, 240);

    QFont bigFont = m_valueLabel->font();
    bigFont.setPointSize(40);
    m_valueLabel->setFont(bigFont);
    m_valueLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setAlignment(Qt::AlignCenter);

    auto *readButton = new QPushButton("Read");
    auto *resetButton = new QPushButton("Reset");
    readButton->setMinimumHeight(48);
    resetButton->setMinimumHeight(48);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(readButton);
    buttons->addWidget(resetButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_valueLabel);
    layout->addWidget(m_statusLabel);
    layout->addLayout(buttons);

    connect(readButton, &QPushButton::clicked, m_sensor, &TemperatureSensor::readSample);
    connect(resetButton, &QPushButton::clicked, m_sensor, &TemperatureSensor::reset);

    connect(m_sensor, &TemperatureSensor::temperatureChanged, this, &SensorPanel::updateDisplay);

    connect(m_sensor, &TemperatureSensor::temperatureChanged, this, [](double temperature) {
        qDebug() << "temperature =" << temperature;
    });

    updateDisplay(m_sensor->temperature());
}

void SensorPanel::updateDisplay(double temperature)
{
    m_valueLabel->setText(QString::number(temperature, 'f', 1) + " °C");

    if (m_sensor->status() == TemperatureSensor::Status::Overheat)
        m_statusLabel->setText("Overheat!");
    else
        m_statusLabel->setText("Normal");
}
```
::: explain [Giải thích chi tiết]
- `new TemperatureSensor(this)`: `this` là parent của cảm biến. Khi màn hình bị xóa, nó tự xóa cảm biến, nên không cần `delete`.
- Kết nối 1: `QPushButton::clicked(bool)` nối tới `readSample()` và `reset()`, là các hàm thường, không cần khai báo là slot. Tham số `bool` bị bỏ qua.
- Kết nối 2: mỗi lần nhiệt độ đổi, `updateDisplay()` nhận giá trị mới và cập nhật hai nhãn.
- Kết nối 3: signal `temperatureChanged` được nối tới hai nơi (kết nối 2 và 3). Cả hai cùng chạy, theo thứ tự connect. Lambda có context object là `this`, nên kết nối tự hủy khi màn hình bị xóa.
- `updateDisplay(m_sensor->temperature())`: gọi slot trực tiếp để nhãn có giá trị ngay từ đầu, trước khi có signal nào.
- `QString::number(temperature, 'f', 1)`: đổi số thành chuỗi với 1 chữ số sau dấu phẩy, ví dụ `"33.0"`.
- `connect(...)` ở đây gọi không cần `QObject::`, vì `SensorPanel` kế thừa `QObject` qua `QWidget`.
:::

```cpp [main.cpp]
#include <QApplication>

#include "sensorpanel.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    SensorPanel panel;
    panel.show();

    return app.exec();
}
```
::: explain [Giải thích chi tiết]
- `SensorPanel panel`: màn hình là cửa sổ gốc, tạo trên stack của `main()`. Các widget con có parent là `panel` nên tự bị xóa theo.
- `QApplication app(argc, argv)`: tạo trước mọi widget.
- `app.exec()`: chạy event loop, chỉ trả về khi ứng dụng thoát.
:::
::::

### Build và chạy

```bash
cd example
cmake -S . -B build
cmake --build build
./build/signals-slots
```

Bấm "Read" vài lần: nhiệt độ tăng dần 33.0, 41.0, 49.0... và mỗi giá trị mới được in ra console qua lambda. Tới 65.0 °C, dòng trạng thái đổi thành "Overheat!". Bấm "Reset" để về 25.0 °C.

:::tip Kiểm tra giá trị trước khi phát signal
`TemperatureSensor::setTemperature()` chỉ `emit` khi giá trị thật sự đổi. Thói quen này tránh vẽ lại vô ích, và tránh vòng lặp vô hạn khi hai đối tượng nối chéo nhau, ví dụ thanh trượt và ô nhập số đồng bộ giá trị cho nhau.
:::
