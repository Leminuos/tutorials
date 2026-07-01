West là công cụ trung tâm của Zephyr, đảm nhiệm:
- Quản lý source code: Quản lý nhiều git repository thông qua manifest file.
- Build: Wrapper cho CMake.
- Flash & Debug: Giao tiếp với debugger/programmer.

## Khởi tạo workspace mới

```bash
west init ~/zephyrproject
cd ~/zephyrproject
west update
```

## Build example

Cú pháp chung:

```bash
cd ~/zephyrproject/zephyr
west build -p always -b <tên-board> <đường-dẫn-sample>
```

Ví dụ cho một số board phổ biến:

```bash
west build -p always -b stm32_min_dev@blue samples/basic/blinky  # Blue Pill
west build -p always -b stm32_min_dev@blue samples/hello_world
west build -p always -b stm32f4_disco      samples/basic/blinky  # STM32F4 Discovery
```

## Flash firmware lên board

Kết nối board
1. Cắm board vào máy tính
2. Bật nguồn board (optional)
3. Kiểm tra board đã được nhận diện:
   - Linux: ls `/dev/ttyACM*` hoặc `ls /dev/ttyUSB*`
   - macOS: ls /`dev/cu.usbmodem*`
   - Windows: Kiểm tra trong Device Manager

```bash
west flash
```

Lệnh này sẽ tự động phát hiện debug probe J-Link, ST-Link, OpenOCD, pyOCD,...và flash firmware vào board.

Flash với runner cụ thể

```bash
# Dùng J-Link
west flash --runner jlink

# Dùng OpenOCD
west flash --runner openocd
```

## Debug

```bash
west debug
```

Lệnh này sẽ flash firmware và mở GDB session. Các lệnh GDB cơ bản:

```
(gdb) break main        # Đặt breakpoint tại main
(gdb) continue          # Tiếp tục chạy
(gdb) next              # Bước qua (step over)
(gdb) step              # Bước vào (step into)
(gdb) print variable    # In giá trị biến
(gdb) info threads      # Xem danh sách thread
(gdb) bt                # Backtrace
(gdb) quit              # Thoát
```

## Các lệnh west hữu ích khác

```bash
west build -t menuconfig   # Mở Kconfig GUI để cấu hình
west build -t clean        # Xoá build cache
west build -t pristine     # Xoá hoàn toàn thư mục build
west boards                # Liệt kê board
west list                  # Liệt kê các project trong workspace
```

## West manifest

