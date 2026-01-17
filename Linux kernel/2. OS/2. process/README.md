# Process

## Các tham số môi trường ảnh hưởng đến process

Khi chạy một chương trình, kernel sẽ truyền cho nó một danh sách biến môi trường, và các biến này sẽ được lưu vào trong bộ nhớ của process:
- Các biến môi trường được lưu trong một mảng hai chiều, mỗi phần tử trong mảng là một chuỗi string đại diện cho giá trị của một biến môi trường.
- Các thông số môi trường có thể ảnh hưởng đến kết quả cuối cùng của chúng ta.
- Lệnh in một biến môi trường là: `printenv`
- Ta có thể truy cập vào các biến đấy thông qua hàm `getenv` hoặc `setenv`.

Ví dụ: Chạy một chương trình trên môi trường development thì chạy tốt, nhưng chạy trên môi trường user thì nó xuất hiện lỗi.

=> khi lập trình nên quan tâm đến môi trường mà ta thực thi chương trình như thế nào.

## Điểm bắt đầu và kết thúc của một process

Hàm main là điểm bắt đầu của một chương trình, tuy nhiên, trước khi vào hàm main, OS sẽ chạy một số đoạn code ẩn nằm ngoài code của chúng ta => nhằm tạo môi trường để chương trình có thể chạy, ví dụ như: bộ nhớ, stdin, stdout, …

Prototype của hàm main:

```c
int main(int argc, char* argv[])
```

Trong đó:
- `argc` là số tham số truyền vào
- `argv` là các chuỗi ký tự truyền vào khi gọi hàm chương trình

Kết thúc chương trình là điểm cuối cùng trước khi thoát khỏi chương trình, có nhiều cách để kết thúc chương trình:
- Chủ động kết thúc: ví dụ như khi ta return trong hàm main hoặc call câu lệnh exit ở bất kỳ hàm nào trong source code => biết được khi nào kết thúc.
- Bị động kết thúc: ví dụ như khi ta truy cập vào một bộ nhớ không hợp lệ => crash và kết thúc chương trình

Ngoài ra, giá trị trả về của chương trình khi kết thúc có thể nhận được từ chương trình cha. Ví dụ: khi mà dùng terminal và chúng ta sử dụng câu lệnh ls thì cái terminal chính là process cha và giá trị trả về từ câu lệnh ls sẽ có thể xem được trên terminal.

## Cấp phát bộ nhớ động cho process

Khi process kết thúc, OS sẽ tự động free tất cả memory của process đó.

Debug memory leak:
- Quan sát toàn hệ thống, phát hiện process nào gây ra leak.
- Sử dụng tool chuyên biệt để phát hiện dòng code nào gây ra leak.

## Process Identified

Mỗi tiến trình đang chạy trong hệ điều hành sẽ được cấp phát một PID hay Process ID duy nhất. Không có hai tiến trình nào trong hệ thống có cùng PID tại bất kỳ thời điểm nào. Hệ điều hành sử dụng PID này để nhận diện và quản lý các tiến trình.

Ví dụ, khi thực hiện các thao tác như `kill` hoặc `ps`, hệ điều hành sẽ sử dụng PID để xác định chính xác tiến trình nào cần được tác động.

Các child processes của một parent process sẽ có PID riêng. Tuy nhiên, chúng cũng sẽ có một PPID (Parent Process ID) để xác định tiến trình cha mà chúng sinh ra.

**Một số PID đặc biệt:**
- PID 1 là PID của tiến trình init hoặc systemd (tùy vào hệ điều hành), tiến trình này là tiến trình đầu tiên được khởi tạo khi hệ thống khởi động và chịu trách nhiệm cho việc khởi tạo các tiến trình hệ thống khác.
- PID 0 thường được sử dụng cho swapper hoặc scheduler, là tiến trình hệ thống.

Khi bật máy, kernel sẽ load init process thông qua đường dẫn (đường dẫn này được hardcore trong kernel). Init process này đọc file config để load tiếp các process khác.

**Một số lệnh thường dùng với PID:**
- Lệnh `ps`: được sử dụng để liệt kê các tiến trình đang chạy và hiển thị PID của chúng: ps aux
- Lệnh `kill`: được sử dụng để dừng một tiến trình. 
- Lệnh `top`: hiển thị các tiến trình đang chạy trong hệ thống theo thời gian thực, bao gồm cả PID.

## Hàm exit

Hàm exit được sử dụng để kết thúc một tiến trình và trả về mã trạng thái cho hệ điều hành hoặc tiến trình cha.

Trước khi tiến trình thực sự kết thúc, exit sẽ thực hiện một số công việc quan trọng như:
- Đóng tất cả các file descriptors mở trong tiến trình.
- Giải phóng bộ nhớ động mà tiến trình đã cấp phát.

Sau khi giải phóng tài nguyên, exit trả về status code cho hệ điều hành hoặc tiến trình cha, giúp tiến trình cha hoặc hệ điều hành biết được trạng thái kết thúc của tiến trình. Thông thường:
- 0: Thông báo rằng tiến trình kết thúc thành công.
- Khác 0: Thông báo rằng tiến trình kết thúc với lỗi.

Sau khi gọi exit, tiến trình sẽ ngừng hoạt động và hệ điều hành sẽ thu hồi tài nguyên của nó. Tiến trình con cũng sẽ kết thúc khi gọi exit.

Câu lệnh return cũng có thể kết thúc tiến trình nếu được áp dụng trong hàm main.

## Hàm fork

