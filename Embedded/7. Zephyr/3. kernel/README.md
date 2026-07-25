## 1. Kernel và mô hình thread

### Thread

Trong Zephyr, thread là đơn vị thực thi cơ bản. Mỗi thread có stack riêng, một control block (`struct k_thread`) và một mức ưu tiên.

```c
#include <zephyr/kernel.h>

#define STACK_SIZE 1024
#define PRIORITY   5

K_THREAD_STACK_DEFINE(worker_stack, STACK_SIZE);
static struct k_thread worker_data;

static void worker_fn(void *p1, void *p2, void *p3)
{
    while (1) {
        printk("worker running\n");
        k_sleep(K_MSEC(1000));
    }
}

int main(void)
{
    k_thread_create(&worker_data, worker_stack, STACK_SIZE,
                    worker_fn, NULL, NULL, NULL,
                    PRIORITY, 0, K_NO_WAIT);
    return 0;
}
```

Ngoài cách tạo runtime như trên, Zephyr còn cho phép khai báo thread tĩnh tại thời điểm compile. Cách này tiết kiệm hơn vì toàn bộ cấu trúc được đặt sẵn trong `.bss`:

```c
K_THREAD_DEFINE(worker_tid, STACK_SIZE, worker_fn, NULL, NULL, NULL,
                PRIORITY, 0, 0);
```

Tham số cuối là delay khởi động tính bằng ms, đặt `0` để chạy ngay, đặt `K_TICKS_FOREVER` để thread ở trạng thái chờ cho tới khi ta gọi `k_thread_start()`.

### Mức ưu tiên và scheduler

Zephyr dùng một con số nguyên để biểu diễn độ ưu tiên, **số càng nhỏ thì ưu tiên càng cao**. Dải giá trị chia làm hai vùng:

```
-CONFIG_NUM_COOP_PRIORITIES  ...  -1    →  Cooperative thread
 0  ...  CONFIG_NUM_PREEMPT_PRIORITIES-1 →  Preemptive thread
```

Cooperative thread (ưu tiên âm) khi đã chạy thì không bị thread khác giành CPU, nó chỉ nhường khi tự block hoặc gọi `k_yield()`. Loại này hữu ích cho đoạn code cần tính nguyên tử mà không muốn tắt ngắt.

Preemptive thread (ưu tiên không âm) có thể bị chiếm quyền bất cứ lúc nào bởi thread ưu tiên cao hơn sẵn sàng chạy.

Scheduler của Zephyr là priority-based preemptive. Nguyên tắc rất đơn giản: tại mọi thời điểm, thread có độ ưu tiên cao nhất trong trạng thái ready sẽ được chạy. Nếu có nhiều thread cùng mức ưu tiên, mặc định thread đang chạy sẽ chạy tới khi block, trừ khi ta bật time slicing:

```conf
CONFIG_TIMESLICING=y
CONFIG_TIMESLICE_SIZE=10        # mỗi lát 10 ms
CONFIG_TIMESLICE_PRIORITY=0     # chỉ áp dụng cho thread ưu tiên >= 0
```

Có hai thread đặc biệt luôn tồn tại:

- **main thread**: chạy các init level cuối cùng rồi gọi `main()`. Nếu `main()` return, thread này kết thúc nhưng hệ thống vẫn chạy bình thường.
- **idle thread**: ưu tiên thấp nhất, chạy khi không còn gì để làm. Đây là nơi Zephyr đưa CPU vào chế độ tiết kiệm điện nếu bật `CONFIG_PM`.

### Các trạng thái của thread

```
        k_thread_create()
                │
                ▼
          ┌──────────┐   scheduler chọn   ┌──────────┐
          │  Ready   │ ─────────────────► │ Running  │
          └──────────┘ ◄───────────────── └──────────┘
                ▲       bị preempt / yield      │
                │                               │ k_sleep(), k_sem_take(),
   hết timeout, │                               │ chờ mutex...
   được give    │                               ▼
          ┌──────────┐                    ┌──────────┐
          │ Waiting  │ ◄───────────────── │ Waiting  │
          └──────────┘                    └──────────┘
                                                │ k_thread_abort()
                                                ▼
                                          ┌──────────┐
                                          │Terminated│
                                          └──────────┘
```

## 2. Đồng bộ và truyền dữ liệu giữa thread

Zephyr cung cấp đầy đủ các primitive quen thuộc, tất cả đều nằm trong `zephyr/kernel.h`.

**Semaphore**: đếm sự kiện, dùng nhiều nhất để ISR báo cho thread.

```c
K_SEM_DEFINE(data_ready, 0, 1);   /* giá trị đầu 0, tối đa 1 */

void isr_handler(const struct device *dev, ...)
{
    k_sem_give(&data_ready);      /* an toàn khi gọi từ ISR */
}

void consumer(void)
{
    while (1) {
        k_sem_take(&data_ready, K_FOREVER);
        process_data();
    }
}
```

