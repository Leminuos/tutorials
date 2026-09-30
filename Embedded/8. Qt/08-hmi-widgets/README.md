## Tổng quan

Qt Widgets có hàng chục loại widget, phần lớn dành cho ứng dụng desktop: menu, thanh công cụ, hộp thoại chọn file. Với giao diện HMI, ta chỉ thường xuyên dùng một nhóm nhỏ, chia theo công dụng:

| Nhóm | Widget |
| --- | --- |
| Hiển thị thông tin | `QLabel`, `QLCDNumber`, `QProgressBar` |
| Ra lệnh | `QPushButton`, `QButtonGroup` |
| Nhập giá trị | `QSlider`, `QSpinBox`, `QDoubleSpinBox`, `QLineEdit` |
| Chọn lựa | `QCheckBox`, `QRadioButton`, `QComboBox` |
| Điều hướng | `QStackedWidget` |
| Hỏi – xác nhận | `QMessageBox`, `QDialog` |

## Các lớp cha chung

Nhiều widget trong bảng trên có chung một lớp cha. Nắm được lớp cha thì học một lần, dùng được cho cả họ:

```
QWidget
 +-- QAbstractButton ---- QPushButton, QCheckBox, QRadioButton
 +-- QAbstractSlider ---- QSlider
 +-- QAbstractSpinBox --- QSpinBox, QDoubleSpinBox
 +-- QFrame ------------- QLabel, QLCDNumber, QStackedWidget
 +-- QProgressBar, QLineEdit, QComboBox   (kế thừa thẳng QWidget)
```

**`QAbstractButton`** cung cấp cho mọi loại nút:

| Thành phần | Ý nghĩa |
| --- | --- |
| `setText()`, `setIcon()` | Chữ và biểu tượng trên nút |
| `setCheckable()`, `setChecked()`, `isChecked()` | Nút có giữ trạng thái bật/tắt hay không |
| `setAutoRepeat()` | Giữ tay thì phát `clicked()` lặp lại |
| `pressed()`, `released()` | Chạm xuống, nhấc tay lên |
| `clicked(bool)` | Chạm xuống rồi nhấc tay **bên trong** nút |
| `toggled(bool)` | Trạng thái checked thay đổi |

**`QAbstractSlider`** cung cấp khoảng giá trị nguyên `setRange(min, max)`, giá trị `value()`/`setValue()`, bước nhỏ `singleStep`, bước lớn `pageStep` và signal `valueChanged(int)`.

**`QFrame`** cho phép vẽ khung quanh widget bằng `setFrameShape()`, ví dụ `QFrame::Box`. Trên HMI khung thường được vẽ bằng Style Sheets (QSS, cú pháp giống CSS để đổi giao diện widget), nhưng biết `QLabel` là một `QFrame` giúp hiểu vì sao nhãn có thể có viền.

## QLabel

`QLabel` là widget dùng nhiều nhất. Nó chỉ hiển thị, không nhận thao tác của người dùng.

**Hiển thị số trực tiếp:**

```cpp
label->setNum(36);            // tương đương setText("36")
label->setNum(25.5);          // bản nhận double
```

`setNum()` là slot nên nối thẳng được từ signal mang giá trị số. Vì có hai bản `int` và `double`, khi `connect` phải chọn bản bằng `qOverload<int>(&QLabel::setNum)`.

`setNum(double)` định dạng số theo kiểu ngắn gọn nhất nên 25.0 hiện thành "25". Khi cần số chữ số thập phân cố định, ta tự định dạng:

```cpp
label->setText(QString::number(temperature, 'f', 1) + " °C");   // "25.0 °C"
```

**Căn lề:**

```cpp
label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);   // căn phải, giữa theo chiều dọc
```

Cột số liệu nên căn phải để hàng đơn vị thẳng nhau, dễ so sánh.

**Giữ chiều rộng cố định cho số thay đổi:**

Nhãn đòi chiều rộng vừa đúng nội dung. Khi giá trị đổi từ "9.8" sang "10.2", nhãn rộng thêm, layout tính lại và các widget bên cạnh bị xô lệch. Đặt chiều rộng tối thiểu đủ cho giá trị dài nhất:

```cpp
label->setMinimumWidth(label->fontMetrics().horizontalAdvance("-000.0"));
```

`fontMetrics().horizontalAdvance()` trả về số pixel mà chuỗi chiếm với font hiện tại của nhãn.

