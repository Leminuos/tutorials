Trong C, khi một module cần báo cho module khác biết có sự kiện xảy ra, ta thường dùng con trỏ hàm làm callback, kèm một con trỏ `void*` để truyền ngữ cảnh. Với một nơi nhận thì ổn, nhưng khi có nhiều nơi cùng cần nhận, ta phải tự quản lý mảng con trỏ hàm, mảng ngữ cảnh, tự viết hàm đăng ký, hủy đăng ký, và mọi thứ đều không được kiểm tra kiểu. **Observer** là pattern giải quyết bài toán này một cách có tổ chức: một đối tượng phát thông báo cho nhiều đối tượng khác mà không cần biết chúng là ai.

## Vấn đề thực tế

Giả sử ta có lớp `TemperatureSensor` đọc nhiệt độ định kỳ. Mỗi khi có giá trị mới, ba thành phần cần biết: màn hình hiển thị, bộ ghi nhật ký và bộ cảnh báo vượt ngưỡng. Cách viết đơn giản nhất là để cảm biến gọi trực tiếp từng thành phần:

```cpp
class TemperatureSensor {
public:
    TemperatureSensor(Display& display, Logger& logger, Alarm& alarm)
        : display_(display), logger_(logger), alarm_(alarm) {}

    void update()
    {
        float celsius = readHardware();
        display_.showTemperature(celsius);
        logger_.record(celsius);
        alarm_.check(celsius);
    }

private:
    Display& display_;
    Logger&  logger_;
    Alarm&   alarm_;
};
```

Code chạy được nhưng có nhiều vấn đề:

- **Cảm biến phụ thuộc vào mọi nơi dùng nó.** Lớp `TemperatureSensor` phải include và biết chi tiết của `Display`, `Logger`, `Alarm`, dù việc của nó chỉ là đọc nhiệt độ.
- **Thêm nơi nhận là phải sửa cảm biến.** Muốn gửi thêm dữ liệu lên server qua MQTT, ta phải mở lớp `TemperatureSensor` ra sửa.
- **Không dùng lại được.** Mang lớp cảm biến sang dự án khác không có màn hình thì phải sửa code.
- **Khó test.** Muốn test riêng cảm biến, ta vẫn phải tạo đủ cả ba đối tượng kia.

## Ý tưởng của Observer

Observer đảo ngược sự phụ thuộc. Thay vì cảm biến biết hết mọi nơi nhận, cảm biến chỉ giữ một danh sách người theo dõi (**observer**). Ai muốn nhận thông báo thì tự đăng ký vào danh sách. Khi có giá trị mới, cảm biến duyệt danh sách và báo cho tất cả, mà không cần biết từng người là ai.

```
                 +-----------------------+
                 |   TemperatureSensor   |   (đối tượng được theo dõi)
                 |  danh sách observer   |
                 +-----------+-----------+
                             | thông báo giá trị mới
         +-------------------+-------------------+
         v                   v                   v
    +---------+         +---------+         +---------+
    | Display |         | Logger  |         |  Alarm  |
    +---------+         +---------+         +---------+
       observer            observer            observer
```

Đối tượng phát thông báo thường được gọi là **subject**, các đối tượng nhận gọi là **observer**. Có hai cách cài đặt phổ biến trong C++: dùng interface với hàm ảo, và dùng lambda với `std::function`.

## Observer bằng interface

Trước hết, ta định nghĩa một interface chung cho mọi observer, dùng hàm thuần ảo đã học ở Bài C6:

```cpp
class ITemperatureObserver {
public:
    virtual ~ITemperatureObserver() = default;
    virtual void onTemperatureChanged(float celsius) = 0;
};
```

Cảm biến chỉ làm việc với interface này, không biết gì về các lớp cụ thể:

```cpp
#include <vector>
#include <algorithm>

class TemperatureSensor {
public:
    void attach(ITemperatureObserver* observer)
    {
        observers_.push_back(observer);
    }

    void detach(ITemperatureObserver* observer)
    {
        // xóa observer khỏi danh sách (cách dùng erase kết hợp remove xem ở Bài C9)
        observers_.erase(
            std::remove(observers_.begin(), observers_.end(), observer),
            observers_.end());
    }

    void update(float celsius)    // giả lập vừa đọc được giá trị mới
    {
        for (auto* observer : observers_) {
            observer->onTemperatureChanged(celsius);
        }
    }

private:
    std::vector<ITemperatureObserver*> observers_;
};
```

