## Zephyr là gì?

Zephyr là một hệ điều hành RTOS mã nguồn mở, được quản lý bởi Linux Foundation. Đây là một trong những RTOS phổ biến nhất hiện nay cho các thiết bị embedded và IoT có tài nguyên hạn chế.

Zephyr được thiết kế cho các thiết bị có tài nguyên hạn chế - từ các vi điều khiển nhỏ với vài KB RAM cho đến các hệ thống phức tạp hơn. Nó hỗ trợ hơn 700 board phần cứng từ nhiều hãng như Nordic Semiconductor, STMicroelectronics, NXP, Espressif, Intel, v.v.

## Các thành phần cốt lõi

Kernel: Zephyr cung cấp một kernel multithreaded với các cơ chế đồng bộ như semaphore, mutex, message queue, FIFO/LIFO, và timer. Ta có thể cấu hình kernel từ preemptive đến cooperative scheduling.

Devicetree: Đây là cách Zephyr mô tả phần cứng. Thay vì hardcode địa chỉ thanh ghi, ta khai báo phần cứng trong file `.dts` / `.dtsi` và truy cập qua macro trong code C. Đây là khái niệm quan trọng nhất và cũng thường gây khó khăn nhất cho người mới.

Kconfig: Hệ thống cấu hình kế thừa từ Linux kernel. Mọi tính năng của Zephyr đều được bật/tắt qua file `prj.conf` với các dòng như `CONFIG_GPIO=y`, `CONFIG_BT=y`. Hiểu Kconfig giúp ta kiểm soát kích thước firmware và bật đúng module cần thiết.

West: Công cụ meta-tool chính của Zephyr, dùng để quản lý source code, build, flash firmware, và debug. Các lệnh thường dùng: `west build`, `west flash`, `west debug`.

CMake: Zephyr dùng CMake làm hệ thống build. Mỗi ứng dụng cần tối thiểu một `CMakeLists.txt`, một `prj.conf`, và thư mục `src/` chứa code.

## Kiến trúc tổng thế

Zephyr được tổ chức thành nhiều lớp, mỗi lớp chỉ nói chuyện với lớp ngay dưới nó thông qua một API ổn định. Nhờ vậy code ứng dụng viết cho board này có thể build lại cho board khác mà gần như không phải sửa gì.

```
┌─────────────────────────────────────────┐
│           Application Code              │
├─────────────────────────────────────────┤
│    Subsystems (BLE, Net, USB, FS...)    │
├─────────────────────────────────────────┤
│      Device Drivers & HAL Layer         │
├─────────────────────────────────────────┤
│          Zephyr Kernel                  │
│  (Scheduler, Threads, IPC, Memory)      │
├─────────────────────────────────────────┤
│      Hardware Abstraction (SoC/Board)   │
├─────────────────────────────────────────┤
│           Hardware (MCU)                │
└─────────────────────────────────────────┘
```

Điểm khác biệt lớn nhất của Zephyr so với các RTOS truyền thống (FreeRTOS, ThreadX) là ranh giới giữa các lớp không phải do lập trình viên tự dựng lên, mà do ba cơ chế của chính hệ thống ép buộc:

- **Devicetree** quyết định phần cứng nào tồn tại và nằm ở đâu.
- **Kconfig** quyết định code nào được biên dịch vào firmware.
- **Device driver model** quyết định driver được khởi tạo lúc nào và ứng dụng truy cập nó qua API chung nào.

Ba cơ chế này gặp nhau tại thời điểm build, kết quả là một binary chỉ chứa đúng những gì ứng dụng cần.

## Cấu trúc thư mục source

Hiểu cây thư mục giúp ta biết nên đọc code ở đâu khi cần tra cứu:

```
zephyr/
├── arch/          # Code phụ thuộc kiến trúc CPU (arm, riscv, x86, xtensa...)
│                  # context switch, xử lý exception, khởi tạo stack
├── soc/           # Code riêng cho từng dòng SoC (stm32, nrf, esp32...)
├── boards/        # Định nghĩa board: file .dts, defconfig, pinmux
├── drivers/       # Driver thiết bị: gpio, i2c, spi, uart, sensor...
├── include/       # Public header (zephyr/kernel.h, zephyr/drivers/*.h)
├── kernel/        # Lõi kernel: scheduler, thread, semaphore, mutex, queue
├── lib/           # Thư viện tiện ích: libc, posix, cmsis, json, ring buffer
├── subsys/        # Các subsystem lớn: bluetooth, net, fs, logging, shell, usb
├── dts/           # Devicetree bindings và file .dtsi dùng chung
├── samples/       # Ví dụ mẫu
└── tests/         # Test suite
```

Quy tắc chung: `arch/` và `soc/` là phần "bẩn" nhất, phụ thuộc hoàn toàn vào phần cứng. Từ `kernel/` trở lên hoàn toàn portable. Khi porting Zephyr sang chip mới, ta chỉ chạm vào `soc/` và `boards/`.