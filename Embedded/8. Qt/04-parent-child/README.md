## Vì sao quản lý bộ nhớ quan trọng với thiết bị nhúng?

Ứng dụng trên máy tính thường chỉ chạy vài giờ rồi được tắt. Khi tắt, hệ điều hành thu hồi toàn bộ bộ nhớ nên memory leak ít khi gây hậu quả. Ứng dụng HMI thì khác: nó khởi động cùng thiết bị và chạy liên tục nhiều ngày, nhiều tháng. Nếu mỗi lần chuyển màn hình bị leak vài KB, sau vài ngày hoặc vài tuần thì bộ nhớ RAM sẽ cạn. Khi đó Linux sẽ trigger OOM killer và ứng dụng bị tắt đột ngột, thường vào lúc không ai có mặt để khởi động lại.

C++ yêu cầu mỗi `new` phải có một `delete` tương ứng. Tuy nhiên với giao diện có hàng chục đến hàng trăm widget thì việc tự theo dõi và `delete` từng đối tượng thì ta rất dễ quên một cái (memory leak) hoặc xóa một cái hai lần (crash).

Qt giải quyết vấn đề này bằng object tree.

## Object tree

Mỗi `QObject` có thể có một **parent** (cha) và nhiều **children** (con). Quy tắc cốt lõi:

> Khi đối tượng cha bị xóa, cha xóa tất cả con của nó trước khi kết thúc.

Vì con cũng có thể có con, việc hủy lan xuống toàn bộ nhánh cây:

Lấy ví dụ màn hình `SensorPanel` hiển thị một cảm biến nhiệt độ, gồm hai nhãn, hai nút và một đối tượng `TemperatureSensor` giữ giá trị. Nó tạo thành cây sau:

```
panel  (SensorPanel)                ← chỉ cần hủy panel...
+-- m_sensor      (TemperatureSensor)
+-- layout        (QVBoxLayout)     ← ...toàn bộ các đối tượng bên dưới tự động bị hủy theo
|   +-- buttons   (QHBoxLayout)
+-- m_valueLabel  (QLabel)
+-- m_statusLabel (QLabel)
+-- readButton    (QPushButton)
+-- resetButton   (QPushButton)
```

Nhờ vậy, ta chỉ cần quản lý đối tượng gốc. Mọi đối tượng khác được gắn vào cây sẽ tự được giải phóng.

Đây chính là lý do code tạo màn hình có thể dùng `new` nhiều lần mà không cần một lệnh `delete` nào mà vẫn không bị leak.

## Cách gán cha cho một đối tượng

**1. Truyền cha vào constructor:**

```cpp
auto *label = new QLabel("Xin chào", &window);
```

Đây là lý do mọi lớp kế thừa `QObject` đều có constructor dạng M`yClass(QObject *parent = nullptr)`.

**2. Gọi `setParent()`:**

```cpp
auto *label = new QLabel("Xin chào");
label->setParent(&window);
```

**3. Thêm widget vào layout**

khi gọi `layout->addWidget(label)`, layout tự đặt label làm con của widget đang sở hữu layout đó.

## Stack hay heap

Quy tắc đơn giản:

- Đối tượng gốc (cửa sổ chính, đối tượng không có cha) tạo trên **stack** trong `main()`.
- Mọi đối tượng con tạo bằng `new` trên **heap**, kèm parent.

Đối tượng con trên stack rất dễ gây lỗi. Biến trên stack bị hủy theo thứ tự ngược với lúc khai báo:

```cpp
QObject child;
QObject parent;
child.setParent(&parent);
// Hết hàm: parent bị hủy trước và delete &child (vùng nhớ trên stack) -> crash
```

:::warning Không để đối tượng con nằm trên stack
Khi cha bị hủy, nó gọi `delete` lên từng con. Nếu con nằm trên stack, lệnh `delete` đó nhắm vào vùng nhớ không cấp phát bằng `new` và chương trình abort. Đối tượng con luôn tạo bằng `new`.
:::

## Tự xóa một đối tượng con

Có thể `delete` một đối tượng con bất cứ lúc nào. Destructor của `QObject` tự gỡ nó khỏi danh sách con của cha, nên cha sẽ không xóa lại lần nữa.

