C++ đã có `std::string`, `std::vector`, `std::map`, vậy tại sao Qt lại có `QString`, `QList`, `QMap`?. Qt vẫn có kiểu riêng vì hai lý do:

- Toàn bộ API của Qt nhận và trả về kiểu Qt: `QLabel::setText` nhận `QString`, `QSerialPort::readAll` trả về `QByteArray`, `property()` trả về `QVariant`. Dùng kiểu của Qt giúp tránh chuyển đổi qua lại liên tục.
- Hỗ trợ Unicode: QString xử lý tiếng Việt có dấu một cách tự nhiên, điều mà `std::string` không làm được một cách dễ dàng.
- Implicit sharing: sao chép gần như không tốn chi phí, sẽ giải thích ngay dưới đây.

Ta vẫn có thể dùng thư viện chuẩn C++ bên trong phần logic của mình. Nhưng tại chỗ tiếp xúc với Qt, dùng kiểu của Qt sẽ gọn gàng hơn.

## Implicit sharing

Hầu hết kiểu dữ liệu của Qt (`QString`, `QByteArray`, `QList`, `QMap`, `QVariant`...) dùng cơ chế copy-on-write: khi sao chép, hai biến dùng chung một vùng dữ liệu và chỉ khi một trong hai bị sửa, dữ liệu mới thực sự được sao chép.

```
QString a = "Bình thường";
QString b = a;             a ──┐
                               ├──► [ "Bình thường" ]   (dùng chung, chưa sao chép)
                           b ──┘

b.append("!");             a ────► [ "Bình thường"  ]
                           b ────► [ "Bình thường!" ]   (lúc này mới tách ra)
```

Ta có thể kiểm chứng bằng cách so sánh địa chỉ vùng dữ liệu:

```cpp
QString s1 = "Bình thường";
QString s2 = s1;
qDebug() << (s1.constData() == s2.constData());   // true: dùng chung

s2.append("!");
qDebug() << (s1.constData() == s2.constData());   // false: đã tách
```

Nhờ cơ chế này, việc trả về `QString` hay `QList` từ hàm hoặc truyền chúng qua signal rất thuận tiện. Tuy vậy, khi truyền vào hàm, ta vẫn nên dùng `const QString`.

Việc tách ra (hay gọi là detach) có thể xảy ra ngầm ở những chỗ ta không ngờ tới. Mục **Lỗi thường gặp** sẽ chỉ ra các trường hợp này.

## Kiểu số nguyên

Khi làm việc với phần cứng, ta cần biết chính xác một số chiếm bao nhiêu byte. Qt cung cấp các kiểu:

| Kiểu | Kích thước | Phạm vi |
| --- | --- | --- |
| `quint8` / `qint8`    | 1 byte | 0 – 255 / -128 – 127 |
| `quint16` / `qint16`  | 2 byte | 0 – 65535 / -32768 – 32767 |
| `quint32` / `qint32`  | 4 byte | |
| `quint64` / `qint64`  | 8 byte | |

Chúng tương đương `uint8_t`, `int16_t`...của C++

## QString

`QString` lưu văn bản dạng Unicode (bên trong mã hóa UTF-16), nên tiếng Việt có dấu hoạt động bình thường với mọi thao tác: đếm ký tự, cắt, tìm kiếm, so sánh.

Một hệ quả cần nhớ: `length()` đếm số ký tự, không phải số byte. Chuỗi `"Cảm biến"` có 8 ký tự, nhưng khi mã hóa UTF-8 để gửi qua UART thì chiếm 12 byte.

### Tạo và nối chuỗi

```cpp
QString name = "Cảm biến";
QString full = name + " số " + QString::number(3);   // "Cảm biến số 3"
full.append(" (lỗi)");                                // nối vào cuối, sửa trực tiếp full
```

`QString::number()` đổi số sang chuỗi, có thể chọn định dạng:

```cpp
QString::number(3.14159, 'f', 2);        // "3.14": dạng thập phân, 2 chữ số sau dấu phẩy
QString::number(255, 16).toUpper();      // "FF": hệ 16
```

### Định dạng bằng arg()

Khi chuỗi có nhiều phần thay đổi, nối bằng `+` khó đọc. Ta viết một chuỗi mẫu chứa các placeholder `%1`, `%2`... rồi gọi `arg()` để chèn giá trị chuỗi vào:

```cpp
double temp = 25.456;
QString line = QString("%1: %2 °C").arg(name).arg(temp, 0, 'f', 1); // "Cảm biến: 25.5 °C"
```

`arg()` có nhiều overload hỗ trợ các kiểu dữ liệu khác nhau.

**Chuỗi:**

```cpp
QString arg(const QString &a, int fieldWidth = 0, QChar fillChar = QLatin1Char(' ')) const
```

| Tham số | Ý nghĩa |
| --- | --- |
| `a` | Chuỗi cần điền |
| `fieldWidth` | Độ rộng tối thiểu. Trong đó:<br/>Nếu > 0: căn phải (padding bên trái).<br/>Nếu < 0: căn trái (padding bên phải).<br/>0: không padding |
| `fillChar` | Ký tự dùng để padding, mặc định là dấu cách |

Ví dụ:

```cpp
QString("[%1]").arg("OK", 5);               // "[   OK]": căn phải
QString("[%1]").arg("OK", -5);              // "[OK   ]": căn trái
QString("[%1]").arg("OK", 5, QChar('.'));   // "[...OK]"
```

**Số nguyên:**

```cpp
QString arg(int a, int fieldWidth = 0, int base = 10, QChar fillChar = QLatin1Char(' ')) const
```

| Tham số | Ý nghĩa |
| --- | --- |
| `a` | Số cần điền |
| `fieldWidth` | Độ rộng tối thiểu, quy tắc dấu giống overload chuỗi |
| `base` | Hệ cơ số: 2 (nhị phân), 8(bát phân), 10 (thập phân), 16 (hex) |
| `fillChar` | Ký tự padding. Dùng `QChar('0')` để đệm số 0 |

```cpp
QString("[%1]").arg(42, 5);                        // "[   42]"
QString("%1").arg(7, 3, 10, QChar('0'));           // "007"
QString("0x%1").arg(255, 4, 16, QChar('0'));       // "0x00ff"
QString("%1").arg(5, 8, 2, QChar('0'));            // "00000101": 8 bit nhị phân
QString("%1").arg(-7, 4, 10, QChar('0'));          // "-007": dấu trừ đứng trước số 0
```

Muốn đệm số 0 thì phải truyền đủ cả `base` vì `fillChar` đứng sau nó.

**Số thực:**

```cpp
QString arg(double a, int fieldWidth = 0, char format = 'g', int precision = -1,
            QChar fillChar = QLatin1Char(' ')) const
```

| Tham số | Ý nghĩa |
| --- | --- |
| `a` | Số cần điền |
| `fieldWidth` | Độ rộng tối thiểu, tính cả dấu chấm và phần thập phân |
| `format` | `'f'`: thập phân cố định.<br/>`'e'`/`'E'`: exponential (dạng khoa học, ví dụ 3.14e+02).<br/> `'g'`/`'G'`: optional |
| `precision` | Với `'f'`, `'e'`: số chữ số sau dấu chấm. Với `'g'`: tổng số chữ số có nghĩa. -1: mặc định là 6 |
| `fillChar` | Ký tự đệm |

```cpp
double t = 25.456;
QString("%1").arg(t);                          // "25.456": 'g', tối đa 6 chữ số có nghĩa
QString("%1").arg(t, 0, 'f', 1);               // "25.5": làm tròn 1 chữ số thập phân
QString("[%1]").arg(t, 8, 'f', 2);             // "[   25.46]": rộng 8 ký tự
QString("%1").arg(t, 0, 'e', 2);               // "2.55e+01"
QString("%1").arg(t, 0, 'g', 3);               // "25.5": 3 chữ số có nghĩa
```

Khi hiển thị giá trị đo trên HMI nên dùng `'f'` với `precision` cố định. Dạng `'g'` có thể đổi số chữ số theo giá trị làm con số trên màn hình nhảy độ dài liên tục.

**Nhiều chuỗi cùng lúc:**

```cpp
QString arg(const QString &a1, const QString &a2) const
// ... có overload tới 9 chuỗi: arg(a1, a2, ..., a9)
```

```cpp
QString("%1 %2").arg("A", "B");     // "A B": a1 điền %1, a2 điền %2
```

Overload này chỉ nhận `QString` và không có tham số độ rộng hay ký tự đệm. Muốn điền số thì đổi sang chuỗi trước bằng `QString::number()`.

