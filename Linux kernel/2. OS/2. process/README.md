# Process

## 1. Process là gì?

### 1.1. Tại sao cần có khái niệm Process?

Những chiếc máy tính đầu tiên chạy theo mô hình đơn giản: **một chương trình, một máy**. Khi muốn chạy chương trình mới, người vận hành phải dừng chương trình hiện tại, nạp chương trình mới vào bộ nhớ, rồi chạy lại từ đầu. CPU hầu hết thời gian ngồi chờ — chờ người dùng nhập liệu, chờ đọc băng từ, chờ in xong. Tài nguyên đắt tiền bị lãng phí.

Câu hỏi đặt ra: **Tại sao không chạy nhiều chương trình cùng lúc?**

Nhưng ngay lập tức xuất hiện loạt vấn đề nan giải:
- **Bộ nhớ:** Chương trình A và B cùng nạp vào RAM — ai ở đâu? Nếu A ghi nhầm vào vùng nhớ của B thì sao?
- **CPU:** Hai chương trình cùng muốn chạy — ai được chạy trước? Chương trình nào chạy vòng lặp vô tận có thể chiếm CPU mãi mãi.
- **Tài nguyên:** Cả hai cùng muốn ghi vào một file — kết quả sẽ là gì?
- **Lỗi:** Chương trình A crash — B có bị kéo theo không?

Để giải quyết tất cả những vấn đề này, hệ điều hành cần một đơn vị quản lý — một cái hộp cô lập cho mỗi chương trình đang chạy, có ranh giới rõ ràng về bộ nhớ, quyền hạn, và thời gian CPU. Đơn vị đó được gọi là **process**.

:::tip Kết luận
Process không phải là chương trình. Process là môi trường được kiểm soát mà trong đó chương trình được phép thực thi.
:::

### 1.2. Góc nhìn từ embedded developer

Khi lập trình vi điều khiển (MCU), developer quen với mô hình:
- **1 chip = 1 chương trình** chạy hoàn toàn một mình
- Chương trình toàn quyền truy cập RAM, ngoại vi, CPU
- Không có khái niệm "chương trình khác đang chạy song song"
- Khi reset → chương trình bắt đầu lại từ đầu

Linux hoàn toàn khác:
- Hàng chục đến hàng trăm chương trình chạy đồng thời
- Mỗi chương trình bị cô lập — không thể đọc/ghi bộ nhớ của chương trình khác
- Kernel là trọng tài: phân phối CPU, RAM, quyền truy cập tài nguyên
- Khi một chương trình crash, các chương trình còn lại không bị ảnh hưởng

:::warning Câu hỏi
Trên STM32, nếu một task FreeRTOS ghi đè bộ nhớ của task khác, điều gì xảy ra? Trên Linux thì sao?

Trên STM32 (không có MPU hoặc MPU không được cấu hình), tất cả các task chia sẻ chung một không gian địa chỉ vật lý — task A hoàn toàn có thể ghi đè lên stack hay biến của task B mà không hề có cảnh báo. Hậu quả thường là hard fault hoặc hành vi không xác định (undefined behavior) trên toàn hệ thống.

Trên Linux, mỗi process có không gian địa chỉ ảo riêng biệt, được bảo vệ bởi MMU. Nếu process A cố truy cập vùng nhớ không thuộc về nó, MMU phát hiện vi phạm và kernel gửi signal SIGSEGV (Segmentation Fault) — chỉ process A bị kill, các process khác không bị ảnh hưởng.
:::

### 1.3. Program vs Process

| | Program | Process |
|---|---|---|
| Bản chất | File binary trên disk (ELF) | Thực thể đang chạy trong RAM |
| Tồn tại | Tĩnh, không thay đổi | Động, có vòng đời |
| Số lượng | 1 file | Có thể tạo nhiều process từ 1 file |
| Ví dụ | `/bin/bash` | Mỗi terminal bạn mở là 1 process bash riêng |

