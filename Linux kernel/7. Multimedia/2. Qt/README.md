# Qt

Qt (đọc là "cute") là một framework C++ đa nền tảng dùng để phát triển ứng dụng có giao diện đồ họa (GUI) và cả ứng dụng không có GUI. Qt được dùng rất rộng rãi trong:
- Hệ thống nhúng (ô tô, thiết bị y tế, công nghiệp)
- Desktop (Linux, Windows, macOS)
- Mobile (Android, iOS)
- Embedded Linux trên các board như BBB, Raspberry Pi...

## 1. Signals/Slots

Signal/Slot là cơ chế giao tiếp giữa các đối tượng trong Qt, ý tưởng của nó là:
- Một đối tượng bắn một sự kiện $\rightarrow$ gọi là signal
- Một đối tượng khác hứng và xử lý sự kiện đó $\rightarrow$ gọi là slot

Signal là một hàm đặc biệt được MOC sinh ra để phát thông điệp ra ngoài. Nó không có phần thân trong code, nhưng MOC tự sinh ra phần gọi.

Một signal có thể kết nối tới nhiều slot cùng lúc, khi có sự kiện thì signal sẽ gửi tới tất cả slot được connect tới singal và gọi slot tương ứng (ngay lập tức hoặc thông qua queued nếu khác thread).

![Multi slot](img/multi-slot.png)

### 1.1. Manual Connect

Đây là cách ta chủ động kết nối signal và slot bằng code C++.

Cú pháp (Qt 5+):

```c++
QObject::connect(sender, &SenderClass::signalName,
                 receiver, &ReceiverClass::slotName);
```

Ví dụ:

- File `sensor.h` - class đọc cảm biến trên BBB

```h
#pragma once
#include <QObject>
#include <QTimer>

class Sensor : public QObject {
    Q_OBJECT

public:
    explicit Sensor(QObject *parent = nullptr);
    void start();   // bắt đầu đọc

signals:
    void temperatureChanged(float celsius);   // phát khi có giá trị mới
    void errorOccurred(const QString &msg);   // phát khi lỗi

public slots:
    void stop();    // dừng đọc

private slots:
    void readHardware();   // slot nội bộ, nối với QTimer

private:
    QTimer *m_timer;
    int m_fd;   // file descriptor của I2C/UART
};
```

- File `sensor.cpp`

```cpp
#include "sensor.h"
#include <QDebug>

Sensor::Sensor(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);   // parent = this → tự dọn bộ nhớ
    connect(m_timer, &QTimer::timeout,
            this, &Sensor::readHardware);
}

void Sensor::start() {
    m_fd = openDevice("/dev/i2c-1", 0x48);
    if (m_fd < 0) {
        emit errorOccurred("Cannot open I2C device");
        return;
    }

    m_timer->start(500);   // đọc mỗi 500ms
}

void Sensor::readHardware() {
    float temp = readDevice(m_fd);
    if (temp < -40 || temp > 125) {
        emit errorOccurred("Temperature out of range");
        return;
    }
    
    emit temperatureChanged(temp);   // phát signal cho UI
}
```

- File `main.cpp` - kết nối mọi thứ lại

```cpp
Sensor *sensor = new Sensor();
QLabel *label = new QLabel("--");
QLabel *errLabel = new QLabel();

// Signal từ sensor → cập nhật UI
connect(sensor, &Sensor::temperatureChanged, [=](float temp) {
    label->setText(QString("%1 °C").arg(temp, 0, 'f', 1));
});

// Signal lỗi → hiển thị cảnh báo
connect(sensor, &Sensor::errorOccurred, [=](const QString &msg) {
    errLabel->setText("Error: " + msg);
    errLabel->setStyleSheet("color: red");
});

sensor->start();
```

### 2.2. Auto connect

Auto connect là tính năng của Qt Designer/`uic` - khi ta đặt tên slot theo quy tắc đặc biệt, Qt tự động kết nối mà không cần gọi connect().

Quy tắc đặt tên:

```
on_<objectName>_<signalName>(<param>)
```