**Hiển thị hình ảnh:**

Ví dụ biểu tượng trạng thái:

```cpp
label->setPixmap(QPixmap(":/icons/warning.png"));
label->setScaledContents(true);   // co giãn hình theo kích thước nhãn
```

Đường dẫn bắt đầu bằng `:/` là đường dẫn tới file được nhúng vào chương trình (resource). Co giãn ảnh mỗi lần vẽ tốn CPU. Nếu được thì nên chuẩn bị ảnh đúng kích thước hiển thị và bỏ `setScaledContents`.

**Chữ có định dạng bằng một tập con của HTML:**

```cpp
label->setText("Trạng thái: <b><font color='red'>QUÁ NHIỆT</font></b>");
```

Cách này tiện nhưng tốn công xử lý hơn chữ thường. Với nhãn cập nhật liên tục thì nên dùng chữ thường và đổi màu bằng Style Sheets.

Mặc định `QLabel` tự đoán chuỗi là chữ thường hay HTML. Nếu nhãn hiển thị chuỗi nhận từ thiết bị, chuỗi có ký tự `<` có thể bị hiểu nhầm thành thẻ HTML. Gọi `label->setTextFormat(Qt::PlainText)` để luôn hiển thị nguyên văn.

**Xuống dòng tự động:**

`label->setWordWrap(true)` cho phép chữ dài xuống dòng thay vì bị cắt, hữu ích với thông báo lỗi trên màn hình hẹp. Mặc định nhãn không xuống dòng và sẽ đòi chiều rộng bằng cả dòng chữ, có thể làm vỡ bố cục.

## QLCDNumber

`QLCDNumber` hiển thị số theo kiểu LED 7 đoạn, nét to và dễ đọc từ xa. Hợp với giá trị chính của màn hình: nhiệt độ, tốc độ, bộ đếm sản phẩm.

```cpp
auto *lcd = new QLCDNumber(4);            // 4 vị trí ký tự
lcd->setSegmentStyle(QLCDNumber::Flat);
lcd->display(1250);
```

| Hàm | Ý nghĩa |
| --- | --- |
| `setDigitCount(n)` | Số vị trí ký tự, tính cả dấu âm và dấu chấm |
| `display(int / double / QString)` | Hiển thị giá trị. Đây là slot |
| `setMode(QLCDNumber::Hex)` | Hiển thị theo hệ 16. Ngoài ra có `Dec`, `Oct`, `Bin` |
| `setSmallDecimalPoint(true)` | Dấu chấm nằm giữa hai chữ số, không chiếm một vị trí riêng |
| `checkOverflow(value)` | Kiểm tra trước giá trị có vừa số vị trí hay không |

Ba kiểu vẽ đoạn:

| `SegmentStyle` | Hình dạng |
| --- | --- |
| `Outline` | Chỉ vẽ viền đoạn, bên trong rỗng |
| `Filled` (default) | Đoạn có viền nổi |
| `Flat` | Khối màu đặc, rõ nhất trên màn hình nhỏ |

Giống `QLabel::setNum`, `display(double)` định dạng số theo kiểu ngắn gọn. Muốn luôn có một chữ số thập phân, truyền chuỗi đã định dạng: `lcd->display(QString::number(v, 'f', 1))`.

Ngoài chữ số, `QLCDNumber` hiển thị được một số chữ cái (như `A`–`F`, `H`, `L`, `P`, `r`, `u`), dấu `-`, dấu `:` và khoảng trắng. Dấu nháy đơn `'` được vẽ thành ký hiệu độ, nên `lcd->display("25'C")` hiện nhiệt độ kèm đơn vị. Ký tự ngoài danh sách được vẽ thành khoảng trắng.

Khi giá trị có nhiều ký tự hơn `digitCount`, `QLCDNumber` phát signal `overflow()` và không hiển thị giá trị mới. Luôn tính đủ vị trí cho giá trị lớn nhất, dấu âm và dấu chấm.

## QProgressBar

`QProgressBar` hiển thị một giá trị trong khoảng min–max dưới dạng thanh: mức nước trong bồn, phần trăm tải, tiến trình cập nhật firmware.

```cpp
auto *level = new QProgressBar;
level->setRange(0, 100);
level->setValue(40);
level->setFormat("%v %");        // chữ trên thanh: "40 %"
```