Khi kernel load một program để chạy, nó tạo ra một process bằng cách:
1. Cấp phát không gian địa chỉ ảo
2. Load code và data từ file ELF vào RAM
3. Tạo kernel stack và user stack
4. Cấp PID và đưa vào hàng đợi scheduler

### 1.4. Kernel quản lý process như thế nào

Kernel đại diện mỗi process bằng một struct gọi là `task_struct` (định nghĩa trong `include/linux/sched.h`).

Đây là "hồ sơ" của process trong kernel, chứa toàn bộ thông tin cần thiết:

```
task_struct
├── pid          — Process ID
├── ppid         — Parent PID
├── state        — Trạng thái hiện tại (R/S/D/T/Z)
├── mm           — Con trỏ đến memory map (địa chỉ ảo)
├── files        — Danh sách file descriptors đang mở
├── signal       — Thông tin xử lý signal
├── sched_class  — Thuật toán scheduling áp dụng
├── prio         — Priority hiện tại
└── ...          — Hàng trăm trường khác
```

:::tip Không cần thuộc lòng các trường
Quan trọng là hiểu: **mọi thứ kernel cần biết về một process đều nằm trong `task_struct`**. Khi scheduler cần chọn task, nó đọc vruntime. Khi context switch, nó save/load registers thông qua task_struct. Khi cần chuyển address space, nó dùng con trỏ mm.
:::

### 1.5. Process ID (PID)

Mỗi process đang chạy trong hệ điều hành sẽ được cấp phát một PID hay Process ID duy nhất:
- Số nguyên dương, duy nhất tại một thời điểm
- Dải mặc định từ 1 đến 32768, có thể tăng lên 4194304 (cấu hình qua `/proc/sys/kernel/pid_max`)
- Khi process kết thúc, PID được giải phóng và có thể được tái sử dụng

Không có hai process nào trong hệ thống có cùng PID tại bất kỳ thời điểm nào. Hệ điều hành sử dụng PID để nhận diện và quản lý process — ví dụ khi gửi signal (`kill`), theo dõi trạng thái (`ps`, `top`), hay khi process cha cần chờ process con kết thúc (`wait`).

Các child process có PID riêng, nhưng cũng lưu **PPID (Parent Process ID)** — PID của process cha đã tạo ra chúng.

**Một số PID đặc biệt:**
| PID | Process | Vai trò |
|---|---|---|
| 0 | swapper/idle | Process đầu tiên kernel tạo ra, chạy scheduler. Không xuất hiện trong `ps` vì nó là một phần của kernel. |
| 1 | `init` hoặc `systemd` | Process userspace đầu tiên, được kernel khởi tạo thông qua đường dẫn hardcode trong kernel (thường là `/sbin/init`). Process này đọc file cấu hình để khởi tạo toàn bộ hệ thống — mount filesystem, start services, spawn login prompts. |

:::warning Vai trò đặc biệt của PID 1
Ngoài việc khởi tạo hệ thống, init/systemd còn là **"cha nuôi" mặc định** — khi một process cha kết thúc trước process con, kernel tự động gán PID 1 làm cha mới cho process con mồ côi đó (orphan process). Init sẽ gọi `wait()` để thu dọn tài nguyên, tránh zombie.
:::

**Một số lệnh thường dùng với PID:**
| Lệnh | Công dụng | Ví dụ |
|---|---|---|
| `ps aux` | Liệt kê tất cả process đang chạy với PID, CPU%, MEM% | `ps aux \| grep myapp` |
| `kill <pid>` | Gửi signal đến process (mặc định là `SIGTERM`) | `kill 1234` hoặc `kill -9 1234` (force kill) |
| `top` / `htop` | Hiển thị process theo thời gian thực, sắp xếp theo CPU/RAM | `top -p 1234` (theo dõi 1 process cụ thể) |
| `pstree` | Hiển thị cây quan hệ cha-con giữa các process | `pstree -p` (kèm PID) |

### 1.6. Process address space

