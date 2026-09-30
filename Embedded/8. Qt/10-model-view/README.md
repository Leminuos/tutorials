Bài toán: hiển thị dữ liệu dạng danh sách và bảng

HMI nào cũng có những màn hình kiểu này:
- Bảng thông số: tên thông số, giá trị hiện tại, đơn vị, ngưỡng.
- Danh sách cảnh báo: thời gian, nội dung, mức độ, đã xác nhận hay chưa.
- Lịch sử vận hành: hàng trăm, hàng nghìn dòng ghi nhận.

Cách làm đơn giản nhất là tạo một `QLabel` cho mỗi ô rồi `setText()` khi giá trị đổi. Cách này nhanh chóng trở nên rối:

- Dữ liệu nằm rải rác trong các widget, muốn lấy lại giá trị phải đọc ngược từ chữ trên nhãn.
- Danh sách cảnh báo dài dần. Tạo mỗi dòng một widget sẽ tốn bộ nhớ và chậm.
- Cùng một dữ liệu muốn hiện ở hai nơi (bảng và đồ thị) thì phải cập nhật hai lần.

Qt giải quyết bằng cách tách việc lưu dữ liệu khỏi việc hiển thị:

```
                 +--------------------+
                 |       Model        |  ← giữ dữ liệu, trả lời câu hỏi
                 |  (danh sách cảnh   |
                 |   báo, thông số)   |
                 +---------┬----------+
          hỏi dữ liệu      |      báo dữ liệu đã thay đổi
            +--------------+--------------+
            |              |              |
            v              v              v
     +------------+ +------------+ +------------+
     | QTableView | | QListView  | | View khác  |  ← chỉ lo hiển thị
     +------------+ +------------+ +------------+
           |
           v
     +------------+
     |  Delegate  |  ← vẽ và chỉnh sửa từng ô
     +------------+
```

- **Model** giữ dữ liệu (hoặc biết cách lấy dữ liệu) và cung cấp nó qua một giao diện chuẩn. Model không biết gì về cách dữ liệu được hiển thị.
- **View** hiển thị dữ liệu. Nó hỏi model rồi vẽ ra màn hình. View không lưu dữ liệu.
- **Delegate** là thành phần view dùng để vẽ từng ô.

Hai đặc điểm quan trọng rút ra từ kiến trúc này:
- View chỉ hỏi dữ liệu của những ô đang hiển thị. Danh sách 5000 cảnh báo trên màn hình chỉ vừa 6 dòng thì view chỉ hỏi khoảng 6 dòng. Đây là lý do Model/View xử lý tốt dữ liệu lớn trên BBB.
- Một model dùng chung cho nhiều view. Khi model báo có thay đổi, mọi view đang hiển thị nó tự cập nhật.

## Chọn loại model và view

| Cần | Model | View |
|---|---|---|
| Danh sách chuỗi đơn giản | `QStringListModel` | `QListView` |
| Bảng nhỏ, không muốn viết lớp | `QStandardItemModel` | `QTableView` |
| Dữ liệu của riêng ta, cập nhật thường xuyên | Tự viết từ `QAbstractTableModel` / `QAbstractListModel` | `QTableView` / `QListView` |

Qt còn có `QTableWidget` và `QListWidget`: view có sẵn model bên trong, thao tác theo từng item. Chúng tiện cho danh sách tĩnh nhỏ, nhưng dữ liệu vẫn bị gắn chặt vào widget. Với dữ liệu cảm biến, nên tự viết model.

## QModelIndex và role

View hỏi model qua hàm `data(index, role)`:

- **`QModelIndex`** xác định một ô: `index.row()`, `index.column()`. Với bảng và danh sách, `parent` luôn là index không hợp lệ (gốc).
- **Role** cho biết view đang hỏi khía cạnh nào của ô.

| Role | View hỏi | Trả về |
|---|---|---|
| `Qt::DisplayRole` | Chữ hiển thị | `QString` hoặc số |
| `Qt::TextAlignmentRole` | Căn lề | `int` từ `Qt::Alignment` |
| `Qt::ForegroundRole` | Màu chữ | `QBrush` |
| `Qt::BackgroundRole` | Màu nền ô | `QBrush` |
| `Qt::DecorationRole` | Biểu tượng | `QIcon`, `QPixmap` hoặc `QColor` |