| Hàm | Ý nghĩa |
| --- | --- |
| `setFormat()` | Chữ trên thanh: `%v` giá trị, `%p` phần trăm, `%m` giá trị max |
| `setTextVisible(false)` | Ẩn chữ, chỉ còn thanh |
| `setOrientation(Qt::Vertical)` | Thanh dọc, hợp để vẽ mức chất lỏng |
| `setInvertedAppearance(true)` | Đảo chiều tăng: từ phải sang trái, hoặc từ trên xuống |
| `reset()` | Đưa thanh về trạng thái chưa có giá trị |

Giá trị của `QProgressBar` là số nguyên. Với đại lượng thực như áp suất 0.0–10.0 bar, ta nhân lên thành số nguyên: `setRange(0, 100)` và `setValue(qRound(pressure * 10))`, rồi tự viết chữ bằng `setFormat(QString::number(pressure, 'f', 1) + " bar")`.

Đặt `setRange(0, 0)` biến thanh thành chế độ "bận": không hiện giá trị, chỉ có hiệu ứng chạy liên tục để báo đang xử lý. Hiệu ứng này bắt widget vẽ lại liên tục, tốn CPU trên board không có GPU; chỉ nên dùng trong thời gian ngắn.

:::warning Giá trị ngoài khoảng bị bỏ qua
`setValue()` với giá trị nằm ngoài min–max không có tác dụng: thanh vẫn hiện giá trị cũ. Khi cảm biến trả về số vượt thang đo, thanh "đứng yên" và người vận hành tưởng giá trị không đổi. Luôn giới hạn trước khi đặt: `bar->setValue(qBound(0, raw, 100))`, và báo riêng tình trạng vượt thang.
:::

## QPushButton

Hai kiểu dùng chính:

| Kiểu | Thiết lập | Signal nên dùng |
|---|---|---|
| Nút lệnh | Mặc định | `clicked()` |
| Nút bật/tắt | `setCheckable(true)` | `toggled(bool checked)` |

**Thứ tự signal khi chạm:**

Chạm vào nút rồi trượt ngón tay ra ngoài trước khi nhấc thì `clicked()` không phát. Đây là cách người dùng hủy một lần bấm nhầm nên lệnh quan trọng luôn nối vào `clicked()`, không nối vào `pressed()`.

`pressed()` và `released()` dùng cho kiểu điều khiển giữ để chạy (jog): chạm giữ thì motor chạy, nhấc tay thì dừng.

**`clicked` hay `toggled` với nút bật/tắt:**

- `toggled(bool)` phát mỗi khi trạng thái đổi, kể cả khi code gọi `setChecked()`.
- `clicked(bool)` chỉ phát khi người dùng bấm hoặc code gọi `click()`.

Khi nút phản ánh trạng thái thiết bị, hai signal này có vai trò khác nhau. Lệnh gửi xuống thiết bị nối vào `clicked`. Khi thiết bị báo trạng thái về, gọi `setChecked()` để cập nhật nút. Nếu lệnh nối vào `toggled`, mỗi lần cập nhật từ thiết bị lại sinh ra một lệnh gửi ngược xuống.

**Biểu tượng:**

```cpp
startButton->setIcon(QIcon(":/icons/start.png"));
startButton->setIconSize(QSize(32, 32));
```

Nút chỉ có biểu tượng tiết kiệm diện tích trên màn hình nhỏ, nhưng biểu tượng phải đủ rõ nghĩa; lệnh nguy hiểm như "Stop" nên có cả chữ.

**Khóa nút tạm thời:** `setEnabled(false)` làm nút mờ đi và không nhận chạm. Dùng khi lệnh đang được thực hiện, tránh người dùng bấm liên tục gửi nhiều lệnh trùng.

**Auto-repeat:**

Tính năng **auto-repeat** rất có ích cho nút "+" và "−": giữ tay trên nút thì `clicked()` phát liên tục.

```cpp
plusButton->setAutoRepeat(true);
plusButton->setAutoRepeatDelay(400);     // giữ 400 ms mới bắt đầu lặp
plusButton->setAutoRepeatInterval(100);  // sau đó lặp mỗi 100 ms
```

`setAutoRepeatDelay()` là thời gian giữ trước khi bắt đầu lặp, `setAutoRepeatInterval()` là chu kỳ lặp.

## QSlider

Thanh trượt cho phép chọn một giá trị nguyên trong khoảng, ví dụ tốc độ động cơ hay độ sáng.

