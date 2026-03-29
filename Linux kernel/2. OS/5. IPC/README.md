# Inter process communication - IPC

## 1. Tại sao cần IPC?

Trên vi điều khiển (STM32, ESP32...), toàn bộ firmware chạy trong một không gian địa chỉ duy nhất. Mọi task — dù dùng RTOS hay bare-metal — đều có thể đọc/ghi trực tiếp vào bất kỳ biến nào:

```c
// Task A ghi
sensor_buffer[idx] = read_adc();

// Task B đọc — trực tiếp, không cần cơ chế gì thêm
process(sensor_buffer[idx]);
```

Giao tiếp giữa các task chỉ là chia sẻ biến toàn cục hoặc dùng queue nội bộ của RTOS. Đơn giản, trực tiếp.

Đối với Linux, như đã nói trong bài [Virtual memory](/linux-kernel/os/virtual-memory). Mỗi process trong hệ thống thường chạy trong một không gian bộ nhớ ảo riêng biệt, nghĩa là process A không thể truy cập vào bộ nhớ của process B theo cách thông thường. Tuy nhiên, trong một sản phẩm embedded sẽ do nhiều process đảm nhiệm, các process này cần phải giao tiếp với nhau hoặc trong các ứng dụng phức tạp như hệ thống cơ sở dữ liệu hoặc ứng dụng mạng phân tán, nhiều process có thể cần truy cập và cập nhật các dữ liệu giống nhau.

Ví dụ: microservice là một kỹ thuật thay thế cho việc tất cả tính năng gộp vào một process, điều này có thể gây ra lỗi toàn bộ hệ thống khi một tính năng bị lỗi thì ta chia các tính năng ra làm nhiều process, lúc này khi một service bị lỗi thì chỉ process thực hiện service đó bị lỗi và không làm ảnh hưởng đến các service khác.

**Vậy làm sao để hai process giao tiếp với nhau?** Câu trả lời nằm ở **IPC — Inter-Process Communication**

## 2. Phân loại các IPC

### 2.1. Các nhóm mục đích IPC

Trước khi đi vào từng cơ chế, cần phân biệt mục đích giao tiếp:

| Nhóm | Mục đích | Các cơ chế |
|---|---|---|
| **Data Transfer** | Truyền dữ liệu giữa các process, dữ liệu đi qua kernel | Pipe, Named Pipe (FIFO), Message Queue, Socket |
| **Shared Memory** | Nhiều process cùng truy cập một vùng nhớ chung, không copy qua kernel | Shared Memory, Memory-Mapped File (mmap) |
| **Synchronization** | Kiểm soát thứ tự truy cập tài nguyên chung, tránh race condition | Semaphore, Mutex/Futex, File Lock (flock/fcntl) |
| **Signaling** | Gửi thông báo bất đồng bộ giữa các process hoặc từ kernel đến process | Signal, eventfd, signalfd |
| **Linux-specific** | Các cơ chế IPC đặc trưng của Linux, phục vụ mục đích chuyên biệt | D-Bus, Netlink Socket, ptrace |

Nhiều bài toán thực tế cần kết hợp cả hai — ví dụ: Shared Memory (data transfer) + Semaphore (synchronization).

### 2.2. Danh sách đầy đủ các cơ chế IPC trên linux

Linux cung cấp rất nhiều cơ chế IPC, được phát triển qua nhiều thập kỷ. Dưới đây là toàn bộ danh sách — kể cả những cơ chế ít gặp trong embedded:

| Nhóm | Cơ chế | Nguồn gốc | Ghi chú |
|---|---|---|---|
| **Data Transfer** | Anonymous Pipe | Unix truyền thống | Một chiều, chỉ giữa process cha-con |
|   | Named Pipe (FIFO) | Unix truyền thống | Một chiều, tồn tại trên filesystem, process không liên quan cũng có thể dùng |
|   | Message Queue (System V) | System V IPC | Gửi/nhận message có cấu trúc, tồn tại độc lập với process |
|   | Message Queue (POSIX) | POSIX | Tương tự System V nhưng API hiện đại hơn, hỗ trợ notification |
|   | Unix Domain Socket | BSD | Hai chiều, hỗ trợ cả stream và datagram, chỉ trên cùng máy - giao tiếp local |
|   | TCP/UDP Socket | BSD | Hai chiều, giao tiếp qua mạng (TCP/UDP) |
| **Shared Memory** | Shared Memory (System V) | System V IPC | Nhiều process ánh xạ chung vùng nhớ, tốc độ nhanh nhất trong các IPC |
|   | Shared Memory (POSIX) | POSIX | API đơn giản hơn System V, dùng `shm_open` |
| **Synchronization** | Semaphore (System V) | System V IPC | Kiểm soát truy cập tài nguyên chung |
|   | Semaphore (POSIX) | POSIX | Nhẹ hơn System V, có named và unnamed |
|   | Mutex / Futex | Linux (futex) | Khóa nhanh ở userspace, futex là đặc trưng Linux |
|   | File Lock (flock/fcntl) | Unix truyền thống | Đồng bộ qua cơ chế khóa file |
| **Signaling** | Signal | Unix truyền thống | Thông báo bất đồng bộ (`SIGKILL`, `SIGTERM`,...) |
|   | eventfd | Linux | File descriptor cho event counter, nhẹ và nhanh |
|   | signalfd | Linux | Nhận signal qua file descriptor thay vì handler |
| **Linux-specific** | D-Bus | Freedesktop.org | Message bus cấp cao, phổ biến trên desktop Linux |
|   | Netlink Socket | Linux | Giao tiếp giữa kernel và userspace |
|   | ptrace | Unix/Linux | Kiểm soát process khác, dùng trong debugger |

:::warning Lưu ý cho embedded developer
Không cần biết hết tất cả — cần biết *cái nào tồn tại* để không tự phát minh lại bánh xe, và biết *khi nào tìm đến cái nào*.
:::

## 3. Pipe & FIFO

### 3.1. Anonymous Pipe

Pipe là cơ chế IPC đơn giản nhất trên Linux, ra đời từ những ngày đầu của UNIX. Mô hình rất trực quan: dữ liệu chảy **một chiều** từ đầu ghi vào đầu đọc. Ta có thể hình dung nó như một ống nước — dữ liệu chảy vào từ một đầu và chảy ra ở đầu kia.

```
Process A (writer)          Process B (reader)
     │                            │
     │ write(fd[1], ...)          │ read(fd[0], ...)
     ▼                            ▼
  [fd[1]] ──────────────────► [fd[0]]
            kernel buffer
            (64KB mặc định)
```

Điểm quan trọng là pipe không tồn tại trên filesystem — nó chỉ là một được buffer cấp pháp trong kernel memory, mặc định là 64KB trên linux (có thể thay đổi). Khi buffer đầy, process ghi sẽ bị block. Khi buffer rỗng, process đọc sẽ bị block. Đây chính là cơ chế đồng bộ tự nhiên của pipe.

**Cách tạo pipe**

Gọi system call `pipe()`, kernel trả về mảng 2 file descriptor:

```c
int fd[2];
pipe(fd);
// fd[0] = đầu đọc (read end)
// fd[1] = đầu ghi (write end)
```

Tại thời điểm này, cả hai đầu đều nằm trong cùng một process — chưa có gì hữu ích. Pipe chỉ thực sự có ý nghĩa khi kết hợp với `fork()`.

