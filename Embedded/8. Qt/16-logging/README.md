## Vì sao logging quan trọng với thiết bị nhúng?

Khi phát triển trên máy tính, ta có console để xem `qDebug`, có debugger để dừng chương trình và xem giá trị biến. Trên thiết bị đã lắp ở hiện trường thì khác hẳn:
- Màn hình ILI9341 chỉ hiển thị giao diện, không có console nào để đọc.
- Ứng dụng tự khởi động bằng systemd và chạy một mình, không có ai theo dõi.
- Lỗi thường xuất hiện sau nhiều giờ hoặc nhiều ngày, trong điều kiện khó tái hiện: nhiễu điện, cảm biến lỏng dây, nhiệt độ môi trường cao.

Khi đó, file log là nguồn thông tin duy nhất cho biết chuyện gì đã xảy ra. Một hệ thống log tốt cần trả lời được: sự việc xảy ra lúc nào, ở module nào, nghiêm trọng tới đâu, và trước đó chương trình đang làm gì.

## Mức độ log

Qt cung cấp năm hàm log, tương ứng năm mức độ nghiêm trọng:

| Hàm | Mức | Dùng cho |
|---|---|---|
| `qDebug()` | Debug | Chi tiết để gỡ lỗi. Thường tắt khi chạy thật |
| `qInfo()` | Info | Sự kiện bình thường nhưng đáng ghi lại: khởi động, đổi cấu hình,... |
| `qWarning()` | Warning | Bất thường nhưng vẫn chạy được: vượt ngưỡng, thử kết nối lại |
| `qCritical()` | Critical | Lỗi nghiêm trọng, chức năng bị ảnh hưởng: mất kết nối, không đọc được cảm biến |
| `qFatal()` | Fatal | Lỗi không thể tiếp tục. Chương trình abort sau khi ghi log |

Chọn đúng mức rất quan trọng. Nếu mọi thứ đều là `qDebug`, khi cần tìm lỗi ta phải lội qua hàng nghìn dòng. Nếu mọi thứ đều là `qWarning`, những cảnh báo thật sự sẽ bị chìm.

## Log đi đâu trên thiết bị?

Mặc định, Qt ghi log ra stderr. Nơi nhận stderr tùy cách chương trình được chạy:

| Cách chạy | Log xuất hiện ở |
| --- | --- |
| Từ Qt Creator | Cửa sổ Application Output |
| Qua SSH | Terminal SSH |
| Systemd | journald |

Khi ứng dụng chạy dưới systemd, `stderr` đi vào **journal**:

```bash
journalctl -u hmi-demo -f            # xem liên tục
journalctl -u hmi-demo -p warning    # chỉ từ mức warning trở lên
journalctl -u hmi-demo -b -1         # log của lần khởi động trước
```

journald xếp mức độ cho từng dòng. Nếu dòng bắt đầu bằng tiền tố dạng `<N>` (N theo thang syslog: 3 là error, 4 là warning, 6 là info, 7 là debug), journald dùng mức đó và bỏ tiền tố đi. Nhờ vậy `journalctl -p warning` lọc đúng các dòng `qWarning()`.

Để ghi log ra file, hiển thị log trên màn hình, hay gửi log qua mạng, ta cài một message handler riêng bằng `qInstallMessageHandler()`. Hàm này được gọi cho mọi dòng log trong ứng dụng, kể cả log của Qt.

```cpp
void myHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const QString line = qFormatLogMessage(type, context, msg);   // áp dụng message pattern
    // ghi line ra file, lưu vào bộ nhớ...
}

qInstallMessageHandler(myHandler);   // trả về handler cũ
```

Ba điều bắt buộc khi viết message handler:
- An toàn đa luồng: handler có thể được gọi đồng thời từ nhiều luồng nên phải bảo vệ bằng `QMutex`.
- Không gọi lại log bên trong handler, kể cả gián tiếp nếu không sẽ đệ quy vô hạn.
- Giữ lại handler cũ và gọi nó để log vẫn tiếp tục ra stderr và journald như bình thường.

:::warning Ghi log vào flash có chọn lọc
Ghi mọi dòng debug xuống eMMC hay thẻ nhớ làm hao mòn flash, vốn chỉ chịu được số lần ghi có hạn và file log lớn dần có thể làm đầy phân vùng dữ liệu. Journal đã có cơ chế giới hạn dung lượng riêng. Nếu cần file log riêng, chỉ ghi từ mức warning trở lên, giới hạn kích thước và xoay vòng file.
:::

## Định dạng dòng log

Mặc định, mỗi dòng log chỉ có nội dung, không có thời gian hay mức độ. Ta thay đổi định dạng bằng `qSetMessagePattern()` hoặc biến môi trường `QT_MESSAGE_PATTERN`:

```cpp
qSetMessagePattern("%{time hh:mm:ss.zzz} %{type} [%{category}] %{message}");
```

Các thành phần hay dùng:

| Thành phần | Ý nghĩa |
| --- | --- |
| `%{time hh:mm:ss.zzz}` | Thời gian, theo định dạng tùy chọn |
| `%{type}` | Mức log: debug, info, warning, critical, fatal |
| `%{category}` | Nhóm log |
| `%{message}` | Nội dung |
| `%{threadid}` | Mã luồng, hữu ích với chương trình đa luồng |
| `%{file}`, `%{line}`, `%{function}` | Vị trí trong mã nguồn |
| `%{if-warning}...%{endif}` | Chỉ in phần bên trong khi mức log tương ứng |

