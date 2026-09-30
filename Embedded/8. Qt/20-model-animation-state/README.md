## Vì sao ba chủ đề này đi cùng nhau

Đây là ba thứ khiến Qt Quick khác hẳn Widgets trong cách làm giao diện động:

- **Model**: danh sách dữ liệu (cảnh báo, thông số) hiện bằng `ListView` với một mẫu hiển thị cho mỗi dòng.
- **State**: mô tả giao diện ở từng trạng thái (bình thường, cảnh báo, lỗi) thay vì viết code đổi từng thuộc tính.
- **Animation**: chuyển giữa các trạng thái một cách mượt mà.

Trên board có GPU, ba thứ này giúp giao diện sinh động mà không tốn CPU. Trên BBB với backend phần mềm, model và state vẫn rất có ích; animation thì phải dùng tiết kiệm, vì mỗi khung hình đều do CPU vẽ.

## Model trong QML

| Nguồn dữ liệu | Khai báo | Dùng khi |
|---|---|---|
| Số nguyên | `model: 5` | Lặp một số lần cố định |
| Mảng JavaScript | `model: ["A", "B"]` | Danh sách tĩnh ngắn |
| `ListModel` | `ListModel { ListElement { ... } }` | Dữ liệu nhỏ, quản lý ngay trong QML |
| Model C++ | Lớp kế thừa `QAbstractListModel` | Dữ liệu từ phần cứng, logic, số lượng lớn |

`ListView` tạo một **delegate** (mẫu hiển thị) cho mỗi dòng đang nhìn thấy, không tạo cho các dòng khuất, nên danh sách dài vẫn nhẹ. Trong delegate, dữ liệu của dòng được lấy theo **tên role**:

```
AlarmModel::data(index, Qt::DisplayRole)  <--  model.display  (trong delegate)
```

`QAbstractItemModel::roleNames()` ánh xạ số role sang tên. Mặc định `Qt::DisplayRole` có tên `display`, nên một model chỉ trả `Qt::DisplayRole`, như `AlarmModel` trong ví dụ, dùng được ngay trong QML mà không sửa dòng nào. Muốn thêm các role riêng (ví dụ `time`, `message`), override `roleNames()` trong model C++.

Model C++ phải báo mọi thay đổi: bọc việc thêm dòng giữa `beginInsertRows()`/`endInsertRows()`, phát `dataChanged()` khi một ô đổi giá trị, vì `ListView` cập nhật dựa vào chính các signal đó.

## State

Một **state** là tập hợp các thay đổi thuộc tính, có tên:

```qml
Rectangle {
    id: panel
    state: dangCanhBao ? "alarm" : "normal"     // binding chọn state
    states: [
        State { name: "normal"; PropertyChanges { target: panel; color: "green" } },
        State { name: "alarm";  PropertyChanges { target: panel; color: "red" } }
    ]
}
```

| Thành phần | Ý nghĩa |
|---|---|
| `state` | Tên state hiện tại; chuỗi rỗng là trạng thái gốc |
| `State { name }` | Định nghĩa một state |
| `PropertyChanges { target; ... }` | Các thuộc tính đổi khi vào state; tự khôi phục khi rời state |
| `when:` trong `State` | Điều kiện tự vào state, thay cho việc gán `state` |

So với cách làm ở Widgets (đổi property động rồi gọi `unpolish()`/`polish()` để style sheet áp lại), state gom toàn bộ hình thức của một trạng thái vào một chỗ, dễ đọc và dễ thêm trạng thái mới.

## Animation và Transition

| Kiểu | Tác dụng |
|---|---|
| `NumberAnimation`, `ColorAnimation` | Thay đổi dần một thuộc tính số, màu |
| `SequentialAnimation`, `ParallelAnimation` | Chạy nối tiếp hoặc song song các animation con |
| `Transition` | Animation chạy khi đổi state |
| `Behavior on <thuộc tính>` | Animation chạy mỗi khi thuộc tính đó đổi, không cần state |
| `add`, `remove`, `addDisplaced` của `ListView` | Animation khi thêm/xóa dòng |