**Cơ chế hoạt động với fork**

Khi process gọi `fork()`, child process kế thừa toàn bộ file descriptor của parent, bao gồm cả hai đầu pipe. Lúc này cả parent và child đều giữ `fd[0]` và `fd[1]`. Để tạo kênh truyền một chiều, mỗi bên đóng đầu không cần dùng:

```c
int fd[2];
pipe(fd);
pid_t pid = fork();

if (pid == 0) {
    // Child: chỉ đọc
    close(fd[1]);       // Đóng đầu ghi
    char buf[128];
    read(fd[0], buf, sizeof(buf));
    printf("Child nhận: %s\n", buf);
    close(fd[0]);
} else {
    // Parent: chỉ ghi → đóng đầu đọc
    close(fd[0]);       // Đóng đầu đọc
    write(fd[1], "Hello from parent", 17);
    close(fd[1]);
    wait(NULL);
}
```

Luồng dữ liệu ở đây là: Parent (ghi `fd[1]`) $\rightarrow$ kernel buffer $\rightarrow$ Child (đọc `fd[0]`).

:::warning Tại sao phải đóng đầu không dùng?
Nếu parent giữ cả hai đầu mở, kernel không biết khi nào pipe kết thúc — child sẽ bị block ở `read()` mãi mãi.
:::

**Pipe trên command line**

Khi ta gõ lệnh trên terminal:

```bash
ls -la | grep ".txt" | wc -l
```

Shell thực hiện chính xác những gì mô tả ở trên: tạo pipe giữa các process, nối stdout của process trước vào stdin của process sau. Ký tự `|` chính là pipe.

**Hạn chế của anonymous pipe:**

- Pipe chỉ hoạt động giữa các process có quan hệ cha-con hoặc anh-em cùng fork từ một cha, vì cách duy nhất để chia sẻ file descriptor là thông qua fork.
- Pipe là một chiều — muốn hai chiều phải tạo hai pipe.
- Pipe không có tên nên các process độc lập không thể tìm thấy nhau.
- Dữ liệu trong pipe là byte stream — ta phải tự định nghĩa protocol để phân biệt các message.
- Buffer giới hạn (~64KB trên Linux) — ghi vượt quá sẽ block

### 3.2. Named Pipe (FIFO)

Named Pipe hay FIFO ra đời để giải quyết đúng một hạn chế của pipe thường: các process không có quan hệ cha-con không thể dùng pipe, vì không có cách nào chia sẻ file descriptor.

Giải pháp rất đơn giản — gắn cho pipe một cái tên trên filesystem. Bất kỳ process nào biết tên đó đều có thể mở và sử dụng, giống như mở một file bình thường.

```
producer (process A)              consumer (process B)
         │                                │
         │ open("/tmp/sensor_pipe", O_WRONLY)
         │                                │ open("/tmp/sensor_pipe", O_RDONLY)
         │                                │
         ▼                                ▼
      [write end] ──── kernel ────► [read end]
```

**Tạo Named Pipe**

Có hai cách tạo:
- Trên command line:
```bash
mkfifo /tmp/my_pipe
```
- Trong code C:
```c
mkfifo("/tmp/my_pipe", 0666);
```

Sau khi tạo, ta sẽ thấy nó trên filesystem. Chạy `ls -la` sẽ thấy ký tự `p` ở đầu:

```
prw-r--r-- 1 user user 0 Mar 29 10:00 /tmp/my_pipe
```

Tuy nó xuất hiện trên filesystem nhưng dữ liệu không bao giờ chạm đĩa cứng — nó vẫn nằm trong kernel buffer giống pipe thường. Cái tên trên filesystem chỉ là "địa chỉ" để các process có thể tìm thấy.

**Cách sử dụng**

Khác với pipe thường dùng `pipe()` rồi `fork()`, named pipe dùng `open()` như file bình thường.

- Process ghi (`writer.c`):

```c
int fd = open("/tmp/my_pipe", O_WRONLY);
write(fd, "Hello from writer", 17);
close(fd);
```

- Process đọc (`reader.c`):

```c
int fd = open("/tmp/my_pipe", O_RDONLY);
char buf[128];
read(fd, buf, sizeof(buf));
printf("Nhận được: %s\n", buf);
close(fd);
```

Hai process này hoàn toàn độc lập, không cần fork, không cần biết nhau tồn tại. Chỉ cần cả hai biết đường dẫn `/tmp/my_pipe`.

**Đặc tính blocking khi open**

Đây là điểm đặc trưng quan trọng nhất của named pipe mà pipe thường không có. Khi gọi `open()` trên named pipe, process sẽ bị block cho đến khi đầu bên kia cũng mở.
Cụ thể nếu reader mở trước bằng `open("/tmp/my_pipe", O_RDONLY)`, nó sẽ bị treo tại đó, chờ cho đến khi có một process khác mở `O_WRONLY`. Ngược lại nếu writer mở trước bằng `open("/tmp/my_pipe", O_WRONLY)`, nó cũng bị treo, chờ cho đến khi có process mở `O_RDONLY`.

Hai bên phải "gặp nhau" thì `open()` mới trả về. Giống như hai người phải cùng nhấc máy thì cuộc gọi mới bắt đầu.

Nếu không muốn bị block, thêm flag `O_NONBLOCK`:

```c
int fd = open("/tmp/my_pipe", O_RDONLY | O_NONBLOCK);
```

Lúc này `open()` trả về ngay lập tức, nhưng `read()` sẽ trả về `EAGAIN` nếu chưa có dữ liệu.

**Hạn chế của Named Pipe:**

- Vẫn là stream byte — phải tự định nghĩa protocol để phân biệt các message.
- Buffer giới hạn (~64KB trên Linux) — ghi vượt quá sẽ block
- Chỉ hoạt động trên cùng một máy
- Hiệu năng không cao bằng shared memory vì dữ liệu vẫn phải copy qua kernel.

**Use case điển hình**

Named pipe rất phổ biến trong script. Ví dụ ta có một process log writer liên tục ghi log, và thỉnh thoảng ta muốn đọc log realtime:

```bash
mkfifo /tmp/log_pipe

# Terminal 1: chương trình ghi log
./my_app > /tmp/log_pipe

# Terminal 2: đọc log khi cần
cat /tmp/log_pipe
```

Một ví dụ khác trong ứng dụng embedded: Một process thu thập dữ liệu cảm biến liên tục ghi vào FIFO, một process logging đọc ra và ghi vào flash — tách biệt hoàn toàn hai nhiệm vụ, không cần chia sẻ bộ nhớ.

## 4. Message queue

### 4.1. Vấn đề của Pipe

Pipe và FIFO truyền dữ liệu tốt tuy nhiên dữ liệu là byte stream không có ranh giới. Giống như ta đổ nước vào ống — bên nhận hứng được bao nhiêu thì hứng, không biết đâu là bắt đầu, đâu là kết thúc của mỗi "phần" dữ liệu.

Ví dụ process A ghi hai lần:

```c
write(fd, "MSG1", 4)
write(fd, "MSG1", 4)
```

Thì trong kernel buffer dữ liệu hiện có sẽ như sau:

```
Kernel buffer: [M][S][G][1][M][S][G][2]
```