Hàm fork được sử dụng để tạo một tiến trình mới và được thực thi ngay khi được tạo, tiến trình này là bản sao của tiến trình hiện tại. Lúc này, tiến trình gọi hàm fork sẽ được gọi là tiến trình cha và tiến trình được tạo ra từ hàm fork sẽ được gọi là tiến trình con. Cả hai tiến trình này sẽ thực thi cùng một chương trình nhưng PID sẽ khác nhau.

Ngoài ra, tất cả các biến trong bộ nhớ (memory space) của tiến trình cha sẽ được sao chép vào tiến trình con, bao gồm cả các dữ liệu trong stack, heap và data.

Mỗi tiến trình có không gian bộ nhớ riêng biệt, và các thay đổi trong bộ nhớ của tiến trình cha sẽ không ảnh hưởng đến bộ nhớ của tiến trình con, và ngược lại.

=> Nếu tiến trình cha kết thúc thì tiến trình con vẫn tiếp tục chạy.

Cấu trúc của hàm fork:

```c
pid_t fork(void)
```

**Trong tiến trình cha:** Giá trị trả về của hàm fork là PID của tiến trình con. Giá trị này có thể được sử dụng để theo dõi tiến trình con hoặc để thực hiện các hành động liên quan đến tiến trình con (ví dụ, chờ đợi tiến trình con hoàn thành).

**Trong tiến trình con:** Giá trị trả về là 0, được dùng để xác định đây là tiến trình con và thực hiện các thao tác riêng biệt.

Nếu không thể tạo tiến trình con (do thiếu tài nguyên hệ thống hoặc lỗi khác), fork() sẽ trả về -1 và errno sẽ chứa mã lỗi chi tiết.

## Hàm exec

exec là hàm được sử dụng để thay thế chương trình của tiến trình hiện tại bằng một chương trình khác. Tức là, khi một tiến trình gọi một hàm trong nhóm exec, chương trình hiện tại sẽ bị thay thế hoàn toàn bởi chương trình mới, và mã của chương trình hiện tại sẽ không tiếp tục thực thi nữa.

Dưới đây là các dạng của exec:
- `execv`
- `execp`
- `execvp`
- `execle`
- `execl`
- `execve` (mặc định)

Các hàm này khác nhau ở cách chúng nhận và truyền đối số cho chương trình mới, nhưng mục đích chính vẫn là thay thế chương trình hiện tại bằng chương trình mới.

## Kết hợp giữa fork và exec

Trong lập trình hệ thống, hàm fork và exec thường được sử dụng kết hợp để tạo ra tiến trình con và thay thế mã của tiến trình đó bằng một chương trình khác.

Tiến trình cha có thể tạo ra một tiến trình con bằng cách gọi fork. Sau đó, tiến trình con sẽ thay thế mã của nó bằng một chương trình khác thông qua exec. Điều này đặc biệt hữu ích trong các tình huống mà tiến trình cha muốn khởi động một chương trình con nhưng không muốn chương trình con chia sẻ mã của nó.

Ví dụ như, Shell sử dụng fork để tạo một tiến trình con cho mỗi lệnh, sau đó sử dụng exec để thực thi lệnh đó. Điều này cho phép shell quản lý các tiến trình con mà không cần thay đổi mã nguồn của chính nó.

## Zombie process

Nếu tiến trình cha kết thúc trước tiến trình con thì tiến trình init sẽ làm cha mới của tiến trình con.

Ngoài ra, khi tiến trình con kết thúc chương trình nó sẽ gửi một signal `SIGTRY` cho tiến trình cha, chỉ khi tiến trình cha accept thông tin tiến trình con gửi về thì khi đó hệ điều hành mới thực sự free tiến trình con. Nếu không, tiến trình con vẫn nằm đó và chuyển trạng thái của nó => zombie process. Khi mà một tiến trình ở trạng thái zombie thì không thể kill process được.

## Hàm wait

Hàm wait được tiến trình cha sử dụng để chờ tiến trình con kết thúc và nhận thông tin trạng thái kết thúc của nó, giúp tiến trình cha quản lý các tiến trình con của nó.

```c
#include <sys/wait.h>
pid_t wait(int *status);
```

Khi một tiến trình cha gọi hàm wait, tiến trình cha sẽ bị block và không tiếp tục thực thi cho đến khi một trong các tiến trình con của nó kết thúc.

Sau khi một tiến trình con kết thúc, hàm wait sẽ trả về PID của tiến trình con đó và thông tin trạng thái kết thúc của tiến trình con sẽ được lưu trong status.

Khi một tiến trình con kết thúc, thông tin trạng thái kết thúc của nó cần được accept bởi tiến trình cha. Nếu tiến trình cha không gọi wait để nhận thông tin này, tiến trình con sẽ chuyển thành zombie process. wait giúp loại bỏ tình trạng này bằng cách đảm bảo rằng tiến trình cha nhận trạng thái của tiến trình con khi nó kết thúc.

Bằng cách kiểm tra trạng thái của tiến trình con thông qua các macro như `WIFEXITED` và `WEXITSTATUS`, tiến trình cha có thể xác định liệu tiến trình con có hoàn thành công việc một cách thành công hay không, hoặc liệu có lỗi nào xảy ra trong khi thực thi.

Ngoài ra, nếu tiến trình con bị kết thúc một cách bị động thì mã lỗi của nguyên nhân gây ra của nó vẫn sẽ được wait lưu lại.

Nếu trước khi gọi wait mà tiến trình cha sử dụng fork để tạo ra nhiều tiến trình con thì hàm wait sẽ đợi tiến trình con kết thúc sớm nhất.