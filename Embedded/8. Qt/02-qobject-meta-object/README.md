## Vì sao cần QObject

Khi viết ứng dụng có giao diện, ta thường cần những khả năng mà C++ thuần không có sẵn:

- Biết thông tin về lớp khi chương trình đang chạy: tên lớp là gì, có những thuộc tính nào.
- Truy cập thuộc tính bằng tên chuỗi, ví dụ đọc file cấu hình có dòng `temperature=36.5` rồi gán vào đối tượng mà không cần viết code riêng cho từng thuộc tính.
- Cho các đối tượng thông báo cho nhau khi có thay đổi mà không cần biết trước về nhau. Ví dụ cảm biến báo nhiệt độ vừa đổi và giao diện tự cập nhật.

Qt bổ sung các khả năng này qua **Meta-Object System**, với lớp root là `QObject`.

## QObject cung cấp những gì

QObject là lớp nền của gần như mọi lớp trong Qt. Khi một lớp kế thừa QObject, nó nhận được:

| Khả năng | Ý nghĩa | Ví dụ trên HMI |
|---|---|---|
| Signal và slot | Đối tượng báo sự kiện cho nhau mà không cần biết nhau | Nút báo vừa bị chạm, cảm biến báo có giá trị mới |
| Cây parent-child | Cha tự xóa con, giảm quên `delete` | Đóng một màn hình là xóa luôn mọi widget trên đó |
| Event và timer | Nhận sự kiện từ event loop | Nhận cú chạm, hẹn giờ đọc cảm biến |
| Property | Thuộc tính đọc/ghi được theo tên | Qt Designer, style sheet đọc/ghi thuộc tính của widget |
| Thông tin lớp lúc chạy | Tên lớp, danh sách method, property, enum | In tên trạng thái ra log khi gỡ lỗi |
| `qobject_cast` | Ép kiểu an toàn giữa các lớp QObject | Kiểm tra một widget có đúng loại cần xử lý không |
| Tên đối tượng | `objectName`, tiện khi gỡ lỗi và tìm widget | Phân biệt hai cảm biến cùng lớp trong log |

## Macro Q_OBJECT và moc

Để một lớp dùng được Meta-Object System, ta cần hai điều: kế thừa `QObject` và đặt macro `Q_OBJECT` ở đầu phần khai báo lớp.

```cpp
class TemperatureSensor : public QObject
{
    Q_OBJECT
    // ...
};
```

C++ không tự sinh ra thông tin lớp nên Qt giải quyết bằng một công cụ gọi là **moc** (Meta-Object Compiler). Trước khi biên dịch, moc đọc các file header, tìm các lớp có macro `Q_OBJECT` rồi sinh ra một file `.cpp` chứa bảng thông tin meta của lớp đó.

```
  temperaturesensor.h
          |
         moc
          v
  moc_temperaturesensor.cpp --g++--> moc_temperaturesensor.o --+
                                                               |
  temperaturesensor.cpp -----g++---> temperaturesensor.o ------+--link--> chương trình
```

File `moc_temperaturesensor.cpp` chứa:

- Bảng tên lớp, method, property, enum (đối tượng `QMetaObject` của lớp).
- Phần body của các signal. Ta chỉ khai báo signal, moc viết phần body.
- Hàm `metaObject()`, `qt_metacall()`... mà macro `Q_OBJECT` đã khai báo.

Với CMake, ta chỉ cần bật `CMAKE_AUTOMOC`. CMake tự tìm lớp có `Q_OBJECT`, tự chạy moc và tự biên dịch file sinh ra.

:::note File moc nằm ở đâu
File sinh ra nằm trong thư mục build, ví dụ `build/qobject-demo_autogen/<code>/moc_temperaturesensor.cpp`. Mở file này ra xem là cách tốt để hiểu macro `Q_OBJECT` thực chất làm gì. Không sửa tay file này vì mỗi lần build nó được sinh lại.
:::

## QMetaObject: thông tin lớp lúc chạy

Mỗi lớp `Q_OBJECT` sở hữu một đối tượng `QMetaObject`, lấy bằng method `metaObject()`. Từ đó ta biết được tên lớp, danh sách property, signal, slot... ngay khi chương trình đang chạy.

```cpp
sensor.metaObject()->className();   // "TemperatureSensor"
sensor.inherits("QObject");         // true
```

## Các macro khai báo cho moc

moc không đọc hiểu toàn bộ C++. Nó chỉ tìm vài macro đánh dấu trong khai báo lớp rồi sinh code tương ứng.

| Macro | Tác dụng |
|---|---|
| `Q_OBJECT` | Bật Meta-Object System cho lớp |
| `Q_PROPERTY(...)` | Khai báo thuộc tính đọc/ghi được theo tên |
| `Q_ENUM(...)` | Đăng ký enum, nhờ đó chuyển được giữa giá trị và tên dạng chuỗi. Rất tiện khi in trạng thái ra log. |
| `Q_INVOKABLE` | Cho phép gọi một method theo tên lúc chạy |
| `signals:` | Khai báo signal, tức thông báo mà đối tượng phát ra |

