## 1. Kconfig là gì?

Kconfig là hệ thống cấu hình có nguồn gốc từ Linux kernel, được Zephyr kế thừa và mở rộng. Ý tưởng cốt lõi rất đơn giản: thay vì dùng `#ifdef` nằm rải rác trong source code để bật/tắt feature thì ta tập trung toàn bộ cấu hình vào một chỗ và build system sẽ tự động sinh ra một header file chứa các macro `#define` tương ứng.

Khi ta viết `CONFIG_BT=y` trong file cấu hình, build system sẽ sinh ra `#define CONFIG_BT 1` trong file `autoconf.h` và toàn bộ code liên quan đến bluetooth sẽ được compile. Nếu ta bỏ dòng đó hoặc đặt `CONFIG_BT=n` thì code bluetooth bị loại khỏi firmware giúp tiết kiệm flash và RAM.

## 2. Các loại symbol trong Kconfig

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

Ngoài ra còn `range` để giới hạn miền giá trị của `int`/`hex` và `help` để mô tả chi tiết, phần text này chính là nội dung ta thấy khi bấm `?` trong menuconfig.

```conf
config UART_TX_BUF_SIZE
    int "UART TX buffer size"
    range 32 4096
    default 256
    help
      Kích thước buffer truyền của UART tính theo byte.
      Giá trị lớn hơn giảm số lần ngắt nhưng tốn thêm RAM.
```

Nếu ta đặt giá trị ngoài `range`, Kconfig sẽ cảnh báo và tự kẹp về biên gần nhất chứ không báo lỗi build, đây là điểm rất dễ khiến ta tưởng cấu hình đã được áp dụng.

## 3. Menu, choice và cấu trúc file Kconfig

`menu` dùng để nhóm các symbol liên quan lại với nhau cho dễ đọc trong menuconfig:

```conf
menu "Sensor drivers"

config BME280
    bool "BME280 temperature sensor"

config MPU6050
    bool "MPU6050 IMU"

endmenu
```

`choice` dùng khi các lựa chọn loại trừ lẫn nhau, chỉ được chọn đúng một:

```conf
choice
    prompt "Log output backend"
    default LOG_BACKEND_UART

config LOG_BACKEND_UART
    bool "UART"

config LOG_BACKEND_RTT
    bool "SEGGER RTT"

endchoice
```

`if ... endif` là cách viết gọn thay cho việc lặp lại `depends on` ở từng symbol:

```conf
if BT

config BT_MAX_CONN
    int "Max simultaneous connections"
    default 1

endif # BT
```

Một file Kconfig có thể kéo file khác vào bằng `source` (đường dẫn tuyệt đối tính từ gốc Zephyr) hoặc `rsource` (đường dẫn tương đối so với file hiện tại):

```conf
source "subsys/bluetooth/Kconfig"
rsource "drivers/Kconfig"
```

## 4. Cơ chế phụ thuộc

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

```conf
config BT
    bool "Bluetooth support"
    imply SETTINGS
```

Khác biệt giữa `select` và `imply` rất quan trọng khi debug: `select` là ép buộc, symbol bị select sẽ bật kể cả khi ta đặt `=n` trong `prj.conf` (và Kconfig chỉ in một warning), còn `imply` chỉ đổi giá trị default nên ta vẫn tắt được. Quy ước trong Zephyr là chỉ dùng `select` cho những symbol "vô hình" với người dùng (không có prompt), còn với feature người dùng nhìn thấy thì dùng `imply` hoặc `depends on`.

Cần lưu ý `select` không kiểm tra dependency của symbol được select. Nếu `A select B` mà `B depends on C` và `C=n` thì Kconfig sẽ báo lỗi vòng phụ thuộc không hợp lệ. Cách viết an toàn là `select B if C` hoặc để `A` cũng `depends on C`.

## 5. Thứ tự ưu tiên khi áp dụng cấu hình

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

Một điểm rất hay nhầm: sửa `prj.conf` xong không được sửa trực tiếp `build/zephyr/.config` vì file này sẽ được generate lại.

## 6. Sử dụng cấu hình trong code C

Cách quen thuộc nhất là preprocessor:

```c
#ifdef CONFIG_BT
    bt_enable(NULL);
#endif
```

Tuy nhiên Zephyr khuyến khích dùng `IS_ENABLED()` vì code trong nhánh vẫn được compiler kiểm tra cú pháp, trong khi optimizer vẫn xoá nhánh không dùng nên không tốn flash:

```c
#include <zephyr/kernel.h>

if (IS_ENABLED(CONFIG_BT)) {
    bt_enable(NULL);
}
```

`IS_ENABLED` chỉ hoạt động với symbol kiểu bool, trả về 1 khi symbol `=y` và 0 khi symbol `=n` hoặc không được định nghĩa. Với symbol `int`, `hex`, `string` thì dùng trực tiếp như một hằng số:

```c
static K_THREAD_STACK_DEFINE(worker_stack, CONFIG_MAIN_STACK_SIZE);
static const char *dev_name = CONFIG_BT_DEVICE_NAME;
```

