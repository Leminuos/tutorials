## Qt Designer là gì?

Ở các bài trước, ta dựng giao diện hoàn toàn bằng code: tạo từng widget, từng layout, đặt từng thuộc tính. Với màn hình vài widget thì ổn nhưng màn hình cài đặt có hàng chục ô sẽ sinh ra hàng trăm dòng code. Mỗi lần chỉnh vị trí một nút là phải build lại mới thấy kết quả.

**Qt Designer** là công cụ thiết kế giao diện bằng kéo thả: ta kéo widget vào khung, sắp xếp bằng layout, chỉnh thuộc tính trong bảng và thấy ngay kết quả. Qt Designer được tích hợp sẵn trong Qt Creator: khi mở một file `.ui`, Qt Creator tự chuyển sang chế độ Design.

Cửa sổ Designer gồm các vùng chính:

| Vùng | Chức năng |
| --- | --- |
| Widget Box | Danh sách widget để kéo vào form |
| Form | Khung giao diện đang thiết kế |
| Object Inspector | Cây cha–con của các widget và layout trong form |
| Property Editor | Bảng thuộc tính của widget đang chọn |
| Signal/Slot Editor, Action Editor | Kết nối signal–slot và quản lý action ngay trong Designer |

Object Inspector hiển thị đúng cây đối tượng đã học ở Bài 4. Đây là cách trực quan để kiểm tra widget nào là con của widget nào.

## File .ui

Kết quả thiết kế được lưu vào một file có đuôi `.ui`. Đây là một file XML mô tả cây widget, thuộc tính và layout. Nó không phải code C++.

Cấu trúc tổng thể của file `.ui`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
  <class>...</class>              <!-- tên class C++ sẽ sinh ra -->
  <widget class="QWidget" name="..."> <!-- root widget -->
    <property>...</property>
    <layout>...</layout>
  </widget>
  <customwidgets>...</customwidgets>  <!-- promoted widgets -->
  <resources>...</resources>          <!-- liên kết .qrc -->
  <connections>...</connections>      <!-- signal/slot từ Designer -->
</ui>
```

### Tạo file .ui bằng Qt Designer

1. Mở Qt Creator -> File -> New File or Project... -> Qt -> Qt Designer Form -> Choose
2. Hộp thoại Qt Designer Form xuất hiện, chọn template **Widget** vì mỗi screen là một `QWidget` độc lập.
3. Đặt tên file và lưu file `.ui` tại `src/ui/` (tên file cần trùng với tên class)
3. Tại **Property Editor** tìm `objectName` để đặt tên class (vd `MyScreen`) - tên này sẽ là class XML root, phải khớp với tên C++ class.
3. Kéo thả widget từ **Widget Box** vào canvas, bố trí bằng layout (`QVBoxLayout`, `QHBoxLayout`, `QGridLayout`) - luôn dùng layout, không bao giờ đặt widget bằng toạ độ tuyệt đối.
4. Đặt `objectName` cho mọi widget cần truy cập từ C++ (vd `tempLabel`, `okButton`).
5. Lưu vào `src/ui/MyScreen.ui`.

### Từ file .ui thành code: công cụ uic

Trước khi biên dịch, công cụ **uic** (User Interface Compiler) đọc file và sinh ra một file header C++ chứa code tạo giao diện:

```
mainscreen.ui --> uic --> ui_mainscreen.h
                                | #include
                                v
                          mainscreen.cpp --> g++ --> counterpanel.o
```

Với CMake, chỉ cần `set(CMAKE_AUTOUIC ON)` và liệt kê file `.ui` trong `add_executable`. CMake tự chạy uic, giống cách `CMAKE_AUTOMOC` tự chạy moc cho các lớp có `Q_OBJECT`.

Điểm quan trọng với thiết bị nhúng: file `.ui` được chuyển thành code C++ ngay lúc build. Khi chạy trên BBB, chương trình không phải đọc hay phân tích XML nào cả, nên giao diện thiết kế bằng Designer chạy nhanh đúng bằng giao diện viết tay. File `.ui` cũng không cần chép lên board.

### Bên trong file ui_xxx.h

Code do uic sinh ra có dạng rút gọn như sau:

```cpp
namespace Ui {
class MainScreen
{
public:
    QVBoxLayout *mainLayout;
    QLabel *titleLabel;
    QPushButton *startButton;
    QPushButton *stopButton;
    // ... mỗi widget, layout trong form là một biến thành viên

    void setupUi(QWidget *MainScreen)
    {
        mainLayout = new QVBoxLayout(MainScreen);
        titleLabel = new QLabel(MainScreen);
        startButton = new QPushButton(MainScreen);
        startButton->setMinimumSize(QSize(0, 44));
        // ... tạo và sắp xếp toàn bộ giao diện
        retranslateUi(MainScreen);
    }