Mỗi process có không gian địa chỉ ảo (virtual address space) riêng, được chia thành các vùng rõ ràng. Cấu trúc này được mô tả bởi `mm_struct` mà `task_struct` trỏ tới thông qua con trỏ `mm`.

```
Địa chỉ cao
┌─────────────────┐
│   Kernel space  │  ← Chỉ kernel truy cập được
├─────────────────┤  0xC0000000 (32-bit) / 0xFFFF800000000000 (64-bit)
│      Stack      │  ← Tăng từ cao xuống thấp
│        ↓        │
│   (trống)       │
│        ↑        │
│      Heap       │  ← Tăng từ thấp lên cao (malloc/free)
├─────────────────┤
│  BSS segment    │  ← Biến global chưa khởi tạo (= 0)
├─────────────────┤
│  Data segment   │  ← Biến global đã khởi tạo
├─────────────────┤
│  Text segment   │  ← Code (read-only)
└─────────────────┘
Địa chỉ thấp
```

:::tip Liên hệ với bài Virtual Memory
Đây là không gian địa chỉ ảo — kernel + MMU ánh xạ chúng sang địa chỉ vật lý thực sự. Hai process có thể có cùng địa chỉ ảo nhưng trỏ đến vùng RAM vật lý hoàn toàn khác nhau.
:::

**Quan sát thực tế:**
```bash
# Xem memory map của một process
cat /proc/<pid>/maps

# Xem thống kê bộ nhớ
cat /proc/<pid>/status | grep -i vm
```

## 2. Process Environment

Mỗi process khi được tạo ra không chỉ có code và bộ nhớ — nó còn mang theo một **môi trường (environment)** gồm hai thành phần: command line arguments và environment variables.

### 2.1. Command line arguments

Khi kernel khởi tạo process, nó truyền vào hàm `main()` hai tham số:

```c
int main(int argc, char *argv[])
```

- `argc` — số lượng argument (luôn >= 1)
- `argv[0]` — đường dẫn của chính chương trình
- `argv[1]`, `argv[2]`... — các tham số người dùng truyền vào

Ví dụ:

```bash
$ ./myapp -v --output /tmp/log.txt

argc = 4
argv[0] = "./myapp"
argv[1] = "-v"
argv[2] = "--output"
argv[3] = "/tmp/log.txt"
```

### 2.2. Environment variable

Ngoài arguments, mỗi process còn nhận một danh sách các biến môi trường dưới dạng các cặp `KEY=VALUE`, và các biến này sẽ được lưu vào trong bộ nhớ của process.

```c
#include <stdlib.h>

// Đọc một biến môi trường
char *path = getenv("PATH");
char *user = getenv("USER");
char *home = getenv("HOME");
```

Toàn bộ danh sách env vars có thể truy cập qua biến global:

```c
extern char **environ;  // Mảng các chuỗi "KEY=VALUE", kết thúc bằng NULL

// In tất cả env vars
for (int i = 0; environ[i] != NULL; i++) {
    printf("%s\n", environ[i]);
}
```

```
# Output ví dụ:
PATH=/usr/local/sbin:/usr/local/bin:/usr/bin
USER=root
HOME=/root
LD_LIBRARY_PATH=/usr/local/lib
```

:::warning Lưu ý
Một số chương trình khi chạy trên môi trường development thì chạy tốt, nhưng chạy trên môi trường user thì nó xuất hiện lỗi.

=> khi lập trình nên quan tâm đến môi trường mà ta thực thi chương trình như thế nào.
:::

:::warning Kế thừa environment variables
khi `fork()` được gọi, process con kế thừa toàn bộ env vars của cha. Đây là cơ chế mà shell truyền môi trường xuống cho các lệnh con.
:::

:::tip Liên hệ thực tế với systemd
Khi deploy service trên embedded Linux, có thể truyền config vào chương trình qua env vars thay vì hardcode — linh hoạt hơn khi thay đổi môi trường `dev/production`:

```ini
[Service]
Environment="LOG_LEVEL=debug"
Environment="SENSOR_PORT=/dev/ttyS1"
ExecStart=/usr/local/bin/myservice
```
:::

