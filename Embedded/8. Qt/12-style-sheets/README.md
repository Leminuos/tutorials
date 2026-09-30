Khi chưa chỉnh gì, widget của Qt được vẽ theo một style mặc định. Trên máy tính, style này bắt chước giao diện của hệ điều hành. Trên BBB chạy `linuxfb`, Qt dùng style Fusion với tông xám trung tính. Cả hai đều được thiết kế cho ứng dụng văn phòng dùng chuột, không phù hợp với HMI: nút nhỏ, màu nhạt, khó phân biệt trạng thái từ xa.

Ở các bài trước, ta đã chỉnh giao diện bằng code: `setFont()`, `setMinimumHeight()`,...Nếu tiếp tục làm theo cách đó cho mọi widget, các thiết lập giao diện sẽ nằm rải rác khắp chương trình, rất khó thay đổi đồng bộ.

Qt Style Sheets (QSS) giải quyết vấn đề này bằng cú pháp gần giống CSS của web. Toàn bộ phần trình bày như màu sắc, font chữ, viền, kích thước tối thiểu,...được gom vào một chỗ, tách khỏi phần logic. Muốn đổi giao diện cả ứng dụng, ta chỉ sửa một file.

## Cú pháp cơ bản

Một style sheet gồm các quy tắc, mỗi quy tắc gồm selector cho biết áp dụng cho widget nào và một khối khai báo thuộc tính:

```css
QPushButton {
    background-color: #2d6cdf;
    color: white;
    font-size: 16px;
    min-height: 44px;
    border-radius: 6px;
}
```

Quy tắc trên có nghĩa: mọi `QPushButton` có nền xanh, chữ trắng, cỡ chữ 16 pixel, cao tối thiểu 44 pixel, bo góc 6 pixel. Chú thích trong QSS viết bằng `/* ... */`.

## Apply style sheet

**Cấp ứng dụng,** áp dụng cho mọi widget:

```cpp
app.setStyleSheet("QPushButton { min-height: 44px; }");
```

**Cấp widget,** áp dụng cho widget đó và toàn bộ widget con của nó:

```cpp
settingsScreen->setStyleSheet("QLabel { color: gray; }");
```

Khi cả hai cùng có quy tắc cho một widget, quy tắc gần widget hơn (cấp widget) được ưu tiên.

Cách nên dùng là đặt một style sheet duy nhất ở cấp ứng dụng và chỉ dùng cấp widget cho các trường hợp thật đặc biệt. Như vậy, mọi thiết lập giao diện đều nằm ở một nơi.

Viết style sheet dài trong chuỗi C++ rất bất tiện. Ta đặt toàn bộ QSS trong một file `.qss` riêng, nạp một lần lúc khởi động. Thay giao diện chỉ cần sửa file, không phải sửa code C++.

Để file `.qss` đi cùng chương trình, ta dùng **Qt Resource System**: file `.qrc` liệt kê các file cần nhúng; `CMAKE_AUTORCC` biên dịch chúng vào file chạy.

Trong chương trình, file nhúng được mở bằng đường dẫn bắt đầu bằng `:/`, ví dụ `":/style.qss"`. Đọc file nhúng bằng `QFile` giống hệt đọc file trên đĩa: `open()` rồi `readAll()`:

```cpp
QFile file(":/styles/light.qss");
if (file.open(QIODevice::ReadOnly))
    app.setStyleSheet(QString::fromUtf8(file.readAll()));
```

## Selector

Các selector hay dùng:

| Selector | Áp dụng cho |
|---|---|---|
| `QPushButton` | Mọi `QPushButton` và các lớp kế thừa nó |
| `.QPushButton` | Chỉ đúng lớp `QPushButton`, không gồm lớp con |
| `#startButton` | Widget có `objectName` là `startButton` |
| `QPushButton#startButton` | `QPushButton` có `objectName` là `startButton` |
| `QFrame[tile="true"]` | `QFrame` có property `tile` bằng `true` |
| `#header QLabel` | `QLabel` nằm bên trong `#header` |
| `QFrame > QLabel` | `QLabel` là con trực tiếp của một `QFrame` |
| `*` | Mọi widget |