Có một trường hợp không được `delete` trực tiếp: đối tượng đang ở giữa việc phát signal hoặc xử lý sự kiện, ví dụ xóa chính nút bấm trong slot nối với `clicked` của nó. Code phía sau `emit` vẫn đang dùng đối tượng đó. Khi đó dùng `deleteLater()`: Qt chỉ xóa đối tượng khi quay lại **event loop**, tức vòng lặp chờ và phân phát sự kiện chạy bên trong `app.exec()`, lúc mọi slot đang chạy đã xong.

## Theo dõi đối tượng có thể bị xóa

Một con trỏ thường không biết đối tượng đã bị xóa hay chưa. `QPointer<T>` là con trỏ weak dành cho `QObject`: nó tự thành `nullptr` khi đối tượng bị xóa.

| Công cụ | Dùng khi |
|---|---|
| Parent | Đối tượng thuộc về một đối tượng khác, sống chết theo nó |
| `QPointer<T>` | Giữ con trỏ tới đối tượng mà người khác sở hữu |
| `std::unique_ptr<T>` | Đối tượng không có parent, cần một chủ sở hữu rõ ràng |
| `deleteLater()` | Cần xóa đối tượng đang được dùng trong signal/slot |

Không dùng `std::unique_ptr` cho đối tượng đã có parent. Cả hai cùng xóa một đối tượng sẽ gây crash.

## Ví dụ

Ta dựng một cây gồm các `QObject` và `TemperatureSensor` (lớp cảm biến nhiệt độ giả lập, kế thừa `QObject`), rồi quan sát điều gì xảy ra khi xóa một con, dùng `QPointer`, gọi `deleteLater()` và xóa gốc. Ví dụ là ứng dụng console.

Cây có một nhóm `cabinet` chứa hai cảm biến trong tủ điện, và một cảm biến `ambient` đo nhiệt độ môi trường gắn thẳng vào gốc:

```
panel  (QObject)
+-- cabinet  (QObject)
|   +-- motor  (TemperatureSensor)
|   +-- power  (TemperatureSensor)
+-- ambient  (TemperatureSensor)
```

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
project(parent-child VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core)

add_executable(parent-child
    temperaturesensor.h
    temperaturesensor.cpp
    main.cpp
)

target_link_libraries(parent-child PRIVATE Qt5::Core)
```
::: explain [Giải thích chi tiết]
- `set(CMAKE_AUTOMOC ON)`: `TemperatureSensor` có `Q_OBJECT` nên cần moc.
- `COMPONENTS Core`: ví dụ là ứng dụng console, chỉ cần Qt Core.
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
    // Trạng thái so với ngưỡng cảnh báo
    enum class Status { Normal, Overheat };
    Q_ENUM(Status)

    explicit TemperatureSensor(QObject *parent = nullptr);

    double temperature() const;
    double threshold() const;
    void setThreshold(double threshold);
    Status status() const;

    // Giả lập đọc một mẫu mới: mỗi lần nóng thêm 8 độ
    void readSample();

    // Gọi được theo tên lúc chạy nhờ Q_INVOKABLE
    Q_INVOKABLE void reset();

signals:
    // Phát ra khi nhiệt độ đổi
    void temperatureChanged(double temperature);

private:
    void setTemperature(double temperature);

    double m_temperature = 25.0;
    double m_threshold = 60.0;
};

#endif // TEMPERATURESENSOR_H
```
::: explain [Giải thích chi tiết]
Lớp cảm biến giả lập, kế thừa `QObject`. Với bài này, chỉ cần để ý constructor `TemperatureSensor(QObject *parent = nullptr)`: truyền cha vào đây là cảm biến được gắn vào cây. Các hàm đọc nhiệt độ không dùng tới trong ví dụ.
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
`: QObject(parent)`: constructor chuyển `parent` cho `QObject`. Chính dòng này gắn đối tượng vào danh sách con của cha.
:::