## 3. Vòng đời của process — Sinh ra và kết thúc

### 3.1. Trước khi vào `main()` — C Runtime Startup

Hàm `main()` là điểm bắt đầu của một chương trình theo góc nhìn của developer, nhưng không phải là code đầu tiên chạy trong process. Trước khi `main()` được gọi, kernel và C runtime thực hiện một chuỗi khởi tạo:

```
kernel tạo process
       │
       ▼
   _start()              ← Entry point thực sự (do linker đặt)
       │
       ▼
 __libc_start_main()     ← C runtime setup
       │
       ├── Khởi tạo heap (malloc sẵn sàng)
       ├── Setup stdin/stdout/stderr (fd 0, 1, 2)
       ├── Gọi các constructor (__attribute__((constructor)))
       ├── Setup atexit handlers
       │
       ▼
   main(argc, argv)      ← Code của lập trình viên bắt đầu ở đây
```

### 3.2. Kết thúc process

Process có thể kết thúc theo hai cách:

**Kết thúc chủ động (normal termination):**

- `return` từ hàm `main()` — giá trị return trở thành exit status
- Gọi `exit(status)` từ bất kỳ đâu trong code — thực hiện cleanup rồi kết thúc
- Gọi `_exit(status)` hoặc `_Exit(status)` — kết thúc ngay lập tức, không cleanup

**Kết thúc bị động (abnormal termination):**

- Nhận signal chưa được xử lý: `SIGSEGV` (truy cập bộ nhớ bất hợp lệ), `SIGABRT` (gọi `abort()`), `SIGKILL` (bị force kill),...
- Các lỗi phần cứng: bus error, illegal instruction,...

```
                          main() return
                              │
                              ▼
exit(status) ──────────► C runtime cleanup
                              │
                          ├── Gọi atexit handlers (theo thứ tự ngược)
                          ├── Flush và đóng stdio buffers
                          ├── Gọi destructors
                              │
                              ▼
                         _exit(status)
                              │
                          ├── Kernel đóng tất cả file descriptors
                          ├── Giải phóng memory mappings
                          ├── Gửi SIGCHLD cho process cha
                          ├── Lưu exit status vào task_struct
                              │
                              ▼
                        Process kết thúc
                     (chờ cha gọi wait() để thu dọn)
```

:::warning Giá trị trả về
Exit status của process con có thể được process cha nhận thông qua `wait()`.

Ví dụ, khi chạy `ls` trong terminal thì terminal (shell) là process cha, và giá trị trả về từ `ls` có thể xem bằng `echo $?` ngay sau đó.
:::

### 3.3. Hàm fork

Hàm fork được sử dụng để tạo một process mới và được thực thi ngay khi được tạo, process này là bản sao của process hiện tại. Lúc này, process gọi hàm fork sẽ được gọi là process cha và process được tạo ra từ hàm fork sẽ được gọi là process con. Cả hai process này sẽ thực thi cùng một chương trình nhưng PID sẽ khác nhau.

Cấu trúc của hàm fork:

```c
pid_t fork(void)
```

| Giá trị trả về | Ngữ cảnh | Ý nghĩa |
|---|---|---|
| `> 0` (PID con) | Trong process cha | Cho cha biết PID của con để theo dõi và thực hiện các hành động liên quan đến process con (ví dụ, chờ đợi process con hoàn thành). |
| `0` | Trong process con | Được dùng để xác định đây là process con và thực hiện các thao tác riêng biệt. |
| `-1` | Lỗi | Không tạo được process con (thiếu tài nguyên, vượt limit) |

Ngoài ra, tất cả các biến trong bộ nhớ (memory space) của process cha sẽ được sao chép vào process con, bao gồm cả các dữ liệu trong stack, heap và data. **Kernel sẽ dùng copy-on-write để làm điều này**.

