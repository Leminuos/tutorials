Bộ đệm vòng (**ring buffer**) là cấu trúc quen thuộc với người làm nhúng bằng C: một mảng cố định, hai chỉ số đọc và ghi chạy vòng quanh, thường dùng làm bộ đệm nhận UART. Tuy quen thuộc, nó lại là nơi rất hay sinh lỗi: nhầm giữa trạng thái đầy và rỗng, quên quay vòng chỉ số, và nhất là hỏng dữ liệu khi hai luồng cùng truy cập. Bài này cài đặt ring buffer bằng C++, rồi dùng nó làm nền cho **Producer–Consumer**, mô hình trao đổi dữ liệu giữa các luồng có mặt ở hầu hết ứng dụng giao tiếp phần cứng.

## Vấn đề thực tế

Ứng dụng của ta nhận dữ liệu từ cảm biến qua UART với tốc độ cao. Mỗi khung dữ liệu cần được giải mã, tính toán, ghi vào thẻ nhớ và cập nhật lên màn hình. Nếu làm tất cả trong một vòng lặp:

```cpp
while (running) {
    Frame frame = readFrameFromUart();   // chờ dữ liệu
    processFrame(frame);                 // tính toán
    saveToSdCard(frame);                 // có lúc mất tới 100 ms
}
```

Khi việc ghi thẻ nhớ bị chậm, chương trình không quay lại đọc UART kịp. Bộ đệm của driver trong kernel đầy, và dữ liệu mới đến bị mất. Vấn đề là tốc độ nhận dữ liệu và tốc độ xử lý không đều nhau: dữ liệu đến đều đặn, còn việc xử lý lúc nhanh lúc chậm.

Cách giải quyết tự nhiên là tách thành hai luồng: một luồng chuyên đọc UART, một luồng chuyên xử lý. Câu hỏi còn lại là hai luồng trao đổi dữ liệu với nhau thế nào. Nếu dùng chung một `std::vector`, luồng đọc `push_back`, luồng xử lý xóa phần tử đầu, ta gặp ngay hai vấn đề: hai luồng sửa vector cùng lúc làm hỏng dữ liệu, và vector cấp phát lại bộ nhớ liên tục, không phù hợp với ứng dụng cần chạy ổn định lâu dài.

## Ý tưởng của Producer–Consumer

Mô hình Producer–Consumer gồm ba phần:

- **Producer** (bên sản xuất): tạo ra dữ liệu và đưa vào bộ đệm. Ở đây là luồng đọc UART.
- **Consumer** (bên tiêu thụ): lấy dữ liệu từ bộ đệm ra xử lý. Ở đây là luồng xử lý và ghi thẻ nhớ.
- **Bộ đệm ở giữa**: nơi dữ liệu nằm chờ, cho phép hai bên chạy với tốc độ khác nhau.

```
  +--------------+        +----------------------+        +--------------+
  |   Producer   | -----> |       Bộ đệm         | -----> |   Consumer   |
  | (đọc UART)   |  push  |  [#][#][#][ ][ ][ ]  |  pop   | (xử lý, ghi) |
  +--------------+        +----------------------+        +--------------+
```

Khi consumer bị chậm tạm thời, dữ liệu dồn lại trong bộ đệm thay vì bị mất. Khi consumer chạy nhanh trở lại, nó xử lý hết phần tồn đọng. Bộ đệm đóng vai trò "giảm xóc" giữa hai bên.

Bộ đệm phù hợp nhất cho mô hình này trong nhúng là ring buffer: kích thước cố định, không cấp phát động, thêm và lấy phần tử đều chỉ mất thời gian không đổi.

## Ring buffer hoạt động thế nào

Ring buffer là một mảng kích thước cố định, được dùng như thể hai đầu nối liền thành vòng tròn. Ta dùng hai chỉ số:

- `head`: vị trí sẽ ghi phần tử tiếp theo.
- `tail`: vị trí sẽ đọc phần tử tiếp theo.

Khi ghi, ta đặt phần tử vào vị trí `head` rồi tăng `head`. Khi đọc, ta lấy phần tử ở `tail` rồi tăng `tail`. Khi chỉ số chạy tới cuối mảng, nó quay về 0 nhờ phép chia lấy dư `% N`.

Xét bộ đệm 4 phần tử:

```
Ghi 1, 2, 3, 4:                Đọc ra 1, rồi ghi 5:
 [1][2][3][4]                   [5][2][3][4]
  ^                                 ^
  head = tail = 0 (đầy)             head = tail = 1 (đầy)
```

