Các widget có sẵn của Qt như `QLabel`, `QPushButton`, `QProgressBar` phục vụ tốt nút bấm, nhãn, thanh mức. Nhưng giao diện HMI thường cần những thành phần mà Qt không có:
- Đồng hồ đo (gauge) dạng kim chỉ cho nhiệt độ, áp suất, tốc độ.
- Đèn trạng thái tròn, xanh/đỏ/vàng, có thể nhấp nháy.
- Đồ thị xu hướng (trend) hiển thị giá trị trong vài phút gần nhất.
- Sơ đồ quy trình đơn giản: bồn chứa, đường ống, van.

Có hai cách tạo widget riêng:

|   | Làm thế nào | Dùng khi |
| Ghép widget (composite) | Kế thừa `QWidget`, bên trong đặt các widget có sẵn trong layout | Thành phần là tổ hợp của các widget có sẵn |
| Tự vẽ | Kế thừa `QWidget`, ghi đè `paintEvent()` và vẽ bằng `QPainter` | Hình dạng không có sẵn: đồng hồ đo, đèn, đồ thị |

## Cơ chế vẽ

Như đã nói, vẽ lại là một sự kiện. Khi Qt cần vẽ một widget, nó gửi sự kiện vẽ và hàm `paintEvent()` của widget được gọi. Mọi việc vẽ đều phải nằm trong hàm này.

```cpp
class LedIndicator : public QWidget
{
protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);
        // ... vẽ ở đây
    }
};
```

```
setValue(72) --> update() --> [đánh dấu vùng cần vẽ]
                                    | 
                                    v
                event loop --> paintEvent() --> QPainter vẽ
                                    |
                                    v
                backing store --> linuxfb --> /dev/fb1 --> ILI9341
```

Khi dữ liệu thay đổi và cần vẽ lại, ta không bao giờ gọi `paintEvent()` trực tiếp. Thay vào đó, ta gọi:

| Hàm | Hoạt động |
| --- | --- |
| `update()` | Đặt yêu cầu vẽ lại vào hàng đợi. Nhiều lần `update()` liên tiếp được gộp thành một lần vẽ. |
| `update(const QRect &)` | Như trên, nhưng chỉ yêu cầu vẽ lại một vùng. |
| `repaint()` | Vẽ lại ngay lập tức, không gộp. |

Ta luôn dùng `update()`. Nếu giá trị cảm biến thay đổi 10 lần trước khi vòng lặp sự kiện kịp vẽ, `update()` chỉ tạo ra 1 lần vẽ, còn `repaint()` sẽ vẽ đủ 10 lần, 9 lần trong số đó là vô ích.

## QPainter cơ bản

`QPainter` là đối tượng thực hiện việc vẽ. Nó được tạo bên trong `paintEvent()`, nhận widget cần vẽ làm tham số và tự kết thúc khi ra khỏi hàm.

Hệ tọa độ bắt đầu ở góc trên bên trái của widget, trục y hướng xuống.

Bút (pen) và cọ (brush):
- Pen quyết định đường viền: màu, độ dày, kiểu nét (liền, đứt).
- Brush quyết định phần tô bên trong hình.

```cpp
painter.setPen(QPen(Qt::black, 2));        // viền đen, dày 2 px
painter.setBrush(QColor(0, 200, 0));       // tô màu xanh lá
painter.drawEllipse(10, 10, 40, 40);       // hình tròn có viền đen, tô xanh

painter.setPen(Qt::NoPen);                 // không viền
painter.setBrush(Qt::NoBrush);             // không tô
```

Các hàm vẽ thường dùng:

| Hàm | Vẽ |
| --- | -- |
| `drawLine(x1, y1, x2, y2)` | Đoạn thẳng |
| `drawRect(rect)` / `drawRoundedRect(rect, rx, ry)` | Hình chữ nhật / bo góc |
| `drawEllipse(rect)` | Hình elip, hình tròn nếu rect vuông |
| `drawArc(rect, startAngle, spanAngle)` | Cung tròn |
| `drawPie(rect, startAngle, spanAngle)` | Hình quạt |
| `drawPolyline(points)` | Đường gấp khúc nối nhiều điểm |
| `drawPolygon(points)` | Đa giác kín |
| `drawText(rect, alignment, text)` | Chữ, căn trong một vùng |
| `drawPixmap(x, y, pixmap)` | Hình ảnh |

Góc trong `drawArc` và `drawPie` có quy ước riêng, rất dễ nhầm:
- Đơn vị là 1/16 độ: 90° phải viết là `90 * 16`.
- Góc 0 nằm ở hướng 3 giờ.
- Góc dương đi ngược chiều kim đồng hồ.

```
             90°
              |
   180° ------+------ 0°  (hướng 3 giờ)
              |
            270°
```

Ví dụ, cung của một đồng hồ đo kiểu 270° bắt đầu ở góc dưới bên trái (225°) và quét theo chiều kim đồng hồ 270° tới góc dưới bên phải:

```cpp
painter.drawArc(rect, 225 * 16, -270 * 16);   // góc quét âm = theo chiều kim đồng hồ
```

**Khử răng cưa (antialiasing)** giúp đường cong và đường xiên mịn hơn:

```cpp
painter.setRenderHint(QPainter::Antialiasing);
```

Khử răng cưa tốn thêm CPU. Trên BBB, ta dùng nó cho các hình tròn, đường cong nhỏ và cân nhắc tắt khi vẽ các đồ thị có nhiều điểm.