```cpp
auto *speedSlider = new QSlider(Qt::Horizontal);
speedSlider->setRange(0, 3000);
speedSlider->setSingleStep(10);     // bước khi dùng phím mũi tên
speedSlider->setPageStep(100);      // bước khi chạm vào thanh, bên cạnh tay nắm
speedSlider->setValue(1200);
```

`setValue()` tự giới hạn giá trị trong khoảng min–max: gọi `setValue(5000)` với khoảng 0–3000 thì giá trị thành 3000.

Các thiết lập hiển thị:

| Hàm | Ý nghĩa |
| --- | --- |
| `Qt::Vertical` (tham số constructor) | Thanh dọc, hợp với âm lượng, mức |
| `setTickPosition(QSlider::TicksBelow)` | Vẽ vạch chia bên dưới thanh |
| `setTickInterval(n)` | Khoảng cách giữa hai vạch chia, tính theo giá trị |
| `setInvertedAppearance(true)` | Đảo chiều: giá trị lớn ở bên trái (hoặc bên dưới) |

Các signal quan trọng:

| Signal | Phát ra khi |
| --- | --- |
| `valueChanged(int)` | Giá trị thay đổi, kể cả liên tục trong lúc đang kéo và khi code gọi `setValue()` |
| `sliderMoved(int)` | Người dùng đang kéo tay nắm; không phát khi code đổi giá trị |
| `sliderPressed()` | Bắt đầu kéo |
| `sliderReleased()` | Thả tay nắm |

Khi kéo thanh trượt, `valueChanged` có thể được phát hàng chục lần mỗi giây. Nếu mỗi lần như vậy ta gửi một lệnh xuống phần cứng qua UART, đường truyền sẽ bị ngập lệnh. Có hai cách xử lý:
- Dùng `valueChanged` để cập nhật nhãn hiển thị trong lúc kéo còn lệnh xuống phần cứng chỉ gửi ở `sliderReleased`.
- Gọi `setTracking(false)`: khi đó `valueChanged` chỉ được phát một lần khi người dùng thả tay.

Tay nắm mặc định của thanh trượt khá nhỏ, ta cần phóng to bằng Style Sheets khi dùng với cảm ứng. Cảm ứng điện trở lại kém chính xác, kéo tới đúng một giá trị như 47 thường khó; nên kết hợp thêm nút "+"/"−" để chỉnh tinh.

## QSpinBox và QDoubleSpinBox

Spin box là ô hiển thị một số kèm hai nút mũi tên tăng/giảm. `QSpinBox` dùng cho số nguyên, `QDoubleSpinBox` cho số thực.

```cpp
auto *threshold = new QDoubleSpinBox;
threshold->setRange(0.0, 150.0);
threshold->setDecimals(1);          // 1 chữ số sau dấu chấm
threshold->setSingleStep(0.5);      // mỗi lần bấm mũi tên
threshold->setSuffix(" °C");        // hiện "75.0 °C"
threshold->setValue(75.0);
```

| Hàm | Ý nghĩa |
| --- | --- |
| `setRange()`, `setSingleStep()` | Khoảng giá trị và bước tăng/giảm |
| `setPrefix()`, `setSuffix()` | Chữ đứng trước, sau số: đơn vị, ký hiệu |
| `setDecimals(n)` | Số chữ số thập phân (chỉ `QDoubleSpinBox`); giá trị bị làm tròn theo số chữ số này |
| `setWrapping(true)` | Vượt max thì quay về min và ngược lại |
| `setSpecialValueText("Off")` | Chữ hiện thay cho số khi giá trị bằng min |
| `setAccelerated(true)` | Giữ nút mũi tên lâu thì bước tăng nhanh dần |
| `stepUp()`, `stepDown()` | Slot tăng/giảm một bước, như khi bấm mũi tên |
| `setKeyboardTracking(false)` | Khi gõ bằng bàn phím, chỉ phát `valueChanged` lúc nhấn Enter hoặc rời ô |

So với `QSlider`, spin box cho giá trị chính xác và hiện luôn đơn vị, nhưng hai mũi tên mặc định chỉ cao khoảng 10 px, gần như không chạm được trên cảm ứng điện trở. Cách thường dùng trên HMI là ẩn mũi tên bằng `setButtonSymbols(QAbstractSpinBox::NoButtons)` rồi đặt hai `QPushButton` lớn bên cạnh, nối tới `stepDown()` và `stepUp()`. Spin box vẫn lo phần giới hạn khoảng, làm tròn và hiển thị đơn vị.