Phần tử 5 được ghi vào ô 0 vừa trống, không cần dịch chuyển phần tử nào.

Hình trên cũng cho thấy một chi tiết quan trọng: khi bộ đệm đầy, `head == tail`. Nhưng lúc mới khởi tạo, bộ đệm rỗng, cũng có `head == tail`. Chỉ nhìn hai chỉ số thì không phân biệt được đầy hay rỗng. Có hai cách giải quyết: dùng thêm biến đếm số phần tử, hoặc luôn để trống một ô. Ta dùng cách đếm cho phiên bản đầu tiên vì dễ hiểu hơn, và sẽ gặp cách thứ hai ở phần hàng đợi không khóa.

## Cài đặt ring buffer

Ta viết ring buffer dạng template (Bài C8), với kiểu phần tử `T` và kích thước `N` xác định lúc biên dịch. Dữ liệu được lưu trong `std::array` (Bài C8), nên toàn bộ bộ nhớ được cấp phát sẵn, không có cấp phát động:

```cpp
#include <array>
#include <cstddef>

template <typename T, size_t N>
class RingBuffer {
public:
    bool push(const T& item)
    {
        if (count_ == N) {
            return false;            // đầy
        }
        buffer_[head_] = item;
        head_ = (head_ + 1) % N;
        count_++;
        return true;
    }

    bool pop(T& item)
    {
        if (count_ == 0) {
            return false;            // rỗng
        }
        item = buffer_[tail_];
        tail_ = (tail_ + 1) % N;
        count_--;
        return true;
    }

    bool   empty() const { return count_ == 0; }
    bool   full()  const { return count_ == N; }
    size_t size()  const { return count_; }

private:
    std::array<T, N> buffer_{};
    size_t head_  = 0;    // vị trí ghi tiếp theo
    size_t tail_  = 0;    // vị trí đọc tiếp theo
    size_t count_ = 0;    // số phần tử hiện có
};
```

Hàm `pop` trả về `bool` báo thành công và ghi kết quả qua tham chiếu, theo mẫu đã học ở Bài C2.

Chạy thử đúng tình huống trong hình:

```cpp
RingBuffer<int, 4> rb;

rb.push(1);
rb.push(2);
rb.push(3);
rb.push(4);
std::printf("%d\n", rb.push(5));        // in ra: 0 (đầy, không ghi được)

int x;
rb.pop(x);                              // x = 1
rb.push(5);                             // ghi vào ô 0

while (rb.pop(x)) {
    std::printf("%d ", x);              // in ra: 2 3 4 5
}
```

Thứ tự đọc ra đúng với thứ tự ghi vào, dù phần tử 5 nằm ở đầu mảng. Ring buffer là hàng đợi FIFO (vào trước ra trước).

:::tip Chọn N là lũy thừa của 2
Nên chọn `N` là lũy thừa của 2 (16, 64, 256...). Khi đó compiler tự thay phép chia lấy dư `% N` bằng phép AND bit `& (N - 1)`, nhanh hơn nhiều trên các CPU không có lệnh chia phần cứng hoặc lệnh chia chậm.
:::

## Xử lý khi bộ đệm đầy

Dù bộ đệm lớn đến đâu, vẫn có lúc consumer chậm quá lâu và bộ đệm đầy. Ta phải quyết định chính sách cho trường hợp này, và lựa chọn phụ thuộc vào loại dữ liệu:

| Chính sách | Cách làm | Phù hợp với |
|---|---|---|
| Bỏ dữ liệu mới | `push` trả về `false` | Dữ liệu cũ quan trọng hơn, như lệnh đã xếp hàng |
| Ghi đè dữ liệu cũ nhất | Bỏ phần tử ở `tail` rồi ghi | Chỉ cần dữ liệu mới nhất, như giá trị cảm biến hiển thị lên màn hình |
| Bắt producer chờ | `push` chờ tới khi có chỗ trống | Không được mất dữ liệu nào, như các byte của một giao thức |

Phiên bản `push` hiện tại đã cài đặt chính sách đầu tiên. Chính sách ghi đè cần thêm một hàm:

```cpp
void pushOverwrite(const T& item)
{
    if (count_ == N) {               // đầy: bỏ phần tử cũ nhất
        tail_ = (tail_ + 1) % N;
        count_--;
        dropped_++;
    }
    push(item);
}

size_t dropped() const { return dropped_; }
```