Nó còn an toàn hơn cách gọi nối tiếp khi giá trị điền vào có chứa ký tự `%`. Với cách gọi nối tiếp, `%2` nằm trong giá trị vừa điền sẽ bị lần `arg()` sau thay tiếp:

```cpp
QString name = "50%2";
QString("%1 = %2").arg(name).arg(10);                        // "5010 = 10": sai
QString("%1 = %2").arg(name, QString::number(10));           // "50%2 = 10": đúng
```

Ngoài ra còn có overload cho `QChar` và `char`, với tham số `fieldWidth` và `fillChar` giống overload chuỗi.

### Chuyển chuỗi sang số

```cpp
bool ok = false;
int a = QString("42").toInt(&ok);          // 42, ok = true
int b = QString("abc").toInt(&ok);         // 0,  ok = false
int c = QString("FF").toInt(&ok, 16);      // 255: đọc theo hệ 16
double d = QString(" 3.5 ").trimmed().toDouble(&ok);   // 3.5
```

Tham số `bool *ok` cho biết việc chuyển đổi có thành công hay không. Nếu bỏ qua nó, ta không phân biệt được chuỗi `"0"` với chuỗi sai định dạng, vì cả hai đều trả về 0.

### Tách, tìm và cắt chuỗi

Các hàm này dùng nhiều khi phân tích lệnh dạng văn bản, ví dụ lệnh nhận qua UART:

```cpp
QString cmd = "  SET,TEMP,25,,  ";
QStringList parts = cmd.trimmed().split(',', Qt::SkipEmptyParts);
// ("SET", "TEMP", "25")
QString joined = parts.join("|");          // "SET|TEMP|25"

QString s = "TEMP=25.4";
s.startsWith("TEMP");     // true
s.contains("=");          // true
s.indexOf('=');           // 4: vị trí đầu tiên, -1 nếu không có
s.left(4);                // "TEMP": 4 ký tự đầu
s.mid(5);                 // "25.4": từ vị trí 5 tới hết
s.section('=', 1);        // "25.4": phần thứ 2 khi tách theo '='
```

| Hàm | Tác dụng |
| --- | --- |
| `trimmed()` | Bỏ khoảng trắng, `\r`, `\n` ở hai đầu |
| `split(sep, Qt::SkipEmptyParts)` | Tách thành `QStringList`, bỏ phần rỗng |
| `left(n)` / `right(n)` / `mid(pos, n)` | Lấy n ký tự đầu / cuối / từ vị trí pos |
| `indexOf()` / `contains()` | Tìm vị trí / kiểm tra có chứa |
| `startsWith()` / `endsWith()` | Kiểm tra phần đầu / cuối |
| `toUpper()` / `toLower()` | Đổi chữ hoa / thường |
| `replace(a, b)` | Thay mọi chỗ `a` bằng `b` |

Các hàm như `trimmed()`, `left()`, `toUpper()` trả về chuỗi mới, không sửa chuỗi gốc. Ngược lại `append()`, `replace()`, `remove()` sửa trực tiếp chuỗi đang gọi.

So sánh không phân biệt hoa thường dùng `compare()`:

```cpp
QString("ok").compare("OK", Qt::CaseInsensitive) == 0;   // true
```

### Chuỗi rỗng và chuỗi null

`QString` phân biệt hai trạng thái: null (chưa từng được gán) và rỗng (có giá trị nhưng dài 0).

```cpp
QString n;          // n.isNull() == true,  n.isEmpty() == true
QString e = "";     // e.isNull() == false, e.isEmpty() == true
```

Trong thực tế, gần như luôn kiểm tra bằng `isEmpty()` vì nó đúng cho cả hai trường hợp.

## QByteArray

`QByteArray` là một mảng byte thô. Đây là kiểu ta dùng cho mọi dữ liệu nhị phân: khung dữ liệu nhận từ UART, gói tin qua mạng, nội dung file. `QSerialPort::readAll()` và `QTcpSocket::readAll()` đều trả về `QByteArray`.

Mỗi phần tử của `QByteArray` có kiểu `char`. Điều này quan trọng khi so sánh giá trị byte, sẽ nói rõ ở phần dưới.

### Tạo mảng byte

