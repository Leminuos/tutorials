## Vì sao có Qt Quick, và vì sao để tới phần tùy chọn

Qt Widgets mô tả giao diện bằng C++: tạo đối tượng, đặt thuộc tính, nối signal. **Qt Quick** mô tả giao diện bằng ngôn ngữ **QML**: ta viết "màn hình có những gì và chúng liên hệ với nhau thế nào", Qt lo phần còn lại. Qt Quick mạnh ở giao diện động: chuyển trang có hiệu ứng, danh sách cuộn mượt, thành phần co giãn theo nội dung.

Qt Quick vẽ bằng **scene graph**, được thiết kế cho GPU. Trên board có GPU (i.MX6, i.MX8, Raspberry Pi, AM57x...), với plugin `eglfs`, giao diện Qt Quick mượt hơn Widgets. Trên BBB + ILI9341 không dùng GPU, Qt Quick phải chạy bằng **backend phần mềm** (software adaptation): chạy được, nhưng chậm hơn và thiếu một số tính năng như shader effect. Vì vậy với BBB + ILI9341, Widgets là lựa chọn chính; Qt Quick đáng chọn khi dự án chuyển sang board có GPU.

Ví dụ trong bài vẫn giữ khung 320×240 và chạy được trên BBB bằng backend phần mềm, để dễ so sánh với cách làm bằng Widgets.

Cài module trên Ubuntu:

```bash
sudo apt install qtdeclarative5-dev qtquickcontrols2-5-dev \
    qml-module-qtquick2 qml-module-qtquick-window2 qml-module-qtquick-controls2 \
    qml-module-qtquick-layouts
```

## Cú pháp QML

```qml
import QtQuick 2.15                // module và phiên bản

Rectangle {                        // tạo một đối tượng kiểu Rectangle
    id: box                        // tên để tham chiếu trong cùng file
    width: 100; height: 40         // thuộc tính
    color: "green"

    property int count: 0          // thuộc tính tự khai báo
    signal pressed()               // signal tự khai báo

    Text {                         // đối tượng con: nằm trong box
        text: box.count            // binding tới thuộc tính của box
        anchors.centerIn: parent
    }
}
```

| Khái niệm | Ví dụ | Ghi chú |
|---|---|---|
| Kiểu đối tượng | `Rectangle { }` | Tên kiểu viết hoa chữ đầu |
| `id` | `id: box` | Duy nhất trong file, dùng thay cho con trỏ |
| Thuộc tính | `width: 100` | Kiểu có sẵn: `int`, `real`, `bool`, `string`, `color`... |
| Thuộc tính tự khai báo | `property int count: 0` | Giống `Q_PROPERTY` của C++, tự có signal `countChanged` |
| Signal handler | `onClicked: ...` | Tên là `on` + tên signal viết hoa chữ đầu |
| Biểu thức JavaScript | `count >= 10 ? "đỏ" : "trắng"` | Dùng trong binding và handler |

QML trong Qt 5 luôn cần số phiên bản sau tên module khi `import`. Với Qt 5.15, `QtQuick 2.15` và `QtQuick.Window 2.15` là phiên bản mới nhất.

## Property binding

Đây là khái niệm quan trọng nhất của QML:

```qml
text: window.count
color: window.overLimit ? "#ff3b30" : "white"
```

Vế phải không phải một giá trị được gán một lần mà là một **binding**: QML ghi nhớ biểu thức, theo dõi mọi thuộc tính xuất hiện trong đó, và tự tính lại mỗi khi chúng đổi. Trong ví dụ, chỉ cần `count` đổi, số hiển thị, màu chữ và dòng trạng thái đều tự cập nhật; không có dòng `setText()` nào.

So với Widgets:

```
Widgets:          valueChanged --connect--> setNum()        <- tự nối từng chỗ
QML:              text: window.count                          <- khai báo quan hệ
```

:::warning Gán bằng JavaScript làm mất binding
Trong handler, câu lệnh `valueText.text = "abc"` **thay thế** binding bằng một giá trị cố định. Từ đó về sau, `text` không còn tự cập nhật theo `count` nữa. Muốn thay đổi thứ đang hiển thị, hãy đổi dữ liệu gốc (`window.count = 0`), để binding tự làm phần còn lại.
:::

## Các kiểu cơ bản của Qt Quick

| Kiểu | Dùng cho |
|---|---|
| `Item` | Đối tượng hình học trống, dùng để nhóm |
| `Rectangle` | Hình chữ nhật có màu, viền, bo góc |
| `Text` | Chữ |
| `Image` | Ảnh từ file hoặc resource |
| `MouseArea` | Vùng nhận chạm/chuột, có signal `clicked`, thuộc tính `pressed` |
| `Row`, `Column`, `Grid` | Xếp các con theo hàng, cột, lưới |
| `Window` (module `QtQuick.Window`) | Cửa sổ gốc |