Mỗi thành phần muốn nhận thông báo chỉ cần kế thừa interface và cài đặt hàm `onTemperatureChanged`:

```cpp
class Display : public ITemperatureObserver {
public:
    void onTemperatureChanged(float celsius) override
    {
        std::printf("[Display] Temperature: %.1f degC\n", celsius);
    }
};

class Logger : public ITemperatureObserver {
public:
    void onTemperatureChanged(float celsius) override
    {
        std::printf("[Logger] Recorded: %.1f\n", celsius);
    }
};

class Alarm : public ITemperatureObserver {
public:
    Alarm(float threshold) : threshold_(threshold) {}

    void onTemperatureChanged(float celsius) override
    {
        if (celsius > threshold_) {
            std::printf("[Alarm] Above threshold %.1f degC!\n", threshold_);
        }
    }

private:
    float threshold_;
};
```

Ghép lại:

```cpp
TemperatureSensor sensor;
Display display;
Logger  logger;
Alarm   alarm(40.0f);

sensor.attach(&display);
sensor.attach(&logger);
sensor.attach(&alarm);

sensor.update(25.3f);
sensor.update(41.2f);
```

Kết quả:

```
[Display] Temperature: 25.3 degC
[Logger] Recorded: 25.3
[Display] Temperature: 41.2 degC
[Logger] Recorded: 41.2
[Alarm] Above threshold 40.0 degC!
```

Giờ muốn thêm bộ gửi MQTT, ta chỉ viết thêm một lớp kế thừa `ITemperatureObserver` rồi gọi `attach`, không sửa một dòng nào trong `TemperatureSensor`.

Danh sách lưu con trỏ thay vì tham chiếu vì hai lý do: `std::vector` không chứa được tham chiếu, và hàm `detach` cần so sánh địa chỉ để tìm đúng observer cần xóa.

### Quản lý vòng đời observer

Cách cài đặt trên có một lỗ hổng nguy hiểm: cảm biến giữ con trỏ tới observer, nhưng không biết observer còn sống hay không. Nếu observer bị hủy mà quên `detach`, lần thông báo tiếp theo sẽ gọi hàm qua một con trỏ treo:

```cpp
TemperatureSensor sensor;
{
    Display display;
    sensor.attach(&display);
}                           // display bị hủy, nhưng vẫn nằm trong danh sách

sensor.update(25.0f);       // gọi hàm trên đối tượng đã bị hủy: hành vi không xác định
```

Áp dụng RAII từ Bài C4, ta để observer tự đăng ký trong constructor và tự hủy đăng ký trong destructor. Như vậy không thể quên được nữa:

```cpp
class Display : public ITemperatureObserver {
public:
    Display(TemperatureSensor& sensor) : sensor_(sensor)
    {
        sensor_.attach(this);
    }

    ~Display() override
    {
        sensor_.detach(this);
    }

    // cấm sao chép (Bài C7): bản sao sẽ không được đăng ký đúng cách
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    void onTemperatureChanged(float celsius) override
    {
        std::printf("[Display] Temperature: %.1f degC\n", celsius);
    }

private:
    TemperatureSensor& sensor_;
};
```

Ta cấm sao chép vì bản sao của `Display` sẽ có địa chỉ khác, chưa từng `attach`, nhưng destructor của nó lại gọi `detach`, gây hành vi khó lường.

:::warning Subject phải sống lâu hơn observer
Destructor của observer còn gọi `sensor_.detach()`, nên cảm biến phải bị hủy sau mọi observer của nó. Các biến cục bộ bị hủy theo thứ tự ngược với thứ tự khai báo, nên ta chỉ cần khai báo subject trước các observer.
:::

```cpp
TemperatureSensor sensor;      // khai báo trước → bị hủy sau cùng
Display display(sensor);       // bị hủy trước sensor
```

## Observer bằng lambda và std::function

Cách dùng interface buộc mọi observer phải là một class kế thừa. Với những việc nhỏ như "in ra màn hình" hay "lưu giá trị lớn nhất", viết hẳn một class là quá cồng kềnh. Cách thứ hai cho phép đăng ký bất kỳ hàm nào có đúng chữ ký, kể cả lambda (Bài C9):

