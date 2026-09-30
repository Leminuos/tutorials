## Widget là gì

**Widget** là mọi thành phần hiển thị trên giao diện: nhãn chữ, nút bấm, thanh trượt, cho tới cả cửa sổ chính. Tất cả đều kế thừa `QWidget` mà `QWidget` lại kế thừa `QObject` nên widget có signal/slot và cây parent-child.

Mỗi widget là một vùng hình chữ nhật trên màn hình, có ba nhiệm vụ:
- Tự vẽ nội dung của mình.
- Nhận sự kiện xảy ra trong vùng của nó: chạm, nhấn phím.
- Chứa các widget con. Bất kỳ widget nào cũng có thể là một hộp chứa.

Vị trí của widget con được tính theo tọa độ của widget cha, không phải theo màn hình:
- Widget con được vẽ bên trong widget cha, tọa độ tính từ góc trên bên trái của cha.
- Widget con bị cắt nếu vượt ra ngoài cha.
- Ẩn cha thì mọi con cũng bị ẩn.

Các hàm liên quan:

| Hàm | Ý nghĩa |
| --- | ------ |
| `pos()` | Vị trí góc trên trái, trong tọa độ của cha |
| `size()` | Kích thước (width × height) |
| `geometry()` | Vị trí và kích thước, dạng `QRect` |
| `setGeometry(x, y, w, h)` | Đặt vị trí và kích thước |
| `rect()` | Vùng của chính widget, luôn bắt đầu từ (0,0) |

Các thuộc tính cơ bản của widget
- `show()` / `hide()` / `isVisible():` hiện, ẩn, kiểm tra trạng thái hiển thị.
- `setEnabled(bool)`: bật hoặc vô hiệu hóa. Widget bị vô hiệu hóa hiển thị mờ đi và không nhận thao tác. Vô hiệu hóa một widget cha sẽ vô hiệu hóa toàn bộ widget con.
- `setMinimumSize()`, `setMaximumSize()`, `setFixedSize()`, `setMinimumHeight()`...: giới hạn kích thước.
- `setFont()`: đặt font chữ.

Ngoài ra, Widget không có parent là **cửa sổ độc lập** (top-level window).

## Layout

Ta hoàn toàn có thể đặt từng widget bằng `setGeometry` với tọa độ cố định. Nhưng cách này nhanh chóng gặp vấn đề:
- Đổi font hoặc cỡ chữ, chữ tràn ra khỏi nút.
- Xoay màn hình từ 320×240 sang 240×320 hoặc chuyển sang board có màn hình khác: phải tính lại toàn bộ tọa độ.
- Thêm một nút vào giữa: phải dịch chuyển mọi widget phía sau.

**Layout** tự tính vị trí và kích thước cho các widget con dựa trên quy tắc ta đặt ra và tự tính lại khi có thay đổi.

Các loại layout:

| Layout | Cách xếp | Dùng cho |
|---|---|---|
| `QHBoxLayout` | Xếp các widget theo hàng ngang, từ trái sang phải. | Thanh nút, thanh tiêu đề |
| `QVBoxLayout` | Xếp theo cột dọc, từ trên xuống dưới. | Bố cục tổng của màn hình |
| `QGridLayout` | Xếp theo lưới hàng × cột. Một widget có thể chiếm nhiều ô. | Các ô giá trị, bàn phím số |
| `QFormLayout` | Xếp theo cặp label - input | Màn hình cài đặt |

Mỗi widget chỉ có một layout chính. Có hai cách gắn layout vào widget:

```cpp
auto *layout = new QVBoxLayout(widget);   // truyền widget vào constructor
// hoặc
auto *layout = new QVBoxLayout;
widget->setLayout(layout);
```

Giao diện thực tế luôn phức tạp hơn một hàng hay một cột nên ta lồng layout vào nhau: layout con được tạo không có cha rồi thêm vào layout mẹ bằng addLayout().

```cpp
auto *mainLayout = new QVBoxLayout(this);   // layout chính gắn vào widget

auto *buttonRow = new QHBoxLayout;          // layout con
buttonRow->addWidget(startButton);
buttonRow->addWidget(stopButton);

mainLayout->addLayout(buttonRow);           // lồng vào layout chính
```

Khi widget được thêm vào layout, nó tự trở thành con của widget sở hữu layout. Vì vậy các widget trong layout có thể tạo bằng new mà không cần truyền cha.

## Spacing và margin

Mỗi layout có hai thông số khoảng trống:

- **Contents margin (lề)**: khoảng trống giữa viền widget và nội dung bên trong.
- **Spacing**: khoảng cách giữa các widget trong layout.

```cpp
layout->setContentsMargins(4, 4, 4, 4);   // trái, trên, phải, dưới
layout->setSpacing(4);
```

Lề mặc định của layout do style quyết định, thường khoảng 9–11 px mỗi phía. Trên màn hình 1920×1080 con số này không đáng kể nhưng trên 320×240 thì hai lề trái phải đã mất khoảng 20 px. Với màn hình nhỏ, ta tự đặt lề và spacing khoảng 4 px.

