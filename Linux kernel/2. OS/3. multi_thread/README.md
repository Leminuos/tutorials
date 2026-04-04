# Multi thread

## 1. Thread là gì & sinh ra để giải quyết vấn đề gì?

### 1.1. Định nghĩa

**Thread là một luồng xử lý mà kernel coi là một đối tượng được scheduler quản lý**

Trong Linux kernel, mọi đơn vị thực thi — dù là process hay thread — đều được biểu diễn bằng một `task_struct`. Kernel không phân biệt process và thread ở cấp scheduler; tất cả đều là **task**. Mỗi thread là một đối tượng được scheduler quản lý và phân phối CPU.
 
Khi một process được tạo ra và bắt đầu chạy, nó chỉ có duy nhất một luồng thực thi — gọi là **main thread**. Từ main thread, ta có thể tạo thêm các thread khác thông qua hàm `pthread_create()`. Các thread trong cùng một process chia sẻ chung vùng nhớ (heap, global variables, file descriptors), nhưng mỗi thread có stack riêng và luồng thực thi độc lập.

:::warning Vòng đời của main thread
Khi main thread kết thúc (hàm `main()` return hoặc gọi `exit()`), toàn bộ process sẽ bị terminate — kể cả các thread khác đang chạy. Nếu muốn main thread kết thúc mà các thread con vẫn tiếp tục chạy, ta phải gọi `pthread_exit()` thay vì return từ `main()`. Khi đó, process chỉ kết thúc khi thread cuối cùng kết thúc.
:::

### 1.2. Tại sao cần thread?

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

### 2.1. Các trạng thái của thread

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

### 2.2. Runqueue — Hàng đợi của scheduler

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

### 2.3. CFS — Completely Fair Scheduler

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

### 2.4. Time slice — Mỗi lần chạy bao lâu?

Mỗi lần được chọn, thread không chạy mãi mãi. Scheduler cho thread chạy một khoảng thời gian ngắn (time slice, thường vài milliseconds), sau đó preempt — tạm dừng và cho thread khác chạy.

```
Timeline trên 1 core:
──[Thread A]──[Thread B]──[Thread C]──[Thread A]──[Thread B]──►
  4ms         4ms         4ms         4ms         4ms
  └─ time slice ─┘
```

### 2.5. Context Switch — Chuyển đổi giữa các thread
 
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
- **TLB (Translation Lookaside Buffer)** bị flush vì các ánh xạ virtual $\rightarrow$ physical address của process cũ không còn hợp lệ.
 
**Tóm tắt chi phí context switch:**
 
| Loại switch | Khi nào xảy ra | Thao tác | Chi phí |
|---|---|---|---|
| Thread context switch | Mọi lần chuyển task | Save/Load registers từ `task_struct` | Thấp (~μs) |
| Process context switch | Chuyển sang thread thuộc process khác | Thêm: load `mm_struct` mới, flush TLB | Cao hơn (do TLB miss sau flush) |

## 3. pthreads API

### 3.1. Tạo thread

```c
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);
```

| Tham số | Ý nghĩa |
|---|---|
| `thread` | Con trỏ tới biến `pthread_t` — sau khi tạo thành công, biến này chứa ID của thread mới. |
| `attr` | Thuộc tính của thread (stack size, detach state...). Truyền `NULL` để dùng giá trị mặc định. |
| `start_routine` | Hàm mà thread sẽ thực thi. Hàm này nhận `void *` và trả về `void *`. |
| `arg` | Argument truyền vào `start_routine`. Chỉ nhận 1 pointer — nếu cần nhiều giá trị, dùng struct (xem mục 3.4). |

Ví dụ:

```c
#include <pthread.h>
#include <stdio.h>

void *worker(void *arg) {
    int n = *(int *)arg;
    printf("Thread nhận: %d\n", n);
    return NULL;
}

int main() {
    pthread_t tid;
    int value = 42;

    int ret = pthread_create(&tid, NULL, worker, &value);
    if (ret != 0) {
        fprintf(stderr, "pthread_create failed: %d\n", ret);
        return 1;
    }

    pthread_join(tid, NULL);  // chờ thread kết thúc
    return 0;
}
```