Trong đó:
- `on_` - tiền tố cố định
- `objectName` - tên object trong Qt Designer (thuộc tính objectName)
- `signalName` - tên signal (ví dụ: clicked, valueChanged, textChanged)

Ví dụ nếu ta có một widget `QPushButton` đặt tên là `btnStart` trong Qt Designer với signal `clicked`, ta chỉ cần khai báo slot sau trong class, Qt sẽ tự động connect cho ta mà không cần viết `connect()` thủ công:

```cpp
void on_btnStart_clicked();
```

**Manual connect + Auto-connect không ghi đè nhau**

Nếu ta có:

```cpp
connect(lightButton, &QPushButton::clicked, this, &MainWindow::foo);
```

Và ta có slot auto-connect:

```cpp
void MainWindow::on_lightButton_clicked();
```

Thì khi click:
- `on_lightButton_clicked()` được gọi (auto)
- `foo()` được gọi (manual)

=> Hai kết nối độc lập, không overwrite.

### 2.3. Ưu/nhược của hai phương pháp connect

|     | Manual connect | Auto connect |
| --- | --- | --- |
| Ưu điểm | - Linh hoạt, rõ ràng<br/>- Dùng được mọi nơi, không cần phụ thuộc vào Qt designer | - Không cần viết connect<br/>- Tích hợp tốt với Qt designer |
| Nhược điểm | Phải tự viết connect cho từng cặp | - Tên slot phải đúng quy tắc đặt tên<br/>- Khó debug, không rõ kết nối ở đâu<br>- Dễ bị double connect nếu không cẩn thận |

## 3. Cơ chế Meta-Object Compiler (MOC)

MOC là bộ tiền xử lý (preprocessor) do Qt cung cấp. Nó phân tích mã nguồn C++ và tự động sinh ra mã C++ bổ sung để hỗ trợ các tính năng mà C++ chuẩn không có, như:
- Signal & Slot (giao tiếp giữa các đối tượng)
- `Q_OBJECT` macro
- `Q_PROPERTY` / `Q_CLASSINFO` / `Q_ENUM`
- Reflection (truy cập thông tin class lúc runtime)

### 3.1. Cơ chế hoạt động

1. Khi biên dịch project Qt, build system sẽ tự động detect các class chứa `Q_OBJECT`. Ví dụ:

    ```cpp
    class MyClass : public QObject {
        Q_OBJECT

    public:
        explicit MyClass(QObject *parent = nullptr);

    signals:
        void valueChanged(int);

    private slots:
        void updateValue(int v);
    };
    ```

2. Chạy moc để xử lý và Qt compiler sẽ tạo thêm file `moc_myclass.cpp` tương ứng với class chứa `Q_OBJECT`.

3. File này sẽ chứa các metadata object `QMetaObject` chứa danh sách signal, slot, property.

### 3.2. Reflection

Trong quá trình runtime, khi ta muốn:
- Lấy type property
- Liệt kế tất cả property của một object
- Đọc giá trị của property bằng tên chuỗi
- Gọi method bằng tên chuỗi
- Kiểm tra xem class có thuộc tính mong muốn hay không

-> Những điều này C++ không hỗ trợ, do đó Qt sinh ra MOC để mô phỏng reflection.

Với cơ chế này, ta có thể truy cập vào thuộc tính của một QObject khi chương trình đang chạy bằng tên chuỗi. Ví dụ:

```cpp
QObject *obj = ...
obj->setProperty("temperature", 25);
qDebug() << obj->property("temperature");
```

<b style="color:red; font-size: 20px">Tại sao lại cần truy cập thuộc tính bằng string</b>

**Khi build UI từ file config, json hoặc XML.**

Ví dụ:

```json
{
  "speed": 120,
  "mode": "auto",
  "accel": 10
}
```

Ta không thể hardcode kiểu này:

```cpp
motor.Speed = json["speed"];
motor.Mode = json["mode"];
motor.Accel = json["accel"];
```