**Mutex**: bảo vệ tài nguyên dùng chung, có hỗ trợ priority inheritance để tránh priority inversion. Không được gọi `k_mutex_lock()` trong ISR.

```c
K_MUTEX_DEFINE(i2c_lock);

k_mutex_lock(&i2c_lock, K_FOREVER);
i2c_write(dev, buf, len, addr);
k_mutex_unlock(&i2c_lock);
```

**Message queue**: hàng đợi các phần tử có kích thước cố định, dữ liệu được copy vào queue nên bên gửi không cần giữ buffer.

```c
struct sensor_sample {
    int32_t temp;
    uint32_t timestamp;
};

K_MSGQ_DEFINE(sample_q, sizeof(struct sensor_sample), 10, 4);

/* producer */
struct sensor_sample s = { .temp = 25, .timestamp = k_uptime_get_32() };
k_msgq_put(&sample_q, &s, K_NO_WAIT);

/* consumer */
struct sensor_sample recv;
k_msgq_get(&sample_q, &recv, K_FOREVER);
```

**FIFO / LIFO**: hàng đợi các phần tử kích thước tuỳ ý, chỉ truyền con trỏ nên không tốn chi phí copy, nhưng bên gửi phải đảm bảo vùng nhớ còn sống. Phần tử phải dành 4 byte đầu làm con trỏ liên kết.

**Work queue**: cơ chế đẩy công việc từ ISR sang ngữ cảnh thread. Đây là mẫu thiết kế cực kỳ phổ biến vì trong ISR ta không được phép block.

```c
static struct k_work my_work;

static void work_handler(struct k_work *work)
{
    /* chạy trong ngữ cảnh thread, được phép block */
    i2c_read(dev, buf, sizeof(buf), addr);
}

void isr_handler(void)
{
    k_work_submit(&my_work);   /* trả về ngay */
}

int main(void)
{
    k_work_init(&my_work, work_handler);
    return 0;
}
```

Zephyr có sẵn một system work queue (`k_sys_work_q`), đủ dùng cho phần lớn trường hợp. Nếu công việc chạy lâu hoặc cần ưu tiên riêng thì nên tạo work queue riêng bằng `k_work_queue_start()` để không làm nghẽn các subsystem khác.

## 3. Quản lý bộ nhớ

Zephyr hạn chế tối đa việc cấp phát động vì trên hệ nhúng, phân mảnh heap là nguyên nhân của những lỗi rất khó tái hiện. Có ba cơ chế chính:

**Static allocation**: cách được khuyến khích nhất. Mọi macro `K_*_DEFINE` đều cấp phát tĩnh tại thời điểm compile, không có rủi ro hết bộ nhớ lúc chạy.

**Memory slab**: cấp phát các block có kích thước cố định, thời gian cấp phát là hằng số và không bao giờ phân mảnh.

```c
K_MEM_SLAB_DEFINE(packet_slab, 128, 8, 4);   /* 8 block, mỗi block 128 byte */

void *block;
if (k_mem_slab_alloc(&packet_slab, &block, K_NO_WAIT) == 0) {
    /* dùng block */
    k_mem_slab_free(&packet_slab, block);
}
```

**Heap**: cấp phát kích thước tuỳ ý, dùng khi thật sự cần.

```c
K_HEAP_DEFINE(my_heap, 2048);

void *p = k_heap_alloc(&my_heap, 256, K_NO_WAIT);
k_heap_free(&my_heap, p);
```

Nếu muốn dùng `malloc()`/`free()` của libc, phải cấp kích thước heap qua Kconfig, mặc định là 0 nên `malloc()` sẽ luôn trả về NULL:

```conf
CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=4096
```

Về stack, mỗi thread có stack riêng và tràn stack là lỗi phổ biến nhất với người mới. Bật hai option sau khi phát triển, chúng sẽ báo lỗi rõ ràng thay vì để hệ thống chạy sai một cách khó hiểu:

```conf
CONFIG_THREAD_STACK_INFO=y
CONFIG_THREAD_ANALYZER=y
CONFIG_THREAD_ANALYZER_AUTO=y   # in mức dùng stack định kỳ
```

## 4. Xử lý ngắt

Zephyr cho phép đăng ký ISR theo hai cách. Cách tĩnh, quyết định tại thời điểm build và nhanh nhất:

```c
#include <zephyr/irq.h>

#define MY_IRQ      12
#define MY_IRQ_PRIO 2

static void my_isr(const void *arg)
{
    ARG_UNUSED(arg);
    k_sem_give(&data_ready);
}

IRQ_CONNECT(MY_IRQ, MY_IRQ_PRIO, my_isr, NULL, 0);
irq_enable(MY_IRQ);
```