:::note Signal bị overload trong Qt 5
Trong Qt 5, `QSpinBox::valueChanged` có hai bản: `int` và `QString` (bản `QString` đã lỗi thời từ Qt 5.14 nhưng vẫn còn). Viết `&QSpinBox::valueChanged` trong `connect` sẽ báo lỗi vì trình biên dịch không biết chọn bản nào, phải chỉ rõ: `qOverload<int>(&QSpinBox::valueChanged)`. Tương tự với `qOverload<double>(&QDoubleSpinBox::valueChanged)` và `qOverload<int>(&QComboBox::currentIndexChanged)`.
:::

## QLineEdit

`QLineEdit` là ô nhập một dòng chữ: tên thiết bị, địa chỉ IP, mật khẩu bảo trì.

```cpp
auto *ipEdit = new QLineEdit;
ipEdit->setPlaceholderText("192.168.1.10");     // chữ gợi ý mờ khi ô trống
ipEdit->setInputMask("000.000.000.000;_");      // khuôn nhập: chỉ cho phép chữ số

auto *passEdit = new QLineEdit;
passEdit->setEchoMode(QLineEdit::Password);     // hiện dấu chấm thay cho ký tự
passEdit->setMaxLength(8);
```

Để giới hạn nội dung hợp lệ, gắn một **validator**, đối tượng kiểm tra chuỗi nhập:

```cpp
auto *addrEdit = new QLineEdit;
addrEdit->setValidator(new QIntValidator(1, 247, addrEdit));   // địa chỉ 1..247
```

Khi có validator, ô không nhận ký tự sai (như chữ cái), và `hasAcceptableInput()` cho biết nội dung hiện tại có hợp lệ không.

Các signal:

| Signal | Phát ra khi |
| --- | --- |
| `textChanged(QString)` | Nội dung đổi, kể cả khi code gọi `setText()` |
| `textEdited(QString)` | Nội dung đổi do người dùng gõ |
| `returnPressed()` | Nhấn Enter, và nội dung hợp lệ với validator |
| `editingFinished()` | Nhấn Enter hoặc rời khỏi ô, và nội dung hợp lệ |

`setReadOnly(true)` biến ô thành chỗ hiển thị chữ có thể chọn và cuộn, dùng khi chuỗi dài hơn chiều rộng ô.

Vấn đề lớn nhất của `QLineEdit` trên board là **không có bàn phím**: chạm vào ô thì con trỏ nhấp nháy nhưng không có cách nào gõ. Thực tế HMI thường làm thế này: chạm vào ô thì mở một hộp thoại bàn phím ảo tự làm, nhập xong thì gán kết quả vào ô bằng `setText()`. Bài 13 xây dựng một bàn phím số như vậy.

## QCheckBox và QRadioButton

Cả hai đều là nút checkable kế thừa `QAbstractButton`, nên dùng được `setChecked()`, `isChecked()` và `toggled(bool)` như `QPushButton`. Khác nhau ở cách chọn:

| Widget | Cách chọn | Dùng cho |
| --- | --- | --- |
| `QCheckBox` | Mỗi ô bật/tắt độc lập | Các tùy chọn không loại trừ nhau: "Bật còi", "Ghi log" |
| `QRadioButton` | Chọn một trong nhóm | Các chế độ loại trừ nhau: "Manual" / "Auto" / "Off" |

**QCheckBox:**

```cpp
auto *buzzer = new QCheckBox("Buzzer on alarm");
buzzer->setChecked(true);
connect(buzzer, &QCheckBox::toggled, this, [](bool on) {
    // bật/tắt còi báo
});
```

`QCheckBox` còn có trạng thái thứ ba "chọn một phần" (`Qt::PartiallyChecked`) khi bật `setTristate(true)`, dùng cho ô "chọn tất cả" khi chỉ một số mục con được chọn. Trạng thái ba mức đọc bằng `checkState()` và signal `stateChanged(int)`; với ô hai trạng thái thì `toggled(bool)` đơn giản hơn.

**QRadioButton:**