:::warning Lưu ý
`%{file}`, `%{line}`, `%{function}` mặc định chỉ có trong bản build debug. Muốn có trong bản release, cần định nghĩa macro `QT_MESSAGELOGCONTEX`T khi biên dịch.
:::

## QLoggingCategory

Với ứng dụng có nhiều module (cảm biến, UART, giao diện, mạng...), ta cần bật/tắt log theo từng module. Ví dụ: khi nghi ngờ lỗi ở cảm biến, ta muốn xem log debug của riêng module cảm biến mà không bị log giao diện làm rối.

`QLoggingCategory` cho phép làm việc này. Mỗi nhóm có một tên dạng phân cấp, thường theo quy ước `tênứngdụng.module`:

```cpp
// logging.h — khai báo, để các file khác dùng được
Q_DECLARE_LOGGING_CATEGORY(lcSensor)

// logging.cpp — định nghĩa, chỉ một lần
Q_LOGGING_CATEGORY(lcSensor, "hmi.sensor")
```

Khi log, ta dùng các hàm có chữ C (category) ở giữa:

```cpp
qCDebug(lcSensor)    << "Giá trị thô:" << raw;
qCInfo(lcSensor)     << "Bắt đầu đọc cảm biến";
qCWarning(lcSensor)  << "Lỗi CRC";
qCCritical(lcSensor) << "Mất kết nối cảm biến";
```

Có thể đặt mức tối thiểu mặc định cho một nhóm ngay khi định nghĩa:

```cpp
Q_LOGGING_CATEGORY(lcSensor, "hmi.sensor", QtInfoMsg)   // debug bị tắt mặc định
```

Nhóm này chỉ in từ mức Info trở lên, cho tới khi ta chủ động bật debug.

## Bật/tắt log mà không cần biên dịch lại

Luật có dạng `<category>.<mức>=true|false`, dùng được ký tự `*`:

Đây là tính năng giá trị nhất của `QLoggingCategory` với thiết bị nhúng: ta điều chỉnh log ngay trên board, không phải build và deploy lại. Quy tắc có dạng `<category>.<mức>=true|false` và dùng được dấu `*`:

```
hmi.sensor.debug=true      # bật debug cho module cảm biến
hmi.*.debug=false          # tắt debug cho mọi module của hmi
qt.qpa.input=true          # bật log của Qt về thiết bị nhập (cảm ứng)
```

Có ba cách đặt quy tắc:

1. Biến môi trường `QT_LOGGING_RULES`, các quy tắc cách nhau bởi dấu chấm phẩy:

```bash
QT_LOGGING_RULES="hmi.sensor.debug=true" ./LogDemo -platform linuxfb:fb=/dev/fb1
```

Khi ứng dụng chạy dưới dạng service systemd, thêm `Environment=QT_LOGGING_RULES=hmi.sensor.debug=true` vào file service là bật được log chi tiết cho một module mà không cần build lại.

2. File cấu hình `qtlogging.ini`, đặt ở `~/.config/QtProject/qtlogging.ini` hoặc ở đường dẫn tùy chọn khai báo qua biến môi trường `QT_LOGGING_CONF`:

```ini
[Rules]
hmi.sensor.debug=true
```

3. Trong code, bằng `QLoggingCategory::setFilterRules()`. Cách này cho phép bật/tắt từ chính giao diện HMI, ví dụ từ một màn hình chẩn đoán dành cho kỹ thuật viên.

Khi nhiều nguồn cùng có quy tắc, biến môi trường `QT_LOGGING_RULES` có ưu tiên cao nhất, ghi đè các quy tắc đặt bằng code.

## Log của chính Qt

Các module của Qt cũng dùng `QLoggingCategory`, với tên bắt đầu bằng `qt.`. Chúng tắt ở mức debug theo mặc định nhưng khi bật lên lại rất hữu ích để tìm lỗi trên board:

| Quy tắc | Cho biết |
| --- | --- |
| `qt.qpa.input=true` | Qt tìm thấy thiết bị cảm ứng nào, nhận sự kiện ra sao |
| `qt.qpa.*=true` | Toàn bộ hoạt động của platform plugin (linuxfb...) |
| `qt.core.plugin.loader=true` | Qt đang tìm và nạp plugin ở đâu, nạp thất bại vì sao |

Khi màn hình cảm ứng không phản hồi hoặc gặp lỗi `Could not find the Qt platform plugin`, bật các nhóm này thường chỉ ra nguyên nhân ngay.

## Khi chương trình crash

Log cho biết những gì xảy ra trước crash nhưng không cho biết crash ở dòng code nào. Thêm hai công cụ:

- **Core dump**: ảnh bộ nhớ của tiến trình lúc crash. Nếu image có `systemd-coredump`, xem danh sách bằng `coredumpctl list` và mở trong gdb bằng `coredumpctl gdb`. Chép file core về máy tính để phân tích bằng gdb cho ARM (`gdb-multiarch` hoặc gdb trong SDK) cùng sysroot của board.
- **Tên file và số dòng trong log**: Qt chỉ điền thông tin này khi macro `QT_MESSAGELOGCONTEXT` được định nghĩa; bản Release mặc định không có. Ví dụ định nghĩa macro này trong `CMakeLists.txt` để log warning trỏ được tới dòng code.