Vì nếu thêm trường mới thì ta lại phải chỉnh sửa lại code. Ta có thể làm như sau để linh hoạt hơn:

```cpp
foreach (var kv in json)
{
    var prop = t.GetProperty(kv.Key);
    if (prop != null)
        prop.SetValue(motor, kv.Value);
}
```

**Kết nối với QML**

QML dùng property theo tên chuỗi:

```qml
Text { 
    text: device.temperature
}
```

Nếu không có property dưới dạng meta-object:
- QML không thể biết temperature là gì
- Không thể binding
- Không thể animate

## 4. Property

`Q_PROPERTY` có tác dụng biến một biến member thành property động, từ đó có thể:
- Được đọc/ghi bởi Qt Meta-Object System
- Sử dụng trong QML, QDesigner, hoặc QVariant
- Được dùng trong binding, animation, data serialization

Cú pháp đơn giản như sau:

```CPP
Q_PROPERTY(type name
           READ getFunction
           WRITE writeFunction
           NOTIFY notifySignal
           RESET resetFunction)
```

## 5. Kiến trúc Model-View-Controler (MVC)

MVC là một mẫu kiến trúc phần mềm để tạo giao diện người dùng. Nó chia thành ba phần được kết nối với nhau và mỗi thành phần đều có một nhiệm vụ riêng của nó và độc lập với các thành phần khác.

1. Model

- Có nhiệm vụ thao tác với Database
- Nó chứa tất cả các hàm, các phương thức truy vấn trực tiếp với dữ liệu
- Controller sẽ thông qua các hàm, phương thức đó để lấy dữ liệu rồi gửi qua View

2. View
- Là giao diện người dùng (User Interface)
- Chứa các thành phần tương tác với người dùng như menu, button, image, text,...
- Nơi nhận dữ liệu từ Controller và hiển thị

3. Controller
- Là thành phần trung gian giữa Model và View
- Đảm nhận vai trò tiếp nhận yêu cầu từ người dùng, thông qua Model để lấy dữ liệu sau đó thông qua View để hiển thị cho người dùng

Qt dùng một biến thể của mô hình MVC, chính xác hơn là Model-View-Delegate.

![MVC in Qt](img/mvc-in-qt.png)

Nó tách rõ 3 vai trò:

| Thành phần   | Vai trò                             | Lớp cơ sở             |
| ------------ | ----------------------------------- | --------------------- |
| **Model**    | Lưu và quản lý dữ liệu              | `QAbstractItemModel`  |
| **View**     | Hiển thị và xử lý tương tác         | `QAbstractItemView`   |
| **Delegate** | Quy định cách hiển thị và chỉnh sửa | `QStyledItemDelegate` |

Qt không có Controller riêng như MVC, mà View kiêm luôn vai trò của Controller (nhận input, tương tác người dùng, rồi báo lại Model).

1. Model

Model là nơi dữ liệu trung tâm.
View không giữ dữ liệu, mà chỉ hỏi model để lấy thông tin hiển thị.
Model chịu trách nhiệm:
- Lưu và tổ chức dữ liệu (list, bảng, cây, database…)
- Cung cấp dữ liệu cho view
- Nhận dữ liệu từ view khi người dùng chỉnh sửa
- Báo cho view biết khi dữ liệu thay đổi

2. View
View là lớp giao diện, chịu trách nhiệm:
- Hiển thị dữ liệu từ Model (list, tree, table)
- Nhận tương tác từ người dùng (click, chọn, edit)
- Thông báo lại cho Model khi có thay đổi

3. Delegate

Delegate là lớp trung gian giữa Model và View, phụ trách:
- Vẽ dữ liệu (paint) – định nghĩa cách hiển thị mỗi item.
- Tạo editor (createEditor) – định nghĩa cách chỉnh sửa dữ liệu (textbox, combobox…).

## 6. Widget

### 6.1. Splitter

`QSplitter` là một container widget cho phép chứa nhiều widget con và ngăn cách chúng bằng một thanh chia gọi là split handle. Người dùng có thể kéo handle để thay đổi kích thước widget con.

