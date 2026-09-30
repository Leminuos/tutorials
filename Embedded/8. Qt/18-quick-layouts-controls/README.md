## Vì sao cần layout và Controls

Chỉ với QtQuick cơ bản, ta tự làm nút bằng `Rectangle` + `MouseArea` và đặt vị trí bằng `anchors` (gắn cạnh đối tượng vào cạnh đối tượng khác). Cách đó ổn cho một nút, nhưng một màn hình thật cần thanh trượt, công tắc, thanh tiến trình, thanh chọn trang, và cần sắp xếp chúng gọn gàng, co giãn theo chỗ trống như `QVBoxLayout` của Qt Widgets. Qt Quick có hai module cho việc này:

| Module | Import | Nội dung |
|---|---|---|
| Qt Quick Layouts | `import QtQuick.Layouts 1.15` | `RowLayout`, `ColumnLayout`, `GridLayout`, `StackLayout` |
| Qt Quick Controls 2 | `import QtQuick.Controls 2.15` | `Button`, `Slider`, `Switch`, `ProgressBar`, `Label`, `TabBar`, `ApplicationWindow`... |

## Bốn cách đặt vị trí

| Cách | Ví dụ | Dùng khi |
|---|---|---|
| Tọa độ | `x: 10; y: 20` | Vị trí cố định, hiếm khi cần |
| Anchors | `anchors.bottom: parent.bottom` | Gắn một đối tượng vào cạnh của đối tượng khác |
| Positioner | `Row`, `Column`, `Grid` | Xếp liên tiếp, không co giãn |
| Layout | `RowLayout`, `ColumnLayout`, `GridLayout` | Xếp và **co giãn** theo chỗ trống, như layout của Widgets |

Trong layout, mỗi con dùng các **attached property** `Layout.*` để nói nó muốn chiếm chỗ thế nào:

| Thuộc tính | Tương đương ở Widgets |
|---|---|
| `Layout.fillWidth: true`, `Layout.fillHeight: true` | Size policy `Expanding` |
| `Layout.preferredWidth`, `Layout.minimumWidth`, `Layout.maximumWidth` | `sizeHint`, `setMinimumWidth`, `setMaximumWidth` |
| `Layout.alignment: Qt.AlignHCenter` | Căn lề trong ô của layout |
| `Item { Layout.fillWidth: true }` | `addStretch()` |

`StackLayout` giữ nhiều con nhưng chỉ hiện con có chỉ số `currentIndex`, tương đương `QStackedWidget` của Widgets.

## Qt Quick Controls 2

| Control | Thuộc tính / signal chính | Tương đương Widgets |
|---|---|---|
| `Label` | `text`, `font`, `wrapMode` | `QLabel` |
| `Button` | `text`, `autoRepeat`, `onClicked` | `QPushButton` |
| `Slider` | `from`, `to`, `stepSize`, `value`, `onMoved` | `QSlider` |
| `Switch` | `checked`, `onToggled` | `QPushButton` checkable |
| `ProgressBar` | `from`, `to`, `value` | `QProgressBar` |
| `TabBar` + `TabButton` | `currentIndex` | `QButtonGroup` với các nút checkable |
| `ApplicationWindow` | `header`, `footer` | Cửa sổ chính có sẵn chỗ cho thanh trên và dưới |

## Hai chiều: control và dữ liệu

Mẫu nên dùng để nối một control với dữ liệu của ứng dụng:

```qml
Slider {
    value: root.setpoint                   // dữ liệu -> control (binding)
    onMoved: root.setpoint = value         // người dùng -> dữ liệu
}
```

`onMoved` chỉ phát khi **người dùng** kéo, không phát khi `value` đổi do binding. Nhờ vậy dữ liệu có một nguồn duy nhất (`root.setpoint`), và mọi nơi khác (nút "+", trang "Overview") cùng đọc từ đó. Tương tự, `Switch` dùng `onToggled`, `Button` dùng `onClicked`.

Nếu dùng `onValueChanged` thay cho `onMoved`, handler cũng chạy khi giá trị đổi do nơi khác cập nhật. Với dữ liệu đi xuống phần cứng, điều đó có thể gửi lệnh thừa.

## Style của Controls và hiệu năng