`SequentialAnimation on opacity { loops: Animation.Infinite; ... }` gắn animation lặp vô hạn vào một thuộc tính, dùng cho đèn nhấp nháy.

:::warning Animation trên backend phần mềm
Mỗi khung hình của animation là một lần vẽ lại bằng CPU và một lần đẩy dữ liệu qua SPI tới màn hình. Giữ animation ngắn (vài trăm mili giây), trên vùng nhỏ, và **dừng hẳn** animation lặp khi không cần: gắn `running` vào điều kiện hiển thị, như chấm nhấp nháy trong ví dụ chỉ chạy khi đang cảnh báo. Một animation lặp vô hạn chạy ngầm trên đối tượng bị che vẫn làm CPU bận liên tục.
:::

## Ví dụ

Màn hình giám sát bằng QML, dùng lại hai lớp C++ không sửa gì:

| Lớp | Vai trò |
|---|---|
| `SensorBackend` | Lớp cầu nối cho QML (xây dựng ở Bài 32): property `temperature`, `limit`, `state` (`Normal`, `Alarm`, `SensorError`) có NOTIFY; tự đọc cảm biến theo timer; signal `stateChanged` |
| `AlarmModel` | `QAbstractListModel` chứa danh sách cảnh báo, mới nhất ở trên; `addAlarm(QString)` thêm một dòng kèm giờ |

Giao diện gồm:

- Bảng trạng thái có 3 state `normal`, `alarm`, `error`, mỗi state một màu và nội dung; chuyển state có animation đổi màu 300 ms.
- Chấm trắng nhấp nháy ở mép trái bảng, chỉ chạy khi đang cảnh báo.
- `ListView` hiển thị `AlarmModel`; dòng mới hiện dần, các dòng cũ trượt xuống.
- Slider đặt ngưỡng, ghi thẳng vào property `limit` của `SensorBackend`.

Việc ghi dòng cảnh báo do C++ làm (trong `main.cpp`); QML chỉ hiển thị.

```
+------------------------------+
|+----------------------------+|
|| o   Alarm 24.0 °C          ||  <- state "alarm", nền đỏ
|+----------------------------+|
| 19:52:10  Over limit 20 °C   |  <- ListView + AlarmModel
| 19:51:40  Back to normal     |
|                              |
| Limit 20 o================== |
+------------------------------+
```

```
example/
+-- CMakeLists.txt
+-- qml.qrc
+-- main.qml
+-- src/
    +-- hal.h, hardware.*, linuxhardware.*, simhardware.*   <- phần cứng
    +-- alarmmonitor.*                                      <- logic cảnh báo
    +-- alarmmodel.*                                        <- danh sách cảnh báo
    +-- sensorbackend.*                                     <- cầu nối C++/QML
    +-- main.cpp                                            <- mới
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.16)
project(qml-model-state VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Quick QuickControls2)

add_executable(qml-model-state
    src/hal.h
    src/hardware.h src/hardware.cpp
    src/linuxhardware.h src/linuxhardware.cpp
    src/simhardware.h src/simhardware.cpp
    src/alarmmonitor.h src/alarmmonitor.cpp
    src/alarmmodel.h src/alarmmodel.cpp
    src/sensorbackend.h src/sensorbackend.cpp
    src/main.cpp
    qml.qrc
)

target_include_directories(qml-model-state PRIVATE src)
target_link_libraries(qml-model-state PRIVATE Qt5::Quick Qt5::QuickControls2)
install(TARGETS qml-model-state RUNTIME DESTINATION bin)
```
::: explain [Giải thích chi tiết]
- `set(CMAKE_AUTORCC ON)` và `qml.qrc`: nhúng `main.qml` vào file chạy.
- `COMPONENTS Quick QuickControls2`: giao diện dùng `Slider` của Qt Quick Controls 2.
- Các file trong `src/` được dùng lại, không sửa gì; chỉ `src/main.cpp` là mới.
- `target_include_directories(... PRIVATE src)`: cho phép `#include "sensorbackend.h"` mà không cần ghi `src/`.
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