Biến `dropped_` đếm số phần tử bị bỏ. Dù chọn chính sách nào, ta cũng nên đếm số lần mất dữ liệu và ghi log. Con số này cho biết bộ đệm có đủ lớn không, hoặc consumer có đang quá chậm không, những thông tin rất quý khi thiết bị chạy ngoài thực tế.

Chính sách thứ ba, bắt producer chờ, cần đến cơ chế đồng bộ giữa các luồng ở phần tiếp theo.

## Đưa ring buffer vào môi trường đa luồng

`RingBuffer` ở trên chỉ an toàn khi dùng trong một luồng. Nếu producer gọi `push` đúng lúc consumer gọi `pop`, cả hai cùng đọc và sửa `count_`. Lệnh `count_++` thực chất gồm ba bước đọc, tăng, ghi, và hai luồng có thể xen kẽ nhau, khiến `count_` bị sai. Đây là **race condition** đã học ở Bài C18.

Ta không sửa `RingBuffer` mà viết một lớp mới bọc bên ngoài, thêm mutex để bảo vệ, và `condition_variable` để luồng có thể chờ:

```cpp
#include <mutex>
#include <condition_variable>

template <typename T, size_t N>
class BlockingQueue {
public:
    void push(const T& item)
    {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            notFull_.wait(lock, [this] { return !ring_.full(); });   // chờ có chỗ trống
            ring_.push(item);
        }
        notEmpty_.notify_one();      // báo cho consumer: có dữ liệu mới
    }

    T pop()
    {
        T item;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            notEmpty_.wait(lock, [this] { return !ring_.empty(); }); // chờ có dữ liệu
            ring_.pop(item);
        }
        notFull_.notify_one();       // báo cho producer: có chỗ trống
        return item;
    }

private:
    RingBuffer<T, N>        ring_;
    std::mutex              mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
};
```

Có ba điểm cần hiểu trong đoạn code này.

**Consumer chờ mà không tốn CPU.** Khi bộ đệm rỗng, `notEmpty_.wait()` đưa luồng consumer vào trạng thái ngủ và tạm thời nhả mutex. Luồng chỉ được đánh thức khi producer gọi `notify_one()`. So sánh với hai cách chờ hay gặp khác:

```cpp
while (!ring.pop(item)) {}             // chờ bận: chiếm 100% một nhân CPU

while (!ring.pop(item)) {
    std::this_thread::sleep_for(10ms); // ngủ định kỳ: dữ liệu bị trễ tới 10 ms
}
```

`condition_variable` vừa không tốn CPU, vừa phản hồi ngay khi có dữ liệu.

**Luôn chờ kèm điều kiện.** Hàm `wait` nhận một lambda kiểm tra điều kiện. Nó tương đương với vòng lặp `while (!điều_kiện) wait(lock);`. Điều kiện phải được kiểm tra lại sau khi thức dậy, vì luồng có thể bị đánh thức dù chẳng ai gọi `notify` (gọi là **spurious wakeup**), hoặc một consumer khác đã lấy mất dữ liệu trước.

**Báo hiệu sau khi đã nhả khóa.** Lời gọi `notify_one()` đặt ngoài khối `{}` chứa `lock`, tức là sau khi mutex đã được mở. Nếu báo hiệu trong khi còn giữ khóa, luồng được đánh thức sẽ lập tức bị chặn lại vì chưa lấy được mutex. Cách viết này tránh được một lần chuyển luồng thừa.

### Dừng hàng đợi đúng cách

`BlockingQueue` hiện tại có một vấn đề khi chương trình kết thúc: consumer đang ngủ trong `pop()` chờ dữ liệu, mà producer đã dừng nên không bao giờ có dữ liệu nữa. Consumer ngủ mãi, và lệnh `join()` chờ nó kết thúc cũng treo theo.

Ta thêm chức năng đóng hàng đợi. Sau khi đóng, `push` bị từ chối, còn `pop` trả về hết dữ liệu còn lại rồi báo "không còn gì nữa". Để biểu diễn "không có giá trị", `pop` trả về `std::optional<T>` (Bài C11):

```cpp
#include <optional>

bool push(const T& item)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        notFull_.wait(lock, [this] { return !ring_.full() || closed_; });
        if (closed_) {
            return false;                // đã đóng: không nhận thêm
        }
        ring_.push(item);
    }
    notEmpty_.notify_one();
    return true;
}

std::optional<T> pop()
{
    T item;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmpty_.wait(lock, [this] { return !ring_.empty() || closed_; });
        if (ring_.empty()) {
            return std::nullopt;         // đã đóng và hết dữ liệu
        }
        ring_.pop(item);
    }
    notFull_.notify_one();
    return item;
}

void close()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
    }
    notEmpty_.notify_all();              // đánh thức mọi luồng đang chờ
    notFull_.notify_all();
}
```