Cách động, đăng ký lúc chạy, cần bật `CONFIG_DYNAMIC_INTERRUPTS=y`:

```c
irq_connect_dynamic(MY_IRQ, MY_IRQ_PRIO, my_isr, NULL, 0);
```

Nguyên tắc bắt buộc khi viết ISR:

- Không bao giờ block. Mọi lời gọi API kernel trong ISR phải dùng `K_NO_WAIT`.
- Không dùng mutex, không gọi `k_sleep()`, không dùng số thực nếu chưa cấu hình FPU cho ngữ cảnh ngắt.
- Giữ ISR càng ngắn càng tốt: đọc thanh ghi, xoá cờ ngắt, rồi đẩy phần việc nặng sang thread qua semaphore hoặc work queue.

Khi cần bảo vệ một đoạn code cực ngắn khỏi cả ngắt lẫn thread khác, dùng spinlock (hoạt động đúng cả trên hệ SMP):

```c
static struct k_spinlock lock;

k_spinlock_key_t key = k_spin_lock(&lock);
shared_counter++;
k_spin_unlock(&lock, key);
```

## 5. Device driver model

Đây là phần làm nên tính portable của Zephyr. Mọi driver đều được bọc trong `struct device`, ứng dụng lấy con trỏ tới nó từ devicetree rồi gọi API chung, hoàn toàn không biết bên dưới là chip nào.

```c
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

/* Lấy thiết bị từ node devicetree có nhãn led0 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

int main(void)
{
    if (!device_is_ready(led.port)) {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

    while (1) {
        gpio_pin_toggle_dt(&led);
        k_sleep(K_MSEC(500));
    }
}
```

Đoạn code trên chạy được trên mọi board có alias `led0` mà không cần sửa một dòng nào. Kiểm tra `device_is_ready()` trước khi dùng là bắt buộc, vì driver có thể khởi tạo thất bại (thiếu clock, sai cấu hình pin) và khi đó con trỏ vẫn hợp lệ nhưng thiết bị không hoạt động.

Bên trong, mỗi driver định nghĩa một struct chứa các con trỏ hàm (API vtable) và đăng ký với hệ thống:

```c
static const struct gpio_driver_api my_gpio_api = {
    .pin_configure = my_gpio_configure,
    .port_set_bits_raw = my_gpio_set_bits,
    /* ... */
};

DEVICE_DT_INST_DEFINE(0,
                      my_gpio_init,      /* hàm init */
                      NULL,              /* PM device */
                      &my_gpio_data,     /* dữ liệu runtime, nằm trong RAM */
                      &my_gpio_config,   /* cấu hình tĩnh, nằm trong flash */
                      POST_KERNEL,       /* init level */
                      CONFIG_GPIO_INIT_PRIORITY,
                      &my_gpio_api);
```

Cách tách `data` (RAM) và `config` (const, nằm trong flash) là quy ước chung của mọi driver Zephyr, mục đích là để phần bất biến không chiếm RAM.

## 6. Trình tự khởi động

Hiểu thứ tự boot rất quan trọng khi ta gặp lỗi kiểu "driver A cần driver B nhưng B chưa sẵn sàng".

```
Reset vector
     │
     ▼
Khởi tạo phần cứng tối thiểu (stack, clock, MPU/MMU)  ← arch/ và soc/
     │
     ▼
z_prep_c: copy .data từ flash sang RAM, xoá .bss
     │
     ▼
z_cstart: khởi tạo kernel
     │
     ├── EARLY         : chạy trước cả kernel service, dùng cho code arch đặc biệt
     ├── PRE_KERNEL_1  : driver không cần kernel service (clock control, pinctrl)
     ├── PRE_KERNEL_2  : driver cần cái ở PRE_KERNEL_1 (system timer)
     │
     ▼
Kernel service sẵn sàng, tạo main thread và idle thread
     │
     ├── POST_KERNEL   : đa số driver (gpio, i2c, spi, uart) — được phép dùng API kernel
     ├── APPLICATION   : khởi tạo mức ứng dụng
     │
     ▼
main()
```

Trong cùng một init level, thứ tự được quyết định bởi init priority (số nhỏ chạy trước). Đây là lý do các Kconfig như `CONFIG_I2C_INIT_PRIORITY`, `CONFIG_SENSOR_INIT_PRIORITY` tồn tại: driver cảm biến trên bus I2C phải có priority lớn hơn driver I2C.

Ta cũng có thể đăng ký hàm init của riêng mình vào chuỗi này:

```c
#include <zephyr/init.h>

static int my_early_setup(void)
{
    /* cấu hình gì đó trước khi main() chạy */
    return 0;
}

SYS_INIT(my_early_setup, APPLICATION, 90);
```