Process B đọc có thể nhận được "MSG1" trong một lần `read()`, hoặc "MS" rồi "G1MSG2" — không có gì đảm bảo cả. Ta phải tự thiết kế protocol để biết mỗi message dài bao nhiêu, kết thúc ở đâu $\rightarrow$ Message Queue giải quyết vấn đề này ở tầng kernel.

### 4.2. Message Queue là gì?

Message queue là cơ chế truyền dữ liệu theo từng message rời rạc, có ranh giới rõ ràng giữa các process.

Hình dung message queue là một hộp thư chung đặt ở trong kernel. Hộp thư này có tên, bất kỳ process nào biết tên đều có thể mở ra để gửi hoặc nhận. Mỗi message là một bưu kiện hoàn chỉnh, có kích thước xác định và có mức ưu tiên.

```c
// Gửi
mq_send(mq, "Hello", 5, priority);
mq_send(mq, "World", 5, priority);

// Nhận — luôn nhận đúng từng message
mq_receive(mq, buf, max_size, &priority);  // → "Hello"
mq_receive(mq, buf, max_size, &priority);  // → "World"
```

Trên Linux có hai phiên bản: System V (ra đời từ những năm 1980) và POSIX (hiện đại hơn). Ta sẽ tập trung vào POSIX message queue vì API sạch hơn và được khuyến khích dùng trong code mới.

### 4.3. Đặc điểm của Message Queue

**1. Tồn tại độc lập với process (Kernel Persistence)**

Đây là đặc điểm khác biệt lớn nhất so với pipe. Khi ta tạo một message queue, nó sống trong kernel và không phụ thuộc vào bất kỳ process nào. Process tạo queue có thể thoát, process gửi message cũng có thể thoát — queue vẫn còn đó với đầy đủ message bên trong. Nó chỉ biến mất khi ta chủ động gọi `mq_unlink()` hoặc khi hệ thống reboot.

Điều này có mặt lợi là linh hoạt, nhưng cũng có mặt hại — nếu ta quên dọn dẹp, queue sẽ tồn tại mãi trong kernel, chiếm tài nguyên. Đây là lý do ta nên luôn có process chịu trách nhiệm gọi `mq_unlink()` khi không còn sử dụng.

**2. Message có ranh giới rõ ràng (Message Boundary)**

Mỗi lần gọi `mq_send()` tạo ra đúng một message. Mỗi lần gọi `mq_receive()` lấy ra đúng một message. Không bao giờ bị cắt nửa chừng, không bao giờ bị gộp hai message thành một. Kernel đảm bảo điều này.

Với pipe, ta gửi "Hello" rồi "World", bên nhận có thể đọc được "HelloWor" rồi "ld". Với message queue, bên nhận luôn nhận được "Hello" rồi "World" — tách bạch, hoàn chỉnh.

**3. Hỗ trợ Priority (Mức ưu tiên)**

Mỗi message khi gửi được gắn một số nguyên priority. Khi nhận, kernel luôn trả message có priority cao nhất trước. Trong cùng mức priority thì theo thứ tự FIFO. Đây là cơ chế có sẵn, ta không cần tự implement.

Pipe không có khái niệm này — dữ liệu luôn ra theo đúng thứ tự vào, không có cách nào "chen hàng".

**4. Blocking và Non-blocking**

Mặc định `mq_send()` sẽ block khi queue đầy, chờ cho đến khi có chỗ trống. `mq_receive()` sẽ block khi queue rỗng, chờ cho đến khi có message.

Có thể chuyển sang non-blocking bằng flag `O_NONBLOCK` khi mở queue:

```c
mqd_t mq = mq_open("/my_queue", O_RDONLY | O_NONBLOCK);
```

Đối với các hàm `mq_send()` và `mq_receive()` thì thay vì block, hàm trả về lỗi `EAGAIN` ngay lập tức. Ngoài ra POSIX còn cung cấp `mq_timedsend()` và `mq_timedreceive()` — block nhưng có timeout, nếu quá thời gian thì trả về lỗi `ETIMEDOUT`. Đây là thứ pipe hoàn toàn không có.

**5. Giới hạn dung lượng (Capacity Limits)**

Message queue có hai giới hạn cứng được thiết lập khi tạo:
- `mq_maxmsg` là số lượng message tối đa queue có thể chứa cùng lúc.
- `mq_msgsize` là kích thước tối đa của một message tính bằng byte.

Hai giới hạn này không thể thay đổi sau khi tạo. Nếu ta cần queue lớn hơn, phải xóa và tạo lại. Hệ thống cũng có giới hạn toàn cục, kiểm tra bằng:

```bash
cat /proc/sys/fs/mqueue/msg_max       # số message tối đa mỗi queue
cat /proc/sys/fs/mqueue/msgsize_max   # kích thước tối đa mỗi message
cat /proc/sys/fs/mqueue/queues_max    # số queue tối đa trên hệ thống
```

**6. Nhiều process cùng dùng (Many-to-Many)**

Không như pipe chỉ phù hợp cho mô hình một process ghi — một process đọc, message queue cho phép nhiều process cùng gửi vào một queue và nhiều process cùng nhận từ cùng queue đó.

Tuy nhiên mỗi message chỉ được đúng một process nhận. Khi process A lấy một message ra, process B sẽ không thấy message đó nữa. Đây là mô hình competing consumers — các worker cạnh tranh lấy task từ hàng đợi, rất phù hợp cho bài toán phân tải.

**7. Thông báo bất đồng bộ (Notification)**

Thay vì phải block chờ hoặc liên tục poll, ta có thể đăng ký để kernel chủ động thông báo khi có message mới đến queue rỗng. Có hai kiểu thông báo: gửi signal (`SIGEV_SIGNAL`) hoặc tạo thread mới chạy callback (`SIGEV_THREAD`).

Đặc biệt notification chỉ kích hoạt khi queue chuyển từ trạng thái rỗng sang có message. Nếu queue đang có sẵn message rồi lại thêm message mới, notification không kích hoạt. Và nó chỉ hoạt động một lần — sau khi nhận thông báo, ta phải đăng ký lại.

**8. Tính atomic**

Mỗi lần `mq_send()` và `mq_receive()` là một thao tác atomic. Nếu hai process cùng gửi message vào queue, hai message đó không bao giờ bị xen kẽ hay hỏng dữ liệu. Kernel đảm bảo mỗi message được ghi và đọc hoàn chỉnh.

So với pipe, chỉ có ghi dưới `PIPE_BUF` (4096 bytes) mới được đảm bảo atomic. Message queue đảm bảo cho mọi kích thước message (miễn không vượt `mq_msgsize`).

**9. Hạn chế**

Dữ liệu vẫn phải copy hai lần — từ process gửi vào kernel buffer, rồi từ kernel buffer ra process nhận. Với dữ liệu lớn, đây là chi phí đáng kể so với shared memory. Kích thước message bị giới hạn — không phù hợp để truyền file hay dữ liệu lớn. Và message queue chỉ hoạt động trên cùng một máy, không thể giao tiếp qua mạng.

### 4.4. Vòng đời của Message Queue

**Bước 1 — Tạo hoặc mở queue:**

```c
#include <mqueue.h>

struct mq_attr attr;
attr.mq_flags = 0;
attr.mq_maxmsg = 10;      // tối đa 10 message trong queue
attr.mq_msgsize = 256;     // mỗi message tối đa 256 bytes
attr.mq_curmsgs = 0;

mqd_t mq = mq_open("/my_queue", O_CREAT | O_RDWR, 0666, &attr);
```