Qt Quick Controls 2 trong Qt 5.15 có các style `Default`, `Fusion`, `Imagine`, `Material`, `Universal`. Chọn style bằng `QQuickStyle::setStyle()` trong `main()`, bằng biến `QT_QUICK_CONTROLS_STYLE`, hoặc file cấu hình `qtquickcontrols2.conf` trong resource.

:::warning Chọn style nhẹ khi vẽ bằng CPU
Style `Material` và `Universal` có bóng đổ, hiệu ứng gợn sóng khi chạm và các animation chuyển trạng thái. Với GPU, chúng không đáng kể; với backend phần mềm trên BBB, mỗi hiệu ứng là thêm nhiều khung hình phải vẽ bằng CPU. Tương tự, `SwipeView` trượt trang bằng animation, còn `StackLayout` đổi trang tức thì. Trên board không dùng GPU, chọn style `Default` và `StackLayout`.
:::

## Ví dụ

Ứng dụng 3 trang dùng chung một giá trị đặt 0–100:

- **Overview**: giá trị đặt dạng số lớn và `ProgressBar`.
- **Control**: `Slider` và hai nút "−"/"+" có `autoRepeat`.
- **Settings**: `Switch` bật chế độ tự động, dòng mô tả đổi theo trạng thái.

Thanh `TabBar` nằm ở `footer` của `ApplicationWindow`, chọn trang cho `StackLayout`.

```
 Trang Control
+------------------------------+
|             43 %             |
| ==============o============= |
| [  −  ]              [  +  ] |
|                              |
| [Overview][Control][Settings]|
+------------------------------+
```

```
example/
+-- CMakeLists.txt
+-- main.cpp
+-- qml.qrc
+-- main.qml
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.16)
project(quick-controls VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Quick QuickControls2)

add_executable(quick-controls
    main.cpp
    qml.qrc
)

target_link_libraries(quick-controls PRIVATE Qt5::Quick Qt5::QuickControls2)
install(TARGETS quick-controls RUNTIME DESTINATION bin)
```
::: explain [Giải thích chi tiết]
- `QuickControls2` và `Qt5::QuickControls2`: cần cho `QQuickStyle` trong C++. Chỉ dùng Controls trong QML thì không bắt buộc liên kết, nhưng module QML `QtQuick.Controls` vẫn phải có trên máy chạy.
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