```cpp
QByteArray frame;
frame.append(char(0xAA));   // byte bắt đầu khung
frame.append(char(0x01));
frame.append(char(0x00));
frame.append(char(0xFA));
// frame: AA 01 00 FA, size() == 4

const char raw[] = {0x01, 0x00, 0x02};
QByteArray a(raw, sizeof(raw));              // 3 byte, kể cả byte 0x00

QByteArray zeros(4, '\0');                   // 4 byte giá trị 0
QByteArray b = QByteArray::fromHex("AA 01 00 FA");   // giống frame, tiện khi viết test
```

Với dữ liệu nhị phân, luôn tạo bằng constructor có độ dài `QByteArray(data, length)`. Constructor chỉ nhận `const char *` coi byte `0x00` là điểm kết thúc chuỗi và cắt mất phần sau.

### Đọc từng byte

`at(i)` và `operator[]` trả về `char`. Để lấy giá trị 0–255, ta ép sang `quint8`:

```cpp
if (quint8(frame.at(0)) == 0xAA) {
    // đúng byte bắt đầu khung
}
```

:::warning char có dấu hay không tùy kiến trúc
Trên máy tính x86, `char` là kiểu có dấu: byte `0xAA` được đọc thành -86 nên `frame.at(0) == 0xAA` luôn sai. Trên ARM Linux như BBB, `char` mặc định không dấu nên phép so sánh đó lại đúng. Kết quả là code chạy đúng trên board nhưng sai trên máy tính hoặc ngược lại. Luôn ép byte sang `quint8` trước khi so sánh hoặc tính toán.
:::

### Ghép và tách số nhiều byte

Giá trị 16 bit thường được gửi thành 2 byte. Thứ tự byte (endianness) là quy ước byte nào đi trước: big-endian gửi byte cao trước, little-endian gửi byte thấp trước.

Cách ghép thủ công bằng phép dịch bit:

```cpp
// frame: AA 01 00 FA, giá trị ở byte 2 và 3, big-endian
quint16 value = quint16((quint8(frame.at(2)) << 8) | quint8(frame.at(3)));   // 250
```

Qt có sẵn hàm trong `<QtEndian>` để làm việc này gọn hơn:

```cpp
#include <QtEndian>

quint16 value = qFromBigEndian<quint16>(frame.constData() + 2);   // đọc 2 byte từ vị trí 2: 250

QByteArray out(2, '\0');
qToBigEndian<quint16>(250, out.data());   // ghi 250 vào out: 00 FA
```

Tương tự có `qFromLittleEndian()` và `qToLittleEndian()`. Các hàm này cho kết quả đúng bất kể CPU đang chạy là big hay little-endian.

### Tìm và cắt trong bộ đệm

Dữ liệu từ UART có thể đến rời rạc: một khung bị chia làm nhiều lần đọc hoặc một lần đọc chứa nhiều khung. Cách thường làm là gom vào một bộ đệm rồi tìm và cắt từng khung:

```cpp
QByteArray buffer = QByteArray::fromHex("01 02 AA 01 00 FA 05");

int start = buffer.indexOf(char(0xAA));   // 2: vị trí byte bắt đầu khung
buffer.remove(0, start);                  // bỏ byte rác phía trước: AA 01 00 FA 05

if (buffer.size() >= 4) {                 // đủ một khung 4 byte
    QByteArray one = buffer.left(4);      // AA 01 00 FA
    buffer.remove(0, 4);                  // còn lại: 05, chờ lần đọc sau
}
```

| Hàm | Tác dụng |
| --- | --- |
| `size()` | Số byte |
| `indexOf(byte)` | Vị trí đầu tiên của byte, -1 nếu không có |
| `left(n)` / `mid(pos, n)` | Lấy n byte đầu / từ vị trí pos |
| `remove(pos, n)` | Xóa n byte từ vị trí pos |
| `startsWith()` / `endsWith()` | Kiểm tra phần đầu / cuối |
| `constData()` | Con trỏ `const char *` tới dữ liệu, dùng với hàm C |

### In ra để debug

`qDebug() << frame` in các byte không in được dưới dạng `\xAA`, khá khó đọc. Dùng `toHex()` để in dạng hex:

```cpp
qDebug() << frame.toHex(' ');   // "aa 01 00 fa"
```

### QByteArray chứa văn bản

Với giao thức dạng văn bản như lệnh AT, `QByteArray` cũng có các hàm giống `QString`:

```cpp
QByteArray cmd = QByteArray::number(25) + "\r\n";   // "25\r\n"
QByteArray reply = QByteArray("OK\r\n").trimmed();  // "OK"
```

Nếu dữ liệu có thể chứa chữ có dấu, đổi sang `QString` bằng `QString::fromUtf8()` trước khi xử lý.

### Chuyển đổi với QByteArray và std::string

`QString` là văn bản còn dữ liệu đi qua UART, socket, file là byte. Khi đi qua ranh giới đó, ta phải chọn cách mã hóa:

| Chiều | Hàm | Ghi chú |
| --- | --- | --- |
| `QString` → byte | `toUtf8()` | Mặc định nên dùng |
| byte → `QString` | `QString::fromUtf8(bytes)` | Dữ liệu UTF-8 |
| `QString` → byte | `toLatin1()` | Chỉ ký tự ASCII/Latin-1, chữ có dấu sẽ hỏng |
| `QString` ↔ `std::string` | `toStdString()` / `QString::fromStdString()` | Dùng UTF-8 |

```cpp
QByteArray bytes = QString("Nhiệt độ").toUtf8();   // 13 byte
QString text = QString::fromUtf8(bytes);           // "Nhiệt độ"
```

:::tip QStringLiteral cho chuỗi hằng
`QString s = "abc";` phải đổi chuỗi C sang UTF-16 lúc chạy, mỗi lần dòng đó được thực thi. `QStringLiteral("abc")` tạo sẵn dữ liệu lúc biên dịch nên không tốn công chuyển đổi. Trên CPU yếu như BBB nên dùng nó cho chuỗi hằng nằm trong hàm được gọi thường xuyên.
:::

## Containers

| Kiểu | Tương đương | Đặc điểm |
|---|---|---|
| `QVector<T>` | `std::vector<T>` | Danh sách phần tử liên tiếp; lựa chọn mặc định |
| `QList<T>` | | API Qt trả về; ví dụ `QStringList` là `QList<QString>` |
| `QMap<K, V>` | `std::map<K, V>` | Tra theo khóa, duyệt theo thứ tự khóa |
| `QHash<K, V>` | `std::unordered_map<K, V>` | Tra theo khóa nhanh, không có thứ tự |
| `QSet<T>` | `std::unordered_set<T>` | Tập hợp không trùng lặp |

:::note QList trong Qt 5
Trong Qt 5, `QList<T>` lưu mỗi phần tử lớn hơn một con trỏ trong một vùng nhớ riêng, tốn thêm bộ nhớ và kém thân thiện với cache CPU. Với dữ liệu mẫu đo thì nên dùng `QVector<T>`. Từ Qt 6, `QList` và `QVector` là một nên code dùng `QVector` vẫn chuyển sang Qt 6 dễ dàng.
:::

### QVector

```cpp
QVector<double> samples;
samples.reserve(100);                 // cấp phát trước 100 phần tử
samples.append(25.1);
samples << 25.3 << 25.2;              // cách viết ngắn của append

samples.size();                       // 3
samples.at(0);                        // 25.1
samples.first();  samples.last();     // phần tử đầu / cuối
samples.removeFirst();                // bỏ mẫu cũ nhất: (25.3, 25.2)
```

`reserve()` giúp tránh cấp phát lại nhiều lần khi thêm phần tử. Khi đã biết trước số phần tử tối đa như bộ đệm 100 mẫu đo gần nhất, nên gọi nó một lần lúc khởi tạo.

Để đọc phần tử, dùng `at(i)` thay vì `[]` khi không cần sửa. `at()` là hàm `const` nên không bao giờ gây detach.

### Duyệt container

```cpp
double total = 0;
for (double x : qAsConst(samples))
    total += x;
```

`qAsConst()` (có từ Qt 5.7, tương đương `std::as_const` của C++17) biến container thành `const` trong vòng lặp. Lý do: vòng lặp range-for trên container không `const`. Nếu container đang dùng chung dữ liệu với một bản sao khác, hàm này kích hoạt detach, tức sao chép toàn bộ dữ liệu dù ta chỉ đọc.

### QMap và QHash

Hai kiểu này lưu cặp key-value, ví dụ bảng ánh xạ tên thông số sang địa chỉ thanh ghi:

