## Qt là gì

Qt là một framework C++ đa nền tảng. Cùng một mã nguồn có thể build cho Linux desktop, Windows, Android và Linux nhúng.

Qt không chỉ là thư viện vẽ giao diện. Nó cung cấp gần như mọi thứ một ứng dụng nhúng cần: chuỗi, đa luồng, file, JSON, cổng serial, network,...Các chức năng này được chia thành module và ta chỉ khai báo những module mình dùng:

| Module | Chức năng | Ví dụ trên thiết bị |
|---|---|---|
| Qt Core | QObject, signal/slot, event loop, timer, container, file | Hẹn giờ đọc cảm biến, lưu cấu hình |
| Qt Gui | Quản lý đồ họa: cửa sổ, ảnh, font | Tự vẽ đồng hồ đo, đồ thị |
| Qt Widgets | Các thành phần giao diện build-in: button, label, table... | Màn hình HMI: button, label, slider |
| Qt Serial Port | Giao tiếp UART | Đọc module cảm biến qua UART |
| Qt Serial Bus | Modbus, CAN | Đọc thiết bị công nghiệp qua RS-485 |
| Qt Network | TCP, UDP | Gửi số liệu về máy chủ |
| Qt Test | Unit test | Kiểm tra logic mà không cần board |
| Qt Quick / QML | Giao diện khai báo, tăng tốc bằng GPU | Giao diện nhiều hiệu ứng trên board có GPU |

## Qt Widgets hay Qt Quick

Qt có hai cách làm giao diện:

| | Qt Widgets | Qt Quick (QML) |
|---|---|---|
| Ngôn ngữ | C++ | QML + C++ |
| Cách vẽ | Bằng CPU | Scene graph, thiết kế cho GPU |
| Hợp với | Board không có GPU, giao diện dạng form, bảng | Board có GPU, giao diện nhiều hiệu ứng |

Nhiều board nhúng không có GPU hoặc có GPU nhưng driver khó dùng. Ví dụ BeagleBone Black có GPU PowerVR SGX530 nhưng driver của nó phụ thuộc vào phiên bản kernel và image. Ngoài ra, màn hình nhỏ nối qua SPI không đi qua đường hiển thị của GPU trên bất kỳ board nào.

Qt Quick vẫn có backend vẽ bằng phần mềm nhưng giới hạn tính năng và không phải hướng tối ưu cho CPU yếu. Do đó giao diện chính ta dùng là Qt Widgets. Qt Quick chỉ nên chọn khi board có GPU được hỗ trợ tốt.

## Platform plugin

Qt không vẽ thẳng lên phần cứng. Giữa ứng dụng và màn hình có một lớp gọi là **QPA** (Qt Platform Abstraction). Mỗi môi trường hiển thị có một **platform plugin** riêng.

```
            Máy tính (Ubuntu)                    Board nhúng
        +---------------------+          +---------------------+
        |       Ứng dụng      |          |       Ứng dụng      |  <- cùng mã nguồn
        +---------------------+          +---------------------+
        |     Qt Widgets      |          |     Qt Widgets      |
        +---------------------+          +---------------------+
        |     Qt Gui (QPA)    |          |     Qt Gui (QPA)    |
        +---------------------+          +---------------------+
        | plugin xcb/wayland  |          |   plugin linuxfb    |  <- chỉ khác ở đây
        +---------------------+          +---------------------+
        | X11/Wayland, GPU PC |          | /dev/fbN (driver)   |
        +---------------------+          +---------------------+
                                         | Màn hình SPI, HDMI, |
                                         | LCD song song...    |
                                         +---------------------+
```

Trên board, plugin **linuxfb** vẽ vào một **framebuffer** `/dev/fbN` (N là số thứ tự: `fb0`, `fb1`...). Framebuffer là một thiết bị Linux đại diện cho vùng nhớ màn hình. Driver màn hình đọc vùng nhớ này và đẩy dữ liệu ra màn hình theo cách của phần cứng. Ví dụ với màn hình SPI dùng chip ILI9341 hoặc ST7789, driver fbtft hoặc driver DRM gửi dữ liệu qua bus SPI; với màn hình HDMI, display controller của SoC làm việc này.

Để biết board của mình có những framebuffer nào và mỗi cái ứng với màn hình nào:

```bash
ls /dev/fb*                          # các framebuffer đang có
cat /sys/class/graphics/fb*/name     # tên driver của từng framebuffer
dmesg | grep -i -E "fb|drm"          # log lúc driver màn hình khởi động
```

Điểm quan trọng: ứng dụng không biết mình đang chạy trên plugin nào. Vì vậy ta viết và thử trên máy tính trước rồi chỉ cần build lại cho board.

**Các platform plugin**

Qt5 có các plugin sau cho Linux:

| Plugin | Hiển thị qua | Khi nào dùng |
|---|---|---|
| `xcb` | X11 | Máy tính Ubuntu (phiên X11) |
| `wayland` | Wayland compositor | Máy tính hoặc board có Wayland |
| `eglfs` | OpenGL ES trực tiếp, không cần window system | Board có GPU, giao diện Qt Quick |
| `linuxfb` | Framebuffer `/dev/fbN` | Board không dùng GPU |
| `vnc` | Máy chủ VNC, xem qua mạng | Thử giao diện nhúng trên máy tính hoặc xem màn hình board từ xa |
| `offscreen` | Không hiển thị | Test tự động, chạy trên máy không có màn hình |

Plugin được chọn bằng tham số `-platform <tên>:<tham số>` hoặc biến môi trường `QT_QPA_PLATFORM`. Tham số dòng lệnh được ưu tiên hơn.

**Tham số của plugin linuxfb**

| Tham số | Ý nghĩa |
|---|---|
| `fb=/dev/fb1` | Framebuffer cần dùng. Không ghi thì Qt dùng `/dev/fb0` |
| `mmsize=57x43` | Kích thước vật lý (mm), dùng khi driver không báo hoặc báo sai |
| `offset=XxY` | Vị trí góc trên bên trái của vùng vẽ |
| `tty=/dev/ttyN` | Console ảo mà Qt chuyển sang chế độ đồ họa |
| `nographicsmodeswitch` | Không chuyển console sang chế độ đồ họa |

Các tham số nối với nhau bằng dấu `:`, ví dụ `-platform linuxfb:fb=/dev/fb1:mmsize=57x43`.

Các biến môi trường hay dùng với linuxfb:

| Biến | Tác dụng |
|---|---|
| `QT_QPA_FB_HIDECURSOR=1` | Ẩn con trỏ chuột, cần khi dùng màn hình cảm ứng |
| `QT_QPA_FB_DISABLE_INPUT=1` | Không tự dò thiết bị input |
| `QT_QPA_FONTDIR` | Thư mục chứa font, khi font không nằm ở chỗ mặc định |
| `QT_LOGGING_RULES="qt.qpa.*=true"` | In log chi tiết của tầng QPA để gỡ lỗi |

## Quy trình desktop -> board

Build và deploy chương trình lên board sau mỗi lần sửa code rất mất thời gian. Vì vậy ta làm việc theo quy trình sau:

```
  +--------------+    +---------------+    +---------------+    +---------------+
  | Viết code    | -> | Build + chạy  | -> | Cross-compile | -> | Chạy trên     |
  | (Qt Creator) |    | trên Ubuntu   |    | cho board     |    | board thật    |
  +--------------+    +---------------+    +---------------+    +---------------+
  |<-- phần lớn thời gian làm việc -->|    |<---- khi giao diện đã ổn ----->|
```

```
            ┌────────────── Cùng một source code ──────────────┐
            ▼                                                  ▼
   Build bằng compiler máy tính                  Build bằng cross-compiler (ví dụ ARM)
            │                                                  │
            ▼                                                  ▼
   Chạy và thử ngay trên máy tính               Deploy lên board
   (phần lớn thời gian làm ở đây)               để chạy thử trên phần cứng thật
```

Làm trên máy tính nhanh hơn nhiều: build vài giây, có debugger, không cần nạp file xuống board. Chỉ khi giao diện và logic đã ổn, ta mới chuyển sang board.

## Cài đặt môi trường trên Ubuntu

Chuỗi bài dùng Qt 5.15, cài từ gói của Ubuntu (Ubuntu 22.04 có Qt 5.15.3):

```bash
sudo apt install build-essential cmake qtbase5-dev qtcreator
```

- `build-essential`, `cmake`: trình biên dịch C++ và công cụ build.
- `qtbase5-dev`: thư viện và header của Qt Core, Qt Gui, Qt Widgets.
- `qtcreator`: IDE của Qt, có sẵn Qt Designer để vẽ giao diện bằng cách kéo thả.

Các module khác như Serial Port, Serial Bus...sẽ được cài thêm ở bài dùng tới chúng.

:::note Qt 5.15 và Qt 6
Qt 5.15 là bản cuối của dòng Qt 5. Chuỗi bài chỉ dùng những API không bị đánh dấu deprecated trong 5.15 nên code chuyển sang Qt 6 sau này sẽ ít phải sửa. Khi đọc tài liệu trên mạng, cần để ý: nhiều ví dụ mới viết cho Qt 6, dùng lệnh CMake như `qt_add_executable` mà Qt 5 không có.
:::

## Ví dụ

Ví dụ hiển thị dòng chữ "Hello Qt" ở giữa một cửa sổ. Phần ví dụ sau giả định board là BeagleBone Black gắn màn hình SPI ILI9341 độ phân giải 320×240.

Tạo một thư mục, ví dụ `hello-qt/` chứa hai file:

```
hello-qt/
+-- CMakeLists.txt
+-- main.cpp
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.16)
project(hello-qt VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tìm Qt 5, chỉ cần module Widgets (tự kéo theo Core và Gui)
find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(hello-qt
    main.cpp
)

target_link_libraries(hello-qt PRIVATE Qt5::Widgets)
```
::: explain [Giải thích chi tiết]
- `find_package(Qt5 REQUIRED COMPONENTS Widgets)`: tìm Qt 5 trên máy. Thiếu Qt thì CMake dừng ngay với thông báo lỗi.
- `add_executable`: lệnh CMake thông thường, tạo file thực thi `hello-qt` từ `main.cpp`.
- `target_link_libraries(... Qt5::Widgets)`: link chương trình với Qt Widgets. Qt Core và Qt Gui được kéo theo tự động, include path cũng được thêm đúng.
:::

```cpp [main.cpp]
#include <QApplication>
#include <QLabel>

constexpr int ScreenWidth = 320;
constexpr int ScreenHeight = 240;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QLabel label("Hello Qt");
    label.setFixedSize(ScreenWidth, ScreenHeight);  // cửa sổ bằng đúng màn hình
    label.setAlignment(Qt::AlignCenter);            // căn chữ ra giữa
    label.show();

    return app.exec();
}
```
::: explain [Giải thích chi tiết]
- `ScreenWidth`, `ScreenHeight`: độ phân giải khai báo ở một chỗ duy nhất. Với màn hình khác, ví dụ 480×272 hay 800×480, chỉ cần sửa hai dòng này. Trên board, xem độ phân giải thật bằng `cat /sys/class/graphics/fbN/virtual_size`.
- `QApplication app(argc, argv)`: tạo đối tượng quản lý toàn bộ ứng dụng. Mọi ứng dụng Widgets phải tạo đối tượng này đầu tiên, trước mọi widget. Nó chọn platform plugin, nạp font và đọc các tham số dòng lệnh của Qt như `-platform`.
- `QLabel label("Hello Qt")`: `QLabel` là widget tạo một nhãn chữ. Một widget không có cha sẽ tự trở thành cửa sổ nên ở đây chính nhãn chữ là cửa sổ của ứng dụng.
- `setAlignment(Qt::AlignCenter)`: căn chữ vào giữa theo cả chiều ngang và chiều dọc.
- `setFixedSize(ScreenWidth, ScreenHeight)`: khóa kích thước cửa sổ bằng đúng màn hình. Người dùng không kéo giãn được.
- `show()`: widget chỉ hiện ra khi được gọi `show()`.
- `app.exec()`: chạy **event loop**, vòng lặp chờ và xử lý sự kiện (chạm, vẽ lại, timer...). Hàm chỉ trả về khi ứng dụng thoát: trên máy tính là khi đóng cửa sổ, trên board là khi dừng chương trình bằng Ctrl+C.
:::
::::

### Build và chạy trên máy tính

```bash
cd hello-qt
cmake -S . -B build
cmake --build build
./build/hello-qt
```

Nếu Qt nằm ở vị trí khác thư mục hệ thống thì cần chỉ cho CMake biết Qt ở đâu:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=<thư-mục-cài-Qt>
```

Với Qt Creator: chọn **File -> Open File or Project**, mở `CMakeLists.txt`, chọn kit Qt 5.15 rồi bấm **Run**.

:::tip Xem đúng kích thước thật trên màn hình máy tính
Qt 5 mặc định không phóng to giao diện theo DPI nên cửa sổ trên máy tính có đúng số điểm ảnh đã khai báo như trên board. Đặt màn hình máy tính sát cạnh board thật sẽ thấy ngay chữ có quá nhỏ hay không. Nếu cửa sổ lại to hơn, kiểm tra `env | grep QT_`: các biến như `QT_SCALE_FACTOR` hay `QT_AUTO_SCREEN_SCALE_FACTOR` sẽ bật phóng to.
:::

### Chạy trên board

Chạy ứng dụng với plugin linuxfb, chỉ rõ framebuffer của ILI9341:

```bash
./hello-qt -platform linuxfb:fb=/dev/fb1
```

Hoặc đặt bằng biến môi trường, tiện khi viết script tự khởi động ứng dụng:

```bash
export QT_QPA_PLATFORM=linuxfb:fb=/dev/fb1
./hello-qt
```

Chữ "Hello Qt" hiện ở giữa màn hình ILI9341.

Với board hoặc màn hình khác, cách làm giống hệt, chỉ cần thay `/dev/fb1` bằng framebuffer của màn hình mình dùng.

:::warning Số thứ tự framebuffer phụ thuộc board và image
Thứ tự `fb0`, `fb1`... do thứ tự nạp driver quyết định, có thể khác giữa các board, các image, thậm chí giữa hai phiên bản kernel. Luôn kiểm tra bằng `cat /sys/class/graphics/fb*/name` trước khi chạy. Chọn nhầm framebuffer thì ứng dụng vẫn chạy nhưng màn hình mong muốn không hiện gì.
:::
