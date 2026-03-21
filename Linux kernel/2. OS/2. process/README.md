# Process

## Process là gì?

### Tại sao cần có khái niệm Process?

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

### Góc nhìn từ embedded developer

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
:::

### Program vs Process

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

### Kernel quản lý process như thế nào

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
Quan trọng là hiểu: **mọi thứ kernel cần biết về một process đều nằm trong `task_struct`**.
:::

### Process Identified

Mỗi process đang chạy trong hệ điều hành sẽ được cấp phát một PID hay Process ID duy nhất:
- Số nguyên dương, duy nhất tại một thời điểm
- Dải từ 1 đến 32768 (mặc định), có thể tăng lên 4194304
- Khi process kết thúc, PID được giải phóng và có thể cấp lại

Không có hai process nào trong hệ thống có cùng PID tại bất kỳ thời điểm nào. Hệ điều hành sử dụng PID này để nhận diện và quản lý các process.

Ví dụ, khi thực hiện các thao tác như `kill` hoặc `ps`, hệ điều hành sẽ sử dụng PID để xác định chính xác process nào cần được tác động.

Các child processes của một parent process sẽ có PID riêng. Tuy nhiên, chúng cũng sẽ có một PPID (Parent Process ID) để xác định process cha mà chúng sinh ra.

**Một số PID đặc biệt:**
- PID 1 là PID của process `init` hoặc `systemd` (tùy vào hệ điều hành), process này là process đầu tiên được khởi tạo khi hệ thống khởi động và chịu trách nhiệm cho việc khởi tạo các process hệ thống khác.
- PID 0 thường được sử dụng cho swapper hoặc scheduler, là process hệ thống.

Khi bật máy, kernel sẽ load init process thông qua đường dẫn (đường dẫn này được hardcore trong kernel). Init process này đọc file config để load tiếp các process khác.

**Một số lệnh thường dùng với PID:**
- Lệnh `ps`: được sử dụng để liệt kê các process đang chạy và hiển thị PID của chúng: ps aux
- Lệnh `kill`: được sử dụng để dừng một process. 
- Lệnh `top`: hiển thị các process đang chạy trong hệ thống theo thời gian thực, bao gồm cả PID.

### Process address space

Mỗi process có không gian địa chỉ ảo riêng, chia thành các vùng:

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

## Process Environment

Mỗi process khi được tạo ra không chỉ có code và bộ nhớ — nó còn mang theo một **môi trường (environment)** gồm hai thành phần: command line arguments và environment variables.

### Command line arguments

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

### Environment variable

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

## Sinh ra và kết thúc của một process

Hàm main là điểm bắt đầu của một chương trình, tuy nhiên, trước khi vào hàm main, OS sẽ chạy một số đoạn code ẩn nằm ngoài code của chúng ta => nhằm tạo môi trường để chương trình có thể chạy, ví dụ như: bộ nhớ, stdin, stdout, …

Kết thúc chương trình là điểm cuối cùng trước khi thoát khỏi chương trình, có nhiều cách để kết thúc chương trình:
- Chủ động kết thúc: ví dụ như khi ta return trong hàm main hoặc call câu lệnh exit ở bất kỳ hàm nào trong source code => biết được khi nào kết thúc.
- Bị động kết thúc: ví dụ như khi ta truy cập vào một bộ nhớ không hợp lệ => crash và kết thúc chương trình

Ngoài ra, giá trị trả về của chương trình khi kết thúc có thể nhận được từ chương trình cha.

Ví dụ: khi mà dùng terminal và chúng ta sử dụng câu lệnh ls thì cái terminal chính là process cha và giá trị trả về từ câu lệnh ls sẽ có thể xem được trên terminal.

### Hàm fork

Hàm fork được sử dụng để tạo một process mới và được thực thi ngay khi được tạo, process này là bản sao của process hiện tại. Lúc này, process gọi hàm fork sẽ được gọi là process cha và process được tạo ra từ hàm fork sẽ được gọi là process con. Cả hai process này sẽ thực thi cùng một chương trình nhưng PID sẽ khác nhau.

Ngoài ra, tất cả các biến trong bộ nhớ (memory space) của process cha sẽ được sao chép vào process con, bao gồm cả các dữ liệu trong stack, heap và data.

Mỗi process có không gian bộ nhớ riêng biệt, và các thay đổi trong bộ nhớ của process cha sẽ không ảnh hưởng đến bộ nhớ của process con, và ngược lại.

=> Nếu process cha kết thúc thì process con vẫn tiếp tục chạy.

Cấu trúc của hàm fork:

```c
pid_t fork(void)
```

**Trong process cha:** Giá trị trả về của hàm fork là PID của process con. Giá trị này có thể được sử dụng để theo dõi process con hoặc để thực hiện các hành động liên quan đến process con (ví dụ, chờ đợi process con hoàn thành).

