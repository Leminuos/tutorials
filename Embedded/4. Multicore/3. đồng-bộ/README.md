## Atomic operation

Như đã nói trong bài [Đồng bộ hoá và mutual exclusion](#/docs/embedded/freertos/dong-bo-hoa-va-mutual-exclusion), khi hai hoặc nhiều task cùng sử dụng biến toàn cục mà không có cơ chế critical section thì rất dễ xảy ra race condition.

Tuy nhiên, đây là đối với hệ thống đơn core, việc disable interrupt trước khi vào critical section và enable interrupt khi thoát critical section là điều khả thi. Nhưng đối với hệ thống đa core thì khác, việc thực hiện disable interrupt hay enable interrupt sẽ chỉ ảnh hướng đến core hiện tại, các core khác sẽ không bị ảnh hưởng vì mỗi core là một thực thể độc lập.

Do đó không thể áp dụng critical section để giải quyết vấn đề race condition trong hệ thống đa core. Vậy có thể sử dụng cách gì? đó chính là atomic operation.

Atomic operation là một thao tác mà hệ thống đảm bảo thực hiện phép toán trọn vẹn, không thể bị gián đoạn bởi:
- interrupt
- task khác
- core khác

Nói cách khác: **Hoặc phép toán phải hoàn thành toàn bộ, hoặc hoàn toàn chưa xảy ra, không tồn tại trạng thái trung gian.**

Đặc điểm:
- atomic operation là nền tảng để xây dựng mutex, spinlock, semaphore,...trong hệ thống đa core.
- một atomic operation mất nhiều chu kỳ CPU nên nó phù hợp với:
  - dữ liệu nhỏ như flag, counter,...
  - thao tác đơn giản

**Sử dụng atomic operation bằng thư viện C/C++**

C11 định nghĩa atomic chuẩn hóa, compiler sẽ sinh instruction atomic phù hợp với CPU.

Ví dụ:

```c
#include <stdatomic.h>

atomic_int counter;
atomic_fetch_add(&counter, 1);
```

**Sử dụng atomic operation bằng Compare-And-Swap (CAS)**

Compare-And-Swap thực hiện atomic như sau:
- Kiểm tra trạng thái hiện tại
- Chỉ cập nhật nếu chưa bị core khác thay đổi
- Nếu bị thay đổi → phát hiện và retry 

Hầu hết CPU không có lệnh CAS trực tiếp mà dùng cặp lệnh đặc biệt:
- ARM: `LDREX`/`STREX`
- x86: `LOCK CMPXCHG`
- RISC-V: `LR`/`SC`

Ví dụ như sau:

```c
do {
    old = __LDREX(&counter);
    new = old + 1;
} while (__STREX(new, &counter));
```

## Spinlock

Mutex và semaphore khi block sẽ đưa task vào trạng thái block và chuyển CPU cho task khác. Điều này hợp lý khi thời gian chờ dài (khoảng ms trở lên). Nhưng nếu critical section chỉ mất vài µs, overhead của context switch (vài µs trên ESP32) có thể lâu hơn cả thời gian chờ. Đó là lúc spinlock có ý nghĩa.

Spinlock hoạt động đơn giản: thay vì block và nhường CPU, task chạy vòng lặp liên tục kiểm tra (busy-wait hoặc spin) cho đến khi chiếm được quyền truy cập vào tài nguyên dùng chung.

:::tip
Spinlock hoạt động dựa trên atomic operation.
:::

Trên ESP32 dual-core, spinlock đặc biệt quan trọng khi cả hai core cùng truy cập một biến. FreeRTOS trên ESP32 cung cấp `portMUX_TYPE`:

```c
static portMUX_TYPE state_spinlock = portMUX_INITIALIZER_UNLOCKED;

void update_machine_state(float new_temp) {
    taskENTER_CRITICAL(&state_spinlock);
    g_state.water_temp = new_temp;
    g_state.last_update = esp_timer_get_time();
    taskEXIT_CRITICAL(&state_spinlock);
}
```

`taskENTER_CRITICAL` trên ESP32 làm hai việc: disable interrupt trên core hiện tại và acquire spinlock để core kia không vào được. Đây là cơ chế bảo vệ mạnh nhất nhưng cũng có chi phí cao nhất nếu dùng sai - vì khi interrupt bị disable, hệ thống không phản hồi được bất kỳ event nào.

**Khác biệt giữa spinlock và mutex**

Khi task A giữ spinlock, nếu task B trên core kia cũng muốn vào, task B sẽ quay vòng lặp while chờ, chiếm 100% CPU core đó và interrupt trên core A cũng bị tắt. Toàn bộ hệ thống gần như đóng băng trong thời gian spinlock được giữ.

Khi task A giữ mutex, nếu task B muốn vào, RTOS đưa task B vào trạng thái ngủ, chuyển CPU cho task khác làm việc hữu ích, và đánh thức B khi A trả mutex. Hệ thống vẫn chạy bình thường.

**Khi nào dùng spinlock thay vì mutex?**

Khi tất cả các điều kiện sau đều đúng: critical section cực ngắn (dưới 10-20µs, chỉ gán vài biến), cần bảo vệ trên cả hai core đồng thời, và không có operation nào bên trong có thể block (không gọi I2C, không gọi malloc, không gọi bất kỳ API nào có thể chờ).

:::tip Kết luận
Khi phân vân giữa spinlock và mutex, ta nhìn vào code bên trong lock và xem đoạn code này xử lý trong bao lâu. Ngoài ra, nếu tắt toàn bộ interrupt trong khoảng thời gian lock thì sẽ xảy ra vấn đề gì?
:::

## Memory barrier

Nhiều người thường cho rằng: “Code sẽ được thực thi theo đúng thứ tự mình viết”. Tuy nhiên, điều này chỉ đúng trong các hệ thống đơn giản, và sai trong hệ thống multicore.

Trong hệ thống multicore, thứ tự ta viết code khác thứ tự mà core thực sự truy cập bộ nhớ. Các nguyên nhân gây đảo thứ tự (reordering):
- Compiler optimization: Trình biên dịch có thể hoán đổi lệnh load/store.
- CPU out-of-order execution: Trong các hệ thống hiện đại, vi xử lý chạy nhanh hơn bộ nhớ nhiều lần, do đó khi vi xử lý chờ dữ liệu trả về nó có thể xử lý một lượng lớn lệnh -> điều này có thể khiến vi xử lý thực hiện các lệnh sẵn sàng được xử lý bất kể thứ tự ban đầu trong chương tình -> có thể gây reordering.
- Cache & bus: Ghi vào cache chưa chắc đã ra RAM ngay.

Ví dụ:

```c
// Core 0
data = 42;
ready = 1;

// Core 1
while (!ready);
printf("%d\n", data);
```

-> Khi reordering xảy ra thì core 1 có thể thấy `ready = 1` nhưng chưa chắc thấy `data = 42`.

Reordering này không thể nhìn thấy được, và developer thường không cần phải quan tâm đến khái niệm memory barrier. Tuy nhiên, trong một số trường hợp, chúng ta cần quan tâm đến vấn đề reordering này khi có một số vấn đề xảy ra. Ví dụ, device driver hoặc khi chương trình cần đồng bộ hóa dữ liệu giữa các luồng.

Kiến trúc ARM cung cấp một số lệnh memory barrier, cho phép chương trình buộc core phải đợi việc truy cập bộ nhớ phải hoàn thành.

Memory barrier hay còn gọi là memory fence là tập các instruction đảm bảo rằng các thao tác đọc/ghi bộ nhớ trước barrier phải hoàn tất trước khi các thao tác sau barrier được phép xảy ra. Điều này giúp đảm bảo thứ tự các phép đọc/ghi bộ nhớ được thực hiện tuần tự không bị reording.

**DSB - Data Synchronization Barrier**

Lệnh này buộc processor phải đợi toàn bộ các truy cập dữ liệu đang chờ được hoàn tất, trước khi bất kỳ lệnh nào khác có thể được thực hiện.

**DMB - Data Memory Barrier**

Lệnh này sử dụng giữa hai lệnh truy cập bộ nhớ để đảm bảo lệnh đứng trước DMB sẽ hoàn thành trước lệnh đứng sau của DMB, đảm bảo 2 lần truy cập bộ nhớ sẽ không bị ảnh hưởng đến nhau. Lệnh này chỉ ảnh hưởng đến truy cập dữ liệu, không ảnh hưởng đến thứ tự thực hiện các lệnh của processor.

Ví dụ 1: Trong trường hợp dưới đây với hệ thống sử dụng 2 core và cùng truy cập vào vùng shared memory, core 1 ghi data vào địa chỉ lưu trong thanh ghi R1, và cùng lúc thì core 2 cũng truy cập read vào địa chỉ này:

Core 1:

```asm
STR R5, [R1];   // set new data 
LDR R6, [R2];
```

Core 2:

```asm
STR R6, [R2];
LDR R5, [R1];   //read new data
```

Đối với bài toán này, developer có thể giải quyết bằng cách sau: Tạo thêm một cờ để đợi việc ghi hoàn thành, cờ được lưu vào địa chỉ trong thanh ghi R2.

Core 1:

```asm
STR R5, [R1];   // set new data 
STR R0, [R2];   // send flag indicating data ready 
```

Core 2:

```asm
WAIT([R2]==1);  // wait on flag 
LDR R5, [R1];   // read new data
```

Tuy nhiên với cách này, việc ordering có thể ảnh hưởng đến kết quả. Ở core 1, việc ghi chưa thực hiện xong, nhưng cờ đã được set lên. Lúc này core 2 đã đợi được cờ và tiến hành đọc dữ liệu.
    
Trong trường hợp này, Data Memory Barrier có thể được sử dụng để đảm bảo quá trình set và check cờ hoạt động đúng.

Core 1:

```asm
STR R5, [R1];   // set new data
DMB;            // ensure all observers observe data before the flag
STR R0, [R2];   //send flag indicating data ready
```

Core 2:

```asm
WAIT([R2]==1);  // wait on flag 
DMB;            // ensure that the load of data is after setting flag
LDR R5, [R1];
```