Khi cần chọn giữa hai đoạn code ở mức tiền xử lý (ví dụ khai báo biến khác nhau), dùng `COND_CODE_1`:

```c
COND_CODE_1(CONFIG_LOG_BACKEND_RTT,
            (static uint8_t rtt_buf[256];),
            (/* không dùng RTT */))
```

## 7. Viết Kconfig cho ứng dụng

Kconfig không chỉ dành cho Zephyr core, ta hoàn toàn có thể định nghĩa symbol riêng cho application. Chỉ cần tạo file `Kconfig` ngay cạnh `CMakeLists.txt`:

```
my_app/
├── CMakeLists.txt
├── Kconfig
├── prj.conf
└── src/main.c
```

Nội dung `Kconfig`:

```conf
mainmenu "My Application Configuration"

config APP_SAMPLE_INTERVAL_MS
    int "Sensor sampling interval (ms)"
    range 100 60000
    default 1000
    help
      Chu kỳ đọc cảm biến của ứng dụng.

config APP_ENABLE_DEBUG_SHELL
    bool "Enable application debug shell"
    depends on SHELL
    default n

source "Kconfig.zephyr"
```

Dòng `source "Kconfig.zephyr"` ở cuối là bắt buộc, nó kéo toàn bộ Kconfig của Zephyr vào, thiếu dòng này thì mọi `CONFIG_` của Zephyr sẽ biến mất.

Sau đó bật trong `prj.conf` và dùng trong code như symbol bình thường:

```conf
CONFIG_APP_SAMPLE_INTERVAL_MS=500
CONFIG_APP_ENABLE_DEBUG_SHELL=y
CONFIG_SHELL=y
```

```c
k_sleep(K_MSEC(CONFIG_APP_SAMPLE_INTERVAL_MS));
```

## 8. Công cụ chỉnh sửa cấu hình

Thay vì sửa tay `prj.conf`, ta có thể duyệt toàn bộ cây cấu hình bằng giao diện:

```bash
west build -t menuconfig   # giao diện terminal (ncurses)
west build -t guiconfig    # giao diện đồ hoạ (Tk)
```

Trong menuconfig, phím `/` để tìm symbol, `?` để xem help kèm danh sách dependency và những symbol nào đang `select` nó. Đây là cách nhanh nhất để trả lời câu hỏi "tại sao option này bị mờ, không bật được".

Lưu ý quan trọng: thay đổi trong menuconfig chỉ ghi vào `build/zephyr/.config` và sẽ mất khi build sạch. Sau khi tìm được cấu hình ưng ý, ta phải chép thủ công dòng `CONFIG_...` tương ứng vào `prj.conf`.

Một target hữu ích khác là `hardenconfig`, nó so sánh cấu hình hiện tại với khuyến nghị bảo mật của Zephyr và liệt kê những mục nên đổi:

```bash
west build -t hardenconfig
```

## 9. Debug các vấn đề thường gặp

Xem giá trị cuối cùng của một symbol:

```bash
grep CONFIG_MAIN_STACK_SIZE build/zephyr/.config
```

Nếu kết quả không có dòng nào hoặc là `# CONFIG_X is not set` thì symbol đang tắt.

Các lỗi hay gặp:

`warning: attempt to assign the value 'y' to the undefined symbol X`: symbol không tồn tại. Nguyên nhân thường là gõ sai tên, hoặc module/driver chứa symbol đó chưa được thêm vào build (thiếu west module, thiếu `source` trong Kconfig).

Đặt `CONFIG_X=y` trong `prj.conf` nhưng `.config` vẫn là `n`: symbol có `depends on` chưa thoả mãn. Mở menuconfig, tìm symbol và xem phần "Depends on" để biết còn thiếu gì.

Đặt `CONFIG_X=n` nhưng `.config` vẫn là `y`: có symbol khác đang `select X`. Dùng menuconfig xem mục "Selected by" để tìm thủ phạm, muốn tắt `X` thì phải tắt symbol đang select nó.

Cấu hình có vẻ không được áp dụng: kiểm tra xem file conf có thực sự được nạp không bằng cách xem log CMake ở đầu quá trình build, dòng `Using boards ... .conf` và `Merged configuration ...` liệt kê chính xác những file nào đã được dùng.

## 10. Kconfig và Devicetree khác nhau thế nào

Hai hệ thống này rất dễ nhầm vì cùng là "cấu hình", nhưng vai trò tách bạch:

Devicetree mô tả phần cứng có gì, nó nằm ở đâu, dùng chân nào, địa chỉ bao nhiêu. Đây là mô tả tĩnh của board và thường không đổi khi ta viết ứng dụng.

Kconfig mô tả phần mềm nào được biên dịch vào firmware, driver nào được bật, buffer lớn bao nhiêu.

Quan hệ điển hình là devicetree khai báo có một chip cảm biến trên bus I2C, còn Kconfig quyết định có biên dịch driver cho chip đó hay không. Trong Zephyr hiện đại, nhiều driver dùng cơ chế `default y` kèm `depends on DT_HAS_<compatible>_ENABLED`, nghĩa là chỉ cần devicetree có node tương thích thì driver tự động được bật, ta không cần khai báo gì thêm trong `prj.conf`.