```cpp
QMap<QString, int> regs;
regs.insert("temp", 0x10);
regs.insert("hum", 0x11);

regs.value("temp");          // 16
regs.value("xyz", -1);       // -1: khóa không có thì trả về giá trị mặc định
regs.contains("xyz");        // false

for (auto it = regs.cbegin(); it != regs.cend(); ++it)
    qDebug() << it.key() << it.value();   // "hum" 17, rồi "temp" 16: theo thứ tự khóa
```

Khi chỉ đọc, dùng `value()` thay cho `[]`. Toán tử `[]` trên map không `const` sẽ **tự thêm khóa** nếu khóa chưa tồn tại:

```cpp
int x = regs["pres"];   // x = 0 và regs giờ có thêm khóa "pres"
```

`QHash` có API giống `QMap`, nhanh hơn khi tra cứu nhưng không giữ thứ tự khóa. Nếu cần in hay duyệt theo thứ tự thì nên dùng `QMap`.

```cpp
QHash<int, QString> errors;
errors.insert(1, "Mất kết nối");
errors.value(2, "Không rõ");
```

### QSet

`QSet` chỉ giữ mỗi giá trị một lần, hữu ích để ghi nhớ những gì đã gặp:

```cpp
QSet<int> seen;
seen.insert(3);
seen.insert(3);
seen.size();          // 1
seen.contains(3);     // true
```

## QVariant

`QVariant` là một hộp chứa được một giá trị thuộc nhiều kiểu khác nhau: `int`, `double`, `QString`, `QByteArray` và kiểu tự định nghĩa nếu đã đăng ký.

`QVariant` xuất hiện ở nhiều nơi trong Qt: `QObject::property()` trả về `QVariant`, `QSettings` lưu giá trị cấu hình dưới dạng `QVariant`, model của bảng và danh sách trả dữ liệu từng ô bằng `QVariant`.

| Việc | Code |
|---|---|
| Tạo | `QVariant v = 42;` |
| Lấy ra | `v.toInt()`, `v.toString()`, `v.value<T>()` |
| Kiểm tra | `v.isValid()`, `v.canConvert<T>()`, `v.typeName()` |
| Kiểu tự định nghĩa | `Q_DECLARE_METATYPE(T)` rồi `QVariant::fromValue(x)` |

### Cất vào và lấy ra

```cpp
QVariant v = 42;
v.toInt();          // 42
v.toString();       // "42": QVariant tự chuyển đổi khi được
v.typeName();       // "int"

QVariant empty;
empty.isValid();    // false: không chứa gì
```

Các hàm `toInt()`, `toDouble()` cũng nhận `bool *ok` giống `QString`. Cần chú ý rằng `canConvert<T>()` chỉ cho biết hai kiểu có thể chuyển đổi, không đảm bảo giá trị cụ thể chuyển được:

```cpp
bool ok = false;
QVariant s = "12abc";
s.canConvert<int>();    // true: QString nói chung đổi được sang int
s.toInt(&ok);           // 0, ok = false: chuỗi này thì không
```

### Ví dụ: property của QObject

```cpp
QObject obj;
obj.setProperty("threshold", 30.5);
obj.property("threshold").toDouble();   // 30.5
obj.property("none").isValid();         // false: property không tồn tại
```

Kiểm tra `isValid()` là cách phân biệt "không có giá trị" với "giá trị bằng 0".

### Kiểu tự định nghĩa

Để đặt một struct của ta vào `QVariant`, cần đăng ký kiểu đó với hệ thống meta-type của Qt bằng `Q_DECLARE_METATYPE`:

```cpp
struct SensorReading {
    QString name;
    double value = 0;
};
Q_DECLARE_METATYPE(SensorReading)   // đặt ngay sau định nghĩa, ở phạm vi toàn cục

SensorReading r{"Nhiệt độ", 25.4};
QVariant v = QVariant::fromValue(r);
SensorReading back = v.value<SensorReading>();   // back.value == 25.4
```

Kiểu dùng với `Q_DECLARE_METATYPE` cần có constructor mặc định, copy constructor và destructor public. Struct đơn giản như trên thỏa mãn sẵn.

## Lỗi thường gặp

**Trình biên dịch cảnh báo `'QString::SkipEmptyParts' is deprecated`**