Nhiều bộ chọn dùng chung một khối khai báo thì cách nhau bằng dấu phẩy:

```css
QSpinBox, QDoubleSpinBox, QLineEdit {
    min-height: 36px;
    font-size: 16px;
}
```

Khi nhiều quy tắc cùng áp cho một widget, quy tắc cụ thể hơn thắng: `#id` mạnh hơn property, property mạnh hơn tên lớp.

## Trạng thái (pseudo-state)

Widget có nhiều trạng thái và mỗi trạng thái có thể có giao diện riêng. Trạng thái được viết sau selector, bắt đầu bằng dấu hai chấm:

```css
QPushButton:pressed  { background-color: #1a4fa8; }   /* đang bị nhấn */
QPushButton:checked  { background-color: #2e9d4f; }   /* nút checkable đang bật */
QPushButton:disabled { background-color: #b0b0b0; color: #707070; }   /* bị vô hiệu hóa */
```

| Trạng thái | Ý nghĩa |
| --- | --- |
| `:pressed` | Đang bị nhấn giữ |
| `:checked` / `:!checked` | Đang bật / không bật (dấu `!` là phủ định) |
| `:disabled` / `:enabled` | Bị vô hiệu hóa / hoạt động |
| `:focus` | Đang giữ tiêu điểm bàn phím |
| `:hover` | Con trỏ chuột đang nằm trên widget |

Có thể ghép nhiều trạng thái: `QPushButton:checked:disabled`.

Với màn hình cảm ứng, trạng thái `:pressed` rất quan trọng: nó cho người dùng phản hồi thị giác rằng lần chạm đã được nhận. Ngược lại, không nên dùng `:hover`: cảm ứng được coi như chuột nên sau khi chạm, Qt vẫn nghĩ con trỏ đang nằm ở vị trí cuối cùng và widget đó bị kẹt ở trạng thái hover cho tới lần chạm tiếp theo ở chỗ khác.

## Thành phần con (sub-control)

Nhiều widget được ghép từ các phần nhỏ: thanh trượt có rãnh và tay nắm, thanh cuộn có tay kéo, ô đánh dấu có ô vuông. QSS cho phép định dạng từng phần, viết sau selector bằng hai dấu hai chấm `::`:

```css
/* Thanh trượt: rãnh dày hơn, tay nắm to hơn */
QSlider::groove:horizontal {
    height: 8px;
    background: #c8c8c8;
    border-radius: 4px;
}
QSlider::handle:horizontal {
    width: 32px;
    margin: -12px 0;          /* tay nắm tràn ra ngoài rãnh theo chiều dọc */
    background: #2d6cdf;
    border-radius: 16px;
}

/* Ô đánh dấu to hơn */
QCheckBox::indicator {
    width: 28px;
    height: 28px;
}

/* Thanh cuộn rộng, dễ kéo bằng tay */
QScrollBar:vertical {
    width: 24px;
}
QScrollBar::handle:vertical {
    min-height: 40px;
    background: #909090;
    border-radius: 6px;
}

/* Phần đã chạy của thanh tiến trình */
QProgressBar::chunk {
    background-color: #2e9d4f;
}

/* Tiêu đề cột của bảng */
QHeaderView::section {
    padding: 4px;
    font-weight: bold;
}
```

Một số thành phần con hay dùng:

| Widget | Sub-control |
| --- | --- |
| `QSlider` | `::groove`, `::handle`, `::add-page`, `::sub-page` |
| `QScrollBar` | `::handle`, `::add-line`, `::sub-line` |
| `QCheckBox`, `QRadioButton` | `::indicator` |
| `QSpinBox` | `::up-button`, `::down-button` |
| `QComboBox` | `::drop-down`, `::down-arrow` |
| `QProgressBar` | `::chunk` |
| `QHeaderView` | `::section` |
| `QTableView`, `QListView` | `::item` |

Thành phần con cũng kết hợp được với trạng thái: `QSlider::handle:pressed`, `QCheckBox::indicator:checked`.

## Mô hình hộp