Role nào không xử lý thì trả về `QVariant()` rỗng, view sẽ dùng giá trị mặc định.

## Viết model dạng bảng

Kế thừa `QAbstractTableModel` và override:

| Hàm | Bắt buộc | Nhiệm vụ |
|---|---|---|
| `rowCount()` | Có | Số hàng |
| `columnCount()` | Có | Số cột |
| `data()` | Có | Nội dung ô theo role |
| `headerData()` | Không | Tiêu đề cột/hàng |

Với `QAbstractListModel` chỉ cần `rowCount()` và `data()`.

Khi dữ liệu đổi, model **phải** báo cho view:

| Thay đổi | Cách báo |
|---|---|
| Giá trị trong ô đổi | `emit dataChanged(topLeft, bottomRight, roles)` |
| Thêm hàng | `beginInsertRows(parent, first, last)` -> sửa dữ liệu -> `endInsertRows()` |
| Xóa hàng | `beginRemoveRows(...)` -> sửa dữ liệu -> `endRemoveRows()` |
| Đổi toàn bộ | `beginResetModel()` -> sửa dữ liệu -> `endResetModel()` |

:::tip Báo thay đổi càng hẹp càng tốt
Trên BBB, vẽ lại qua SPI tốn thời gian. `dataChanged` chỉ cho một ô khiến view chỉ vẽ lại ô đó. `beginResetModel()`/`endResetModel()` bắt view dựng lại toàn bộ, chỉ dùng khi cả tập dữ liệu thay đổi, ví dụ khi nạp file cấu hình mới.
:::

## View cho màn hình cảm ứng

Mặc định `QTableView` và `QListView` được thiết kế cho chuột và bàn phím: cho sửa ô, cho chọn, cuộn bằng thanh cuộn. Trên HMI, ta thường tắt bớt:

- `setEditTriggers(NoEditTriggers)`: không cho sửa trực tiếp trong ô.
- `setSelectionMode(NoSelection)`: chạm vào không làm hàng bị tô chọn.
- `QScroller::grabGesture(viewport, QScroller::LeftMouseButtonGesture)`: kéo bằng ngón tay để cuộn, có quán tính, giống điện thoại.
- `verticalHeader()->setDefaultSectionSize(24)`: chiều cao hàng cố định.

## Ví dụ

Màn hình giám sát: bảng 4 thông số ở trên, danh sách cảnh báo ở dưới. Mỗi giây, giá trị dao động ngẫu nhiên. Giá trị vượt ngưỡng hiện màu đỏ, và lần đầu vượt ngưỡng thì thêm một dòng cảnh báo. Mỗi thông số là một `struct Parameter` gồm tên, giá trị, đơn vị và ngưỡng.

```
+------------------------------+
| Parameter      Value   Limit |
| Temperature  25.3 °C    30.0 |
| Humidity      61.2 %    80.0 |
| Pressure  1013.0 hPa  1030.0 |
| Current      5.2 A       5.0 |  <- chữ đỏ
| Alarms                       |
| 18:20:05  Current over limit |
| 18:19:41  Temperature over li|
+------------------------------+
```

```
example/
+-- CMakeLists.txt
+-- parametermodel.h
+-- parametermodel.cpp
+-- alarmmodel.h
+-- alarmmodel.cpp
+-- monitorscreen.h
+-- monitorscreen.cpp
+-- main.cpp
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.16)
project(model-view VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(model-view
    parametermodel.h
    parametermodel.cpp
    alarmmodel.h
    alarmmodel.cpp
    monitorscreen.h
    monitorscreen.cpp
    main.cpp
)

target_link_libraries(model-view PRIVATE Qt5::Widgets)
```
::: explain [Giải thích chi tiết]
- `set(CMAKE_AUTOMOC ON)`: tự chạy moc cho các lớp có `Q_OBJECT`.
- `COMPONENTS Widgets` và `Qt5::Widgets`: ứng dụng có giao diện nên cần Qt Widgets; Qt Core và Qt Gui được kéo theo tự động.
- Mỗi model một cặp `.h`/`.cpp` riêng, tách khỏi màn hình hiển thị.
:::