:::tip Copy-on-write (COW)
Khi `fork()` được gọi, kernel không copy toàn bộ bộ nhớ ngay lập tức — thay vào đó, cha và con cùng trỏ đến các page vật lý giống nhau (đánh dấu read-only). Chỉ khi một trong hai process ghi vào một page, kernel mới thực sự tạo bản copy riêng cho page đó. Cơ chế này giúp `fork()` rất nhanh, ngay cả khi process cha có hàng GB bộ nhớ.
:::

Mỗi process có không gian bộ nhớ riêng biệt, và các thay đổi trong bộ nhớ của process cha sẽ không ảnh hưởng đến bộ nhớ của process con, và ngược lại.

=> Nếu process cha kết thúc thì process con vẫn tiếp tục chạy.

**Ví dụ:**

```c
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    int x = 100;
    pid_t pid = fork();
 
    if (pid < 0) {
        perror("fork failed");
        return 1;
    }
    else if (pid == 0) { // Process con
        x = 200;    // Chỉ thay đổi bản copy của con (COW)
        printf("Child: PID=%d, x=%d\n", getpid(), x);
    }
    else {              // Process cha
        wait(NULL); // Chờ con kết thúc
        printf("Parent: PID=%d, x=%d\n", getpid(), x);  // x vẫn = 100
    }
    return 0;
}
```

```
Output:
Child: PID=1235, x=200
Parent: PID=1234, x=100    ← x không bị ảnh hưởng bởi con
```

### 3.4. Hàm exec

`exec` là hàm được sử dụng để thay thế chương trình của process hiện tại bằng một chương trình khác. Tức là, khi một process gọi một hàm trong nhóm exec, chương trình hiện tại sẽ bị thay thế hoàn toàn bởi chương trình mới, và mã của chương trình hiện tại sẽ không tiếp tục thực thi nữa.

Dưới đây là các biến thể của exec:
- `execv`
- `execp`
- `execvp`
- `execle`
- `execl`
- `execve` (mặc định)

Các hàm này khác nhau ở cách chúng nhận và truyền đối số cho chương trình mới, nhưng mục đích chính vẫn là thay thế chương trình hiện tại bằng chương trình mới.

:::warning Chú ý
`execve()` là system call thực sự — tất cả các biến thể khác đều là wrapper trong C library, cuối cùng gọi `execve()`.
:::

### 3.5. Kết hợp giữa fork và exec

Trong lập trình hệ thống, hàm fork và exec thường được sử dụng kết hợp để tạo ra process con và thay thế mã của process đó bằng một chương trình khác.

Process cha có thể tạo ra một process con bằng cách gọi fork. Sau đó, process con sẽ thay thế mã của nó bằng một chương trình khác thông qua exec. Điều này đặc biệt hữu ích trong các tình huống mà process cha muốn khởi động một chương trình con nhưng không muốn chương trình con chia sẻ mã của nó.

Ví dụ như, Shell sử dụng fork để tạo một process con cho mỗi lệnh, sau đó sử dụng exec để thực thi lệnh đó. Điều này cho phép shell quản lý các process con mà không cần thay đổi mã nguồn của chính nó.

```
Process cha (shell)
       │
       ├── fork() ──────────────► Process con (bản sao của shell)
       │                                │
       │                          exec("/bin/ls")
       │                                │
       │                                ▼
       │                          Process con chạy ls
       │                                │
       │                          ls kết thúc, gọi exit()
       │                                │
       ├── wait() ◄─────────────── Nhận exit status
       │
       ▼
  Shell tiếp tục chờ lệnh mới
```

## 4. Hàm wait

Hàm `wait` được process cha sử dụng để chờ process con kết thúc và nhận thông tin trạng thái kết thúc của nó, giúp process cha quản lý các process con của nó.

```c
#include <sys/wait.h>
pid_t wait(int *status);
```

Khi một process cha gọi hàm `wait`, process cha sẽ bị block và không tiếp tục thực thi cho đến khi một trong các process con của nó kết thúc.

Sau khi một process con kết thúc, hàm `wait` sẽ trả về PID của process con đó và thông tin trạng thái kết thúc của process con sẽ được lưu trong status.