Nói cách khác, nó là layout có thể kéo giãn động bằng chuột.

![Splitter](img/splitter.png)

### 6.2. Buddy

Buddy là một widget được gán cho một `QLabel` để khi người dùng nhấn tổ hợp phím tắt thì sẽ tự động chuyển đến widget đó.

Cơ chế hoạt động: **Ký tự sau dấu “&” sẽ làm phím tắt.**

Ví dụ:

```cpp
QLabel *label = new QLabel("&Username:");
QLineEdit *lineEdit = new QLineEdit;
label->setBuddy(lineEdit);
```

- Dấu “&” trong "&Username:" cho Qt biết rằng phím “U” là phím tắt.
- Khi ứng dụng chạy, label sẽ hiển thị “Username:” với ký tự “U” có gạch chân.
- Khi người dùng nhấn Alt + U, focus sẽ nhảy vào ô lineEdit.

## 7. Layout

Layout trong Qt là cơ chế tự động quản lý vị trí và kích thước của các widget con bên trong một widget cha.

Ta không cần tính toán thủ công tọa độ như:

```cpp
button->setGeometry(x, y, w, h);
```

Điều này có một số vấn đề khi kích thước cửa sổ thay đổi:
- Các widget không tự co giãn.
- UI bị méo, tràn hoặc đè nhau.

Phải tính toán lại từng vị trí → rất tốn công. Thay vào đó, layout giúp:
- Co giãn giao diện theo kích thước cửa sổ.
- Giữ khoảng cách chuẩn giữa các widget.
- Sắp xếp widget theo hàng/cột hoặc theo lưới.
- Tự động bố trí lại khi theme thay đổi hoặc DPI thay đổi.
- Tự động quản lý minimum/maximum size, sizePolicy, stretch factors.

### 7.1. Stretch factor

Stretch factor quy định phân chia tỷ lệ giữa các widget con.

Ví dụ:

```cpp
auto *layout = new QHBoxLayout();
layout->addWidget(btn1, 1);   // Stretch = 1
layout->addWidget(btn2, 3);   // Stretch = 3
```

Nếu layout có 400 px chiều rộng còn lại để chia:
- btn1 nhận: 1 phần → 100 px
- btn2 nhận: 3 phần → 300 px

### 7.2. Size Policy

Size policy quyết định cách widget phản ứng với layout:
- Có cho phép giãn không?
- Có muốn giãn không?
- Có nên ưu tiên widget này khi co/giãn không?

### 7.3. Spacer item

Spacer item là một đối tượng đặc biệt không hiển thị, có tác dụng nhằm chiếm chỗ trong layout. Khi layout tính toán vị trí các widget con, spacer được dùng để:
- Tạo khoảng trống cố định hoặc giãn linh hoạt giữa các widget.
- Cân bằng, đẩy widget ra biên trái/phải, trên/dưới.
- Giúp UI tự động co giãn khi cửa sổ thay đổi kích thước.

## 8. QString

QString lưu trữ chuỗi dưới dạng UNICODE UTF-16 và cung cấp nhiều method để thao tác với chuỗi.

Một số ví dụ:

- *Hiển thị chuỗi trong GUI*

```cpp
ui->label->setText("Temperature: " + QString::number(temp) + "°C");
```

- *Đọc chuỗi từ file*

```cpp
QFile file("data.txt");
if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QString content = file.readAll();
    file.close();
}
```

**Placeholder format**

Qt định nghĩa cú pháp `%1`, `%2`, `%3`, ... làm placeholder, đây là vị trí chèn giá trị vào chuỗi. Mỗi `arg()` sẽ thay lần lượt các placeholder này bằng giá trị tương ứng. Ví dụ như sau:

```cpp
QString s = QString("Hello %1, temperature is %2°C")
                .arg("BBB")
                .arg(27);
```

Hàm `arg` có nhiều overload hỗ trợ các kiểu dữ liệu khác nhau.

- Nhóm số nguyên

Cú pháp:

```cpp
QString QString::arg(
    int value,
    int fieldWidth = 0,
    int base = 10,
    QChar fillChar = QLatin1Char(' ')
) const;
```

Trong đó:
- `value`: Giá trị số cần thay thế vào placeholder.
- `fieldWidth`: Độ rộng tối thiểu của phần in ra.
  + Nếu > 0 → căn phải, chèn ký tự `fillChar` bên trái.
  + Nếu < 0 → căn trái, chèn ký tự `fillChar` bên phải.
- `base`: Hệ cơ số: 2 (nhị phân), 8 (bát phân), 10 (thập phân), 16 (thập lục phân).
- `fillChar`: Ký tự padding để đủ độ rộng.

- Nhóm số thực

Cú pháp:

```cpp
QString QString::arg(
    double value,
    int fieldWidth = 0,
    char format = 'g',
    int precision = -1,
    QChar fillChar = QLatin1Char(' ')
) const;
```

Trong đó:
- `value`: Giá trị dấu phẩy động cần thay vào.
- `fieldWidth`: Tương tự nhóm số nguyên.
- `format`: Kiểu định dạng số:
  + 'f': fixed-point
  + 'e': exponential (dạng khoa học, e.g. 3.14e+02)
  + 'g': general (Qt tự chọn giữa 'f' hoặc 'e')
- `precision`: Số chữ số sau dấu thập phân
- `fillChar`: Tương tự nhóm số nguyên.

- Nhóm ký tự

Cú pháp:

```cpp
QString QString::arg(
    QChar ch,
    int fieldWidth = 0,
    QChar fillChar = QLatin1Char(' ')
) const;
```

- Nhóm string

```cpp
QString QString::arg(
    const QString &value,
    int fieldWidth = 0,
    QChar fillChar = QLatin1Char(' ')
) const;
```

## 9. Các bước cross compiler Qt5 cho BBB

**Bước 1: Chuẩn bị Yocto**

```bash
# Clone Poky (Yocto base) - dùng Kirkstone (LTS)
mkdir ~/yocto && cd ~/yocto

git clone git://git.yoctoproject.org/poky.git -b kirkstone
cd poky
```

**Bước 2: Clone các layer cần thiết**

```bash
# Trong ~/yocto/poky/

# 1. Layer cho BBB
git clone git://git.openembedded.org/meta-openembedded -b kirkstone

# 2. Layer Qt5
git clone https://github.com/meta-qt5/meta-qt5.git -b kirkstone

# 3. Layer cho BBB (Texas Instruments)
git clone https://git.yoctoproject.org/meta-ti -b kirkstone
```

:::warning Lưu ý
Phải clone đúng branch với branch của poky, nếu không sẽ gặp lỗi.
:::

Kiểm tra cấu trúc:

```
~/yocto/poky/
├── meta/
├── meta-poky/
├── meta-openembedded/
│   ├── meta-oe/
│   ├── meta-python/
│   └── meta-networking/
├── meta-qt5/
└── meta-ti/
```

**Bước 3: Setup build environment**

```bash
cd ~/yocto/poky

# Tạo build directory
source oe-init-build-env ../build-bbb
# Lệnh này tự động cd vào build-bbb/
```

**Bước 4: Thêm các layer vào bblayers.conf**

```bash
vim conf/bblayers.conf
```

```bash
BBLAYERS ?= " \
  /home/$USER/.../yocto/poky/meta \
  /home/$USER/.../yocto/poky/meta-poky \
  /home/$USER/.../yocto/poky/meta-yocto-bsp \
  /home/$USER/.../yocto/poky/meta-openembedded/meta-oe \
  /home/$USER/.../yocto/poky/meta-openembedded/meta-python \
  /home/$USER/.../yocto/poky/meta-openembedded/meta-networking \
  /home/$USER/.../yocto/poky/meta-qt5 \
  /home/$USER/.../yocto/poky/meta-ti/meta-ti-bsp \
  "
```

**Bước 5: Cấu hình local.conf cho BBB**

```bash
vim conf/local.conf
```