```cpp [parametermodel.h]
#ifndef PARAMETERMODEL_H
#define PARAMETERMODEL_H

#include <QAbstractTableModel>
#include <QVector>

// Một thông số đo: tên, giá trị, đơn vị, ngưỡng cảnh báo
struct Parameter
{
    QString name;
    double value = 0.0;
    QString unit;
    double limit = 0.0;
};

// Bảng thông số: mỗi hàng một Parameter, 3 cột
class ParameterModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { NameColumn, ValueColumn, LimitColumn, ColumnCount };

    explicit ParameterModel(QObject *parent = nullptr);

    // Bốn hàm bắt buộc/hay dùng của một model dạng bảng
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    // API riêng để phần còn lại của chương trình cập nhật dữ liệu
    void addParameter(const Parameter &parameter);
    void setValue(int row, double value);
    const Parameter &parameter(int row) const;

private:
    QVector<Parameter> m_items;
};

#endif // PARAMETERMODEL_H
```
::: explain [Giải thích chi tiết]
- `enum Column`: đặt tên cho chỉ số cột, tránh số "ma thuật" 0, 1, 2 rải rác trong code. `ColumnCount` luôn nằm cuối nên tự bằng số cột.
- `addParameter()`, `setValue()`: phần còn lại của chương trình chỉ gọi các hàm này, không đụng tới view.
:::

```cpp [parametermodel.cpp]
#include "parametermodel.h"

#include <QBrush>

ParameterModel::ParameterModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ParameterModel::rowCount(const QModelIndex &parent) const
{
    // Bảng phẳng: chỉ gốc (parent không hợp lệ) mới có hàng
    return parent.isValid() ? 0 : m_items.size();
}

int ParameterModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant ParameterModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return QVariant();

    const Parameter &p = m_items.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case NameColumn:
            return p.name;
        case ValueColumn:
            return QString("%1 %2").arg(p.value, 0, 'f', 1).arg(p.unit);
        case LimitColumn:
            return QString::number(p.limit, 'f', 1);
        }
        break;
    case Qt::TextAlignmentRole:
        if (index.column() != NameColumn)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        break;
    case Qt::ForegroundRole:
        // Giá trị vượt ngưỡng hiện màu đỏ
        if (index.column() == ValueColumn && p.value > p.limit)
            return QBrush(Qt::red);
        break;
    }
    return QVariant();
}

QVariant ParameterModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case NameColumn:
        return "Parameter";
    case ValueColumn:
        return "Value";
    case LimitColumn:
        return "Limit";
    }
    return QVariant();
}

void ParameterModel::addParameter(const Parameter &parameter)
{
    const int row = m_items.size();
    beginInsertRows(QModelIndex(), row, row); // báo trước cho view
    m_items.append(parameter);
    endInsertRows();                          // view tự thêm hàng
}

void ParameterModel::setValue(int row, double value)
{
    if (row < 0 || row >= m_items.size())
        return;

    m_items[row].value = value;

    // Chỉ báo ô Giá trị của hàng này đã đổi: view chỉ vẽ lại đúng ô đó
    const QModelIndex cell = index(row, ValueColumn);
    emit dataChanged(cell, cell, {Qt::DisplayRole, Qt::ForegroundRole});
}

const Parameter &ParameterModel::parameter(int row) const
{
    return m_items.at(row);
}
```
::: explain [Giải thích chi tiết]
- `rowCount()` trả về 0 khi `parent.isValid()`: quy ước của model phẳng. View có thể hỏi "ô này có hàng con không", và câu trả lời phải là không.
- `data()`: kiểm tra `index` trước, rồi trả về theo role. Cột Giá trị ghép số với đơn vị bằng `QString("%1 %2").arg(...)`, số làm tròn 1 chữ số sau dấu phẩy. Các cột số căn phải để các chữ số thẳng hàng.
- `Qt::ForegroundRole` trả về `QBrush(Qt::red)` khi vượt ngưỡng.
- `addParameter()`: `beginInsertRows()` phải gọi **trước** khi sửa `m_items`, `endInsertRows()` gọi **sau**.
- `setValue()`: `dataChanged` kèm danh sách role đã đổi, view chỉ cập nhật những gì cần.
:::

