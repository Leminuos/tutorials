## 1. Zephyr tìm board của bạn ở đâu?

Giả sử board nằm ở `D:\zephyr_stm32_f4ve\boards\` còn Zephyr nằm ở `D:\zephyrproject\zephyr\`. Hai nơi khác nhau, vậy thì làm sao Zephyr tìm ra?

Câu trả lời nằm ở `app/CMakeLists.txt`:

```cmake
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
```

`BOARD_ROOT` là danh sách các thư mục chứa board. Với mỗi mục trong danh sách, Zephyr tự động ghép thêm `/boards` rồi quét đệ quy để tìm file `board.yml`. Ở ví dụ trên, `BOARD_ROOT` trỏ tới `D:\zephyr_stm32_f4ve\` nên Zephyr sẽ quét `D:\zephyr_stm32_f4ve\boards\`.

Cấu trúc thư mục `boards/<vendor>/<board>/` cũng là bắt buộc theo chuẩn Zephyr 3.7 trở lên.
 
Ngoài `CMakeLists.txt`, ta cũng có thể khai báo `BOARD_ROOT` qua:

- Biến môi trường `BOARD_ROOT`
- Dòng lệnh: `west build -- -DBOARD_ROOT=D:/zephyr_stm32_f4ve`
- Khai báo module trong `zephyr/module.yml`

Cách đặt trong `CMakeLists.txt` là gọn nhất cho project nhỏ.

Ba điểm cần lưu ý:

| Vấn đề | Hệ quả |
|---|---|
| Đặt `list(APPEND BOARD_ROOT ...)` sau `find_package(Zephyr ...)` | Biến chưa tồn tại khi Zephyr quét -> `board not found`. |
| Trỏ `BOARD_ROOT` thẳng vào thư mục `boards/` | Zephyr sẽ đi tìm `boards/boards/` -> không thấy gì. Phải trỏ vào thư mục cha của `boards/`. |

## 2. Cấu trúc thư mục chuẩn

```
zephyr_stm32_f4ve/
├── app/
│   ├── CMakeLists.txt          <- khai báo BOARD_ROOT ở đây
│   ├── prj.conf
│   └── src/main.c
└── boards/
    └── others/                 <- <vendor>
        └── stm32_f4ve/         <- <board>
            ├── board.yml
            ├── Kconfig.stm32_f4ve
            ├── stm32_f4ve.dts
            ├── stm32_f4ve_defconfig
            ├── stm32_f4ve.yaml
            ├── board.cmake
            ├── Kconfig.defconfig
            ├── Kconfig
            └── doc/index.rst
```

## 3. Chi tiết từng file

### 3.1. File board.yml

Khi Zephyr quét các thư mục `BOARD_ROOT`, nó tìm chính xác file tên `board.yml`. Nếu thấy file này, thì nó sẽ hiểu thư mục này là một board.

Ví dụ nội dung `board.yml`:

```
board:
  name: stm32_f4ve
  full_name: Black STM32 F4VE Development Board
  vendor: others
  socs:
    - name: stm32f407xx
```

Giải thích từng trường:

- `name`: định danh dùng ở dòng lệnh. Chính vì dòng này mà ta mới gõ được:
   
```bash
west build -b stm32_f4ve app
```

  Tên này cũng sinh ra symbol Kconfig `BOARD_STM32_F4VE`.

- `full_name`: chỉ hiển thị trong tài liệu và output của `west boards`. Không ảnh hưởng đến quá trình build.
- `socs`: Đây là trường quan trọng. Nó nói board này gắn với SoC nào, từ đó Zephyr mới biết phải kéo:
  - Thư mục SoC `soc/st/stm32/stm32f4x/` (code khởi tạo, linker script, cấu hình vector table)
  - Kiến trúc ARM Cortex-M4F và toolchain tương ứng
  - Các driver cho dòng SoC đó

Nếu xoá file này thì khi thực hiện lệnh `west build -b stm32_f4ve` nó sẽ báo lỗi như sau:

```
No board named 'stm32_f4ve' found. Did you mean:

stm32f411e_disco
stm32f4_disco

Run 'west boards' for the full list.
CMake Error at D:/zephyrproject/zephyr/cmake/modules/boards.cmake:221 (message):
  Invalid BOARD; see above.
Call Stack (most recent call first):
  D:/zephyrproject/zephyr/cmake/modules/zephyr_default.cmake:128 (include)
  D:/zephyrproject/zephyr/share/zephyr-package/cmake/ZephyrConfig.cmake:66 (include)
  D:/zephyrproject/zephyr/share/zephyr-package/cmake/ZephyrConfig.cmake:92 (include_boilerplate)
  CMakeLists.txt:5 (find_package)