Tên queue bắt buộc bắt đầu bằng `/` và không chứa thêm `/` nào nữa. Ví dụ `/my_queue `là hợp lệ, `/path/to/queue` là không hợp lệ. Đây là quy ước của POSIX, khác với named pipe dùng đường dẫn filesystem bình thường.

**Bước 2 — Gửi message:**

```c
// Tham số cuối là priority — số càng lớn, ưu tiên càng cao
mq_send(mq, "Lệnh khởi động", strlen("Lệnh khởi động"), 1);
mq_send(mq, "Lệnh cấp cứu", strlen("Lệnh cấp cứu"), 9);
mq_send(mq, "Lệnh bình thường", strlen("Lệnh bình thường"), 1);
```

**Bước 3 — Nhận message:**

```c
char buf[256];
unsigned int priority;

mq_receive(mq, buf, 256, &priority);
// Nhận "Lệnh cấp cứu" trước (priority 9)

mq_receive(mq, buf, 256, &priority);
// Rồi mới đến "Lệnh khởi động" (priority 1, vào trước)

mq_receive(mq, buf, 256, &priority);
// Cuối cùng "Lệnh bình thường" (priority 1, vào sau)
```

:::warning Lưu ý quan trọng
Tham số `size` truyền vào `mq_receive()` phải lớn hơn hoặc bằng `mq_msgsize` mà ta đã thiết lập khi tạo queue. Nếu nhỏ hơn, hàm trả về lỗi ngay lập tức, không phải trả về một phần message. Đây là điểm hay bị sai khi mới dùng.
:::

**Bước 4 — Đóng và dọn dẹp:**

```c
mq_close(mq);           // đóng — giống close file
mq_unlink("/my_queue"); // xóa queue khỏi hệ thống
```

`mq_close()` chỉ đóng file descriptor của process hiện tại, queue vẫn tồn tại trong kernel. `mq_unlink()` mới thực sự xóa queue. Giống như đóng cửa phòng và phá bỏ phòng là hai việc khác nhau.

:::warning Compile flag
POSIX `mq` yêu cầu link với `-lrt` trên một số hệ thống: `gcc main.c -lrt`
:::

### 4.5. Ví dụ

**Producer**

```c
#include <mqueue.h>

typedef struct {
    int sensor_id;
    float value;
} SensorMsg;

struct mq_attr attr = {
    .mq_maxmsg  = 10,
    .mq_msgsize = sizeof(SensorMsg)
};

mqd_t mq = mq_open("/sensor_queue", O_CREAT | O_WRONLY, 0666, &attr);

SensorMsg msg = {.sensor_id = 3, .value = 85.0};

// Gửi cảnh báo nhiệt độ cao với priority 10
mq_send(mq, (char*)&msg, sizeof(msg), 10);

// Gửi telemetry thường với priority 1
msg.value = 36.5;
mq_send(mq, (char*)&msg, sizeof(msg), 1);

mq_close(mq);
```

**Consumer**

```c
mqd_t mq = mq_open("/sensor_queue", O_RDONLY);

SensorMsg msg;
unsigned int priority;

// Lần đọc đầu tiên → nhận message priority 10 (cảnh báo)
mq_receive(mq, (char*)&msg, sizeof(msg), &priority);
printf("Priority %u: sensor %d = %.1f\n", priority, msg.sensor_id, msg.value);
// output: Priority 10: sensor 3 = 85.0

// Lần đọc thứ hai → nhận message priority 1 (telemetry)
mq_receive(mq, (char*)&msg, sizeof(msg), &priority);
printf("Priority %u: sensor %d = %.1f\n", priority, msg.sensor_id, msg.value);
// output: Priority 1: sensor 3 = 36.5

mq_close(mq);
mq_unlink("/sensor_queue");
```

### 4.6. Kiểm tra queue trên hệ thống

POSIX message queue được mount như một filesystem ảo. Ta có thể xem các queue đang tồn tại:

```bash
# Mount nếu chưa có
mkdir /dev/mqueue
mount -t mqueue none /dev/mqueue

# Xem danh sách queue
ls /dev/mqueue

# Xem thông tin chi tiết một queue
cat /dev/mqueue/demo_queue
```

### 4.7. Khi nào dùng Message Queue

Message queue phù hợp khi bạn cần truyền từng lệnh, task, hoặc event có cấu trúc giữa các process. Ví dụ một process nhận request từ user, đẩy vào queue, nhiều worker process lấy ra xử lý. Hoặc một hệ thống cần phân loại message theo mức ưu tiên. Hoặc process gửi và process nhận không chạy cùng lúc.

Nếu dữ liệu lớn, message queue không phải lựa chọn tốt vì mỗi message có giới hạn kích thước và phải copy qua kernel. Lúc đó shared memory phù hợp hơn.

## 5. Shared Memory

### 5.1 Tại sao Shared Memory nhanh nhất?

Để hiểu tại sao shared memory nhanh nhất, trước hết phải hiểu các cơ chế khác chậm ở đâu.

Với pipe, message queue, socket — mỗi lần truyền dữ liệu, dữ liệu phải đi qua hai lần copy:

```
Pipe / Message Queue:
 Process A              kernel buffer            Process B
[user space] ──copy──► [kernel space] ──copy──► [user space]
```

Mỗi lần copy là một lần gọi system call, mỗi system call phải chuyển từ user mode sang kernel mode rồi quay lại. Với dữ liệu nhỏ thì không sao, nhưng nếu ta cần truyền hàng MB hoặc truyền liên tục tần suất cao thì chi phí này rất đáng kể.

Shared memory loại bỏ hoàn toàn cả hai lần copy đó. Thay vì gửi dữ liệu qua kernel, hai process cùng map vào chung một vùng nhớ vật lý. Khi process A ghi vào vùng nhớ đó, process B đọc được ngay lập tức — không copy, không system call, không đi qua kernel. Giống như hai người cùng nhìn vào một tấm bảng trắng — người này viết, người kia thấy ngay.

```
Shared Memory:
Process A                                         Process B
[user space]                                      [user space]
   │                                                   │
   └──────────────► [physical RAM] ◄───────────────────┘
                    (shared page)
```

### 5.2. Cơ chế hoạt động

Bình thường mỗi process có bảng page table riêng, map địa chỉ ảo sang địa chỉ vật lý riêng biệt. Đây là cách MMU cô lập bộ nhớ giữa các process.

Khi dùng shared memory, kernel chỉnh page table của cả hai process sao cho một vùng địa chỉ ảo của process A và một vùng địa chỉ ảo của process B cùng trỏ về cùng một vùng nhớ vật lý. Địa chỉ ảo ở mỗi process có thể khác nhau, nhưng phía sau chúng là cùng một vùng RAM.

Việc setup này chỉ xảy ra một lần khi tạo và ánh xạ shared memory. Sau đó mọi thao tác đọc/ghi chỉ là truy cập RAM bình thường — nhanh như truy cập biến local, không có overhead nào thêm.

### 5.3. API chuẩn POSIX

**Bước 1 — Tạo vùng shared memory:**