Mỗi widget được vẽ theo mô hình hộp giống CSS:

```
+---------------- margin ----------------+
|  +------------ border --------------+  |
|  |  +-------- padding ----------+   |  |
|  |  |         content           |   |  |
|  |  +---------------------------+   |  |
|  +----------------------------------+  |
+----------------------------------------+
```

- margin: khoảng trống bên ngoài viền.
- border: đường viền.
- padding: khoảng trống giữa viền và nội dung.
- content: chữ, biểu tượng.

```css
QFrame#card {
    border: 1px solid #a0a0a0;
    border-radius: 6px;
    padding: 2px;
    margin: 0px;
}
```

## Các thuộc tính hay dùng

| Thuộc tính | Ví dụ |
| --- | --- |
| `color` | `color: white;` |
| `background-color` | `background-color: #202020;` |
| `border` | `border: 2px solid red;` |
| `border-radius` | `border-radius: 6px;` |
| `font-size` | `font-size: 16px;` |
| `font-weight` | `font-weight: bold;` |
| `font-family` | `font-family: "DejaVu Sans";` |
| `padding`, `margin` | `padding: 4px 8px;` (trên-dưới 4, trái-phải 8) |
| `min-height`, `min-width` | `min-height: 44px;` |
| `qproperty-<tên>` | `qproperty-alignment: AlignCenter;` |
| `selection-background-color` | màu nền hàng đang chọn trong bảng, danh sách |
| `alternate-background-color` | màu hàng xen kẽ khi dùng `setAlternatingRowColors` |

Màu viết dạng `#rrggbb`, dạng tên (red, white) hoặc `rgb(45, 108, 223)`.

Tiền tố `qproperty-` cho phép đặt bất kỳ property nào khai báo bằng `Q_PROPERTY`, kể cả của widget tự viết. Nhờ vậy, màu của widget tự vẽ cũng được quản lý chung trong style sheet thay vì viết cứng trong code.

## Đổi giao diện theo trạng thái

HMI hay cần đổi màu theo trạng thái: nhãn nhiệt độ hiển thị màu bình thường nhưng chuyển sang nền đỏ khi vượt ngưỡng. Cách không nên làm là gọi `setStyleSheet()` mỗi lần giá trị thay đổi:

```c++
label->setStyleSheet(value > 40 ? "background: red;" : "");   // tốn kém
```

Mỗi lần gọi `setStyleSheet()`, Qt phải phân tích lại chuỗi QSS và tính lại giao diện cho widget cùng các con của nó. Với nhãn cập nhật mỗi giây thì chi phí này khá đáng kể.

Cách đúng là dùng selector theo property. Style sheet ứng dụng khai báo sẵn giao diện cho từng trạng thái:

```css
QLabel#tempValue[alarm="true"] {
    background-color: #e02020;
    color: white;
}
```

Trong code, ta chỉ đổi property. Tuy nhiên, Qt không tự áp dụng lại style khi property thay đổi; ta phải yêu cầu nó tính lại giao diện cho widget:

```cpp
void setAlarmState(QWidget *w, bool alarm)
{
    if (w->property("alarm").toBool() == alarm)
        return;                      // không đổi thì không làm gì
    w->setProperty("alarm", alarm);
    w->style()->unpolish(w);         // bỏ giao diện cũ
    w->style()->polish(w);           // tính lại theo property mới
}
```

Hàm trên được gọi chỉ khi trạng thái cảnh báo thay đổi, không phải mỗi lần giá trị thay đổi và không phải phân tích lại chuỗi QSS nào.

:::warning QSS tốn CPU trên board không có GPU
Mỗi lần polish, Qt phải tìm các quy tắc match với widget. Mỗi lần vẽ, các hiệu ứng như `border-radius`, gradient phải tính cho từng điểm ảnh. Trên BBB, style sheet phức tạp áp cho hàng trăm widget có thể làm chậm rõ rệt lúc khởi động và lúc chuyển trang. Giữ QSS đơn giản, chỉ đổi property ở widget thật sự cần đổ và không gọi `setStyleSheet()` liên tục lúc chạy.
:::