Hằng này bị thay thế từ Qt 5.14. Dùng `Qt::SkipEmptyParts`. Nhiều tài liệu cũ vẫn dùng cách viết cũ.

**`toInt()` hoặc `toDouble()` trả về 0 dù dữ liệu sai**

Khi chuỗi không phải số, các hàm này trả về 0 mà không báo lỗi. Với dữ liệu từ phần cứng, luôn truyền `bool ok` và kiểm tra vì 0 có thể là một giá trị đo hợp lệ.

**`QByteArray("\x01\x00\x02")` chỉ có 1 byte**

Constructor nhận `const char *` coi byte `0x00` là điểm kết thúc chuỗi. Với dữ liệu nhị phân, dùng `QByteArray(data, length)` hoặc `append()` từng byte.

**So sánh byte với hằng như `0xAA` cho kết quả sai hoặc chạy đúng trên board mà sai trên máy tính**

Xem khối cảnh báo phía trên về `char` có dấu. GCC cảnh báo `comparison is always false due to limited range of data type` khi bật `-Wextra`. Ép byte sang `quint8`.

**Chữ tiếng Việt hiện sai thành ký tự lạ**

Dữ liệu UTF-8 bị đổi sang `QString` bằng `fromLatin1()` hoặc `fromLocal8Bit()` với locale không phải UTF-8. Dùng `QString::fromUtf8()`. Cũng kiểm tra file nguồn `.cpp` được lưu ở UTF-8.

**Biên dịch báo `static assertion failed: Type is not registered, please use the Q_DECLARE_METATYPE macro...`**

Đưa một kiểu tự định nghĩa vào `QVariant` khi chưa đăng ký. Thêm `Q_DECLARE_METATYPE(TênKiểu)` ngay sau định nghĩa kiểu, ở phạm vi toàn cục.

**Crash khi gọi `at()` hoặc `[]`**

Chỉ số nằm ngoài phạm vi, thường gặp khi khung dữ liệu nhận được ngắn hơn dự kiến. Bản Qt build debug sẽ báo `ASSERT failure ... index out of range`; bản release không kiểm tra và có thể đọc sai vùng nhớ. Luôn kiểm tra `size()` trước khi đọc từng byte của dữ liệu nhận từ bên ngoài.

**Vòng lặp chỉ đọc container nhưng chương trình chậm và tốn bộ nhớ bất thường**

Vòng lặp range-for trên container không `const` gọi `begin()` không `const`. Nếu container đang dùng chung dữ liệu với một bản sao khác, ví dụ một `QVector` vừa được gán từ biến khác hay nhận qua signal, lời gọi này gây detach: toàn bộ dữ liệu bị sao chép dù ta chỉ đọc.

```cpp
QVector<int> a(100000, 1);
QVector<int> copy = a;              // a và copy dùng chung dữ liệu

for (int x : a) { /* chỉ đọc */ }            // a bị detach, sao chép 100000 phần tử
for (int x : qAsConst(a)) { /* chỉ đọc */ }  // không detach
```

Khi chỉ đọc, dùng `qAsConst()` trong vòng lặp và `at(i)` thay cho `[]`. Hàm trả về container nên khai báo biến nhận là `const` nếu không cần sửa.

**Sửa phần tử qua iterator làm thay đổi cả bản sao**

Iterator lấy từ container trước khi sao chép vẫn trỏ vào vùng dữ liệu dùng chung. Ghi qua iterator đó sẽ thay đổi cả hai biến:

```cpp
QVector<int> v(3, 0);
auto it = v.begin();       // lấy iterator trước
QVector<int> w = v;        // v và w dùng chung dữ liệu
*it = 5;                   // v: (5, 0, 0), w cũng thành (5, 0, 0)
```

Không sao chép container trong lúc đang giữ iterator không `const` của nó. Nếu cần sao chép, lấy lại iterator sau khi sao chép. Quy tắc tương tự áp dụng với con trỏ lấy từ `data()`.

**`QMap` hoặc `QHash` tự có thêm khóa lạ**

Toán tử `[]` trên map không `const` tự thêm khóa với giá trị mặc định nếu khóa chưa tồn tại. Chỉ một câu kiểm tra như `if (regs["pres"] == 0)` cũng đủ thêm khóa `"pres"` vào map. Khi chỉ đọc, dùng `value(key, giá_trị_mặc_định)` hoặc kiểm tra bằng `contains()` trước.