```c
#include <sys/mman.h>
#include <fcntl.h>

// Tạo shared memory object có tên
int shm_fd = shm_open("/my_shm", O_CREAT | O_RDWR, 0666);

// Đặt kích thước cho vùng nhớ — bắt buộc phải gọi, mặc định là 0 bytes
ftruncate(shm_fd, 4096);
```

Giống message queue, tên bắt đầu bằng `/`. `shm_open()` trả về file descriptor để định danh cho vùng shared memory, nhưng lúc này chưa dùng được — ta mới tạo "vùng đất", chưa "đặt chân" vào.

**Bước 2 — Ánh xạ vào address space (mmap):**

```c
void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_SHARED, shm_fd, 0);
```

Đây mới là bước quan trọng. `mmap()` yêu cầu kernel chỉnh page table để vùng nhớ vật lý được ánh xạ vào address space của process hiện tại. Sau bước này, `ptr` trỏ thẳng vào vùng nhớ chung — đọc/ghi qua `ptr` chính là đọc/ghi vào shared memory.

**Bước 3 — Sử dụng như bộ nhớ bình thường:**

```c
// Process A — ghi
sprintf(ptr, "Hello from A");

// Process B — đọc (sau khi cũng mmap cùng vùng)
printf("Nhận: %s\n", (char *)ptr);
```

Không cần `send()`, không cần `receive()`, không cần system call nào. Chỉ là đọc/ghi biến bình thường.

**Bước 4 — Dọn dẹp:**

```c
munmap(ptr, 4096);         // gỡ ánh xạ khỏi address space
close(shm_fd);             // đóng file descriptor
shm_unlink("/my_shm");     // xóa shared memory khỏi kernel
```

### 5.4. Ví dụ

**Producer**

```c
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#define SHM_NAME "/sensor_buf"
#define SHM_SIZE 4096

// Tạo shared memory
int main() {
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, SHM_SIZE);

    // Map vào address space
    float *buf = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
                    MAP_SHARED, fd, 0);
    close(fd);  // fd không cần sau khi mmap

    buf[0] = 36.5;  // ghi trực tiếp vào shared memory
}
```

**Consumer**

```c
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = shm_open(SHM_NAME, O_RDONLY, 0666);

    float *buf = mmap(NULL, SHM_SIZE, PROT_READ,
                    MAP_SHARED, fd, 0);
    close(fd);

    printf("Nhiệt độ: %.1f°C\n", buf[0]);  // output: Nhiệt độ: 36.5°C
}
```

**Cleanup:**

```c
munmap(buf, SHM_SIZE);
shm_unlink(SHM_NAME);  // xóa entry trong /dev/shm
```

### 5.5. Vấn đề đồng bộ hoá

Đây là trace-off phải trả cho tốc độ. Với pipe hay message queue, kernel lo việc đồng bộ — `read()` tự block khi chưa có dữ liệu, `mq_receive()` tự chờ khi queue rỗng. Ta không cần nghĩ về race condition.

Shared memory thì kernel không quản lý gì cả. Nó chỉ cấp cho ta một vùng nhớ chung rồi bỏ đó. Ta phải tự xử lý mọi vấn đề:
- Process A đang ghi dở, process B đọc vào giữa chừng — nhận được dữ liệu nửa cũ nửa mới.
- Hai process cùng ghi một lúc — dữ liệu hỏng.
- Process B đọc liên tục nhưng process A chưa ghi gì — đọc dữ liệu rác.

Hãy hình dung một tấm bảng trắng trong văn phòng. Hai người cùng muốn ghi lên bảng — nếu không có quy tắc "ai đang ghi thì người kia chờ", kết quả trên bảng sẽ là nội dung hỗn loạn của cả hai.

```
Producer đang ghi dở struct (chưa xong)
    buf->temp = 36.5;
    // ← context switch xảy ra ở đây!
    buf->pressure = ...;  // chưa ghi

Consumer đọc đúng lúc này
    → đọc được temp đúng nhưng pressure sai
    → data corruption im lặng, không có error nào
```

Đây là loại bug nguy hiểm nhất: **không crash, không báo lỗi** — chỉ cho kết quả sai.

$\rightarrow$ Giải pháp là phải kết hợp shared memory với cơ chế đồng bộ — thường là **semaphore** hoặc **mutex**.

## 6. Semaphore

### 6.1. Cơ chế hoạt động

Semaphore không phải cơ chế truyền dữ liệu. Nó không gửi message, không chia sẻ bộ nhớ. Semaphore là một bộ đếm atomic nằm trong kernel, dùng để kiểm soát ai được phép làm gì và khi nào.

Hình dung semaphore là một hộp chứa chìa khóa. Trong hộp có N chìa khóa. Muốn vào phòng (truy cập tài nguyên), ta phải lấy một chìa ra khỏi hộp. Khi hộp hết chìa, người tiếp theo phải đứng chờ. Khi ta ra khỏi phòng, bỏ chìa lại vào hộp, người đang chờ mới được vào.

**Hai thao tác cơ bản:**
- `sem_wait()` (hay còn gọi acquire) — lấy một chìa khóa ra. Nếu bộ đếm > 0 thì giảm đi 1 và tiếp tục. Nếu bộ đếm = 0 thì block, chờ cho đến khi có chìa.
- `sem_post()` (hay còn gọi release) — bỏ một chìa khóa vào. Tăng bộ đếm lên 1. Nếu có process đang chờ, đánh thức nó dậy.

### 6.2. Binary Semaphore vs Counting Semaphore

Binary semaphore có giá trị chỉ là 0 hoặc 1. Giống như một phòng chỉ có một chìa khóa — tại một thời điểm chỉ một process được vào. Dùng để bảo vệ tài nguyên chỉ cho phép một người truy cập, tương tự mutex.

```c
sem_init(&sem, 1, 1);  // giá trị khởi tạo = 1

// Process A
sem_wait(&sem);         // 1 → 0, vào được
// ... truy cập tài nguyên ...
sem_post(&sem);         // 0 → 1, nhả ra

// Process B (nếu gọi khi A đang giữ)
sem_wait(&sem);         // đếm = 0, block chờ A nhả
```

Counting semaphore có giá trị từ 0 đến N. Giống phòng có N chìa khóa — cho phép tối đa N process cùng truy cập. Ví dụ ta có connection pool 5 kết nối database:

```c
sem_init(&sem, 1, 5);  // 5 chìa khóa

// Mỗi process muốn dùng connection
sem_wait(&sem);         // lấy 1 connection
// ... dùng database ...
sem_post(&sem);         // trả lại connection
```

Process thứ 6 gọi `sem_wait()` sẽ bị block cho đến khi một trong 5 process kia trả connection.

### 6.3. Hai loại Semaphore trên Linux

**POSIX Named Semaphore** — tồn tại trên filesystem, các process độc lập có thể dùng chung:

```c
#include <semaphore.h>

// Tạo hoặc mở
sem_t *sem = sem_open("/my_sem", O_CREAT, 0666, 1);

// Sử dụng
sem_wait(sem);
// ... critical section ...
sem_post(sem);

// Dọn dẹp
sem_close(sem);
sem_unlink("/my_sem");
```

Giống message queue và shared memory, tên bắt đầu bằng `/`. Semaphore tồn tại độc lập với process — phải `sem_unlink()` mới xóa.

**POSIX Unnamed Semaphore** — nằm trong bộ nhớ, phải đặt trong vùng shared memory để nhiều process dùng:

```c
sem_t sem;

// Tham số thứ 2: 0 = dùng giữa threads, 1 = dùng giữa processes
// Tham số thứ 3: giá trị khởi tạo
sem_init(&sem, 1, 1);

sem_wait(&sem);
// ... critical section ...
sem_post(&sem);

sem_destroy(&sem);
```

Khi dùng giữa các process, unnamed semaphore phải nằm trong shared memory, vì nếu nằm trong bộ nhớ riêng của một process thì process khác không thấy.

## 7. Signal

### 7.1. Signal là gì?

Signal khác hoàn toàn với các cơ chế IPC đã nói trước đó. Pipe, message queue, shared memory đều dùng để truyền dữ liệu. Signal dùng để thông báo sự kiện — nó chỉ nói "có chuyện xảy ra rồi", không kèm theo dữ liệu gì đáng kể.

Hình dung signal giống như vỗ vai ai đó. Ta vỗ vai một người, người đó giật mình, dừng việc đang làm, quay lại xử lý. Ta không đưa cho họ cái gì cả — chỉ là một cú vỗ để báo hiệu.

Cụ thể hơn, signal là một số nguyên được gửi từ kernel hoặc từ process này đến process khác. Khi process nhận được signal, nó bị ngắt ngay lập tức bất kể đang làm gì, rồi nhảy vào xử lý signal đó.

```
Process đang chạy bình thường
    │
    │  ... main loop ...
    │
    ◄── signal SIGTERM đến (từ kernel hoặc process khác)
    │
    ▼
[signal handler chạy]
    │
    ▼
quay lại main loop (hoặc exit)
```

**Signals có thể được gửi theo hai cách:**
- Từ kernel: Các signal có thể được gửi từ kernel đến process khi có một sự kiện cần sự can thiệp, chẳng hạn như như `SIGSEGV` (vi phạm phân vùng bộ nhớ) hoặc SIGFPE (lỗi phép toán số học, ví dụ chia cho 0) thường được kernel gửi cho process khi nó gặp phải một lỗi phần cứng hoặc lỗi phần mềm.
- Từ process khác: Một process có thể gửi một tín hiệu đến process khác bằng cách sử dụng lệnh kill. Chú ý rằng mặc dù tên gọi là kill, lệnh này có thể được dùng để gửi bất kỳ tín hiệu nào, không chỉ `SIGKILL`.
   - Trên command line:
    ```bash
    kill -SIGTERM 1234     # gửi SIGTERM đến process 1234
    kill -9 1234           # gửi SIGKILL (signal số 9)
    killall -SIGUSR1 myapp # gửi cho tất cả process tên myapp
    ```
   - Trong code C:
    ```c
    #include <signal.h>

    kill(pid, SIGTERM);         // gửi SIGTERM đến process có pid
    kill(getpid(), SIGUSR1);    // gửi cho chính mình
    ```

**Khi một process nhận được một signal, có ba cách để xử lý:**
- Mặc định: Mỗi signal có một hành động mặc định mà kernel sẽ thực hiện nếu process không xử lý signal đó. Ví dụ, hành động mặc định của `SIGKILL` là dừng process, hành động mặc định của `SIGSEGV` là làm cho process gặp lỗi và kết thúc.
- Catch signal: Process có thể chặn một số signal để không nhận chúng. Ví dụ, một process có thể chặn `SIGSTOP` hoặc `SIGTERM` để không bị dừng lại hay kết thúc.
- Xử lý signal: Một process có thể cài đặt một signal handler để xử lý các tín hiệu này. Signal handler là một hàm được gọi khi một signal nhất định được gửi đến process đó. Ví dụ, một process có thể cài đặt một signal handler cho `SIGINT` để thực hiện một hành động đặc biệt khi người dùng nhấn Ctrl + C.

```c
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void handler(int signum) {
    printf("Nhận được signal %d\n", signum);
}

int main() {
    signal(SIGINT, handler);  // đăng ký handler cho Ctrl+C

    while (1) {
        printf("Đang chạy... nhấn Ctrl+C\n");
        sleep(1);
    }
    return 0;
}
```

:::warning Ngoại lệ
`SIGKILL` và `SIGSTOP` không thể bắt và không thể bỏ qua — kernel dành riêng hai signal này để đảm bảo luôn có cách dừng một process bất kỳ.
:::

**Một số signal phổ biến:**

| Signal | Giá trị | Ý nghĩa mặc định |
|---|---|---|
| `SIGTERM` | 15 | Terminate (có thể bắt) |
| `SIGINT` | 2 | Interrupt (Ctrl+C) |
| `SIGKILL` | 9 | Kill (không thể bắt) |
| `SIGSEGV` | 11 | Segmentation fault |
| `SIGALRM` | 14 | Timer hết hạn |
| `SIGUSR1/2` | 10/12 | User-defined | 
| `SIGCHLD` | 17 | Child process kết thúc |

Tìm hiểu thêm tại: https://faculty.cs.niu.edu/~hutchins/csci480/signals.htm

**Hạn chế của Signal:**
- Signal mang rất ít thông tin — chỉ là một con số. Không thể kèm dữ liệu (trừ khi dùng `sigqueue()` với `SA_SIGINFO`, nhưng cũng chỉ gửi được một giá trị integer hoặc pointer).
- Signal không queue — gửi cùng signal 5 lần liên tiếp, process có thể chỉ nhận 1 lần.
- Signal handler rất khó viết đúng vì giới hạn async-signal-safety. Đây là nguồn bug khó debug nhất trong lập trình hệ thống.
- Signal phù hợp để thông báo sự kiện đơn giản như shutdown, reload, child exit. Nếu cần truyền dữ liệu, hãy dùng pipe, message queue, hoặc shared memory.

### 7.2. signal() vs sigaction() — nên dùng cái nào

Hàm `signal()` đơn giản nhưng có nhiều vấn đề. Hành vi của nó khác nhau trên các hệ điều hành khác nhau. Trên một số hệ thống, sau khi handler chạy xong, signal tự động bị reset về default `SIG_DFL` — nếu nhận thêm signal lần nữa, process bị kill. Không kiểm soát được những gì xảy ra khi đang xử lý signal thì nhận thêm signal khác.

`sigaction()` giải quyết tất cả những vấn đề đó:

```c
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void handler(int signum) {
    // xử lý signal
}

int main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  // tự restart system call bị ngắt

    sigaction(SIGINT, &sa, NULL);

    while (1) {
        printf("Đang chạy...\n");
        sleep(1);
    }
    return 0;
}
```

`sa.sa_mask` cho phép ta chỉ định những signal nào bị block tạm thời khi handler đang chạy — tránh handler bị ngắt giữa chừng bởi signal khác.

`SA_RESTART` đặc biệt quan trọng — khi process đang block trong system call như `read()`, `sleep()`, nếu signal đến, system call bị ngắt. Không có `SA_RESTART` thì `read()` trả về lỗi `EINTR`, ta phải tự retry. Có `SA_RESTART` thì kernel tự động gọi lại system call.

### 7.3. Async-Signal-Safety

Signal handler có thể bị gọi bất kỳ lúc nào — kể cả đúng lúc process đang thực thi một hàm khác. Điều này tạo ra một ràng buộc nghiêm khắc: chỉ được gọi các hàm async-signal-safe bên trong handler.