```cpp [main.cpp]
#include <QCoreApplication>
#include <QDebug>
#include <QPointer>

#include "temperaturesensor.h"

// In cây đối tượng, mỗi cấp thụt vào 2 dấu cách
void printTree(const QObject *obj, int level = 0)
{
    qDebug().noquote() << QString(level * 2, ' ') + obj->objectName()
                       << "-" << obj->metaObject()->className();
    for (const QObject *child : obj->children())
        printTree(child, level + 1);
}

// Đặt tên và in ra khi đối tượng bị xóa
void track(QObject *obj, const QString &name)
{
    obj->setObjectName(name);
    QObject::connect(obj, &QObject::destroyed, qApp, [name]() {
        qDebug() << "  deleted:" << name;
    });
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "1. Build the object tree";
    auto *panel = new QObject;                      // gốc, không có cha
    auto *cabinet = new QObject(panel);             // nhóm cảm biến trong tủ điện
    auto *motor = new TemperatureSensor(cabinet);
    auto *power = new TemperatureSensor(cabinet);
    auto *ambient = new TemperatureSensor(panel);
    track(panel, "panel");
    track(cabinet, "cabinet");
    track(motor, "motor");
    track(power, "power");
    track(ambient, "ambient");
    printTree(panel);

    qDebug() << "2. Delete a child: it leaves the tree";
    delete power;
    printTree(panel);

    qDebug() << "3. QPointer watches motor";
    QPointer<TemperatureSensor> watch(motor);
    qDebug() << "  motor alive:" << !watch.isNull();

    qDebug() << "4. deleteLater: deleted only when the event loop runs";
    ambient->deleteLater();
    qDebug() << "  ambient still in tree:" << panel->children().contains(ambient);
    // Xếp lệnh quit vào hàng đợi, chạy event loop một vòng rồi thoát
    QMetaObject::invokeMethod(&app, &QCoreApplication::quit, Qt::QueuedConnection);
    app.exec();

    qDebug() << "5. Delete the root: the whole tree goes with it";
    delete panel;
    qDebug() << "  motor alive:" << !watch.isNull();

    return 0;
}
```
::: explain [Giải thích chi tiết]
- `printTree()`: duyệt đệ quy qua `children()` để in cây. `QString(level * 2, ' ')` tạo chuỗi gồm `level * 2` dấu cách.
- `track()`: nối `destroyed` với một lambda để biết đối tượng nào bị xóa. `qApp` là con trỏ toàn cục tới đối tượng ứng dụng, dùng làm **context object**: tham số thứ ba của `connect`, kết nối tự hủy khi đối tượng này bị xóa. `qApp` sống lâu hơn mọi đối tượng trong cây nên lambda luôn chạy được.
- Bước 1: `panel` không có cha nên ta phải tự xóa nó ở bước 5. Các đối tượng khác nhận cha qua constructor.
- Bước 2: `delete power` gỡ `power` khỏi cây. Cây in ra sau đó không còn `power`.
- Bước 3: `QPointer` trỏ tới `motor`. Ta không gọi `delete motor` mà để cây xóa nó ở bước 5.
- Bước 4: sau `deleteLater()`, `ambient` vẫn còn trong cây. Chỉ khi `app.exec()` chạy event loop, `ambient` mới bị xóa. Lệnh `quit` được xếp hàng sau đó để event loop thoát ngay.
- Bước 5: `delete panel` xóa toàn bộ cây, theo thứ tự cha trước rồi tới con. `watch` tự trở thành `nullptr`.
:::
::::

### Build và chạy

```bash
cd example
cmake -S . -B build
cmake --build build
./build/parent-child
```

Kết quả:

```
1. Build the object tree
panel - QObject
  cabinet - QObject
    motor - TemperatureSensor
    power - TemperatureSensor
  ambient - TemperatureSensor
2. Delete a child: it leaves the tree
  deleted: "power"
panel - QObject
  cabinet - QObject
    motor - TemperatureSensor
  ambient - TemperatureSensor
3. QPointer watches motor
  motor alive: true
4. deleteLater: deleted only when the event loop runs
  ambient still in tree: true
  deleted: "ambient"
5. Delete the root: the whole tree goes with it
  deleted: "panel"
  deleted: "cabinet"
  deleted: "motor"
  motor alive: false
```

:::tip Kiểm tra rò rỉ bộ nhớ trên board
Cho ứng dụng chạy lâu và mở/đóng các màn hình nhiều lần, rồi theo dõi bộ nhớ bằng `grep VmRSS /proc/$(pidof <tên-app>)/status`. Nếu con số tăng đều mà không giảm, có đối tượng được tạo mà không có parent hoặc không bao giờ bị xóa.
:::