ApplicationWindow {
    id: root
    width: 320
    height: 240
    visible: true
    title: "Quick Controls"
    font.pixelSize: 14

    // Dữ liệu dùng chung cho cả 3 trang: một nguồn duy nhất, các trang chỉ đọc/ghi vào đây
    property int setpoint: 40
    property bool autoMode: false

    // Thanh chọn trang ở dưới cùng
    footer: TabBar {
        id: tabBar
        TabButton { text: "Overview" }
        TabButton { text: "Control" }
        TabButton { text: "Settings" }
    }

    // Chỉ hiện trang có chỉ số bằng tab đang chọn, chuyển trang không có animation
    StackLayout {
        anchors.fill: parent
        anchors.margins: 6
        currentIndex: tabBar.currentIndex

        // Trang 1: Tổng quan
        ColumnLayout {
            Label { text: "Power setpoint" }
            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: root.setpoint + " %"
                font.pixelSize: 48
                font.bold: true
            }
            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 100
                value: root.setpoint
            }
        }

        // Trang 2: Điều khiển
        ColumnLayout {
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: root.setpoint + " %"
                font.pixelSize: 32
            }
            Slider {
                id: slider
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 1
                value: root.setpoint                         // hiển thị theo dữ liệu
                onMoved: root.setpoint = Math.round(value)   // người dùng kéo: cập nhật dữ liệu
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: "−"
                    autoRepeat: true                          // giữ tay: lặp lại
                    Layout.preferredWidth: 72
                    onClicked: root.setpoint = Math.max(0, root.setpoint - 1)
                }
                Item { Layout.fillWidth: true }               // khoảng trống co giãn
                Button {
                    text: "+"
                    autoRepeat: true
                    Layout.preferredWidth: 72
                    onClicked: root.setpoint = Math.min(100, root.setpoint + 1)
                }
            }
        }

        // Trang 3: Cài đặt
        ColumnLayout {
            Switch {
                text: "Auto mode"
                checked: root.autoMode
                onToggled: root.autoMode = checked
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: root.autoMode ? "The setpoint follows the sensor."
                                    : "The setpoint is set by hand on the Control page."
            }
            Item { Layout.fillHeight: true }
        }
    }
}
```
::: explain [Giải thích chi tiết]
- `ApplicationWindow`: cửa sổ của Controls, có `footer` đặt sẵn ở đáy. `font.pixelSize: 14` áp cho mọi control con; cỡ theo pixel không đổi theo DPI mà driver màn hình báo.
- `property int setpoint`: một nguồn dữ liệu duy nhất cho cả ba trang.
- `StackLayout { currentIndex: tabBar.currentIndex }`: binding giữa thanh tab và trang đang hiện, không cần handler nào.
- `Label { Layout.fillWidth: true; Layout.fillHeight: true }` ở trang 1: số lớn chiếm toàn bộ chỗ trống còn lại.
- `Slider`: mẫu `value` + `onMoved`. `Math.round(value)` giữ `setpoint` là số nguyên.
- `Button { autoRepeat: true }`: giữ tay trên nút thì `clicked` phát liên tục. `Math.max`, `Math.min` giới hạn 0–100.
- `Item { Layout.fillWidth: true }`: khoảng trống co giãn đẩy hai nút ra hai đầu.
- `Switch { checked: root.autoMode; onToggled: ... }`: cùng mẫu hai chiều như `Slider`.
:::

```cpp [main.cpp]
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Style "Default" nhẹ nhất: không bóng đổ, không hiệu ứng gợn sóng
    QQuickStyle::setStyle("Default");

    QQmlApplicationEngine engine;
    engine.load(QUrl("qrc:/main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
```
::: explain [Giải thích chi tiết]
- `QQuickStyle::setStyle("Default")` phải gọi **trước** `engine.load()`.
:::
::::

### Build và chạy

```bash
cd example
cmake -S . -B build
cmake --build build
./build/quick-controls
```

Đã kiểm tra tự động: bấm "+" 3 lần thì giá trị, `Slider` và `ProgressBar` cùng thành 43; kéo slider tới 70 rồi bấm "+" thì giá trị thành 71 và slider theo đúng giá trị mới, tức binding của slider không bị mất.

### Chạy trên board

```bash
QT_QUICK_BACKEND=software ./quick-controls -platform linuxfb:fb=/dev/fb1
```

Nếu có một ứng dụng Widgets tương đương, chạy cả hai trên cùng board và so sánh: dùng `top` để xem CPU khi kéo slider, và đo thời gian từ lúc chạy tới lúc giao diện hiện ra.

## Lỗi thường gặp

**Log hiện `ERROR: QQuickStyle::setStyle() must be called before loading QML that imports Qt Quick Controls 2.`**

Đặt style sau khi đã nạp QML. Chuyển `QQuickStyle::setStyle()` lên trước `engine.load()`.

**Log hiện `Detected anchors on an item that is managed by a layout. This is undefined behavior; use Layout.alignment instead.`**

Một con của `RowLayout`/`ColumnLayout` dùng `anchors`. Layout tự quản lý vị trí; dùng `Layout.alignment` và các thuộc tính `Layout.*` khác.

**Control trong layout nhỏ xíu hoặc không giãn ra**

Mặc định control chỉ lấy kích thước tự nhiên (implicit size). Thêm `Layout.fillWidth: true` hoặc `Layout.preferredWidth`.

**Kéo slider xong thì nút "+" không còn làm slider di chuyển**

Handler đã gán thẳng giá trị cho `slider.value` (hoặc dùng `onValueChanged` để gán ngược), làm mất binding `value: root.setpoint`: gán bằng JavaScript thay binding bằng giá trị cố định. Dùng mẫu `value: dữ liệu` + `onMoved: dữ liệu = value`.

**Giao diện rất chậm trên board, nhất là khi chạm vào control**

Đang dùng style có hiệu ứng (`Material`, `Universal`) hoặc `SwipeView` với backend phần mềm. Chuyển sang style `Default` và `StackLayout`.

**Log hiện `module "QtQuick.Layouts" ... is not installed` hoặc tương tự cho `QtQuick.Controls`**

Module QML chưa có trên máy chạy. Trên Ubuntu cài `qml-module-qtquick-layouts`, `qml-module-qtquick-controls2`; trên board, thêm các module này vào image.