```cpp
#include <functional>
#include <vector>
#include <algorithm>

class TemperatureSensor {
public:
    using Callback = std::function<void(float)>;

    int subscribe(Callback callback)
    {
        int id = nextId_++;
        subscribers_.push_back({id, std::move(callback)});
        return id;                // trả về mã để hủy đăng ký sau này
    }

    void unsubscribe(int id)
    {
        subscribers_.erase(
            std::remove_if(subscribers_.begin(), subscribers_.end(),
                           [id](const Subscriber& s) { return s.id == id; }),
            subscribers_.end());
    }

    void update(float celsius)
    {
        for (auto& s : subscribers_) {
            s.callback(celsius);
        }
    }

private:
    struct Subscriber {
        int      id;
        Callback callback;
    };

    std::vector<Subscriber> subscribers_;
    int nextId_ = 0;
};
```

Vì lambda không có địa chỉ ổn định để so sánh như con trỏ observer, hàm `subscribe` trả về một mã số, dùng để hủy đăng ký sau này.

Cách sử dụng gọn hơn hẳn:

```cpp
TemperatureSensor sensor;

int logId = sensor.subscribe([](float celsius) {
    std::printf("[Logger] %.1f\n", celsius);
});

float maxTemp = -100.0f;
sensor.subscribe([&maxTemp](float celsius) {
    if (celsius > maxTemp) {
        maxTemp = celsius;
    }
});

sensor.update(25.3f);         // in ra: [Logger] 25.3
sensor.update(27.8f);         // in ra: [Logger] 27.8
sensor.unsubscribe(logId);
sensor.update(26.1f);         // không in gì, chỉ cập nhật maxTemp
// maxTemp = 27.8
```

Ta vẫn dùng được với các class có sẵn bằng cách gọi hàm thành viên bên trong lambda:

```cpp
Alarm alarm(40.0f);
sensor.subscribe([&alarm](float celsius) { alarm.check(celsius); });
```

Lúc này `Alarm` không cần kế thừa interface nào cả.

:::note Lambda capture tham chiếu cũng có vấn đề vòng đời
Lambda capture theo tham chiếu (`[&maxTemp]`, `[&alarm]`) mang đúng vấn đề vòng đời như cách 1. Nếu biến được capture bị hủy mà lambda vẫn còn trong danh sách, lần thông báo sau sẽ truy cập vùng nhớ không hợp lệ.
:::

### Hủy đăng ký tự động bằng đối tượng Subscription

Để giải quyết vấn đề vòng đời cho cách 2, ta kết hợp RAII với di chuyển (Bài C7): thay vì trả về mã số, `subscribe` trả về một đối tượng `Subscription`. Khi đối tượng này bị hủy, nó tự động hủy đăng ký.

```cpp
class TemperatureSensor;   // khai báo trước

class Subscription {
public:
    Subscription(TemperatureSensor* sensor, int id) : sensor_(sensor), id_(id) {}
    ~Subscription();   // định nghĩa sau khi TemperatureSensor đã đầy đủ

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept
        : sensor_(other.sensor_), id_(other.id_)
    {
        other.sensor_ = nullptr;     // nguồn không còn quản lý đăng ký nào
    }

    Subscription& operator=(Subscription&&) = delete;   // giữ ví dụ đơn giản

private:
    TemperatureSensor* sensor_;
    int id_;
};
```

Trong `TemperatureSensor`, hàm `subscribe` đổi thành:

```cpp
Subscription subscribe(Callback callback)
{
    int id = nextId_++;
    subscribers_.push_back({id, std::move(callback)});
    return Subscription(this, id);
}
```

Và destructor của `Subscription`, đặt sau định nghĩa đầy đủ của `TemperatureSensor`:

```cpp
Subscription::~Subscription()
{
    if (sensor_ != nullptr) {
        sensor_->unsubscribe(id_);
    }
}
```

Giờ thời gian đăng ký gắn liền với thời gian sống của đối tượng `Subscription`:

```cpp
TemperatureSensor sensor;
{
    float maxTemp = -100.0f;
    Subscription sub = sensor.subscribe([&maxTemp](float c) {
        if (c > maxTemp) maxTemp = c;
    });

    sensor.update(30.0f);   // lambda được gọi
}                           // sub bị hủy → tự hủy đăng ký, trước khi maxTemp biến mất

sensor.update(31.0f);       // an toàn: lambda không còn trong danh sách
```