Thêm thành viên `bool closed_ = false;` vào phần `private`.

Chú ý thứ tự kiểm tra trong `pop`: ta kiểm tra `ring_.empty()` chứ không kiểm tra `closed_`. Nhờ vậy, khi hàng đợi đã đóng nhưng vẫn còn dữ liệu, consumer vẫn lấy ra xử lý hết, không bỏ sót phần tử nào.

Ghép producer và consumer thành chương trình hoàn chỉnh:

```cpp
#include <thread>
#include <chrono>

BlockingQueue<int, 8> queue;

std::thread producer([&queue] {
    for (int i = 1; i <= 5; i++) {
        queue.push(i);
        std::printf("[Producer] %d\n", i);
    }
    queue.close();
});

std::thread consumer([&queue] {
    while (auto item = queue.pop()) {
        std::printf("    [Consumer] %d\n", *item);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));   // xử lý chậm
    }
    std::printf("    [Consumer] Done\n");
});

producer.join();
consumer.join();
```

Khi biên dịch chương trình dùng luồng, thêm tùy chọn `-pthread`:

```bash
g++ -std=c++17 -Wall -Wextra -pthread main.cpp -o main
```

Một kết quả có thể có (thứ tự xen kẽ giữa hai luồng có thể khác mỗi lần chạy):

```
[Producer] 1
[Producer] 2
    [Consumer] 1
[Producer] 3
[Producer] 4
[Producer] 5
    [Consumer] 2
    [Consumer] 3
    [Consumer] 4
    [Consumer] 5
    [Consumer] Done
```

Producer đưa hết 5 phần tử vào rất nhanh rồi đóng hàng đợi. Consumer xử lý chậm, nhưng vẫn lấy ra đủ cả 5 phần tử trước khi kết thúc. Vòng lặp `while (auto item = queue.pop())` dừng khi `pop` trả về `std::nullopt`.

## Đưa gì vào hàng đợi

Mỗi lần `push` và `pop` đều phải khóa và mở mutex, và có thể phải đánh thức luồng khác. Chi phí này nhỏ với một phần tử, nhưng sẽ đáng kể nếu ta đưa vào hàng đợi từng byte nhận được từ UART ở tốc độ cao.

Cách làm tốt hơn là đưa vào những đơn vị dữ liệu lớn hơn:

```cpp
// Cách 1: mỗi phần tử là một khối byte đọc được trong một lần read()
struct Chunk {
    std::array<uint8_t, 64> data;
    size_t length;
};
BlockingQueue<Chunk, 32> rawQueue;

// Cách 2: producer giải mã luôn, mỗi phần tử là một khung dữ liệu hoàn chỉnh
struct Frame {
    uint8_t id;
    std::array<uint8_t, 8> payload;
};
BlockingQueue<Frame, 32> frameQueue;
```

Cách 2 thường gọn hơn: luồng đọc UART đảm nhận luôn việc ghép byte thành khung (bằng máy trạng thái ở Bài C14), còn consumer chỉ nhận các khung đã hoàn chỉnh và hợp lệ.

## Hàng đợi không khóa cho một producer và một consumer

Mutex an toàn và dễ dùng, nhưng có nhược điểm: một luồng có thể bị chặn khi luồng kia đang giữ khóa. Với các luồng cần phản hồi trong thời gian rất ngắn và ổn định, như luồng lấy mẫu tín hiệu tốc độ cao, ngay cả khoảng chặn ngắn này cũng gây vấn đề.

Khi có đúng một producer và đúng một consumer (**SPSC**: single producer, single consumer), ta có thể bỏ hẳn mutex bằng `std::atomic` (Bài C18):

```cpp
#include <atomic>

template <typename T, size_t N>
class SpscQueue {
public:
    bool push(const T& item)                  // chỉ producer gọi
    {
        size_t head = head_.load(std::memory_order_relaxed);
        size_t next = (head + 1) % N;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;                     // đầy
        }
        buffer_[head] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& item)                         // chỉ consumer gọi
    {
        size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false;                     // rỗng
        }
        item = buffer_[tail];
        tail_.store((tail + 1) % N, std::memory_order_release);
        return true;
    }

private:
    std::array<T, N>    buffer_{};
    std::atomic<size_t> head_{0};    // chỉ producer ghi
    std::atomic<size_t> tail_{0};    // chỉ consumer ghi
};
```