-- Configuring incomplete, errors occurred!
```

Ngoài ra, nếu ta có file `board.yml` rồi mà ta thực hiện lệnh build `west build -b stm32_f4v` (board không cùng tên với trường `name`) thì nó cũng báo lỗi như ở trên.

### 3.2. File `Kconfig.stm32_f4ve`

Tên file bắt buộc có dạng `Kconfig.<tên board trong board.yml>`. Board tên `stm32_f4ve` thì file phải là `Kconfig.stm32_f4ve`. Sai một ký tự thì Zephyr không nạp file và lỗi báo ra rất khó hiểu, thường là lỗi ở tận bước link.

Ví dụ nội dung của file:

```
config BOARD_STM32_F4VE
	select SOC_STM32F407XE
```

Symbol `BOARD_STM32_F4VE` được Zephyr tự động bật khi ta build project với `-b stm32_f4ve`.

**File Kconfig.defconfig**

Nơi đặt giá trị mặc định cho các option Kconfig khi board này được chọn

**File stm32_f4ve_defconfig**

Cấu hình nền tảng luôn được áp dụng cho mọi app build cho board này, trước khi prj.conf của app được merge vào.

### 3.3. File `stm32_f4ve_defconfig`

Là một Kconfig fragment (cú pháp giống `prj.conf`), luôn được áp dụng cho mọi app build cho board này và được merge trước `prj.conf` của app.

```
CONFIG_ARM_MPU=y
CONFIG_HW_STACK_PROTECTION=y
CONFIG_SERIAL=y
CONFIG_CONSOLE=y
CONFIG_UART_CONSOLE=y
CONFIG_GPIO=y
CONFIG_CLOCK_CONTROL=y
```

:::tip Nguyên tắc
Chỉ đặt ở đây những thứ thuộc về bản thân phần cứng.
:::

### 3.4. File `Kconfig.defconfig`

Đây là file dễ nhầm với `<board>_defconfig` nhất. Khác biệt:

| | `<board>_defconfig` | `Kconfig.defconfig` |
|---|---|---|
| Cú pháp | fragment (`CONFIG_X=y`) | ngôn ngữ Kconfig (`config X` / `default`) |
| Bản chất | gán giá trị | đổi giá trị mặc định |

```
if BOARD_STM32_F4VE

config NETWORKING
	default n

endif # BOARD_STM32_F4VE
```

Dùng file này khi ta muốn nói nếu ai đó bật tính năng X trên board này thì giá trị hợp lý là Y, chứ không phải ép bật. Ví dụ: nếu app bật `CONFIG_FLASH`, thì kích thước page mặc định nên là bao nhiêu.

### 3.5. File `board.cmake`

Quyết định `west flash` / `west debug` gọi công cụ nào:

```cmake
board_runner_args(jlink "--device=STM32F407VE" "--speed=4000")
board_runner_args(openocd "--target-handle=_CHIPNAME.cpu")
board_runner_args(dfu-util "--pid=0483:df11" "--alt=0")

include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/dfu-util.board.cmake)
```

Thiếu file này thì build vẫn thành công, chỉ là `west flash` không biết làm gì. Runner đầu tiên được `include` sẽ là mặc định.

### 3.6. File `stm32_f4ve.yaml`

Mô tả năng lực board cho hệ thống test tự động, đồng thời giúp `west build` cảnh báo sớm khi sample yêu cầu tính năng board không có:

```yaml
identifier: stm32_f4ve
name: Black STM32 F4VE
type: mcu
arch: arm
toolchain:
  - zephyr
  - gnuarmemb
ram: 128
flash: 512
supported:
  - gpio
  - uart
  - spi
  - i2c
```

## 4. Luồng xử lý khi build

Khi gõ `west build -b stm32_f4ve app` thì zephyr sẽ thực hiện:
1. CMake chạy `app/CMakeLists.txt` $\rightarrow$ `BOARD_ROOT` được thêm vào
2. `find_package(Zephyr)` $\rightarrow$ quét `<BOARD_ROOT>/boards/**/board.yml`
3. Tìm thấy name: `stm32_f4ve` $\rightarrow$  xác định vendor, SoC
4. Nạp `Kconfig.stm32_f4ve` $\rightarrow$  select `SOC_STM32F407XE` $\rightarrow$ kéo về `soc/st/stm32/`
5. Merge `Kconfig`: defaults $\rightarrow$ `Kconfig.defconfig` $\rightarrow$ `<board>_defconfig` $\rightarrow$ `prj.conf` $\rightarrow$ `build/zephyr/.config`
6. Merge devicetree: soc `.dtsi` $\rightarrow$ board `.dts` $\rightarrow$ app overlay $\rightarrow$ `zephyr.dts` $\rightarrow$ `devicetree_generated.h`
7. Compile, link theo linker script của SoC
8. west flash $\rightarrow$ đọc `board.cmake` $\rightarrow$ gọi `openocd/jlink/dfu-util`