Thông thường, một class observer giữ `Subscription` làm thành viên, nên khi đối tượng bị hủy, việc hủy đăng ký diễn ra tự động.

## So sánh hai cách cài đặt

| | Interface | Lambda + `std::function` |
|---|---|---|
| Observer phải là class kế thừa | Có | Không |
| Đăng ký hàm nhỏ, viết tại chỗ | Cồng kềnh | Rất gọn |
| Một class nhận nhiều loại sự kiện | Kế thừa nhiều interface | Đăng ký nhiều lambda |
| Chi phí | Một lần gọi hàm ảo | `std::function` có thể cấp phát heap khi lambda capture nhiều dữ liệu |
| Dễ đọc khi nhìn vào class observer | Rõ ràng: thấy ngay class nhận sự kiện gì | Phải tìm chỗ gọi `subscribe` |

Trong ứng dụng Linux nhúng, cách lambda thường được ưa dùng hơn nhờ sự linh hoạt. Cách interface phù hợp khi observer là các module lớn, ổn định, hoặc khi cần tránh cấp phát động.

## Những vấn đề cần chú ý khi cài đặt

**Hủy đăng ký ngay trong lúc đang thông báo.** Nếu một callback gọi `unsubscribe` (ví dụ bộ cảnh báo chỉ báo một lần rồi tự hủy), vector bị sửa trong khi vòng lặp `update` vẫn đang duyệt nó, dẫn đến hành vi không xác định. Cách xử lý đơn giản là duyệt trên một bản sao của danh sách:

```cpp
void update(float celsius)
{
    auto snapshot = subscribers_;     // sao chép danh sách hiện tại
    for (auto& s : snapshot) {
        s.callback(celsius);
    }
}
```

Cách này tốn thêm một lần sao chép mỗi lần thông báo, chấp nhận được khi số observer nhỏ. Với tần suất thông báo cao, ta có thể đánh dấu observer cần xóa rồi mới xóa thật sau khi vòng lặp kết thúc.

**Callback chạy chậm làm chậm subject.** Mọi callback chạy tuần tự trong hàm `update` của cảm biến. Nếu bộ ghi nhật ký mất 50 ms để ghi thẻ nhớ, cảm biến cũng bị chậm theo. Observer nên xử lý nhanh; việc nặng nên được đẩy vào hàng đợi để luồng khác xử lý (Bài C15).

**Thông báo từ luồng khác.** Nếu cảm biến được đọc ở một luồng riêng, các callback cũng chạy trên luồng đó. Observer nào truy cập dữ liệu dùng chung với luồng chính phải được bảo vệ bằng mutex (Bài C18), và danh sách observer cũng vậy nếu có đăng ký hoặc hủy đăng ký từ luồng khác.

## Khi nào không nên dùng

Observer không phải lúc nào cũng là lựa chọn tốt:

- **Chỉ có đúng một nơi nhận và không thay đổi.** Gọi hàm trực tiếp đơn giản và dễ đọc hơn. Đừng dựng cả cơ chế đăng ký chỉ cho một lời gọi.
- **Luồng xử lý trở nên khó theo dõi.** Khi đọc `sensor.update()`, ta không thấy được những gì sẽ xảy ra tiếp theo; phải tìm mọi chỗ đăng ký mới biết. Lạm dụng Observer khắp nơi khiến chương trình rất khó debug.
- **Thông báo dây chuyền.** Observer A nhận thông báo rồi cập nhật B, B lại thông báo ngược về A, có thể tạo vòng lặp vô hạn.
- **Thứ tự xử lý quan trọng.** Observer không đảm bảo về mặt thiết kế rằng nơi nhận nào chạy trước. Nếu bộ cảnh báo phải chạy trước bộ ghi nhật ký, nên gọi trực tiếp theo đúng thứ tự.

## Ứng dụng trong nhúng

Observer xuất hiện ở hầu hết ứng dụng nhúng có nhiều module. Dữ liệu từ cảm biến được phân phối tới màn hình, nhật ký, bộ cảnh báo và kết nối mạng. Sự kiện nút bấm được báo tới các màn hình đang hiển thị. Trạng thái kết nối (mất mạng, có mạng trở lại) được báo cho mọi module cần gửi dữ liệu. Nhờ Observer, module phát sự kiện không phụ thuộc vào các module nhận, nên có thể dùng lại ở dự án khác và test riêng được.