Compile: `gcc main.c -lpthread -o main`

:::warning Cẩn thận với lifetime của argument
Khi truyền `&value` vào thread, biến `value` phải sống đủ lâu cho đến khi thread đọc xong. Nếu `value` là biến local trong một hàm đã return, thread sẽ đọc vùng nhớ không hợp lệ — dẫn đến undefined behavior.
:::

**Ví dụ lỗi phổ biến — truyền biến vòng lặp:**
 
```c
// ❌ SAI — tất cả thread có thể đọc cùng giá trị i
for (int i = 0; i < 4; i++) {
    pthread_create(&tid[i], NULL, worker, &i);
    // i thay đổi trước khi thread kịp đọc!
}
 
// ✅ ĐÚNG — mỗi thread có bản sao riêng
int args[4];
for (int i = 0; i < 4; i++) {
    args[i] = i;
    pthread_create(&tid[i], NULL, worker, &args[i]);
}
```
 
Vấn đề ở cách sai: `&i` luôn trỏ tới cùng một ô nhớ. Khi vòng lặp chạy tiếp, giá trị `i` thay đổi — nhưng thread có thể chưa kịp đọc giá trị cũ. Đây là một dạng **race condition** — kết quả phụ thuộc vào thứ tự thực thi không xác định giữa main thread và các thread con.

### 3.2. Kết thúc thread

Có 2 cách kết thúc một thread:

```c
void *worker(void *arg) {
    // Cách 1: return — đơn giản, giống kết thúc hàm bình thường
    return (void *)42;
 
    // Cách 2: pthread_exit — kết thúc thread tại bất kỳ đâu,
    // kể cả từ hàm con lồng sâu bên trong
    pthread_exit((void *)42);
}
```

Cả hai cách đều truyền được giá trị trả về cho thread gọi `pthread_join()`. Khác biệt chính:

| | `return` | `pthread_exit()` |
|---|---|---|
| Dùng ở đâu | Chỉ trong `start_routine` | Bất kỳ hàm nào trong thread |
| Cleanup | Tự động gọi destructor của biến local | Không gọi destructor của biến local trong hàm gọi |
| Khi gọi từ `main()` | Kết thúc toàn bộ process | Chỉ kết thúc main thread — các thread con vẫn chạy |

:::warning Đặc biệt với main()
Gọi `return` từ `main()` tương đương gọi `exit()` — toàn bộ process bị terminate, kể cả các thread đang chạy. Nếu muốn main thread kết thúc mà thread con vẫn sống, phải gọi `pthread_exit(NULL)` thay vì `return`.
:::

### 3.3. Thu hồi tài nguyên

Khi một thread kết thúc, nó không biến mất hoàn toàn ngay lập tức. Kernel vẫn giữ lại một số tài nguyên (exit status, thread ID...) — giống như zombie process — cho đến khi có người dọn. Nếu không dọn, tài nguyên này bị leak cho tới khi process kết thúc.

Có **đúng 2 cách** để thu hồi, và bắt buộc phải chọn một:

**Cách 1: `pthread_join()` — Chờ và lấy kết quả**

```c
int pthread_join(pthread_t thread, void **retval);
```

Thread gọi `join` sẽ block cho đến khi thread đích kết thúc. Sau đó:
- Tài nguyên của thread đích được giải phóng.
- Nếu `retval != NULL`, giá trị return/pthread_exit của thread đích được ghi vào `*retval`.

```c
void *worker(void *arg) {
    int *result = malloc(sizeof(int));
    *result = 42;
    return result;
}

int main() {
    pthread_t tid;
    pthread_create(&tid, NULL, worker, NULL);
 
    void *retval;
    pthread_join(tid, &retval);     // block cho đến khi worker xong
 
    int *result = (int *)retval;
    printf("Kết quả: %d\n", *result);  // 42
    free(result);
    return 0;
}
```

```
Timeline:
 
main   ────── create ──── làm việc khác ──── join (block) ──── nhận kết quả ──►
worker                ──── tính toán ──── return ─┘
```
 
