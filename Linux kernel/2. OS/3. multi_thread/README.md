# Multi thread

## 1. Thread là gì & sinh ra để giải quyết vấn đề gì?

### Định nghĩa

**Thread là một luồng xử lý mà kernel coi là một đối tượng được scheduler quản lý**

Trong Linux kernel, mọi đơn vị thực thi — dù là process hay thread — đều được biểu diễn bằng một `task_struct`. Kernel không phân biệt process và thread ở cấp scheduler; tất cả đều là **task**. Mỗi thread là một đối tượng được scheduler quản lý và phân phối CPU.
 
Khi một process được tạo ra và bắt đầu chạy, nó chỉ có duy nhất một luồng thực thi — gọi là **main thread**. Từ main thread, ta có thể tạo thêm các thread khác thông qua hàm `pthread_create()`. Các thread trong cùng một process chia sẻ chung vùng nhớ (heap, global variables, file descriptors), nhưng mỗi thread có stack riêng và luồng thực thi độc lập.

:::warning vòng đời của main thread
Khi main thread kết thúc (hàm `main()` return hoặc gọi `exit()`), toàn bộ process sẽ bị terminate — kể cả các thread khác đang chạy. Nếu muốn main thread kết thúc mà các thread con vẫn tiếp tục chạy, ta phải gọi `pthread_exit()` thay vì return từ `main()`. Khi đó, process chỉ kết thúc khi thread cuối cùng kết thúc.
:::

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
| **Concurrency** | Khi thread A bị block (chờ I/O, chờ lock...), thread B vẫn chạy — chương trình không bị đứng. Ngay cả trên single-core, CPU vẫn luân phiên giữa các thread để tận dụng thời gian chờ. |
| **Parallelism** | Trên multi-core, nhiều thread chạy **thực sự đồng thời** trên các core khác nhau — tăng throughput cho các tác vụ CPU-bound. |

## 2. Scheduler, Runqueue & Context Switch

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

### Runqueue — Hàng đợi của scheduler

Khi một thread ở trạng thái sẵn sàng chạy (READY/RUNNABLE), nó được đặt vào **runqueue**. Mỗi CPU core có một runqueue riêng. Scheduler sẽ nhìn vào runqueue để quyết định:
- Thread nào được chạy tiếp (dựa trên thuật toán scheduling).
- Chạy trên core nào (load balancing giữa các core).

```
         Core 0                    Core 1
    ┌─────────────┐           ┌─────────────┐
    │  Runqueue 0 │           │  Runqueue 1 │
    │             │           │             │
    │  Thread A ←── đang chạy   │  Thread D ←── đang chạy
    │  Thread B     │           │  Thread E     │
    │  Thread C     │           │             │
    └─────────────┘           └─────────────┘
 
    Scheduler có thể di chuyển thread giữa các runqueue
    để load balancing
```

### CFS — Completely Fair Scheduler

Khi có nhiều thread trong runqueue, scheduler cần quyết định thread nào được chạy tiếp. Linux sử dụng CFS (Completely Fair Scheduler) với nguyên tắc: theo dõi tổng thời gian CPU mà mỗi thread đã dùng thông qua giá trị `vruntime` (virtual runtime). Thread nào có `vruntime` thấp nhất — tức là đã dùng ít CPU nhất — sẽ được ưu tiên chạy tiếp.

```
Runqueue (sắp xếp theo vruntime):
 
Thread A: vruntime = 10ms  ← được chọn chạy tiếp
Thread C: vruntime = 18ms
Thread B: vruntime = 25ms
```

Sau khi Thread A chạy thêm một lúc, `vruntime` của nó tăng lên. Scheduler lại chọn thread có `vruntime` thấp nhất tiếp theo — cứ thế xoay vòng.

CFS sử dụng **red-black tree** để lưu trữ các thread trong runqueue, cho phép tìm thread có `vruntime` nhỏ nhất trong thời gian O(log n).

**Kết quả:** Mỗi thread đều nhận được phần CPU tương đối công bằng, không thread nào bị "chết đói" (starvation).

### Time slice — Mỗi lần chạy bao lâu?

Mỗi lần được chọn, thread không chạy mãi mãi. Scheduler cho thread chạy một khoảng thời gian ngắn (time slice, thường vài milliseconds), sau đó preempt — tạm dừng và cho thread khác chạy.

```
Timeline trên 1 core:
──[Thread A]──[Thread B]──[Thread C]──[Thread A]──[Thread B]──►
  4ms         4ms         4ms         4ms         4ms
  └─ time slice ─┘
```

### Context Switch — Chuyển đổi giữa các thread
 
Khi scheduler quyết định chuyển từ thread đang chạy sang thread khác, nó thực hiện **context switch**. Quá trình này gồm hai phần:
 
**Thread context switch**
 
Đây là bước bắt buộc trong mọi lần chuyển thread. Scheduler cần:
1. **Save** trạng thái hiện tại của task cũ vào `task_struct` của nó: bao gồm giá trị các thanh ghi CPU (program counter, stack pointer, general-purpose registers...).
2. **Load** trạng thái của task mới từ `task_struct` vào CPU: khôi phục các thanh ghi để thread mới tiếp tục chạy từ đúng chỗ nó dừng lần trước.
 
**Process context switch**
 
Nếu thread mới thuộc cùng process với thread cũ (hai thread cùng process), thì chỉ cần thread context switch là đủ — vì chúng chia sẻ chung không gian bộ nhớ ảo.
 
Nhưng nếu thread mới thuộc process khác, scheduler cần thêm một bước: **chuyển đổi không gian bộ nhớ ảo**. Cụ thể:
- Mỗi `task_struct` chứa con trỏ `mm` trỏ tới `mm_struct` — cấu trúc mô tả toàn bộ virtual memory space của process (bao gồm page table, vùng mmap, heap, stack...).
- Khi chuyển sang process khác, scheduler load `mm_struct` mới, cập nhật thanh ghi CR3 (trên x86) để trỏ tới page table mới.
- **TLB (Translation Lookaside Buffer)** bị flush vì các ánh xạ virtual → physical address của process cũ không còn hợp lệ.
 
**Tóm tắt chi phí context switch:**
 
| Loại switch | Khi nào xảy ra | Thao tác | Chi phí |
|---|---|---|---|
| Thread context switch | Mọi lần chuyển task | Save/Load registers từ `task_struct` | Thấp (~μs) |
| Process context switch | Chuyển sang thread thuộc process khác | Thêm: load `mm_struct` mới, flush TLB | Cao hơn (do TLB miss sau flush) |