**Trong process con:** Giá trị trả về là 0, được dùng để xác định đây là process con và thực hiện các thao tác riêng biệt.

Nếu không thể tạo process con (do thiếu tài nguyên hệ thống hoặc lỗi khác), fork() sẽ trả về -1 và errno sẽ chứa mã lỗi chi tiết.

### Hàm exec

exec là hàm được sử dụng để thay thế chương trình của process hiện tại bằng một chương trình khác. Tức là, khi một process gọi một hàm trong nhóm exec, chương trình hiện tại sẽ bị thay thế hoàn toàn bởi chương trình mới, và mã của chương trình hiện tại sẽ không tiếp tục thực thi nữa.

Dưới đây là các dạng của exec:
- `execv`
- `execp`
- `execvp`
- `execle`
- `execl`
- `execve` (mặc định)

Các hàm này khác nhau ở cách chúng nhận và truyền đối số cho chương trình mới, nhưng mục đích chính vẫn là thay thế chương trình hiện tại bằng chương trình mới.

### Kết hợp giữa fork và exec

Trong lập trình hệ thống, hàm fork và exec thường được sử dụng kết hợp để tạo ra process con và thay thế mã của process đó bằng một chương trình khác.

process cha có thể tạo ra một process con bằng cách gọi fork. Sau đó, process con sẽ thay thế mã của nó bằng một chương trình khác thông qua exec. Điều này đặc biệt hữu ích trong các tình huống mà process cha muốn khởi động một chương trình con nhưng không muốn chương trình con chia sẻ mã của nó.

Ví dụ như, Shell sử dụng fork để tạo một process con cho mỗi lệnh, sau đó sử dụng exec để thực thi lệnh đó. Điều này cho phép shell quản lý các process con mà không cần thay đổi mã nguồn của chính nó.

## Hàm wait

Hàm wait được process cha sử dụng để chờ process con kết thúc và nhận thông tin trạng thái kết thúc của nó, giúp process cha quản lý các process con của nó.

```c
#include <sys/wait.h>
pid_t wait(int *status);
```

Khi một process cha gọi hàm wait, process cha sẽ bị block và không tiếp tục thực thi cho đến khi một trong các process con của nó kết thúc.

Sau khi một process con kết thúc, hàm wait sẽ trả về PID của process con đó và thông tin trạng thái kết thúc của process con sẽ được lưu trong status.

Khi một process con kết thúc, thông tin trạng thái kết thúc của nó cần được accept bởi process cha. Nếu process cha không gọi wait để nhận thông tin này, process con sẽ chuyển thành zombie process. wait giúp loại bỏ tình trạng này bằng cách đảm bảo rằng process cha nhận trạng thái của process con khi nó kết thúc.

Bằng cách kiểm tra trạng thái của process con thông qua các macro như `WIFEXITED` và `WEXITSTATUS`, process cha có thể xác định liệu process con có hoàn thành công việc một cách thành công hay không, hoặc liệu có lỗi nào xảy ra trong khi thực thi.

Ngoài ra, nếu process con bị kết thúc một cách bị động thì mã lỗi của nguyên nhân gây ra của nó vẫn sẽ được wait lưu lại.

Nếu trước khi gọi wait mà process cha sử dụng fork để tạo ra nhiều process con thì hàm wait sẽ đợi process con kết thúc sớm nhất.

**Zombie process là gì?**

Nếu process cha kết thúc trước process con thì process init sẽ làm cha mới của process con.

Ngoài ra, khi process con kết thúc chương trình nó sẽ gửi một signal `SIGTRY` cho process cha, chỉ khi process cha accept thông tin process con gửi về thì khi đó hệ điều hành mới thực sự free process con. Nếu không, process con vẫn nằm đó và chuyển trạng thái của nó => zombie process. Khi mà một process ở trạng thái zombie thì không thể kill process được.

### Hàm exit

Hàm exit được sử dụng để kết thúc một process và trả về mã trạng thái cho hệ điều hành hoặc process cha.

Trước khi process thực sự kết thúc, exit sẽ thực hiện một số công việc quan trọng như:
- Đóng tất cả các file descriptors mở trong process.
- Giải phóng bộ nhớ động mà process đã cấp phát.

Sau khi giải phóng tài nguyên, exit trả về status code cho hệ điều hành hoặc process cha, giúp process cha hoặc hệ điều hành biết được trạng thái kết thúc của process. Thông thường:
- 0: Thông báo rằng process kết thúc thành công.
- Khác 0: Thông báo rằng process kết thúc với lỗi.

Sau khi gọi exit, process sẽ ngừng hoạt động và hệ điều hành sẽ thu hồi tài nguyên của nó. process con cũng sẽ kết thúc khi gọi exit.

Câu lệnh return cũng có thể kết thúc process nếu được áp dụng trong hàm main.