```cpp [alarmmodel.h]
#ifndef ALARMMODEL_H
#define ALARMMODEL_H

#include <QAbstractListModel>
#include <QTime>
#include <QVector>

// Danh sách cảnh báo: mới nhất ở trên, giữ tối đa MaxAlarms mục
class AlarmModel : public QAbstractListModel
{
    Q_OBJECT

public:
    static constexpr int MaxAlarms = 50;

    explicit AlarmModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void addAlarm(const QString &message);

private:
    struct Alarm
    {
        QTime time;
        QString message;
    };
    QVector<Alarm> m_items;
};

#endif // ALARMMODEL_H
```
::: explain [Giải thích chi tiết]
- `MaxAlarms = 50`: giới hạn số mục. Thiết bị chạy nhiều tuần, danh sách không giới hạn sẽ ăn dần bộ nhớ.
- `struct Alarm` khai báo bên trong lớp vì chỉ model này dùng.
:::

```cpp [alarmmodel.cpp]
#include "alarmmodel.h"

AlarmModel::AlarmModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AlarmModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant AlarmModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size() || role != Qt::DisplayRole)
        return QVariant();

    const Alarm &a = m_items.at(index.row());
    return a.time.toString("HH:mm:ss") + "  " + a.message;
}

void AlarmModel::addAlarm(const QString &message)
{
    // Thêm vào đầu danh sách
    beginInsertRows(QModelIndex(), 0, 0);
    m_items.prepend({QTime::currentTime(), message});
    endInsertRows();

    // Bỏ mục cũ nhất để bộ nhớ không tăng mãi
    if (m_items.size() > MaxAlarms) {
        const int last = m_items.size() - 1;
        beginRemoveRows(QModelIndex(), last, last);
        m_items.removeLast();
        endRemoveRows();
    }
}
```
::: explain [Giải thích chi tiết]
- `beginInsertRows(QModelIndex(), 0, 0)`: báo sẽ chèn một hàng ở vị trí 0, tức đầu danh sách.
- Khi vượt `MaxAlarms`, xóa mục cuối (cũ nhất) với cặp `beginRemoveRows`/`endRemoveRows`.
:::

```cpp [monitorscreen.h]
#ifndef MONITORSCREEN_H
#define MONITORSCREEN_H

#include <QVector>
#include <QWidget>

class AlarmModel;
class ParameterModel;

// Màn hình giám sát: bảng thông số ở trên, danh sách cảnh báo ở dưới
class MonitorScreen : public QWidget
{
    Q_OBJECT

public:
    explicit MonitorScreen(QWidget *parent = nullptr);

private:
    void simulate();

    ParameterModel *m_parameters;
    AlarmModel *m_alarms;
    QVector<bool> m_overLimit;   // trạng thái vượt ngưỡng lần trước của mỗi hàng
};

#endif // MONITORSCREEN_H
```
::: explain [Giải thích chi tiết]
- `class AlarmModel;`, `class ParameterModel;`: khai báo trước (forward declaration). Header chỉ dùng con trỏ nên không cần include đầy đủ, giảm thời gian biên dịch.
- `simulate()`: hàm giả lập số liệu, được timer gọi định kỳ.
- `m_overLimit`: mỗi hàng một cờ, nhớ lần trước hàng đó có vượt ngưỡng không.
:::