Các `QRadioButton` có cùng widget cha tự loại trừ nhau: chọn nút này thì nút kia tự bỏ chọn. Khi trong cùng một cha có hai nhóm radio độc lập, ta tách chúng bằng hai `QButtonGroup` (giới thiệu ở mục `QStackedWidget` bên dưới), hoặc đặt mỗi nhóm vào một `QGroupBox`, khung chứa có tiêu đề:

```cpp
auto *modeBox = new QGroupBox("Mode");
auto *manual = new QRadioButton("Manual");
auto *automatic = new QRadioButton("Auto");
manual->setChecked(true);

auto *modeLayout = new QHBoxLayout(modeBox);
modeLayout->addWidget(manual);
modeLayout->addWidget(automatic);
```

Ô vuông và ô tròn chọn mặc định chỉ khoảng 13 px, nhỏ so với ngón tay. Chạm vào phần chữ cũng chọn được, nhưng vẫn nên phóng to ô chọn bằng Style Sheets. Một cách khác hay dùng trên HMI là thay bằng `QPushButton` checkable: vùng chạm lớn, trạng thái dễ nhìn.

## QComboBox

`QComboBox` là ô chọn một mục trong danh sách thả xuống: tốc độ baud, đơn vị đo, ngôn ngữ. Nó chỉ chiếm một dòng dù danh sách dài, rất hợp với màn hình nhỏ.

```cpp
auto *baudBox = new QComboBox;
baudBox->addItem("9600", 9600);       // chữ hiển thị, dữ liệu đi kèm
baudBox->addItem("19200", 19200);
baudBox->addItem("115200", 115200);
baudBox->setCurrentIndex(baudBox->findData(19200));

int baud = baudBox->currentData().toInt();
```

Tham số thứ hai của `addItem()` là **user data**, một `QVariant` gắn với mục. Code nên đọc `currentData()` thay vì phân tích chữ hiển thị: chữ có thể đổi khi dịch giao diện, còn dữ liệu thì không.

| Hàm / signal | Ý nghĩa |
| --- | --- |
| `currentIndex()`, `currentText()`, `currentData()` | Mục đang chọn |
| `findData(value)`, `findText(text)` | Tìm chỉ số của mục, trả về -1 nếu không có |
| `setMaxVisibleItems(n)` | Số mục hiện cùng lúc trong danh sách thả xuống |
| `currentIndexChanged(int)` | Mục chọn đổi, kể cả khi code gọi `setCurrentIndex()` |
| `activated(int)` | Người dùng chọn một mục, kể cả chọn lại mục cũ |

Giống `QPushButton`, lệnh gửi xuống thiết bị nên nối vào `activated`, còn `currentIndexChanged` dùng để đồng bộ giao diện. Cả hai signal đều bị overload trong Qt 5, cần `qOverload<int>` khi `connect`.

Chọn bằng combo box cần hai lần chạm: mở danh sách, rồi chọn mục. Với 2–4 lựa chọn, một hàng nút checkable hoặc radio chỉ cần một lần chạm và luôn thấy mục đang chọn; combo box hợp hơn khi danh sách dài.

## QStackedWidget

`QStackedWidget` chứa nhiều widget con, gọi là các trang, nhưng chỉ hiện thị một trang tại một thời điểm. Đây là cách phổ biến nhất để xây dựng hệ thống nhiều màn hình cho HMI.

```cpp
auto *stack = new QStackedWidget;
int mainIndex     = stack->addWidget(new MainScreen);      // trả về chỉ số trang
int settingsIndex = stack->addWidget(new SettingsScreen);

stack->setCurrentIndex(settingsIndex);      // chuyển trang theo chỉ số
stack->setCurrentWidget(mainScreen);        // hoặc theo con trỏ widget
```

Các trang được tạo một lần lúc khởi động và giữ nguyên. Chuyển trang chỉ là ẩn trang cũ, hiện trang mới, không tạo hay xóa widget. Cách này nhanh và không phân mảnh bộ nhớ.

| Hàm / signal | Ý nghĩa |
| --- | --- |
| `addWidget(page)` | Thêm trang vào cuối, trả về chỉ số của trang |
| `setCurrentIndex(i)` | Hiện trang theo chỉ số |
| `setCurrentWidget(page)` | Hiện trang theo con trỏ, không phụ thuộc thứ tự thêm |
| `currentIndex()`, `currentWidget()` | Trang đang hiện |
| `indexOf(page)`, `widget(i)`, `count()` | Tra cứu trang |
| `currentChanged(int)` | Phát sau khi đổi trang |

