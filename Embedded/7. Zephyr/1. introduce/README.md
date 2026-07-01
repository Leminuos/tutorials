## Zephyr là gì?

Zephyr là một hệ điều hành RTOS mã nguồn mở, được quản lý bởi Linux Foundation. Đây là một trong những RTOS phổ biến nhất hiện nay cho các thiết bị embedded và IoT có tài nguyên hạn chế.

Zephyr được thiết kế cho các thiết bị có tài nguyên hạn chế — từ các vi điều khiển nhỏ với vài KB RAM cho đến các hệ thống phức tạp hơn. Nó hỗ trợ hơn 700 board phần cứng từ nhiều hãng như Nordic Semiconductor, STMicroelectronics, NXP, Espressif, Intel, v.v.

## Các thành phần cốt lõi

Kernel: Zephyr cung cấp một kernel multithreaded với các cơ chế đồng bộ như semaphore, mutex, message queue, FIFO/LIFO, và timer. Ta có thể cấu hình kernel từ preemptive đến cooperative scheduling.

Devicetree: Đây là cách Zephyr mô tả phần cứng. Thay vì hardcode địa chỉ thanh ghi, ta khai báo phần cứng trong file `.dts` / `.dtsi` và truy cập qua macro trong code C. Đây là khái niệm quan trọng nhất và cũng thường gây khó khăn nhất cho người mới.

Kconfig: Hệ thống cấu hình kế thừa từ Linux kernel. Mọi tính năng của Zephyr đều được bật/tắt qua file `prj.conf` với các dòng như `CONFIG_GPIO=y`, `CONFIG_BT=y`. Hiểu Kconfig giúp ta kiểm soát kích thước firmware và bật đúng module cần thiết.

West: Công cụ meta-tool chính của Zephyr, dùng để quản lý source code, build, flash firmware, và debug. Các lệnh thường dùng: `west build`, `west flash`, `west debug`.

CMake: Zephyr dùng CMake làm hệ thống build. Mỗi ứng dụng cần tối thiểu một `CMakeLists.txt`, một `prj.conf`, và thư mục `src/` chứa code.

## Kiến trúc tổng thế

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