Vị trí được đặt bằng `x`, `y`, hoặc bằng **anchors**: gắn cạnh của đối tượng vào cạnh của đối tượng khác, ví dụ `anchors.bottom: parent.bottom`, `anchors.centerIn: parent`. Với bố cục co giãn phức tạp hơn, Qt Quick còn có module `QtQuick.Layouts` (`RowLayout`, `ColumnLayout`, `GridLayout`).

## Component: tách QML ra file

Mỗi file `.qml` có tên viết hoa chữ đầu là một **kiểu mới**. File `SimpleButton.qml` nằm cùng thư mục với `main.qml` cho phép viết `SimpleButton { }` trong `main.qml`. Đây là cách dùng lại giao diện, tương tự tự viết một lớp widget con trong C++.

Trong component, `property alias text: label.text` đưa một thuộc tính bên trong ra ngoài, để nơi dùng đặt được chữ của nút mà không cần biết bên trong có `Text` tên `label`.

## C++ nạp QML thế nào

```
main.cpp: QGuiApplication + QQmlApplicationEngine
                | load("qrc:/main.qml")
                v
        QML engine: đọc file, tạo đối tượng, dựng binding
                |
                v
        QQuickWindow --> scene graph --> OpenGL (GPU) hoặc backend phần mềm (CPU)
```

File QML thường được nhúng bằng Qt Resource: liệt kê trong file `.qrc`, bật `CMAKE_AUTORCC`, rồi mở bằng đường dẫn `qrc:/...`. Nhờ vậy chương trình không phụ thuộc file rời trên đĩa.

:::note Backend phần mềm trên BBB
Khi platform plugin không hỗ trợ OpenGL (như `linuxfb`), Qt Quick tự chọn backend phần mềm. Có thể chỉ định rõ bằng biến `QT_QUICK_BACKEND=software`. Backend này không hỗ trợ `ShaderEffect`, hệ thống hạt (particles) và phần lớn hiệu ứng của `QtGraphicalEffects`; dùng các tính năng đó thì phần tương ứng không hiện ra.
:::

## Ví dụ

Màn hình bộ đếm viết hoàn toàn bằng QML: số lớn chuyển màu đỏ khi vượt ±10, dòng trạng thái, ba nút "Step", "Mode", "Reset". Nút là component tự làm `SimpleButton`. Phần C++ chỉ có vài dòng để nạp QML.

```
+------------------------------+
|                              |
|              3               |
|            Normal            |
|                              |
|[ Step ][ Mode: up ][ Reset  ]|
+------------------------------+
```

```
example/
+-- CMakeLists.txt
+-- main.cpp
+-- qml.qrc
+-- main.qml
+-- SimpleButton.qml
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.16)
project(qml-basics VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Quick)

add_executable(qml-basics
    main.cpp
    qml.qrc
)

target_link_libraries(qml-basics PRIVATE Qt5::Quick)
install(TARGETS qml-basics RUNTIME DESTINATION bin)
```
::: explain [Giải thích chi tiết]
- `COMPONENTS Quick` và `Qt5::Quick`: module Qt Quick, tự kéo theo Qt QML.
- Không cần `CMAKE_AUTOMOC` vì không có lớp C++ nào dùng `Q_OBJECT`.
:::

```xml [qml.qrc]
<RCC>
    <qresource prefix="/">
        <file>main.qml</file>
        <file>SimpleButton.qml</file>
    </qresource>
</RCC>
```
::: explain [Giải thích chi tiết]
- `prefix="/"`: file `main.qml` trong resource có đường dẫn `qrc:/main.qml`, chính là đường dẫn `main.cpp` truyền cho `engine.load()`.
- Cả `main.qml` và `SimpleButton.qml` phải được liệt kê ở đây; quên `SimpleButton.qml` thì `main.qml` không dùng được kiểu `SimpleButton`.
:::

```qml [SimpleButton.qml]
import QtQuick 2.15

// Nút bấm tự làm từ Rectangle + Text + MouseArea
Rectangle {
    id: root

    // Thuộc tính và signal mà nơi dùng nút có thể đặt/nối
    property alias text: label.text
    signal clicked()

    width: 96
    height: 48
    radius: 4
    color: mouseArea.pressed ? "#3d8bfd" : "#3a4148"   // đổi màu khi đang nhấn

    Text {
        id: label
        anchors.centerIn: parent
        color: "white"
        font.pixelSize: 16
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
```
::: explain [Giải thích chi tiết]
- `property alias text: label.text`: thuộc tính `text` của nút chính là `text` của nhãn bên trong.
- `signal clicked()`: signal tự khai báo; `MouseArea` phát lại signal này khi được chạm.
- `color: mouseArea.pressed ? ... : ...`: binding theo trạng thái nhấn, nút đổi màu ngay khi chạm mà không cần viết handler.
:::