Khi một process con kết thúc, thông tin trạng thái kết thúc của nó cần được accept bởi process cha. Nếu process cha không gọi `wait` để nhận thông tin này, process con sẽ chuyển thành zombie process. `wait` giúp loại bỏ tình trạng này bằng cách đảm bảo rằng process cha nhận trạng thái của process con khi nó kết thúc.

Bằng cách kiểm tra trạng thái của process con thông qua các macro như `WIFEXITED` và `WEXITSTATUS`, process cha có thể xác định liệu process con có hoàn thành công việc một cách thành công hay không, hoặc liệu có lỗi nào xảy ra trong khi thực thi.

Ngoài ra, nếu process con bị kết thúc một cách bị động thì mã lỗi của nguyên nhân gây ra của nó vẫn sẽ được `wait` lưu lại.

Nếu trước khi gọi `wait` mà process cha sử dụng fork để tạo ra nhiều process con thì hàm `wait` sẽ đợi process con kết thúc sớm nhất.

## 5. Zombie process và orphan process

### 5.1. Zombie process
 
Khi process con kết thúc, nó không biến mất ngay lập tức. Kernel cần giữ lại `task_struct` của nó (chứa exit status, thống kê CPU...) cho đến khi process cha gọi `wait()` để đọc thông tin này. Trong khoảng thời gian chờ cha gọi `wait()`, process con ở trạng thái **zombie** (trạng thái `Z` trong `ps`).
 
```
Process con kết thúc
       │
       ├── Giải phóng: bộ nhớ, file descriptors, ...
       ├── Giữ lại: task_struct (exit status, PID, thống kê)
       ├── Gửi signal SIGCHLD cho process cha
       │
       ▼
  Zombie (chờ cha gọi wait)
       │
       ├── Cha gọi wait() → kernel giải phóng task_struct → process biến mất hoàn toàn
       └── Cha không gọi wait() → zombie tồn tại mãi, chiếm slot trong process table
```
 
:::warning Zombie không thể bị kill
Vì zombie thực chất đã kết thúc rồi — nó không chạy code, không chiếm CPU hay RAM (chỉ chiếm một entry nhỏ trong process table). `kill -9` không có tác dụng vì không có process nào đang chạy để nhận signal. Cách duy nhất để xoá zombie là buộc process cha gọi `wait()`, hoặc kill process cha (khi đó init sẽ nhận nuôi và gọi `wait()`).
:::

### 5.2. Orphan process
 
Khi process cha kết thúc trước process con, process con trở thành **orphan**. Kernel tự động gán init (PID 1) làm cha mới. Init luôn gọi `wait()` cho các con của nó, nên orphan process sẽ được thu dọn sạch sẽ khi kết thúc — không bị zombie.
 
```
Cha kết thúc trước con:
 
Process cha ──── exit() ──── biến mất
       │
       └── Process con (orphan)
                │
                └── Kernel gán init (PID 1) làm cha mới
                         │
                         └── Con kết thúc → init gọi wait() → thu dọn sạch
```

### 6. Hàm exit

Hàm exit được sử dụng để kết thúc một process và trả về mã trạng thái cho hệ điều hành hoặc process cha.

Trước khi process thực sự kết thúc, exit sẽ thực hiện một số công việc quan trọng như:
- Đóng tất cả các file descriptors mở trong process.
- Giải phóng bộ nhớ động mà process đã cấp phát.

Sau khi giải phóng tài nguyên, exit trả về status code cho hệ điều hành hoặc process cha, giúp process cha hoặc hệ điều hành biết được trạng thái kết thúc của process. Thông thường:
- 0: Thông báo rằng process kết thúc thành công.
- Khác 0: Thông báo rằng process kết thúc với lỗi.

Sau khi gọi exit, process sẽ ngừng hoạt động và hệ điều hành sẽ thu hồi tài nguyên của nó. process con cũng sẽ kết thúc khi gọi exit.

Câu lệnh return cũng có thể kết thúc process nếu được áp dụng trong hàm main.