Nếu hàm init trả về giá trị âm, hệ thống coi như thiết bị/thành phần đó khởi tạo thất bại và `device_is_ready()` sẽ trả về false.

## 7. Subsystem

Subsystem là các khối chức năng lớn nằm trên driver, tất cả đều bật/tắt bằng Kconfig:

**Logging** (`CONFIG_LOG`): hệ thống log có phân mức, hỗ trợ deferred mode để không làm chậm ngữ cảnh gọi.

```c
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(my_module, LOG_LEVEL_DBG);

LOG_INF("sensor value: %d", val);
LOG_ERR("i2c failed: %d", ret);
```

**Shell** (`CONFIG_SHELL`): console tương tác qua UART hoặc RTT, cho phép định nghĩa lệnh riêng, rất hữu ích khi debug hiện trường.

**Settings** (`CONFIG_SETTINGS`): lưu cấu hình dạng key-value xuống flash, nền tảng cho việc lưu bonding key của Bluetooth.

**File system** (`CONFIG_FILE_SYSTEM`): hỗ trợ LittleFS, FATFS thông qua một VFS chung.

**Networking / Bluetooth / USB**: các stack lớn, mỗi cái có cây Kconfig riêng rất sâu.

**Power management** (`CONFIG_PM`, `CONFIG_PM_DEVICE`): quản lý trạng thái ngủ ở mức hệ thống và mức từng thiết bị.

## 8. User mode và bảo vệ bộ nhớ

Trên các SoC có MPU hoặc MMU, Zephyr hỗ trợ chạy thread ở chế độ user với vùng nhớ bị cô lập:

```conf
CONFIG_USERSPACE=y
```

Khi bật, thread user không truy cập trực tiếp được vào bộ nhớ kernel, mọi lời gọi API kernel đi qua system call và được kiểm tra quyền. Cơ chế phân quyền dựa trên hai khái niệm:

- **Kernel object**: semaphore, mutex, device... thread user chỉ dùng được object đã được cấp quyền qua `k_object_access_grant()`.
- **Memory domain**: nhóm các vùng nhớ mà một tập thread được phép truy cập.

Đây là tính năng khá nặng về cấu hình, phần lớn ứng dụng nhúng thông thường vẫn chạy toàn bộ ở supervisor mode. Chỉ nên bật khi ứng dụng có phần code không tin cậy hoặc cần đạt chứng nhận an toàn.

## 9. Luồng build

Ráp mọi thứ lại, đây là những gì xảy ra khi ta gõ `west build`:

```
prj.conf + Kconfig files          .dts + .overlay + bindings
        │                                    │
        ▼                                    ▼
  Kconfig processor                    Devicetree compiler
        │                                    │
        ▼                                    ▼
  build/zephyr/.config              build/zephyr/include/generated/
        │                            zephyr/devicetree_generated.h
        ▼                                    │
  autoconf.h (#define CONFIG_x)              │
        │                                    │
        └────────────────┬───────────────────┘
                         ▼
                  Compile source C
                  (kernel + drivers + app)
                         │
                         ▼
                  Link theo linker script
                  (sinh từ SoC + devicetree)
                         │
                         ▼
                  zephyr.elf → zephyr.hex / zephyr.bin
```

Kết quả nằm trong `build/zephyr/`. Vài file đáng chú ý khi debug:

- `.config`: toàn bộ cấu hình Kconfig cuối cùng.
- `zephyr.dts`: devicetree đã được merge và xử lý xong, đọc file này thay vì cố ghép các file `.dtsi` trong đầu.
- `include/generated/zephyr/devicetree_generated.h`: các macro devicetree thực sự được sinh ra.
- `zephyr.map`: bản đồ bộ nhớ, dùng để tìm xem cái gì đang ăn hết flash.

Xem nhanh mức chiếm dụng bộ nhớ:

```bash
west build -t ram_report
west build -t rom_report
```

## 10. Vài nguyên tắc thiết kế nên nhớ

Giữ ISR thật ngắn, đẩy việc sang thread qua semaphore hoặc work queue.

Ưu tiên cấp phát tĩnh, chỉ dùng heap khi không còn cách nào khác.

Không hardcode địa chỉ hay số hiệu chân, lấy mọi thứ từ devicetree để code còn portable.

Luôn kiểm tra `device_is_ready()` và giá trị trả về của API driver, phần lớn API Zephyr trả về mã lỗi âm theo chuẩn errno.

Đặt số lượng thread ở mức tối thiểu. Nhiều thread đồng nghĩa nhiều stack, nhiều context switch và nhiều cơ hội sinh race condition, phần lớn bài toán chỉ cần một work queue là đủ.