Cách cài đặt này hoạt động nhờ ba ý:

- **Mỗi chỉ số chỉ có một luồng ghi.** Producer chỉ ghi `head_`, consumer chỉ ghi `tail_`. Không còn biến nào bị hai luồng cùng sửa, nên không cần khóa. Đây cũng là lý do bỏ biến `count_`, vì nó bị cả hai bên sửa.
- **Để trống một ô để phân biệt đầy và rỗng.** Bộ đệm được coi là đầy khi `head` chỉ còn cách `tail` một ô. Như vậy `head == tail` luôn có nghĩa là rỗng. Đổi lại, bộ đệm `N` ô chỉ chứa được tối đa `N - 1` phần tử.
- **Thứ tự bộ nhớ acquire/release.** `memory_order_release` khi producer cập nhật `head_` đảm bảo dữ liệu đã ghi vào `buffer_` được nhìn thấy trước chỉ số mới. `memory_order_acquire` khi consumer đọc `head_` đảm bảo nó thấy được dữ liệu đó. Thiếu hai thứ này, consumer có thể đọc được chỉ số mới nhưng dữ liệu cũ.

:::warning SpscQueue chỉ dành cho một producer và một consumer
`SpscQueue` chỉ đúng khi có đúng một luồng gọi `push` và đúng một luồng gọi `pop`. Hai producer cùng gọi `push` sẽ ghi đè lên nhau mà không có gì báo lỗi. Hàng đợi này cũng không có cơ chế chờ: consumer phải tự kiểm tra định kỳ, hoặc kết hợp với cách đánh thức khác. Chỉ nên dùng khi đã đo đạc và thấy `BlockingQueue` thật sự không đáp ứng được yêu cầu.
:::

## Khi nào không nên dùng

- **Xử lý đủ nhanh và đều.** Nếu việc xử lý luôn kịp tốc độ dữ liệu đến, làm mọi thứ trong một luồng đơn giản hơn nhiều, không có rủi ro đồng bộ.
- **Hàng đợi che giấu vấn đề tốc độ.** Nếu consumer luôn luôn chậm hơn producer, bộ đệm lớn đến đâu cũng sẽ đầy. Hàng đợi chỉ giải quyết được chênh lệch tốc độ tạm thời. Hãy theo dõi mức đầy của bộ đệm và số lần mất dữ liệu để phát hiện tình huống này.
- **Cần hàng đợi không giới hạn kích thước.** Có thể dùng `std::deque` kèm mutex, nhưng trên thiết bị nhúng, hàng đợi có kích thước cố định an toàn hơn: bộ nhớ dùng tối đa được biết trước, và thiết bị không bao giờ cạn RAM chỉ vì một luồng bị treo.

## Lỗi thường gặp

**Consumer pop khi bộ đệm vẫn rỗng do chờ không kèm điều kiện**

```cpp
notEmpty_.wait(lock);                      // sai
ring_.pop(item);                           // có thể pop khi bộ đệm vẫn rỗng
```

Luồng có thể thức dậy mà điều kiện chưa thỏa mãn. Luôn dùng dạng `wait(lock, điều_kiện)`.

**Producer bị chặn, mất dữ liệu do xử lý dữ liệu trong khi vẫn giữ khóa**

```cpp
std::lock_guard<std::mutex> lock(mutex_);
ring_.pop(item);
saveToSdCard(item);                        // sai: producer bị chặn suốt thời gian ghi thẻ nhớ
```

Khi đó producer không đưa được dữ liệu vào, và ta quay lại đúng vấn đề ban đầu. Chỉ giữ khóa trong lúc thao tác với bộ đệm, xử lý dữ liệu sau khi đã nhả khóa, như cách `BlockingQueue::pop()` trả phần tử ra ngoài.

**Chương trình treo khi thoát**

Consumer đang chờ trong `pop()`, không ai gọi `close()`, nên `consumer.join()` treo mãi. Luôn có một đường dừng rõ ràng cho mọi luồng chờ trên hàng đợi.

**Dữ liệu thỉnh thoảng bị hỏng do dùng hàng đợi SPSC với nhiều luồng**

Một luồng mới được thêm vào sau này, cũng gọi `push` lên hàng đợi SPSC có sẵn. Chương trình không báo lỗi gì, dữ liệu chỉ thỉnh thoảng bị hỏng. Hãy ghi rõ ràng buộc "một producer, một consumer" trong tên lớp hoặc chú thích.