Ví dụ `printf()` bên trong dùng mutex để bảo vệ buffer. Nếu process đang chạy `printf()`, mutex đang bị lock, signal đến, handler gọi `printf()` lần nữa — deadlock. Process treo vĩnh viễn.

Chỉ có một danh sách giới hạn các hàm được phép gọi trong signal handler, gọi là async-signal-safe functions. Danh sách này bao gồm `write()`, `_exit()`, `signal()`, các thao tác trên biến `sig_atomic_t`, và một số ít hàm khác.

Pattern an toàn phổ biến nhất là handler chỉ set một flag, code chính kiểm tra flag đó:

```c
volatile sig_atomic_t got_signal = 0;

void handler(int signum) {
    got_signal = 1;  // chỉ set flag, không làm gì khác
}

int main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigaction(SIGUSR1, &sa, NULL);

    while (!got_signal) {
        // làm việc bình thường
        sleep(1);
    }

    // xử lý signal ở đây — an toàn vì nằm ngoài handler
    printf("Đã nhận signal, đang dọn dẹp...\n");
    return 0;
}
```

`volatile sig_atomic_t` là kiểu dữ liệu duy nhất đảm bảo việc ghi/đọc là atomic trong context của signal handler. Thiếu `volatile` $\rightarrow$ compiler có thể cache giá trị trong register và không bao giờ thấy thay đổi từ handler.

### 7.4. Signal Mask

Đôi khi trong một thời điểm, process không muốn xử lý một số signal nào đó, nó có thể block signal đó:

```c
sigset_t block_set, old_set;
sigemptyset(&block_set);
sigaddset(&block_set, SIGINT);
sigaddset(&block_set, SIGUSR1);

// Chặn SIGINT và SIGUSR1
sigprocmask(SIG_BLOCK, &block_set, &old_set);

// --- đoạn code nhạy cảm ---
// signal đến trong lúc này sẽ bị giữ lại (pending)
// không bị mất, chỉ bị trì hoãn

// Bỏ chặn — signal pending sẽ được gửi ngay
sigprocmask(SIG_SETMASK, &old_set, NULL);
```

Signal bị chặn không bị mất — nó nằm trong trạng thái pending. Nhưng nếu cùng một signal được gửi nhiều lần khi đang bị chặn, chỉ một lần được ghi nhận. Signal không có hàng đợi — đây là hạn chế quan trọng.

### 7.5. Use case thực tế

**Graceful shutdown** là use case phổ biến nhất — systemd gửi `SIGTERM` trước khi kill service, process sẽ có cơ hội đóng file, flush buffer, release resource:

```
systemd                    embedded daemon
   │                            │
   │──── SIGTERM ──────────────►│
   │                            │ handler: g_shutdown = 1
   │                            │ main loop detect → cleanup
   │                            │ close UART, flush log...
   │◄─── exit(0) ───────────────│
   │
   │ (nếu không exit trong timeout)
   │──── SIGKILL ──────────────►│  (bị kill cứng)
```

## 8. Giao tiếp giữa Kernel và User Space

### 8.1. Tại sao IPC thông thường không dùng được từ kernel?

Nhìn lại toàn bộ các cơ chế IPC đã học: Pipe, message queue, shared memory, socket — tất cả đều hoạt động theo cùng một mô hình: application gọi system call để nhờ kernel làm việc. Khi application gọi `write(pipe_fd, data, len)`, thực chất đang nói với kernel rằng "hãy copy dữ liệu của tôi vào buffer của pipe". Kernel là người phục vụ, application là người yêu cầu.

```
Application A                    Application B
     |                                |
     | write(pipe_fd, data)           |
     |----→ [Kernel: copy vào buffer] |
     |                                | read(pipe_fd, buf)
     |       [Kernel: copy ra buf] ←--|
     |                                |
```

Vấn đề khi kernel muốn dùng chính các cơ chế này:

**1. Kernel không thể gọi system call cho chính mình**

System call là cơ chế để chuyển từ user mode sang kernel mode. Kernel đã ở trong kernel mode rồi — gọi system call cho chính mình là vô nghĩa. Giống như ta đã đứng trong bếp mà lại gọi điện cho nhà hàng đặt món để nhà hàng mang vào bếp cho ta nấu.

**2. Kernel không có file descriptor**

File descriptor là khái niệm thuộc về process. Mỗi process có một bảng file descriptor riêng, kernel quản lý bảng đó. Nhưng kernel không phải process — nó không có bảng file descriptor của riêng mình.

Khi application gọi pipe, kernel tạo pipe rồi cấp hai file descriptor cho process đó. Kernel code bên trong không có "fd" để mà gọi `read(fd)` hay `write(fd)`.

```
Process A: fd[0]=3, fd[1]=4    → kernel quản lý bảng này
Process B: fd[0]=3, fd[1]=5    → kernel quản lý bảng này
Kernel:    ???                 → không có bảng fd nào
```

**3. Môi trường thực thi đặc biệt:**

| Đặc điểm | User space | Kernel space |
|---|---|---|
| Memory allocation | `malloc()` / `free()` | `kmalloc()` / `kfree()` |
| In ra màn hình | `printf()` | `printk()` |
| Xử lý lỗi | exception $\rightarrow$ signal | oops $\rightarrow$ kernel panic |
| Stack size | Vài MB | ~8KB (rất nhỏ) |
| Có thể sleep? | Tùy ý | Chỉ trong một số context |
| Floating point | Bình thường | **Không được dùng** |

:::warning Stack 8KB là giới hạn nghiêm khắc
Kernel module không được đệ quy sâu, không được khai báo array lớn trên stack. Đây là nguồn gốc của nhiều kernel stack overflow bug trong driver.
:::

**Kết luận:** Cần các cơ chế được thiết kế đặc biệt cho **kernel $\leftrightarrow$ userspace**.

### 8.2. Các cơ chế giao tiếp giữa kernel và app

#### 8.2.1. ioctl + poll() — cặp đôi kinh điển

`ioctl` là cơ chế để userspace app ra lệnh cho driver và nhận kết quả có kích thước nhỏ. `poll()` là cơ chế để app chờ event từ driver mà không blocking CPU.

```
User app                          Kernel driver
   │                                   │
   │──── ioctl(fd, CMD_SET_RATE, 100) ►│ driver xử lý lệnh
   │◄─── return 0 (ok) ────────────────│
   │                                   │
   │──── poll(fd, POLLIN, timeout) ───►│ app chờ data sẵn sàng
   │     (block, không tốn CPU)        │ hardware interrupt xảy ra
   │                                   │ driver gọi wake_up_interruptible()
   │◄─── POLLIN event ─────────────────│
   │──── read(fd, buf, len) ──────────►│ đọc data
```

**Phía kernel driver:**

```c
static long my_ioctl(struct file *f, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case CMD_SET_RATE:
            sample_rate = arg;
            return 0;
        case CMD_GET_STATUS:
            return copy_to_user((void*)arg, &status, sizeof(status));
    }
    return -EINVAL;
}

static unsigned int my_poll(struct file *f, poll_table *wait) {
    poll_wait(f, &my_wait_queue, wait);
    if (data_ready)
        return POLLIN | POLLRDNORM;
    return 0;
}

// Khi có data (trong interrupt handler):
data_ready = 1;
wake_up_interruptible(&my_wait_queue);  // unblock poll()
```