### Q_OBJECT

`Q_OBJECT` khai báo sẵn các hàm như `metaObject()`, `qt_metacall()`. Phần body của chúng do moc viết trong `moc_*.cpp`. Nếu moc không chạy, compiler vẫn qua nhưng linker báo lỗi thiếu `vtable`.

Ba quy tắc cần nhớ:

- Đặt ở dòng đầu lớp và lớp nên nằm trong file header.
- Macro kết thúc bằng `private:` nên các thành viên viết ngay sau nó mà không ghi `public:` sẽ là private.
- Lớp con của một lớp `Q_OBJECT` cũng phải có `Q_OBJECT` riêng. Nếu thiếu, `className()` trả về tên lớp cha.

### Q_PROPERTY

Dạng hay dùng nhất:

```cpp
Q_PROPERTY(double threshold READ threshold WRITE setThreshold NOTIFY thresholdChanged)
```

| Thành phần | Ý nghĩa |
|---|---|
| `READ` | Hàm đọc, dạng `T name() const`. Bắt buộc phải có |
| `WRITE` | Hàm ghi, dạng `void setName(T)`. Không có `WRITE` thì property chỉ đọc |
| `NOTIFY` | Signal phát ra khi giá trị đổi. Không bắt buộc, nhưng QML cần nó để tự cập nhật giao diện |
| `CONSTANT` | Giá trị không bao giờ đổi. Khi đó không có `WRITE` và `NOTIFY` |

Khi đã khai báo, ta đọc và ghi property bằng tên dạng chuỗi:

```cpp
sensor.setProperty("threshold", 36.5);
double t = sensor.property("threshold").toDouble();
```

Qt Designer, style sheet (QSS) và QML đều đọc/ghi widget qua property. Kiểu của property phải là kiểu mà `QVariant` chứa được, ví dụ `int`, `double`, `bool`, `QString` hoặc enum đã đăng ký bằng `Q_ENUM`.

### Q_ENUM

```cpp
enum class Status { Normal, Overheat };
Q_ENUM(Status)
```

`Q_ENUM` đặt ngay sau enum, trong cùng lớp. Nhờ nó, ta đổi được giá trị `Status::Overheat` thành chuỗi `"Overheat"` lúc chạy. Rất tiện khi in trạng thái ra log.

### Q_INVOKABLE

Method thường của C++ không có trong bảng meta. Đặt `Q_INVOKABLE` trước kiểu trả về thì moc ghi method đó vào bảng, và ta gọi được theo tên bằng `QMetaObject::invokeMethod()`. QML cũng gọi method C++ theo cách này.

### signals và emit

`signals:` là nhãn khai báo signal. Ta chỉ viết khai báo, moc viết phần body. `emit` đặt trước lời gọi signal cho dễ đọc, bản chất vẫn là gọi hàm. Cách nối signal với hàm xử lý sẽ học ở bài Signals & Slots.

## QObject không sao chép được

Mỗi `QObject` là một thực thể riêng: có tên, có cha, có các kết nối signal/slot. Sao chép những thứ đó không có ý nghĩa rõ ràng, nên Qt cấm copy constructor và phép gán.

Hệ quả: không truyền `QObject` theo giá trị, không đặt thẳng vào container như `QList<TemperatureSensor>`. Ta dùng con trỏ `TemperatureSensor *` hoặc tham chiếu.

:::warning Đừng biến mọi thứ thành QObject
Mỗi `QObject` tốn thêm bộ nhớ cho dữ liệu nội bộ và bảng kết nối. Trên BBB chỉ có 512 MB RAM, việc tạo hàng nghìn `QObject` cho từng mẫu dữ liệu cảm biến là lãng phí. Chỉ dùng `QObject` cho đối tượng cần signal/slot hoặc cần sống trong cây parent-child. Dữ liệu thuần như một mẫu đo thì dùng `struct` bình thường.
:::

## Ví dụ

Ta tạo lớp `TemperatureSensor`: giả lập một cảm biến nhiệt độ. Mỗi lần gọi `readSample()`, cảm biến đọc một mẫu mới và nóng thêm 8 độ. Nếu nhiệt độ vượt ngưỡng `threshold`, trạng thái chuyển từ `Normal` sang `Overheat`.