```cpp [monitorscreen.cpp]
#include "monitorscreen.h"
#include "alarmmodel.h"
#include "parametermodel.h"

#include <QHeaderView>
#include <QLabel>
#include <QListView>
#include <QRandomGenerator>
#include <QScroller>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

MonitorScreen::MonitorScreen(QWidget *parent)
    : QWidget(parent)
    , m_parameters(new ParameterModel(this))
    , m_alarms(new AlarmModel(this))
{
    setFixedSize(320, 240);

    m_parameters->addParameter({"Temperature", 25.0, "°C", 30.0});
    m_parameters->addParameter({"Humidity", 60.0, "%", 80.0});
    m_parameters->addParameter({"Pressure", 1013.0, "hPa", 1030.0});
    m_parameters->addParameter({"Current", 4.0, "A", 5.0});
    m_overLimit.fill(false, m_parameters->rowCount());

    // Bảng: chỉ xem, không sửa, không chọn
    auto *table = new QTableView;
    table->setModel(m_parameters);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(24);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    // Danh sách cảnh báo: kéo bằng ngón tay để cuộn
    auto *alarmList = new QListView;
    alarmList->setModel(m_alarms);
    alarmList->setSelectionMode(QAbstractItemView::NoSelection);
    alarmList->setFocusPolicy(Qt::NoFocus);
    alarmList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    QScroller::grabGesture(alarmList->viewport(), QScroller::LeftMouseButtonGesture);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(table);
    layout->addWidget(new QLabel("Alarms"));
    layout->addWidget(alarmList, 1);

    // Giả lập cảm biến: mỗi giây đổi giá trị; khi có phần cứng thì thay bằng code đọc thật
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MonitorScreen::simulate);
    timer->start(1000);
}

void MonitorScreen::simulate()
{
    for (int row = 0; row < m_parameters->rowCount(); ++row) {
        const Parameter &p = m_parameters->parameter(row);

        // Dao động ngẫu nhiên quanh giá trị hiện tại, biên độ 5% ngưỡng
        const double delta = (QRandomGenerator::global()->generateDouble() - 0.5) * p.limit * 0.1;
        m_parameters->setValue(row, p.value + delta);

        // Chỉ báo khi vừa chuyển từ bình thường sang vượt ngưỡng
        const bool over = p.value > p.limit;
        if (over && !m_overLimit.at(row))
            m_alarms->addAlarm(QString("%1 over limit").arg(p.name));
        m_overLimit[row] = over;
    }
}
```
::: explain [Giải thích chi tiết]
- Hai model có parent là `this`. View không sở hữu model: một model có thể dùng cho nhiều view.
- `setSizeAdjustPolicy(AdjustToContents)`: bảng cao vừa đủ cho 4 hàng, phần còn lại dành cho danh sách cảnh báo.
- `QScroller::grabGesture(...)`: bật cuộn bằng cách kéo trên danh sách. `ScrollPerPixel` giúp cuộn mượt thay vì nhảy từng dòng.
- `simulate()`: `p` là tham chiếu tới phần tử trong model, nên sau `setValue()`, `p.value` đã là giá trị mới. `m_overLimit` nhớ trạng thái lần trước để chỉ báo một lần khi vừa vượt ngưỡng, không báo lặp lại mỗi giây.
- `QRandomGenerator::global()->generateDouble()`: số ngẫu nhiên trong khoảng [0, 1).
:::

```cpp [main.cpp]
#include <QApplication>

#include "monitorscreen.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    MonitorScreen screen;
    if (QGuiApplication::platformName() == "linuxfb") {
        screen.setWindowFlags(Qt::FramelessWindowHint);
        screen.showFullScreen();
    } else {
        screen.show();
    }

    return app.exec();
}
```
::: explain [Giải thích chi tiết]
- `QGuiApplication::platformName() == "linuxfb"`: đang chạy trên board. Khi đó cửa sổ bỏ viền (`Qt::FramelessWindowHint`) và phủ toàn màn hình; trên máy tính thì hiện như cửa sổ thường.
- `setWindowFlags()` phải gọi trước `showFullScreen()` hoặc `show()`.
:::
::::

### Build và chạy

```bash
cd example
cmake -S . -B build
cmake --build build
./build/model-view
```

Đợi vài chục giây sẽ có thông số vượt ngưỡng và dòng cảnh báo mới xuất hiện ở đầu danh sách.

:::note Model không phụ thuộc giao diện
`ParameterModel` không biết gì về bảng hay màn hình. Khi có phần cứng thật, chỉ cần thay `simulate()` bằng code đọc cảm biến gọi `setValue()`. Chính model này cũng dùng được làm nguồn dữ liệu cho `ListView` của QML nếu sau này chuyển giao diện sang Qt Quick.
:::