**Phía userspace app:**

```c
int fd = open("/dev/mysensor", O_RDWR);

ioctl(fd, CMD_SET_RATE, 100);

struct pollfd pfd = {.fd = fd, .events = POLLIN};
poll(&pfd, 1, 1000);  // timeout 1 giây

if (pfd.revents & POLLIN) {
    read(fd, &data, sizeof(data));
    printf("Sensor: %.2f\n", data);  // output: Sensor: 36.50
}
```

#### 8.2.2. mmap — chia sẻ bộ nhớ với kernel

Khi data lớn (DMA buffer, camera frame, ADC stream), copy từ kernel sang userspace qua `read()` tốn kém. Kernel có thể ánh xạ vùng nhớ kernel vào address space của application thông qua `mmap`.

```
Userspace app                       Kernel driver
[virtual addr] ──────────────► [DMA buffer trong kernel]
                  mmap()         (cùng physical page)
                  không copy
```

Application ghi vào vùng nhớ, kernel (hoặc phần cứng) đọc trực tiếp — không copy. Tương tự như shared memory giữa các process, nhưng ở đây là giữa user space và kernel/hardware.

**Phía kernel driver:**

```c
static int my_mmap(struct file *f, struct vm_area_struct *vma) {
    return remap_pfn_range(vma,
        vma->vm_start,
        virt_to_phys(dma_buf) >> PAGE_SHIFT,
        vma->vm_end - vma->vm_start,
        vma->vm_page_prot);
}
```

**Phía userspace app:**

```c
int fd = open("/dev/mysensor", O_RDWR);

uint8_t *frame = mmap(NULL, FRAME_SIZE, PROT_READ,
                      MAP_SHARED, fd, 0);

process_frame(frame, FRAME_SIZE);  // đọc trực tiếp — không có copy nào

munmap(frame, FRAME_SIZE);
```

**Đây là kỹ thuật V4L2 (Video4Linux) dùng cho camera:** toàn bộ pipeline camera trên embedded Linux đều dùng mmap để tránh copy frame buffer — vì một frame 1080p = 6MB, copy 30 lần/giây = 180MB/s chỉ để di chuyển data.

#### 8.2.3. Signal

Đã nói chi tiết ở phần trước. Kernel gửi `SIGSEGV` khi truy cập bộ nhớ sai, `SIGCHLD` khi process con chết, `SIGALRM` khi timer hết. Ưu điểm là phản hồi nhanh, bất đồng bộ. Nhược điểm là mang rất ít thông tin.

#### 8.2.4. Netlink Socket

Đây là cơ chế để kernel chủ động gửi thông tin lên user space mà không cần app polling. Netlink là một loại socket đặc biệt, cho phép truyền message có cấu trúc, hai chiều, giữa kernel và application.

```
Kernel driver                    User app
   │                                │
   │  hardware event xảy ra         │ đang chờ trên recvmsg()
   │                                │
   │──── netlink_unicast() ────────►│ nhận event ngay lập tức
   │     (push, không cần poll)     │ xử lý
```

**Phía userspace app — lắng nghe Netlink:**

```c
int sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_USER);

struct sockaddr_nl addr;
memset(&addr, 0, sizeof(addr));
addr.nl_family = AF_NETLINK;
addr.nl_pid    = getpid()
addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;
bind(sock, (struct sockaddr*)&addr, sizeof(addr));

char buf[4096];
recv(sock, buf, sizeof(buf), 0);

struct nlmsghdr *nlh = (struct nlmsghdr*)buf;
printf("Event từ driver: %s\n", (char*)NLMSG_DATA(nlh));
// output: Event từ driver: TEMP_ALERT:85.0
```

**Phía kernel driver — gửi event:**

```c
struct sk_buff *skb = nlmsg_new(msg_size, GFP_ATOMIC);
struct nlmsghdr *nlh = nlmsg_put(skb, 0, 0, NLMSG_DONE, msg_size, 0);

snprintf(NLMSG_DATA(nlh), msg_size, "TEMP_ALERT:%.1f", temp);

netlink_unicast(nl_sock, skb, app_pid, MSG_DONTWAIT);
```

Các ứng dụng sử dụng netlink: `ip` command (thay thế `ifconfig`), `iproute2`, NetworkManager, firewall (nftables), và hầu hết công cụ quản lý mạng hiện đại trên Linux.

**Tổng kết ba mô hình:**

| Mô hình | Chiều giao tiếp | Dùng khi |
|---|---|---|
| `ioctl` | App → Driver (lệnh) | App cần điều khiển driver, nhận kết quả nhỏ |
| `poll` + `read` | Driver → App (data) | App chờ data, không muốn busy-wait |
| `mmap` trên device | Driver → App (buffer) | Data lớn, cần zero-copy |
| Netlink | Driver → App (event) | Driver cần chủ động push event |

Ngoài ra, còn một số cách khác để giao tiếp giữa kernel và application sẽ được làm quen trong phần Driver.

## 9. Tổng kết

### 9.1. Tư duy lựa chọn IPC theo bài toán

Khi gặp bài toán IPC, đặt 5 câu hỏi theo thứ tự:

**Câu hỏi 1. Giao tiếp giữa ai?**
- Kernel $\leftrightarrow$ App $\rightarrow$ ioctl/poll, mmap, Netlink
- Process $\leftrightarrow$ Process (khác máy, phải thông qua mạng) $\rightarrow$ Internet Socket
- Process $\leftrightarrow$ Process (trên cùng một máy) $\rightarrow$ tiếp tục câu hỏi 2

**Câu hỏi 2. Các process có quan hệ parent/child không?**
- Có $\rightarrow$ Anonymous Pipe
- Không $\rightarrow$ tiếp tục câu hỏi 3

**Câu hỏi 3. Cần truyền data hay chỉ báo hiệu/đồng bộ?**
- Báo hiệu có sự kiện xảy ra $\rightarrow$ Signal, eventfd
- Đồng bộ, kiểm soát truy cập tài nguyên $\rightarrow$ Semaphore, Mutex
- Truyền data $\rightarrow$ tiếp tục câu hỏi 4

**Câu hỏi 4. Data là stream byte liên tục hay message rời rạc?**
- Message rời rạc, có ranh giới → Message Queue
- Stream byte → tiếp tục câu hỏi 5

**Câu hỏi 5. Hướng truyền và độ phức tạp?**
- Một chiều, đơn giản → Named Pipe (FIFO)
- Hai chiều hoặc cần mở rộng ra mạng sau này → Unix Domain Socket
- Tốc độ cao, data lớn → Shared Memory + Semaphore/Mutex

### 9.2 Bảng tổng hợp

| Cơ chế | Tốc độ | Cấu trúc | Kernel $\leftrightarrow$ App |
|---|---|---|---|
| Anonymous Pipe | Trung bình | Stream | ✗ |
| FIFO | Trung bình | Stream | ✗ |
| Signal | Rất nhanh | Không | Một chiều |
| Shared Memory + Sem | **Nhanh nhất** | Tùy thiết kế | ✗ |
| Message Queue | Trung bình | ✓ có priority | ✗ |
| ioctl + poll | Nhanh | Nhỏ | ✓ |
| mmap device | **Nhanh nhất** | Buffer lớn | ✓ |
| Netlink | Nhanh | Event | ✓ |