```qml [main.qml]
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Hmi 1.0

ApplicationWindow {
    width: 320
    height: 240
    visible: true
    title: "Model, Animation, State"
    font.pixelSize: 14

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        // Bảng trạng thái: ba State, đổi màu có animation
        Rectangle {
            id: panel
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            radius: 4

            Text {
                id: panelText
                anchors.centerIn: parent
                font.pixelSize: 20
                font.bold: true
            }

            // Chấm nhấp nháy, chỉ chạy khi cảnh báo
            Rectangle {
                id: blinker
                width: 14
                height: 14
                radius: 7
                color: "white"
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 12
                visible: panel.state === "alarm"

                SequentialAnimation on opacity {
                    running: blinker.visible
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.2; duration: 500 }
                    NumberAnimation { to: 1.0; duration: 500 }
                }
            }

            state: Sensor.state === AlarmMonitor.Alarm ? "alarm"
                 : Sensor.state === AlarmMonitor.SensorError ? "error" : "normal"

            states: [
                State {
                    name: "normal"
                    PropertyChanges { target: panel; color: "#2e7d32" }
                    PropertyChanges { target: panelText; color: "white"; text: Sensor.temperature.toFixed(1) + " °C" }
                },
                State {
                    name: "alarm"
                    PropertyChanges { target: panel; color: "#c62828" }
                    PropertyChanges { target: panelText; color: "white"; text: "Alarm " + Sensor.temperature.toFixed(1) + " °C" }
                },
                State {
                    name: "error"
                    PropertyChanges { target: panel; color: "#f9a825" }
                    PropertyChanges { target: panelText; color: "black"; text: "Sensor error" }
                }
            ]

            // Đổi màu trong 300 ms khi chuyển giữa các state
            transitions: Transition {
                ColorAnimation { duration: 300 }
            }
        }

        // Danh sách cảnh báo từ AlarmModel (C++)
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Alarms

            delegate: Text {
                width: ListView.view.width
                height: 24
                verticalAlignment: Text.AlignVCenter
                text: model.display          // Qt::DisplayRole của AlarmModel::data()
                font.pixelSize: 13
            }

            // Dòng mới hiện dần; các dòng cũ trượt xuống
            add: Transition {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 250 }
            }
            addDisplaced: Transition {
                NumberAnimation { property: "y"; duration: 150 }
            }
        }

        RowLayout {
            Label { text: "Limit " + Sensor.limit.toFixed(0) }
            Slider {
                Layout.fillWidth: true
                from: 20
                to: 40
                stepSize: 1
                value: Sensor.limit
                onMoved: Sensor.limit = value
            }
        }
    }
}
```
::: explain [Giải thích chi tiết]
- `state: Sensor.state === ... ? "alarm" : ...`: binding chọn state theo dữ liệu C++. Không có handler nào gán `state` bằng tay.
- `PropertyChanges { target: panelText; text: ... }`: thuộc tính trong `PropertyChanges` cũng là binding, nên nhiệt độ vẫn tự cập nhật khi đang ở một state.
- `transitions: Transition { ColorAnimation { duration: 300 } }`: không ghi `from`/`to` nên áp cho mọi lần đổi state; `ColorAnimation` không chỉ định thuộc tính nên áp cho mọi thuộc tính màu thay đổi.
- `SequentialAnimation on opacity { running: blinker.visible ... }`: animation chỉ chạy khi chấm đang hiện, tức chỉ khi cảnh báo.
- `model: Alarms` và `text: model.display`: đọc `Qt::DisplayRole` của `AlarmModel`.
- `width: ListView.view.width`: delegate rộng bằng danh sách; `ListView.view` là attached property trỏ tới `ListView` đang chứa delegate.
- `clip: true`: không vẽ các dòng tràn ra ngoài vùng danh sách.
- `add` và `addDisplaced`: dòng mới hiện dần từ trong suốt, các dòng cũ trượt xuống vị trí mới.
:::