Thêm/sửa các dòng sau:

```bash
# Target machine BBB
MACHINE = "beaglebone-yocto"

# Qt5 packages
IMAGE_INSTALL:append = " qtbase fontconfig"
PACKAGECONFIG_DISTRO:pn-qtbase = " linuxfb tslib fontconfig"
```

**Bước 7: Generate Qt SDK**

Để build Qt SDK, ta cần chạy recipe sau:

```bash
bitbake meta-toolchain-qt5
```

Qt SDK output tại:

```
build-bbb/tmp/deploy/sdk/
└── poky-glibc-x86_64-meta-toolchain-qt5-cortexa8hf-neon-beaglebone-yocto-toolchain-4.0.x.sh
```

**Bước 8: Install Qt SDK**

Sau khi build xong Qt SDK, ta sẽ có cần install để dễ dàng thiết lập môi trường phát triển ứng dụng. Để có thể cài đặt Qt SDK trên host, ta cần chạy file cài đặt như sau:

```bash
# Cấp quyền execute cho file shell
chmod +x home/$USER/.../poky/build-bbb/tmp/deploy/sdk/*.sh

./poky-glibc-x86_64-meta-toolchain-qt5-cortexa8hf-neon-beaglebone-yocto-toolchain-4.0.x.sh
```

Lúc này, trình cài đặt sẽ hỏi nơi ta muốn cài SDK. Nếu muốn sử dụng thư mục mặc định `/opt/poky/4.0.x`, ta chỉ cần nhấn Enter hai lần để tiếp tục.

Nếu không, ta có thể nhập một đường dẫn tùy chọn, ví dụ:

```bash
$ home/$USER/.../qt-sdk
```

Sau đó nhấn Enter hai lần để hoàn tất.

**Bước 9: Source environment Qt SDK**

Trước khi biên dịch bất kỳ ứng dụng nào bằng Qt SDK, ta cần thiết lập môi trường bằng cách chạy:

```bash
source /opt/poky/4.0.x/environment-setup-cortexa8hf-neon-poky-linux-gnueabi
```

Lệnh này sẽ tự động set một số biến môi trường:
- `OECORE_TARGET_SYSROOT`: Chứa tất cả các file của target trên máy host, bao gồm thư viện và header. Đây là những file sẽ chạy trên target như BBB.
- `OECORE_NATIVE_SYSROOT`: Chứa toolchain và thư viện để biên dịch trên host PC. Các file trong đây chỉ chạy trên máy tính, không dùng trên target.
- `CMAKE_PREFIX_PATH`: Nói cho CMake biết tìm package ở đâu - trỏ vào sysroot của BBB, không phải host.
- `CXX`, `CXXFLAGS` – dùng cho cross compiler.

Kiểm tra:

```bash
echo $CC
# arm-poky-linux-gnueabi-gcc -mfpu=neon -mfloat-abi=hard ...

echo $SDKTARGETSYSROOT
# /opt/poky/sdk/sysroots/cortexa8hf-neon-poky-linux-gnueabi
```

**Bước 10: Build project**

```bash
cd ~/tutorial/QT5_SmartFarm
mkdir build-bbb && cd build-bbb

cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$OECORE_NATIVE_SYSROOT/usr/share/cmake/OEToolchainConfig.cmake \
  -DCMAKE_PREFIX_PATH=$SDKTARGETSYSROOT/usr

make -j$(nproc)
```

## 10. Sử dụng Qt SDK với Qt Creator

Đầu tiên, ta cần cài đặt Qt Creator bằng command sau:

```bash
sudo apt update -y
sudo apt install -y qtcreator qtbase5-dev qt5-qmake cmake
```

Sau đó, ta sẽ export Qt SDK và chạy Qt Creator bằng command sau, mục tiêu của việc này là để Qt Creator có thể sử dụng các biến môi trường mà Qt SDK đã export:

```bash
source /opt/poky/4.0.x/environment-setup-cortexa8hf-neon-poky-linux-gnueabi
qt-creator &
```