    void retranslateUi(QWidget *MainScreen)
    {
        startButton->setText(QCoreApplication::translate("MainScreen", "Chạy"));
        // ... đặt lại mọi chữ hiển thị
    }
};
}
```

Có thể rút ra vài điều:
- Lớp sinh ra nằm trong namespace Ui và có tên trùng với `objectName` của form (widget gốc trong Object Inspector).
- Mỗi widget trong form trở thành một biến thành viên, có tên đúng bằng `objectName` mà ta đặt trong Designer.
- Hàm `setupUi()` nhận vào một widget và dựng toàn bộ giao diện lên widget đó. Mọi widget con đều nhận widget này làm cha nên cơ chế tự giải phóng bộ nhớ vẫn hoạt động như bình thường.
- Hàm `retranslateUi()` trong code sinh ra đặt lại toàn bộ chữ hiển thị thông qua hệ thống dịch của Qt. Khi ứng dụng đổi ngôn ngữ lúc đang chạy, ta chỉ cần gọi `ui->retranslateUi(this)` để cập nhật toàn bộ giao diện.

Lớp `Ui::MainScreen` không phải là widget và không kế thừa QObject. Nó chỉ là một bản hướng dẫn lắp ráp kèm theo các con trỏ để ta truy cập widget.

File `ui_mainscreen.h` nằm trong thư mục build. Mở nó ra đọc là cách rất tốt để hiểu Designer đã tạo giao diện bằng code như thế nào.

### Kết hợp file .ui với lớp C++

Cách phổ biến nhất, cũng là cách Qt Creator tạo sẵn khi ta chọn **File → New File → Qt → Qt Designer Form Class**, là giữ một con trỏ tới lớp Ui:

**mainscreen.h**

```cpp
#pragma once
#include <QWidget>

namespace Ui { class MainScreen; }    // forward-declare class do uic sinh

class MainScreen : public QWidget
{
    Q_OBJECT
public:
    explicit MainScreen(QWidget *parent = nullptr);
    ~MainScreen() override;

private:
    Ui::MainScreen *ui;     // con trỏ tới UI auto-generated
};
```

**mainscreen.cpp**

```cpp
#include "mainscreen.h"
#include "ui_mainscreen.h"

MainScreen::MainScreen(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MainScreen)
{
    ui->setupUi(this);      // dựng giao diện lên chính widget này

    // Từ đây truy cập widget qua ui->...
    connect(ui->startButton, &QPushButton::clicked, this, [this]() {
        ui->stateValue->setText("Đang chạy");
    });
}

MainScreen::~MainScreen()
{
    delete ui;
}
```

Cần lưu ý dòng `delete ui` trong destructor. Vì `Ui::MainScreen` không phải `QObject` và không có cha, cơ chế cha–con không tự giải phóng nó. Ta phải tự `delete` hoặc thay con trỏ thường bằng `std::unique_ptr<Ui::MainScreen>`. Còn các widget mà `setupUi()` tạo ra thì vẫn do MainScreen quản lý như bình thường.

Ta cũng có thể khai báo `Ui::MainScreen ui`, dạng biến thành viên thay vì con trỏ. Cách này không cần `delete`, nhưng header `mainscreen.h` phải `#include "ui_mainscreen.h"`, khiến mọi file include `mainscreen.h` bị biên dịch lại mỗi khi sửa giao diện. Cách dùng con trỏ kèm khai báo trước tránh được việc đó.

### Các tag xml quan trọng

| Tag                | Ý nghĩa                                                              |
| ------------------ | -------------------------------------------------------------------- |
| `<ui>`             | Root XML.                                                            |
| `<class>`          | Tên class UI → `Ui::<class>`.                                        |
| `<widget>`         | Một widget (root hoặc con). `class=` là class Qt, `name=` là biến.   |
| `<layout>`         | Layout chứa widget con (`QVBoxLayout`, `QHBoxLayout`, `QGridLayout`).|
| `<item>`           | Một phần tử trong layout.                                            |
| `<property>`       | Thuộc tính (geometry, text, icon, styleSheet, ...).                  |
| `<spacer>`         | Khoảng trống co giãn (`QSpacerItem`).                                |
| `<customwidgets>`  | Khai báo widget được promote.                                        |
| `<customwidget>`   | Một promoted widget (`<class>`, `<extends>`, `<header>`).            |
| `<resources>`      | Liên kết file `.qrc`.                                                |
| `<connections>`    | Signal/slot tạo bằng Designer.                                       |
| `<action>`         | `QAction` (cho menu/toolbar).                                        |
| `<attribute>`      | Thuộc tính phụ của widget cha (vd tab title trong `QTabWidget`).     |
| `<addaction>`      | Gắn action vào menu/toolbar.                                         |

