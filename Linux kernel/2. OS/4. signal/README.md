# Signal

Signal là một loại cơ chế bất đồng bộ mà OS sử dụng để thông báo cho process về một sự kiện cụ thể. Mỗi signal có một mã số và có thể được gửi từ kernel hoặc từ các process khác.

Các signal có thể được gửi từ kernel đến process khi có một sự kiện cần sự can thiệp, chẳng hạn như việc process cố gắng chia sẻ tài nguyên mà không có quyền truy cập, hoặc khi một process gặp lỗi nghiêm trọng.

OS sẽ tạo ra một bảng signal table khi khởi tạo process, ta có thể đăng ký các hàm xử lý signal vào trong đó. Khi mới được khởi tạo, tất cả các signal handler sẽ là default handler.

Khi ignore một signal number thì mỗi khi signal đó gửi đến thì OS sẽ throw nó đi và không gọi ra signal handler.

Đôi khi trong một thời điểm, process không muốn xử lý một số signal nào đó, nó có thể block signal đó. Signal vẫn được gửi đến cho process nhưng sẽ nằm trong hàng chờ, không được xử lý ngay.

Signals có thể được gửi theo hai cách:
- Từ kernel: Các signals như SIGSEGV (vi phạm phân vùng bộ nhớ) hoặc SIGFPE (lỗi phép toán số học, ví dụ chia cho 0) thường được kernel gửi cho process khi nó gặp phải một lỗi phần cứng hoặc lỗi phần mềm.
- Từ process khác: Một process có thể gửi một tín hiệu đến process khác bằng cách sử dụng lệnh kill. Chú ý rằng mặc dù tên gọi là kill, lệnh này có thể được dùng để gửi bất kỳ tín hiệu nào, không chỉ SIGKILL.

Khi một process nhận được một signal, có ba cách để xử lý:
- Mặc định: Mỗi signal có một hành động mặc định mà kernel sẽ thực hiện nếu process không xử lý signal đó. Ví dụ, hành động mặc định của SIGKILL là dừng process, hành động mặc định của SIGSEGV là làm cho process gặp lỗi và kết thúc.
- Chặn signal: Process có thể chặn một số signal để không nhận chúng. Ví dụ, một process có thể chặn SIGSTOP hoặc SIGTERM để không bị dừng lại hay kết thúc.
- Xử lý signal: Một process có thể cài đặt một signal handler để xử lý các tín hiệu này. Signal handler là một hàm được gọi khi một signal nhất định được gửi đến process đó. Ví dụ, một process có thể cài đặt một signal handler cho SIGINT để thực hiện một hành động đặc biệt khi người dùng nhấn Ctrl + C.

https://faculty.cs.niu.edu/~hutchins/csci480/signals.htm