```
example/
+-- CMakeLists.txt
+-- temperaturesensor.h
+-- temperaturesensor.cpp
+-- main.cpp
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.16)
project(qobject-demo VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tự chạy moc cho các lớp có Q_OBJECT
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core)

add_executable(qobject-demo
    temperaturesensor.h
    temperaturesensor.cpp
    main.cpp
)

target_link_libraries(qobject-demo PRIVATE Qt5::Core)
```
::: explain [Giải thích chi tiết]
- `set(CMAKE_AUTOMOC ON)`: bắt buộc khi có lớp dùng `Q_OBJECT`. Thiếu dòng này sẽ gặp lỗi linker.
- `find_package(Qt5 REQUIRED COMPONENTS Core)`: chỉ cần Qt Core vì ví dụ không có giao diện.
- Liệt kê cả `temperaturesensor.h` trong `add_executable` để Qt Creator hiển thị file này trong cây project.
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
- `Q_OBJECT` bật Meta-Object System cho lớp.
- Hai dòng `Q_PROPERTY` khai báo hai thuộc tính: `temperature` chỉ đọc, còn `threshold` là đọc/ghi.
- `explicit TemperatureSensor(QObject *parent = nullptr)`: là cách viết constructor chuẩn cho mọi lớp kế thừa QObject. `parent` là đối tượng cha. Khi cha bị xóa thì nó xóa luôn cảm biến. Ở đây không cần cha nên để mặc định `nullptr`.
- `void readSample()`: hàm C++ thường, không gọi được theo tên. `reset()` có `Q_INVOKABLE` nên gọi được.
- `signals`: khai báo signal `temperatureChanged`. Ta chỉ khai báo, không viết thân hàm vì moc sẽ tự sinh ra.
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
- `: QObject(parent)`: chuyển con trỏ cha lên lớp `QObject`.
- `status()`: tính trạng thái từ nhiệt độ và ngưỡng, không cần lưu riêng.
- `setTemperature()`: nơi duy nhất đổi nhiệt độ. `qFuzzyCompare()` so sánh hai số `double` có tính sai số làm tròn. Nếu giá trị không đổi thì không phát signal, tránh việc giao diện vẽ lại vô ích.
- `emit temperatureChanged(m_temperature)`: phát signal. `emit` chỉ là từ khóa cho dễ đọc, bản chất là gọi hàm do moc sinh ra.
:::

```cpp [main.cpp]
#include <QCoreApplication>
#include <QDebug>
#include <QMetaEnum>
#include <QMetaMethod>
#include <QMetaProperty>

#include "temperaturesensor.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    TemperatureSensor sensor;
    sensor.setObjectName("cabinetSensor");

    // 1. Thông tin lớp lấy lúc chạy
    const QMetaObject *meta = sensor.metaObject();
    qDebug() << "Class:" << meta->className() << "- superclass:" << meta->superClass()->className();

    // 2. Liệt kê property và method do TemperatureSensor tự khai báo
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i)
        qDebug() << "  property:" << meta->property(i).name();
    for (int i = meta->methodOffset(); i < meta->methodCount(); ++i)
        qDebug() << "  method:" << meta->method(i).methodSignature();

    // 3. Ghi property theo tên, như khi đọc từ file cấu hình
    sensor.setProperty("threshold", 40.0);
    qDebug() << "threshold =" << sensor.property("threshold").toDouble();

    // 4. Đọc vài mẫu, in trạng thái dạng chuỗi nhờ Q_ENUM
    const QMetaEnum statusEnum = QMetaEnum::fromType<TemperatureSensor::Status>();
    for (int i = 0; i < 3; ++i) {
        sensor.readSample();
        qDebug() << "temperature =" << sensor.temperature() << "- status =" << statusEnum.valueToKey(static_cast<int>(sensor.status()));
    }

    // 5. Gọi method theo tên
    QMetaObject::invokeMethod(&sensor, "reset");
    qDebug() << "After reset: temperature =" << sensor.temperature();

    // 6. Ép kiểu an toàn giữa các lớp QObject
    QObject *obj = &sensor;
    if (auto *s = qobject_cast<TemperatureSensor *>(obj))
        qDebug() << s->objectName() << "is a TemperatureSensor";

    return 0;
}
```
::: explain [Giải thích chi tiết]
- `QCoreApplication`: phiên bản không có giao diện của `QApplication` dùng cho ứng dụng console.
- `qDebug() << ...`: in ra màn hình, tự thêm dấu cách giữa các phần, `QString` được in trong dấu ngoặc kép.
- Ta dùng `return 0` thay vì `app.exec()` vì chương trình này không cần chờ sự kiện, chạy xong các dòng lệnh là kết thúc.
:::
::::

:::tip Dùng qobject_cast thay cho dynamic_cast
`qobject_cast` dùng bảng của moc nên không cần RTTI của C++ và thường nhanh hơn `dynamic_cast`. Một số bản build cho thiết bị nhúng tắt RTTI để giảm kích thước thì khi đó `qobject_cast` vẫn hoạt động.
:::

## Build và chạy

```bash
cd example
cmake -S . -B build
cmake --build build
./build/qobject-demo
```

Kết quả:

```
Class: TemperatureSensor - superclass: QObject
  property: temperature
  property: threshold
  method: "temperatureChanged(double)"
  method: "reset()"
threshold = 40
temperature = 33 - status = Normal
temperature = 41 - status = Overheat
temperature = 49 - status = Overheat
After reset: temperature = 25
"cabinetSensor" is a TemperatureSensor
```