`currentChanged` hữu ích để tiết kiệm CPU: trang bị ẩn không cần cập nhật. Ví dụ trang đồ thị chỉ chạy `QTimer` lấy mẫu khi đang hiện và dừng timer khi người dùng chuyển sang trang khác.

Tổ chức điều hướng: các trang không nên tự điều khiển `QStackedWidget`. Mỗi trang chỉ phát signal yêu cầu chuyển trang còn lớp chứa `QStackedWidget` quyết định chuyển tới đâu.

```cpp
// Trong SettingsScreen: chỉ phát signal
signals:
    void backRequested();

// Trong lớp cửa sổ chính: nối signal vào việc chuyển trang
connect(settingsScreen, &SettingsScreen::backRequested, this, [=]() {
    m_stack->setCurrentWidget(mainScreen);
});

connect(mainScreen, &MainScreen::menuRequested, this, [=]() {
    m_stack->setCurrentWidget(settingsScreen);
});
```

Với cách này, nút "Menu" trong màn hình chính ở Bài 7 chỉ cần phát `menuRequested()` và `MainScreen` không cần biết màn hình cài đặt là gì.

## QMessageBox và QDialog

**Hộp thoại** (dialog) là cửa sổ hiện đè lên màn hình chính để hỏi người dùng một việc rồi đóng lại. `QDialog` là lớp cơ sở; `QMessageBox` là hộp thoại dựng sẵn gồm biểu tượng, câu hỏi và các nút chuẩn.

**Cách hộp thoại trả kết quả:**

| Thành phần | Ý nghĩa |
| --- | --- |
| `accept()`, `reject()` | Đóng hộp thoại với kết quả đồng ý / hủy |
| `accepted()`, `rejected()` | Signal tương ứng sau khi đóng |
| `finished(int)` | Signal phát khi đóng, với mọi kết quả |
| `exec()` | Hiện hộp thoại và **chờ** tới khi đóng, trả về kết quả |
| `open()` | Hiện hộp thoại rồi trả về ngay; kết quả nhận qua signal |

`exec()` viết gọn vì kết quả có ngay ở dòng tiếp theo. Nhưng để chờ, nó chạy một event loop lồng bên trong. Trong lúc chờ, timer và dữ liệu từ cổng serial vẫn được xử lý, nên một slot có thể bị gọi lại khi lần gọi trước còn đang dừng ở `exec()`. Các hàm tĩnh tiện lợi như `QMessageBox::question()` cũng dùng `exec()` bên trong. Trên HMI có nhiều nguồn sự kiện chạy song song, `open()` an toàn hơn:

```cpp
auto *box = new QMessageBox(QMessageBox::Warning, "Stop motor",
                            "Stop the motor now?",
                            QMessageBox::Yes | QMessageBox::No, this);
box->setAttribute(Qt::WA_DeleteOnClose);   // tự xóa khi đóng

connect(box, &QMessageBox::buttonClicked, this, [this, box](QAbstractButton *button) {
    if (box->standardButton(button) == QMessageBox::Yes) {
        // gửi lệnh dừng motor
    }
});
box->open();
```

- `QMessageBox::Yes | QMessageBox::No`: các nút chuẩn, chữ trên nút theo ngôn ngữ hệ thống. Ngoài ra còn `Ok`, `Cancel`, `Retry`...
- `setAttribute(Qt::WA_DeleteOnClose)`: hộp thoại được tạo mới mỗi lần hỏi, nên tự xóa sau khi đóng để không rò bộ nhớ.
- `standardButton(button)`: đổi nút được bấm về giá trị `QMessageBox::StandardButton` để so sánh.
- Truyền `this` làm cha giúp hộp thoại hiện giữa cửa sổ chính.

Kiểu biểu tượng: `QMessageBox::Information`, `Warning`, `Critical`, `Question`.

Trên màn hình 320×240, `QMessageBox` mặc định có chữ và nút nhỏ, câu dài bị xuống dòng thành hộp cao. Nên viết câu hỏi ngắn và phóng to nút bằng Style Sheets.

Khi cần hộp thoại có nội dung riêng như bàn phím số hay màn hình xác nhận có nhập mật khẩu, ta tạo lớp kế thừa `QDialog`, tự đặt widget và layout bên trong, rồi gọi `accept()` hoặc `reject()` khi người dùng bấm nút OK / Cancel.