:::warning Chú ý
Mỗi thread chỉ được join đúng 1 lần. Gọi join lần thứ hai trên cùng thread $\rightarrow$ undefined behavior.
:::

**Cách 2: `pthread_detach()` — Tự dọn, không cần chờ**
 
```c
int pthread_detach(pthread_t thread);
```
 
Đánh dấu thread là detached — khi thread kết thúc, kernel tự động giải phóng tài nguyên, không cần ai gọi join. Đổi lại, không thể lấy giá trị trả về của thread.

```c
// Detach sau khi tạo
pthread_create(&tid, NULL, worker, NULL);
pthread_detach(tid);

// Hoặc thread tự detach chính nó
void *worker(void *arg) {
    pthread_detach(pthread_self());
    // ... làm việc ...
    return NULL;  // kernel tự dọn
}

// Hoặc set attribute trước khi create — thread sinh ra đã detached
pthread_attr_t attr;
pthread_attr_init(&attr);
pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
pthread_create(&tid, &attr, worker, NULL);
pthread_attr_destroy(&attr);
```

**Khi nào dùng cách nào?**
 
| Tình huống | Chọn |
|---|---|
| Cần đợi thread xong rồi mới tiếp tục (ví dụ: tổng hợp kết quả) | `join` |
| Thread chạy nền, ví dụ: ghi log, gửi notification | `detach` |

:::warning Không join cũng không detach = resource leak
Một trong hai bắt buộc phải gọi cho mọi thread được tạo. Không gọi cả hai $\rightarrow$ tài nguyên bị giữ mãi cho đến khi process terminate.
:::

### 3.4. Truyền nhiều argument vào thread
 
`pthread_create()` chỉ cho truyền 1 pointer duy nhất. Khi cần truyền nhiều giá trị, giải pháp là đóng gói chúng vào một struct:

```c
typedef struct {
    int   start;
    int   end;
    long  result;  // thread ghi kết quả vào đây
} ThreadArg;

void *count_odd(void *arg) {
    ThreadArg *a = (ThreadArg *)arg;
    a->result = 0;
    for (int i = a->start; i <= a->end; i++)
        if (i % 2 != 0) a->result++;
    return NULL;
}

int main() {
    pthread_t tid[4];
    ThreadArg args[4];
    int range = 1000000;
    int chunk = range / 4;

    // Chia công việc cho 4 thread
    for (int i = 0; i < 4; i++) {
        args[i].start = i * chunk + 1;
        args[i].end   = (i + 1) * chunk;
        pthread_create(&tid[i], NULL, count_odd, &args[i]);
    }

    // Chờ tất cả hoàn thành và tổng hợp
    long total = 0;
    for (int i = 0; i < 4; i++) {
        pthread_join(tid[i], NULL);
        total += args[i].result;
    }

    printf("Tổng số lẻ từ 1 đến %d: %ld\n", range, total);
    return 0;
}
```

```
Phân chia công việc:
 
Thread 0: [     1 — 250000]  →  result = 125000
Thread 1: [250001 — 500000]  →  result = 125000
Thread 2: [500001 — 750000]  →  result = 125000
Thread 3: [750001 —1000000]  →  result = 125000
                                 ──────────────
main tổng hợp:                   total = 500000
```

Lưu ý: mỗi thread ghi vào phần tử riêng của mảng `args[]` nên không xảy ra xung đột — không cần lock. Đây là pattern chia dữ liệu độc lập, cách an toàn nhất để song song hóa.

### 3.5. Lấy Thread ID 
 
```c
pthread_t pthread_self(void);
```

Trả về ID của thread đang gọi. Hữu ích khi thread cần tự identify (ví dụ: debug logging) hoặc tự detach.

```c
void *worker(void *arg) {
    printf("Thread ID: %lu\n", (unsigned long)pthread_self());
    return NULL;
}
```

:::warning pthread_t không phải số nguyên
Trên một số hệ thống, `pthread_t` là struct chứ không phải integer. Để so sánh hai thread ID, luôn dùng `pthread_equal()` thay vì `==`:

```c
if (pthread_equal(tid1, tid2)) {
    // cùng thread
}
```
:::