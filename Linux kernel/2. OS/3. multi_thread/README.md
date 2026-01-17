# Multi thread

**Thread là một luồng xử lý mà kernel coi là một đối tượng được scheduler quản lý:**
- Kernel coi tất cả thread là một `task_struct`.
- Khi process chạy, ban đầu nó sẽ có một luồng duy nhất gọi mà main thread.
- Trong main thread, ta có thể tạo thêm các luồng khác thông qua hàm `pthread_create`.
- Khi main kết thúc, nếu không gọi hàm `pthread_exit` để giữ thread chạy tiếp thì process sẽ kết thúc toàn bộ.
- Các thread sẽ nằm trong hàng đợi **runqueue** và scheduler sẽ lựa chọn task nào sẽ được chạy và chạy tại core nào.

Khi scheduler muốn chuyển task, nó sẽ thực hiện context switch:
- context switch được chia ra làm hai loại là thread context và process context.
- Thread context thì save trạng thái hiện tại của task cũ vào `task_struct` và load trạng thái của task mới từ `task_struct` vào core.
- process context thì nó sẽ thực hiện load TLB nếu thread thuộc về process khác. Để làm được điều này thì nó sẽ thông qua con trỏ `mm_struct` trong `task_struct`, con trỏ này sẽ trỏ tới virtual memory space của process.