## Kconfig là gì?

Kconfig là hệ thống cấu hình có nguồn gốc từ Linux kernel, được Zephyr kế thừa và mở rộng. Ý tưởng cốt lõi rất đơn giản: thay vì dùng `#ifdef` nằm rải rác trong source code để bật/tắt feature thì ta tập trung toàn bộ cấu hình vào một chỗ và build system sẽ tự động sinh ra một header file chứa các macro `#define` tương ứng.

Khi ta viết `CONFIG_BT=y` trong file cấu hình, build system sẽ sinh ra `#define CONFIG_BT 1` trong file `autoconf.h` và toàn bộ code liên quan đến bluetooth sẽ được compile. Nếu ta bỏ dòng đó hoặc đặt `CONFIG_BT=n` thì code bluetooth bị loại khỏi firmware giúp tiết kiệm flash và RAM.

## Các loại symbol trong Kconfig

Mỗi tuỳ chọn cấu hình được gọi là một symbol, có các kiểu sau:

bool: bật hoặc tắt. Đây là loại phổ biến nhất.

```conf
config GPIO
    bool "Enable GPIO driver"
    default y
```

int: giá trị số nguyên, thường dùng cho kích thước buffer, số lượng thread, timeout...

```conf
config MAIN_STACK_SIZE
    int "Main thread stack size"
    default 1024
```

string: chuỗi ký tự, ví dụ tên thiết bị BLE.

```conf
config BT_DEVICE_NAME
    string "Bluetooth device name"
    default "Zephyr"
```

hex: giá trị hex, thường dùng cho địa chỉ bộ nhớ.

```conf
config SRAM_BASE_ADDRESS
    hex "SRAM base address"
    default 0x20000000
```

## Cơ chế phụ thuộc

Kconfig có hệ thống dependency phức tạp, đảm bảo ta không thể bật một feature mà thiếu feature nền tảng của nó.

`depends on`: symbol chỉ xuất hiện và có thể bật khi điều kiện thoả mãn.

```conf
config BT_CENTRAL
    bool "Bluetooth Central role"
    depends on BT_HCI_HOST
```

Ở đây ta không thể bật `BT_CENTRAL` nếu chưa bật `BT_HCI_HOST`. Nếu `BT_HCI_HOST=n`, symbol `BT_CENTRAL` thậm chí không hiển thị trong menuconfig.

`select`: tự động bật symbol khác khi symbol này được bật. Đây là phụ thuộc ngược.

```conf
config BT
    bool "Bluetooth support"
    select NET_BUF
```

Khi ta bật `CONFIG_BT=y`, hệ thống tự động bật `CONFIG_NET_BUF=y` mà không cần ta khai báo. Cơ chế này rất tiện nhưng cũng hay gây nhầm lẫn, ta thấy một config được bật mà không biết ai bật nó.

`imply`: giống `select` nhưng mềm hơn, người dùng vẫn có thể tắt symbol bị imply.

## 

Zephyr áp dụng cấu hình theo thứ tự ưu tiên từ thấp đến cao:
1. default trong file Kconfig: giá trị mặc định do nhà phát triển subsystem đặt
2. Defconfig của board (`boards/<arch>/<board>/<board>_defconfig`): board tự bật các driver và tính năng phù hợp phần cứng
3. `prj.conf`: file cấu hình chính của ứng dụng, đây là nơi ta làm việc nhiều nhất
4. Overlay file (`boards/<board>.conf`): cấu hình riêng cho từng board trong cùng một ứng dụng
5. Extra conf file: truyền qua CMake flag `-DEXTRA_CONF_FILE=...`

Ưu tiên cao hơn sẽ ghi đè thấp hơn. Ví dụ nếu `defconfig` đặt `CONFIG_MAIN_STACK_SIZE=1024` nhưng `prj.conf` đặt `CONFIG_MAIN_STACK_SIZE=2048`, giá trị cuối cùng là 2048.

Luồng xử lý diễn ra như sau:

```
Kconfig files          prj.conf / defconfig
(định nghĩa symbol)    (giá trị người dùng chọn)
        │                       │
        └───────┬───────────────┘
                ▼
        Kconfig processor
        (giải quyết dependency,
         áp dụng default, validate)
                │
                ▼
        .config (file kết quả đầy đủ)
                │
                ▼
        autoconf.h
        (#define CONFIG_xxx ...)
                │
                ▼
        Code C dùng #ifdef CONFIG_xxx
        hoặc IS_ENABLED(CONFIG_xxx)
```

File `.config` nằm trong thư mục build (`build/zephyr/.config`) chứa toàn bộ cấu hình cuối cùng, kể cả những giá trị ta không khai báo mà lấy default. Đây là file rất hữu ích để debug khi ta thắc mắc "tại sao tính năng X không hoạt động".