## Stretch

Khi layout có nhiều chỗ hơn mức các widget cần, phần dư được chia theo hệ số giãn (stretch factor)

```cpp
layout->addWidget(title);          // stretch = 0: chỉ lấy đủ dùng
layout->addStretch();              // thêm một khoảng trống co giãn, đẩy các widget ra hai phía
layout->addWidget(clock);
```

Nếu có nhiều thành phần với stretch khác 0, phần dư được chia theo tỉ lệ. Ví dụ hai widget có stretch 1 và 2 sẽ nhận phần dư theo tỉ lệ 1:2. Với `QGridLayout`, ta dùng `setRowStretch()` và `setColumnStretch()`.

```
auto *layout = new QHBoxLayout();
layout->addWidget(btn1, 1);   // Stretch = 1
layout->addWidget(btn2, 3);   // Stretch = 3
```

Nếu layout có 400 px chiều rộng còn lại để chia:
- `btn1` nhận: 1 phần → 100 px
- `btn2` nhận: 3 phần → 300 px

## Size hint và Size policy

Layout quyết định kích thước từng widget dựa trên hai thông tin mà widget cung cấp:
- Size Hint: kích thước ưa thích, tức vừa đủ để hiển thị nội dung. Với nút bấm, đó là kích thước vừa với chữ trên nút.
- Size Policy: cách widget phản ứng khi được cho nhiều hơn hoặc ít hơn size hint, đặt riêng cho chiều ngang và chiều dọc.

| Chính sách | Ý nghĩa |
| --- | --- |
| Fixed | Luôn đúng bằng size hint, không co không giãn |
| Minimum | Không nhỏ hơn size hint, có thể lớn hơn |
| Maximum | Không lớn hơn size hint, có thể nhỏ hơn |
| Preferred | Ưa size hint, nhưng co giãn được khi cần |
| Expanding	| Muốn chiếm càng nhiều chỗ càng tốt |

Ví dụ quan trọng: `QPushButton` mặc định có chính sách `Fixed` theo chiều dọc nên nút bấm không bao giờ tự cao lên dù layout còn chỗ. Với màn hình cảm ứng, ta phải chủ động tăng chiều cao nút bằng `setMinimumHeight()` hoặc đổi chính sách:

```cpp
button->setMinimumHeight(44);
// hoặc
button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
```

## Ví dụ

Màn hình chính của một trạm giám sát: thanh tiêu đề có tên trạm và đồng hồ, lưới 4 ô giá trị, thanh 3 nút. Giá trị trong các ô tạm thời viết cố định.

```
+------------------------------+
| Station 01           18:15:39|
| +------------++------------+ |
| | Temperature||  Humidity  | |
| |  25.4 °C   ||    61 %    | |
| +------------++------------+ |
| +------------++------------+ |
| |  Pressure  ||   Speed    | |
| |  1013 hPa  ||  1450 rpm  | |
| +------------++------------+ |
|[Overview] [Trend] [Settings] |
+------------------------------+
```

```
example/
+-- CMakeLists.txt
+-- mainscreen.h
+-- mainscreen.cpp
+-- main.cpp
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.16)
project(widgets-layout VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(widgets-layout
    mainscreen.h
    mainscreen.cpp
    main.cpp
)

target_link_libraries(widgets-layout PRIVATE Qt5::Widgets)
```
::: explain [Giải thích chi tiết]
- `set(CMAKE_AUTOMOC ON)`: tự chạy moc cho các lớp có `Q_OBJECT`.
- `COMPONENTS Widgets` và `Qt5::Widgets`: ứng dụng có giao diện nên cần Qt Widgets; Qt Core và Qt Gui được kéo theo tự động.
:::

```cpp [mainscreen.h]
#ifndef MAINSCREEN_H
#define MAINSCREEN_H

#include <QWidget>

class QFrame;
class QLabel;

class MainScreen : public QWidget
{
    Q_OBJECT

public:
    explicit MainScreen(QWidget *parent = nullptr);

private:
    QFrame *createTile(const QString &title, const QString &value);
    void updateClock();

    QLabel *m_clockLabel;
};

#endif // MAINSCREEN_H
```
::: explain [Giải thích chi tiết]
- `createTile()`: hàm phụ tạo một ô giá trị, dùng lại cho cả 4 ô thay vì viết lặp.
:::