```qml [main.qml]
import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    id: window
    width: 320
    height: 240
    visible: true
    title: "QML Counter"
    color: "#202428"

    // Trạng thái của màn hình, khai báo như thuộc tính
    property int count: 0
    property bool countingUp: true
    readonly property bool overLimit: count >= 10 || count <= -10

    Text {
        id: valueText
        anchors.horizontalCenter: parent.horizontalCenter
        y: 20
        text: window.count                              // binding: count đổi thì chữ tự đổi
        color: window.overLimit ? "#ff3b30" : "white"   // binding có điều kiện
        font.pixelSize: 64
        font.bold: true
    }

    Text {
        anchors.top: valueText.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        text: window.overLimit ? "Over limit!" : "Normal"
        color: "#e0e0e0"
        font.pixelSize: 16
    }

    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6

        SimpleButton {
            text: "Step"
            onClicked: window.count += window.countingUp ? 1 : -1
        }
        SimpleButton {
            text: window.countingUp ? "Mode: up" : "Mode: down"
            onClicked: window.countingUp = !window.countingUp
        }
        SimpleButton {
            text: "Reset"
            onClicked: window.count = 0
        }
    }
}
```
::: explain [Giải thích chi tiết]
- `property int count: 0`, `property bool countingUp`: trạng thái của màn hình, khai báo ngay trong QML. Với logic lớn hơn, trạng thái nên nằm trong một lớp C++ và được đưa vào QML (Bài 32).
- `readonly property bool overLimit: ...`: thuộc tính tính từ `count`, chỉ đọc.
- `text: window.count` và `color: ...`: binding; không có code cập nhật giao diện nào.
- `Row`: xếp ba nút theo hàng, cách nhau 6 px; `anchors` đặt hàng nút sát đáy và giữa màn hình.
- `onClicked: window.count += ...`: handler chỉ đổi dữ liệu, giao diện tự theo.
:::

```cpp [main.cpp]
#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    // Qt Quick chỉ cần QGuiApplication, không cần QApplication của Widgets
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    engine.load(QUrl("qrc:/main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;   // file QML lỗi: thông báo lỗi đã được in ra console

    return app.exec();
}
```
::: explain [Giải thích chi tiết]
- `QGuiApplication`: ứng dụng Qt Quick không cần module Widgets.
- `engine.load(QUrl("qrc:/main.qml"))`: đường dẫn `qrc:/` trỏ vào resource. Nếu QML có lỗi, `rootObjects()` rỗng và lỗi đã được in ra console.
:::
::::

### Build và chạy

```bash
cd example
cmake -S . -B build
cmake --build build
./build/qml-basics
```

Hành vi đã được kiểm tra tự động: bấm "Step" 11 lần thì số thành 11, màu đỏ, trạng thái "Over limit!"; đổi chiều rồi bấm "Step" thì còn 10; "Reset" về 0 và "Normal".

### Chạy trên board

```bash
QT_QUICK_BACKEND=software ./qml-basics -platform linuxfb:fb=/dev/fb1
```

Trên board có GPU, dùng plugin `eglfs` và bỏ `QT_QUICK_BACKEND`.

## Lỗi thường gặp

**Log hiện `module "QtQuick.Controls" version 2.xx is not installed`, cửa sổ không hiện**

Module QML chưa được cài, hoặc phiên bản trong `import` cao hơn phiên bản có sẵn. Cài gói `qml-module-...` tương ứng (hoặc thêm vào image Yocto/Buildroot), và dùng phiên bản không vượt quá Qt đang có (với Qt 5.15 là 2.15).

**Log hiện `Cannot assign to non-existent property "colr"`**

Gõ sai tên thuộc tính, hoặc kiểu đó không có thuộc tính này. QML chỉ phát hiện khi nạp file, không phải lúc build C++. Chạy ứng dụng và đọc log sau mỗi lần sửa QML.

**Log hiện `ReferenceError: cout is not defined`**

Biểu thức tham chiếu tới một `id` hoặc thuộc tính không tồn tại trong phạm vi hiện tại. Kiểm tra chính tả, và nhớ rằng `id` chỉ có tác dụng trong cùng file QML.

**Chữ không còn tự cập nhật sau khi bấm một nút**

Handler đã gán thẳng giá trị cho thuộc tính đang có binding, làm mất binding. Đổi dữ liệu gốc thay vì đổi thuộc tính hiển thị.

**Chương trình thoát ngay với mã lỗi 1, không có cửa sổ**

`engine.rootObjects()` rỗng vì `main.qml` không nạp được: sai đường dẫn `qrc:/`, quên liệt kê file trong `.qrc`, hoặc lỗi cú pháp QML. Lỗi cụ thể được in ra console ngay trước đó.

**Trên board, cửa sổ Qt Quick đen hoặc không hiện, log nhắc tới OpenGL hoặc EGL**

Qt Quick đang cố dùng OpenGL trên board không có driver GPU phù hợp. Đặt `QT_QUICK_BACKEND=software`, và dùng plugin `linuxfb` thay vì `eglfs`.