## Thiết kế layout trong Designer

Nguyên tắc về layout vẫn giữ nguyên, chỉ khác cách thao tác:
- Tạo layout: Khi kéo widget vào form, chúng chỉ nằm ở vị trí tuyệt đối. Phải chọn các widget cần sắp xếp rồi nhấn nút layout trên thanh công cụ (Lay Out Horizontally, Vertically, in a Grid, in a Form Layout). Designer sẽ bao chúng trong một layout.
- Layout chính của form: nhấp vào vùng trống của form rồi chọn một kiểu layout. Bước này tương đương với `new QVBoxLayout(this)`. Nếu quên bước này, các widget trong form sẽ không co giãn theo kích thước cửa sổ. Object Inspector đánh dấu form bằng một biểu tượng cấm màu đỏ để nhắc.
- Stretch: thay cho `addStretch()`, ta kéo Horizontal Spacer hoặc Vertical Spacer từ Widget Box vào layout. Hệ số giãn đặt trong thuộc tính `layoutStretch` của layout.
- margin và spacing: chọn layout trong Object Inspector rồi chỉnh `layoutLeftMargin`, `layoutSpacing`... trong Property Editor.
- Kích thước tối thiểu, size policy: chỉnh `minimumSize` và `sizePolicy` của từng widget.

Để thiết kế đúng cho màn hình ILI9341, ta đặt kích thước form là 320×240 (thuộc tính `geometry`) và luôn xem trước trong kích thước này.

Designer cũng hỗ trợ `QStackedWidget`: ta thêm và chuyển qua lại giữa các trang ngay trong Designer bằng các mũi tên nhỏ ở góc trên bên phải widget và thiết kế nội dung từng trang như một form thông thường.

:::tip Xem trước giao diện ngay trong Designer
**Form -> Preview** (Alt+Shift+R) hiện form như lúc chạy thật, không cần build. Dùng tính năng này để kiểm tra nhanh chữ có bị cắt hay không ở kích thước 320×240.
:::

## Kết nối signal và slot

Ba cách kết nối với các widget trong file `.ui`:

**1. Viết connect trong code:**

```cpp
connect(ui->startButton, &QPushButton::clicked, this, &MainScreen::startMachine);
```

Cách này tuân theo đúng những gì ta đã học, mọi kết nối nằm ở một chỗ dễ tìm.

**2. Dùng Signal/Slot Editor trong Designer:**

Phù hợp với các kết nối đơn giản giữa hai widget có sẵn, ví dụ nối `valueChanged(int)` của slider vào `display(int)` của `QLCDNumber`. Kết nối này được lưu trong file `.ui` và được tạo trong `setupUi()`.

**3. Auto connect:**

Auto connect là tính năng của Qt Designer. Nếu trong lớp có slot đặt tên theo mẫu `on_<objectName>_<signal>` thì `setupUi()` sẽ tự động nối nó.

Ví dụ nếu ta có một widget `QPushButton` đặt tên là `btnStart` trong Qt Designer với signal `clicked`, ta chỉ cần khai báo slot sau trong class, Qt sẽ tự động connect cho ta mà không cần viết `connect()` thủ công:

```cpp
private slots:
    void on_startButton_clicked();   // tự nối với ui->startButton, signal clicked
```

Khi nhấp chuột phải vào widget trong Designer và chọn `Go to slot...`, Qt Creator tạo slot theo đúng mẫu này. Cách này nhanh nhưng việc kết nối dựa trên tên dạng chuỗi: nếu sau này đổi `objectName` của nút, kết nối âm thầm biến mất, compiler không báo lỗi, chỉ có một dòng cảnh báo `No matching signal for on_startButton_clicked khi chạy`.

## Kết hợp Designer với code

Không bắt buộc phải làm tất cả bằng Designer. Cách làm thực tế là dùng Designer cho phần cố định của giao diện, còn phần thay đổi theo dữ liệu thì tạo bằng code sau `setupUi()`.

Ví dụ, số lượng card cảm biến phụ thuộc vào file cấu hình của từng máy. Ta đặt sẵn một layout trống tên `sensorGridLayout` trong Designer, rồi thêm card bằng code:

```cpp
ui->setupUi(this);

for (const SensorConfig &cfg : sensorConfigs)
    ui->sensorGridLayout->addWidget(createCard(cfg.name), row, col);
```

Tương tự, những thuộc tính Designer không hỗ trợ tốt thì đặt bằng code. Một ví dụ là cỡ chữ: bảng font trong Property Editor dùng đơn vị point trong khi với màn hình nhúng ta muốn dùng pixel. Ta có thể đặt font bằng code sau `setupUi()` hoặc gọn hơn là dùng Style Sheets với đơn vị px (sẽ được nói rõ ở bài sau).