```cpp [mainscreen.cpp]
#include "mainscreen.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

MainScreen::MainScreen(QWidget *parent)
    : QWidget(parent)
    , m_clockLabel(new QLabel)
{
    setFixedSize(320, 240);

    // Thanh tiêu đề: tên trạm bên trái, đồng hồ bên phải
    auto *header = new QHBoxLayout;
    header->addWidget(new QLabel("Station 01"));
    header->addStretch();              // khoảng trống co giãn đẩy đồng hồ sang phải
    header->addWidget(m_clockLabel);

    // Vùng giữa: lưới 2x2 ô giá trị
    auto *grid = new QGridLayout;
    grid->setSpacing(4);
    grid->addWidget(createTile("Temperature", "25.4 °C"), 0, 0);
    grid->addWidget(createTile("Humidity", "61 %"), 0, 1);
    grid->addWidget(createTile("Pressure", "1013 hPa"), 1, 0);
    grid->addWidget(createTile("Speed", "1450 rpm"), 1, 1);

    // Thanh nút dưới cùng, cao cố định 44 px
    auto *footer = new QHBoxLayout;
    footer->setSpacing(4);
    for (const QString &text : {"Overview", "Trend", "Settings"}) {
        auto *button = new QPushButton(text);
        button->setFixedHeight(44);
        footer->addWidget(button);
    }

    // Ghép ba phần theo chiều dọc; vùng giữa nhận toàn bộ chỗ trống còn lại
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4); // lề mặc định quá rộng cho 320x240
    layout->setSpacing(4);
    layout->addLayout(header);
    layout->addLayout(grid, 1);             // stretch = 1
    layout->addLayout(footer);

    // Timer phát timeout mỗi giây để cập nhật đồng hồ
    auto *clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, &MainScreen::updateClock);
    clockTimer->start(1000);
    updateClock();
}

QFrame *MainScreen::createTile(const QString &title, const QString &value)
{
    auto *tile = new QFrame;
    tile->setFrameShape(QFrame::Box);

    auto *titleLabel = new QLabel(title);
    auto *valueLabel = new QLabel(value);

    // Cỡ chữ theo pixel để giống nhau trên máy tính và trên board
    QFont valueFont = valueLabel->font();
    valueFont.setPixelSize(24);
    valueFont.setBold(true);
    valueLabel->setFont(valueFont);

    titleLabel->setAlignment(Qt::AlignCenter);
    valueLabel->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(tile);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel, 1);
    return tile;
}

void MainScreen::updateClock()
{
    m_clockLabel->setText(QTime::currentTime().toString("HH:mm:ss"));
}
```
::: explain [Giải thích chi tiết]
- `header->addWidget(new QLabel("Station 01"))`: widget tạo không có parent, trở thành con của `MainScreen` khi layout được gắn vào `MainScreen`, nên được xóa cùng `MainScreen`.
- `grid->addWidget(widget, hàng, cột)`: đặt widget vào ô của lưới, đánh số từ 0.
- `button->setFixedHeight(44)`: cố định chiều cao để nút đủ lớn cho ngón tay, chiều rộng để layout tự chia.
- `new QVBoxLayout(this)`: tạo layout và gắn luôn vào `MainScreen`. Chỉ layout ngoài cùng được gắn vào widget; các layout con được thêm bằng `addLayout()`.
- `layout->addLayout(grid, 1)`: lưới có stretch 1, nhận hết chỗ trống. Header và footer có stretch mặc định 0.
- `QFrame` với `setFrameShape(QFrame::Box)`: một widget chỉ để vẽ khung viền và chứa widget khác.
- `valueFont.setPixelSize(24)`: đặt cỡ chữ theo pixel. Lý do ở khối cảnh báo bên dưới.
- `QTime::currentTime().toString("HH:mm:ss")`: giờ hiện tại dạng 24 giờ.
:::

```cpp [main.cpp]
#include <QApplication>

#include "mainscreen.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MainScreen screen;

    if (QGuiApplication::platformName() == "linuxfb") {
        // Trên board: bỏ viền và chiếm toàn màn hình 320x240
        screen.setWindowFlags(Qt::FramelessWindowHint);
        screen.showFullScreen();
    } else {
        // Trên máy tính: giả lập kích thước màn hình ILI9341
        screen.setFixedSize(320, 240);
        screen.show();
    }

    return app.exec();
}
```
::: explain [Giải thích chi tiết]
- `setWindowFlags()` phải gọi trước `show()`. Gọi sau khi cửa sổ đã hiện sẽ làm cửa sổ bị ẩn đi.
:::
::::

:::warning Cỡ chữ theo point có thể khác trên board
Cỡ chữ theo point (`setPointSize`) được đổi ra pixel dựa trên DPI của màn hình. Qt tính DPI từ kích thước vật lý mà driver báo. Driver framebuffer của màn hình SPI thường không báo đúng kích thước này, nên chữ trên board có thể to hoặc nhỏ hơn hẳn trên máy tính. Với màn hình cố định 320×240, dùng `setPixelSize` để chữ có cùng số điểm ảnh ở mọi nơi. Nếu vẫn muốn dùng point, khai báo kích thước vật lý thật của màn hình cho plugin linuxfb bằng tham số `mmsize`, ví dụ `linuxfb:fb=/dev/fb1:mmsize=57x43`.
:::

### Build và chạy

```bash
cd example
cmake -S . -B build
cmake --build build
./build/widgets-layout
```

### Chạy trên board

```bash
./widgets-layout -platform linuxfb:fb=/dev/fb1
```