```cpp [src/main.cpp]
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml>

#include "alarmmodel.h"
#include "sensorbackend.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Default");

    const bool simulate = !app.arguments().contains("--hw");
    SensorBackend sensor(simulate);   // cầu nối cảm biến cho QML
    AlarmModel alarms;                // model C++ dùng nguyên cho QML

    // Ghi lại mỗi lần trạng thái đổi; phần này làm ở C++, QML chỉ hiển thị
    QObject::connect(&sensor, &SensorBackend::stateChanged, &alarms, [&sensor, &alarms]() {
        switch (sensor.state()) {
        case AlarmMonitor::State::Alarm:
            alarms.addAlarm(QString("Over limit %1 °C").arg(sensor.limit(), 0, 'f', 0));
            break;
        case AlarmMonitor::State::SensorError:
            alarms.addAlarm("Sensor error");
            break;
        case AlarmMonitor::State::Normal:
            alarms.addAlarm("Back to normal");
            break;
        }
    });

    qmlRegisterSingletonInstance("Hmi", 1, 0, "Sensor", &sensor);
    qmlRegisterSingletonInstance("Hmi", 1, 0, "Alarms", &alarms);
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
- `AlarmModel alarms`: model C++ dùng nguyên, đăng ký bằng `qmlRegisterSingletonInstance` như backend.
- Lambda nối `SensorBackend::stateChanged` với `AlarmModel::addAlarm`: logic ghi cảnh báo nằm ở C++, QML chỉ hiển thị.
:::
::::

### Build và chạy

```bash
cd example
cmake -S . -B build
cmake --build build
./build/qml-model-state
```

Đã kiểm tra tự động: ban đầu bảng ở state `normal` màu `#2e7d32`, danh sách rỗng; kéo ngưỡng xuống 20 °C thì state thành `alarm`, màu `#c62828`, danh sách có dòng "Over limit 20 °C"; kéo lên 40 °C thì về `normal`, danh sách có thêm "Back to normal" ở đầu.

### Chạy trên board

```bash
QT_QUICK_BACKEND=software ./qml-model-state -platform linuxfb:fb=/dev/fb1
```

Theo dõi CPU bằng `top` khi đang cảnh báo (chấm nhấp nháy chạy) và khi bình thường (không có animation nào): khác biệt đó là cái giá của một animation lặp trên CPU.

:::tip Dùng Behavior cho thay đổi không qua state
Nếu một thuộc tính đổi trực tiếp do binding (không qua state), `Transition` không có tác dụng. Khi đó dùng `Behavior on color { ColorAnimation { duration: 300 } }` ngay trong đối tượng: mọi lần `color` đổi đều có animation.
:::

## Lỗi thường gặp

**`ListView` trống dù model C++ có dữ liệu**

Model không phát đúng signal khi thêm dữ liệu (thiếu `beginInsertRows`/`endInsertRows`), hoặc model chưa được đăng ký trước khi nạp QML. Kiểm tra thuộc tính `count` của `ListView`.

**Delegate hiện chữ rỗng**

Tên role trong delegate không khớp `roleNames()` của model. Với model chỉ trả `Qt::DisplayRole`, tên role là `display`. Muốn dùng tên khác, override `roleNames()`.

**Các dòng của `ListView` chồng lên nhau hoặc không thấy chữ**

Delegate không có chiều cao hoặc chiều rộng xác định. Đặt `height` cho delegate và `width: ListView.view.width`.

**Đổi state mà không có animation**

Thiếu `transitions`, hoặc thuộc tính đổi không nằm trong `PropertyChanges` của state mà đổi trực tiếp do binding. Thêm `Transition` cho state, hoặc `Behavior on <thuộc tính>` cho thay đổi trực tiếp.

**CPU trên board luôn cao dù giao diện đứng yên**

Có animation lặp vô hạn vẫn đang chạy, kể cả trên đối tượng đang ẩn. Gắn `running` của animation vào điều kiện cần hiển thị, và tránh animation lặp trên vùng lớn.

**Thuộc tính không quay về giá trị cũ khi rời state**

Giá trị đã bị gán bằng JavaScript trong lúc đang ở state, làm mất binding gốc: phép gán thay binding bằng một giá trị cố định. Để state và binding quản lý thuộc tính đó, không gán trực tiếp trong handler.
