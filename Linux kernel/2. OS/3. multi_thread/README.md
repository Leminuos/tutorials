# Multi thread

## 1. Thread là gì & sinh ra để giải quyết vấn đề gì?

### Định nghĩa

**Thread là một luồng xử lý mà kernel coi là một đối tượng được scheduler quản lý**

Một chương trình có thể có nhiều thread cùng tồn tại — chúng chia sẻ chung vùng nhớ (heap, global variables) nhưng mỗi thread có stack và luồng thực thi độc lập.

### Tại sao cần thread?

CPU không phải lúc nào cũng có việc để làm. Đôi khi nó phải chờ: chờ đọc file, chờ nhận data từ mạng, chờ sensor trả về giá trị. Trong lúc đó, nếu chương trình chỉ có 1 luồng thực thi — toàn bộ chương trình đứng hình.

```
Không có thread:

main ──── xử lý A ──── chờ I/O ░░░░░░░░░░ ──── xử lý B ──►
                                CPU trống!

Có thread:

Thread 1 ──── xử lý A ──── chờ I/O ░░░░░░░░░░ ──────────►
Thread 2 ──────────────────────── xử lý B ───────────────►
                                CPU luôn có việc!
```

Thread giải quyết 2 bài toán:

| Bài toán | Ý nghĩa |
|---|---|
| **Concurrency** | Khi thread A bị block, thread B vẫn chạy — chương trình không bị đứng |
| **Parallelism** | Trên multi-core, nhiều thread chạy thực sự đồng thời — tăng tốc độ xử lý |

## 1.2 Thread Lifecycle & Scheduler (CFS)

### Các trạng thái của thread

Tại bất kỳ thời điểm nào, mỗi thread đang ở một trong hai trạng thái:

```
  pthread_create()
        │
        ▼
   RUNNING ◄─────────────────────┐
  (đang được CPU thực thi)       │
        │                        │
   chờ I/O,                 I/O xong,
   chờ dữ liệu...           dữ liệu sẵn sàng...
        │                        │
        ▼                        │
  SLEEPING  ─────────────────────┘
  (nhường CPU, chờ được đánh thức)
```

Khi thread cần chờ một sự kiện bên ngoài (đọc file, nhận data từ mạng...), nó **tự nguyện nhường CPU** — đi vào trạng thái sleep. Khi sự kiện xảy ra, OS đánh thức thread và đưa nó trở lại hàng chờ để chạy tiếp.

### CFS — Completely Fair Scheduler

OS cần quyết định: khi có nhiều thread đang ở trạng thái RUNNING, thread nào được chạy tiếp?

Linux dùng CFS (Completely Fair Scheduler) với nguyên tắc đơn giản: theo dõi tổng thời gian CPU mỗi thread đã dùng (`vruntime`). Thread nào dùng ít CPU hơn thì được ưu tiên chạy tiếp.

```
Thread A: vruntime = 10ms  ← được chọn chạy tiếp
Thread B: vruntime = 25ms
Thread C: vruntime = 18ms
```

Sau khi Thread A chạy thêm một lúc, `vruntime` của nó tăng lên. OS lại chọn thread có `vruntime` thấp nhất tiếp theo — cứ thế xoay vòng.

**Kết quả:** mỗi thread đều nhận được phần CPU tương đối công bằng, không thread nào bị "chết đói" (starvation)

### Time slice — Mỗi lần chạy bao lâu?

Mỗi lần được chọn, thread không chạy mãi mãi. OS cho thread chạy một khoảng thời gian ngắn (time slice, thường vài milliseconds), sau đó preempt — tạm dừng và cho thread khác chạy.

```
Timeline:
──[Thread A]──[Thread B]──[Thread C]──[Thread A]──[Thread B]──►
  time slice  time slice  time slice
```