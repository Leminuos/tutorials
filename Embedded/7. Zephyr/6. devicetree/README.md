## 1. Devicetree là gì?

Devicetree là cấu trúc dữ liệu dạng cây mô tả phần cứng. Mỗi node trong cây là một thành phần phần cứng (một UART controller, một chân LED, một cảm biến trên bus I2C...) và mỗi node mang theo các property mô tả thuộc tính của thành phần đó: địa chỉ thanh ghi, số interrupt, tốc độ clock, chân GPIO...

Ý tưởng giống Kconfig ở chỗ tách cấu hình ra khỏi source code, nhưng phân chia vai trò rõ ràng:

- **Kconfig** trả lời câu hỏi *phần mềm nào được biên dịch vào firmware?* (bật driver I2C, bật bluetooth, chọn kích thước stack).
- **Devicetree** trả lời câu hỏi *phần cứng có những gì và nằm ở đâu?* (I2C1 nằm ở địa chỉ `0x40005400`, cảm biến BME280 nằm ở address `0x76` trên bus đó).

Hai hệ thống này bổ trợ cho nhau. Một driver chỉ thực sự hoạt động khi vừa được bật trong Kconfig, vừa có node tương ứng trong devicetree với `status = "okay"`.

Điểm khác biệt lớn nhất so với Linux: Zephyr không đọc devicetree lúc runtime. Không có file `.dtb` nào được nạp vào bộ nhớ. Toàn bộ cây được xử lý tại thời điểm build và biến thành các macro trong header file. Điều này có nghĩa mọi truy cập devicetree đều là hằng số biên dịch, không tốn RAM, không tốn thời gian parse.

## 2. Cú pháp devicetree

Cú pháp device tree có thể xem chi tiết tại:
- https://www.devicetree.org/specifications
- https://leminuos.github.io/tutorial-dashboard/#/docs/linux-kernel/driver/device-tree

Trong zephyr, ta có danh sách thuộc tính của node `/chosen` thường gặp:

| Chosen key | Vai trò | Consumer |
| --- | --- | --- |
| `zephyr,console` | Đích của printk/console | console subsystem |
| `zephyr,shell-uart` | UART backend cho shell | shell subsystem |
| `zephyr,sram` | RAM chính | linker script |
| `zephyr,flash` | Flash chính | linker + flash subsystem |
| `zephyr,code-partition` | Partition chứa code chạy | build/linker |
| `zephyr,flash-controller` | Controller quản lý flash | flash API |
| `zephyr,entropy` | Nguồn entropy (RNG) | crypto/random |
| `zephyr,bt-hci` | Controller Bluetooth HCI | BLE stack |
| `zephyr,canbus` | CAN bus mặc định | CAN subsystem |
| `zephyr,display` | Display mặc định | display subsystem |
| `zephyr,uart-mcumgr` | Transport cho mcumgr | mcumgr |

Ví dụ node `/chosen` tiêu biểu:

```
/ {
    chosen {
        zephyr,console      = &usart1;    /* printk/console đi ra usart1 */
        zephyr,shell-uart   = &usart1;    /* shell cũng dùng usart1 */
        zephyr,sram         = &sram0;     /* RAM chính của hệ thống */
        zephyr,flash        = &flash0;    /* flash chính */
        zephyr,code-partition = &slot0_partition;
    };
};
```

Ngoài ra còn hai directive dùng để xoá node, hay gặp khi đọc file DTS của SoC và khi viết overlay.

**`/omit-if-no-ref/`** đặt ngay trước phần khai báo node, nó nói với trình sinh code rằng:

> Nếu không có ai tham chiếu (`&label`) tới node này ở bất kỳ đâu trong devicetree thì bỏ nó đi — đừng sinh macro, đừng đưa vào build.

Nó sinh ra để giải quyết đúng một bài toán: giảm dung lượng firmware do các node pinctrl không dùng đến.

Một con STM32F4 có hàng nghìn tổ hợp chân và alternate function, file `.dtsi` khai báo sẵn tất cả:

```dts
/* dts/arm/st/f4/stm32f401-pinctrl.dtsi */

/omit-if-no-ref/ usart1_tx_pa9: usart1_tx_pa9 {
    pinmux = <STM32_PINMUX('A', 9, AF7)>;
    bias-pull-up;
};

/omit-if-no-ref/ usart1_tx_pb6: usart1_tx_pb6 {
    pinmux = <STM32_PINMUX('B', 6, AF7)>;
    bias-pull-up;
};
```

Board của ta chỉ dùng một trong hai:

```dts
&usart1 {
    pinctrl-0 = <&usart1_tx_pa9 &usart1_rx_pa10>;
    pinctrl-names = "default";
    status = "okay";
};
```

Kết quả: `usart1_tx_pa9` được giữ lại vì có phandle trỏ tới còn `usart1_tx_pb6` bị xoá sạch do không xuất hiện trong `build/zephyr/zephyr.dts`. Nếu không có directive này thì header sinh ra sẽ phình lên rất lớn chỉ để chứa các tổ hợp chân mà không board nào dùng.

| Tình huống | Kết quả |
|---|---|
| `/omit-if-no-ref/`, có phandle trỏ tới, `status = "disabled"` | Node vẫn tồn tại, vẫn sinh macro |
| `/omit-if-no-ref/`, không ai trỏ tới, `status = "okay"` | Node bị xoá hoàn toàn |
| Không có `/omit-if-no-ref/`, không ai trỏ tới | Node vẫn tồn tại |

Vì vậy nếu `DT_NODELABEL(usart1_tx_pb6)` báo lỗi biên dịch trong khi ta nhìn rõ node đó trong file `.dtsi`, gần như chắc chắn node có `/omit-if-no-ref/` và chưa ai trỏ tới nó.

**`/delete-node/`** thì ngược lại, xoá vô điều kiện. Nó đặt trong overlay để gỡ bỏ một node đã có sẵn từ SoC hoặc board DTS:

```dts
&i2c2 {
    /delete-node/ bme280@76;
};
```

Tương tự nó còn có `/delete-property/` để gỡ một property đơn lẻ:

```dts
&usart1 {
    /delete-property/ current-speed;
};
```

## 3. Devicetree Bindings

### 3.1. Vì sao cần bindings

Devicetree tự nó không có kiểu dữ liệu. Nhìn vào dòng `current-speed = <115200>;`, compiler chỉ thấy một mảng 32 bit, nó không biết đây là một số nguyên hay mảng, cũng không biết node `bosch,bme280` bắt buộc phải có property `reg`.

Bindings sinh ra để khắc phục vấn đề này. Mỗi giá trị `compatible` có một file YAML đóng vai trò **schema**: khai báo node đó được phép có property nào, property nào bắt buộc, kiểu dữ liệu là gì, giá trị nào hợp lệ. Build system dùng schema này để:

1. **Validate**: thiếu property bắt buộc hoặc sai kiểu thì build fail ngay, kèm thông báo rõ ràng.
2. **Sinh macro:** chỉ những property được khai báo trong binding mới có macro `DT_PROP(...)` tương ứng.
3. **Điền default:** property không viết trong DTS vẫn có giá trị nếu binding khai báo `default:`.

Hệ quả quan trọng: **không có binding thì không có macro**. Nếu ta gõ sai một chữ trong `compatible`, build vẫn qua trót lọt nhưng mọi `DT_PROP()` lên node đó đều lỗi biên dịch với thông báo khó hiểu. Đây là lỗi hay gặp thứ hai, chỉ sau `status = "disabled"`.

### 3.2. Build system tìm binding ở đâu

Zephyr quét thư mục con `dts/bindings/` trong các vị trí sau:

| Vị trí | Dùng khi |
|---|---|
| `<zephyr>/dts/bindings/` | Binding chính thức, đi kèm driver upstream |
| `<app>/dts/bindings/` | Binding riêng của ứng dụng, nơi ta tự viết |
| `<board>/dts/bindings/` | Binding riêng cho board |
| `<shield>/dts/bindings/` | Binding cho shield |
| Đường dẫn trong `DTS_ROOT` | Thư viện binding dùng chung nhiều dự án |
| Module có khai báo `dts_root` | Module ngoài |

Tên file không ảnh hưởng tới việc tìm kiếm, build system đọc hết mọi file YAML rồi match theo trường `compatible:` bên trong. Nhưng quy ước là **đặt tên file trùng giá trị compatible**, ví dụ `bosch,bme280.yaml`, để người khác dễ tra cứu.

Cách thêm binding riêng vào một dự án:

```
my_app/
├── CMakeLists.txt
├── prj.conf
├── app.overlay
├── dts/
│   └── bindings/
│       └── mycompany,myled.yaml  <- tự động được nhặt
└── src/
    └── main.c
```

Không cần khai báo gì trong CMake, chỉ cần đúng đường dẫn `dts/bindings/`.

### 3.3. Quy tắc match node với binding

Bắt đầu bằng một ví dụ tối giản. Giả sử devicetree có node sau:

```dts
bar_device: bar-device {
    compatible = "foo-company,bar-device";
    num-foos = <3>;
};
```

Và trong `dts/bindings/` có file YAML:

```yaml
compatible: "foo-company,bar-device"

properties:
  num-foos:
    type: int
    required: true
```

Sợi dây nối hai file này là property `compatible`. Node khai báo `compatible = "foo-company,bar-device"`, giá trị đó trùng đúng với dòng `compatible:` trong binding, nên build system biết phải áp luật của binding này lên node.

Binding nói rằng `num-foos` bắt buộc phải có (`required: true`) và phải là một số nguyên (`type: int`). Build system kiểm tra và thấy node khai báo `num-foos = <3>` — hợp lệ. Nếu node thiếu hẳn dòng đó, hoặc viết `num-foos = "three"`, build sẽ dừng lại kèm thông báo lỗi chỉ rõ node nào và binding nào.

Sau khi validate xong, build system sinh macro trong header để code C đọc được giá trị:

```c
DT_PROP(DT_NODELABEL(bar_device), num_foos)   /* bung ra thành 3 */
```

Chú ý `num-foos` trong DTS thành `num_foos` trong macro — dấu `-` luôn được đổi thành `_`.

Nếu ta thêm vào node một property mà binding không khai báo, ví dụ `num-bars = <5>`, thì build **báo lỗi** vì binding đóng vai trò schema đầy đủ, không cho phép property lạ. Ngược lại, nếu node dùng một `compatible` không có binding nào tương ứng thì build vẫn qua nhưng không macro property nào được sinh ra.

Với node có nhiều `compatible`, build system duyệt danh sách **từ trái sang phải**, dùng binding đầu tiên tìm thấy:

```dts
compatible = "st,stm32f401-uart", "st,stm32-uart";
```

Nếu có binding cho `st,stm32f401-uart` thì dùng nó, không có thì lùi về `st,stm32-uart`. Cơ chế này cho phép mô tả chi tiết mà vẫn tương thích ngược.

Ba trường hợp node không có `compatible` mà vẫn được validate:

- Node con của một binding có `child-binding:` (xem bên dưới).
- Node đặc biệt do Zephyr xử lý riêng: `/aliases`, `/chosen`, `/zephyr,user`.
- Node không match gì cả, vẫn tồn tại trong cây nhưng không sinh macro property.

### 3.4. Cấu trúc một file binding

```yaml
# dts/bindings/sensor/bosch,bme280-i2c.yaml

description: |
  Cảm biến nhiệt độ / áp suất / độ ẩm BME280 giao tiếp qua I2C.

  Ví dụ sử dụng:

      &i2c1 {
          bme280@76 {
              compatible = "bosch,bme280";
              reg = <0x76>;
          };
      };

compatible: "bosch,bme280"

include: [sensor-device.yaml, i2c-device.yaml]

on-bus: i2c

properties:
  reg:
    required: true

  sampling-rate:
    type: int
    default: 10
    description: Tần số lấy mẫu tính bằng Hz
```

Các key ở cấp cao nhất:

| Key | Ý nghĩa |
|---|---|
| `description` | Mô tả. Nên viết kỹ, đây là tài liệu duy nhất người dùng driver đọc. |
| `compatible` | Giá trị compatible mà binding này mô tả. |
| `include` | Kế thừa property từ file YAML khác. |
| `properties` | Danh sách property và ràng buộc của chúng. |
| `child-binding` | Schema áp cho tất cả node con. |
| `bus` | Khai báo node này là một bus controller, ví dụ `bus: i2c`. |
| `on-bus` | Khai báo node này nằm trên một bus, ví dụ `on-bus: i2c`. |
| `<name>-cells` | Đặt tên cho các ô đi sau phandle. |
| `specifier-space` | Chỉ định không gian specifier khi tên property không theo quy ước. |

### 3.5. Các kiểu property

Đây là bảng cần thuộc, vì `type:` sai thì macro sinh ra sẽ khác hoàn toàn:

| `type` | Ví dụ trong DTS | Ghi chú |
|---|---|---|
| `string` | `status = "okay";` | Một chuỗi |
| `int` | `current-speed = <115200>;` | Một số 32 bit |
| `boolean` | `hw-flow-control;` | Có mặt = true. Không bao giờ viết `= <0>` |
| `array` | `offsets = <0x100 0x200>;` | Mảng số 32 bit |
| `uint8-array` | `mac-address = [de ad be ef];` | Chuỗi byte, dùng ngoặc vuông |
| `string-array` | `dma-names = "tx", "rx";` | Nhiều chuỗi |
| `phandle` | `interrupt-parent = <&gic>;` | Một tham chiếu |
| `phandles` | `pinctrl-0 = <&pin_a &pin_b>;` | Nhiều tham chiếu, không có specifier |
| `phandle-array` | `dmas = <&dma0 2>, <&dma0 3>;` | Tham chiếu kèm specifier |
| `path` | `zephyr,uart = &uart0;` | Đường dẫn tới node khác |
| `compound` | `foo = <&lbl>, [01 02];` | Hỗn hợp — **không sinh macro**, tránh dùng |

### 3.6. Ràng buộc trên từng property

```yaml
properties:
  current-speed:
    type: int
    required: true
    description: Baud rate khởi tạo

  maximum-speed:
    type: string
    enum: ["low-speed", "full-speed", "high-speed"]
    default: "full-speed"

  resolution:
    type: int
    enum: [8, 16, 24, 32]

  brightness:
    type: int
    min: 0
    max: 100

  coordinates:
    type: array
    min-len: 3
    max-len: 3

  "#address-cells":
    type: int
    required: true
    const: 1

  old-property:
    type: int
    deprecated: true
```

Ý nghĩa từng key:

- `required: true` - thiếu property này thì build fail. Mặc định là `false`.
- `default:` - giá trị dùng khi DTS không khai báo. **Không được đi cùng `required: true`** (mâu thuẫn logic: đã bắt buộc thì không cần default).
- `enum:` - danh sách giá trị hợp lệ.
- `const:` - property phải đúng bằng giá trị này.
- `min` / `max` - chặn khoảng giá trị cho kiểu số.
- `min-len` / `max-len` - chặn độ dài cho kiểu mảng.
- `deprecated: true` - vẫn dùng được nhưng in cảnh báo lúc build.

Một lưu ý về `default:`: giá trị mặc định nằm trong binding, tức là nằm ở phía driver. Người viết ứng dụng nhìn vào DTS sẽ không thấy nó. Vì vậy chỉ đặt `default:` cho giá trị thực sự an toàn với mọi phần cứng.

### 3.7. Kế thừa với `include`

Rất hiếm khi ta viết binding từ đầu. Zephyr có sẵn một loạt file cơ sở trong `zephyr/dts/bindings/base/`:

```yaml
include: base.yaml            # status, compatible, reg, label...
```

Các file hay dùng:

| File | Cung cấp |
|---|---|
| `base.yaml` | `status`, `compatible`, `reg`, `interrupts`, `clocks`... |
| `i2c-device.yaml` | `reg` bắt buộc + `on-bus: i2c` |
| `spi-device.yaml` | `reg` + các property SPI (`spi-max-frequency`, `duplex`...) |
| `sensor-device.yaml` | Property chung cho sensor |
| `pinctrl-device.yaml` | `pinctrl-0`, `pinctrl-names`... |
| `uart-controller.yaml` | `current-speed`, `parity`, `stop-bits`... |
| `gpio-controller.yaml` | `gpio-controller`, `#gpio-cells` |

Khi include nhiều file, chúng được merge đệ quy. Quy tắc xử lý xung đột: `required: true` **thắng** `required: false`, nhưng không có chiều ngược lại — tức là ta có thể siết chặt thêm ràng buộc kế thừa, không thể nới lỏng.

Nếu chỉ muốn lấy một phần:

```yaml
include:
  - name: base.yaml
    property-allowlist: [status, compatible, reg]

  - name: spi-device.yaml
    property-blocklist: [duplex, frame-format]
```

### 3.8. Specifier cells

Với property kiểu `phandle-array`, các số đi sau phandle cần được đặt tên thì mới truy cập được từ code C. Việc đặt tên nằm ở binding của **node được trỏ tới**, không phải node trỏ đi.

```yaml
# binding của GPIO controller
gpio-cells:
  - pin
  - flags
```

```yaml
# binding của PWM controller
pwm-cells:
  - channel
  - period
  - flags
```

Từ khai báo trên, dòng DTS `gpios = <&gpioa 5 GPIO_ACTIVE_HIGH>;` cho ta:

```c
DT_GPIO_PIN(node, gpios)     /* 5 */
DT_GPIO_FLAGS(node, gpios)   /* GPIO_ACTIVE_HIGH */
DT_GPIO_CTLR(node, gpios)    /* node của gpioa */

/* dạng tổng quát */
DT_PHA(node, gpios, pin)
DT_PHA_BY_IDX(node, gpios, 0, flags)
```

### 3.9. Debug binding

Khi property đọc ra không đúng hoặc macro không tồn tại:

**1. Xem file binding thực sự được dùng.** Build system ghi lại ánh xạ node $\rightarrow$ binding ngay trong `build/zephyr/zephyr.dts` dưới dạng comment ở đầu file và chi tiết hơn trong `build/zephyr/edt.pickle` (dạng binary). Cách nhanh nhất là mở `devicetree_generated.h` và tìm tên node.

**2. Không thấy macro nào cho node.** Chắc chắn là binding không khớp: sai chính tả `compatible`, file YAML đặt sai thư mục, hoặc thiếu `on-bus` khi node nằm trên bus.

**3. Lỗi kiểu `'foo' appears in /path/to/node in ... but is not declared in 'properties:'`.** Property có trong DTS nhưng thiếu trong binding, thêm nó vào `properties:` là xong.

**4. Property có mà `DT_PROP` báo undefined.** Thường do gõ tên với dấu `-` thay vì `_`, hoặc property kiểu `compound` (kiểu này không sinh macro).

**5. Tra cứu binding có sẵn.** Toàn bộ binding chính thức được render thành tài liệu tại trang Devicetree bindings index trên docs.zephyrproject.org.

## 4. Truy cập Devicetree từ code C/C++

Toàn bộ API devicetree nằm trong `<zephyr/devicetree.h>` và đều là macro preprocessor, mọi kết quả là hằng số biên dịch.

Để thống nhất cho mọi ví dụ dưới đây, ta bám vào devicetree mẫu sau:

```dts
/ {
    aliases {
        led0 = &green_led;
    };

    chosen {
        zephyr,console = &uart1;
    };

    leds {
        compatible = "gpio-leds";
        green_led: led_0 {
            gpios = <&gpioa 5 GPIO_ACTIVE_HIGH>;
            label = "Green LED";
        };
    };

    soc {
        uart1: serial@40013800 {
            compatible = "st,stm32-uart";
            reg = <0x40013800 0x400>;
            interrupts = <37 0>;
            current-speed = <115200>;
            status = "okay";
        };

        i2c1: i2c@40005400 {
            compatible = "st,stm32-i2c-v1";
            reg = <0x40005400 0x400>;
            status = "okay";

            bme280: bme280@76 {
                compatible = "bosch,bme280";
                reg = <0x76>;
            };
        };
    };
};
```

### 4.1. Node identifier

Mọi macro devicetree đều cần biết ta đang nói về node nào. Nhưng một node trong file `.dts` không có địa chỉ hay tên biến nào để C tham chiếu trực tiếp. Vì thế Zephyr sinh ra khái niệm *node identifier*.

Node identifier là **token mà C dùng để trỏ tới một node devicetree**. Ta cần nắm ba điều về nó:

- Nó không phải con trỏ, không phải biến, không phải chuỗi. Ta không thể `printf` nó, không thể gán nó vào biến, không thể so sánh hai cái với nhau.
- Nó chỉ tồn tại lúc compile. Preprocessor dùng nó để tra bảng macro rồi thay bằng giá trị thật; sau khi compile xong nó biến mất hoàn toàn.
- Nó luôn đi kèm một macro khác thì mới ra giá trị. Bản thân node identifier chưa là gì cả, nó chỉ là toạ độ để các macro như `DT_PROP`, `DT_REG_ADDR`... biết phải lấy dữ liệu ở node như thế nào.

Hình dung đơn giản: cả cây devicetree sau khi build biến thành một cuốn từ điển khổng lồ gồm các macro `#define`. Node identifier chính là từ khoá để tra cuốn từ điển đó còn các macro như `DT_PROP` là động tác lật tới trang của từ khoá này rồi đọc dòng property tương ứng.

Vì một node có thể được định danh bằng nhiều cách như label, alias, path, chosen đã học ở phần trước, Zephyr cho ta bốn macro để lấy node identifier tương ứng với từng cách. Cả bốn có thể trỏ về cùng một node, chỉ khác nhau ở cách gọi tên:

| Macro | Lấy node theo | Khi nào dùng |
|---|---|---|
| `DT_NODELABEL(lbl)` | label `lbl:` trong DTS | Phổ biến nhất, label ngắn và ổn định |
| `DT_ALIAS(a)` | mục trong `/aliases` | Code chạy trên nhiều board |
| `DT_PATH(...)` | đường dẫn từ root | Khi node không có label |
| `DT_CHOSEN(c)` | mục trong `/chosen` | Lựa chọn cấp hệ thống (console, sram...) |

Áp vào devicetree mẫu ở trên, các dòng sau đều hợp lệ:

```c
#include <zephyr/devicetree.h>

/* uart1 có label 'uart1:', lại được /chosen trỏ tới làm console
   -> hai dòng này trỏ về CÙNG một node */
#define UART  DT_NODELABEL(uart1)
#define CONS  DT_CHOSEN(zephyr_console)

/* led_0 có label 'green_led:', đồng thời có alias 'led0'
   -> hai dòng này cũng trỏ về CÙNG một node */
#define LED_A DT_NODELABEL(green_led)
#define LED_B DT_ALIAS(led0)

/* bme280 nằm sâu trong cây, lấy theo đường dẫn từ root
   các thành phần path viết cách nhau bằng dấu phẩy */
#define SENS  DT_PATH(soc, i2c_40005400, bme280_76)
```

:::tip Quy tắc đổi tên cần biết
Khi viết tên node hay path vào macro, các ký tự `,`, `-`, `@`, `.` đều bị đổi thành `_` và chữ hoa thành chữ thường. Vì thế `serial@40013800` thành `serial_40013800`, `zephyr,console` thành `zephyr_console`, `i2c@40005400` thành `i2c_40005400`. Quên quy tắc này là nguyên nhân số một của lỗi "macro undefined".
:::

Sang các mục sau, ta luôn truyền node identifier (ví dụ `UART`, `LED_A`) làm tham số đầu tiên cho những macro đọc dữ liệu.

### 4.2. Kiểm tra node tồn tại và trạng thái

Nên kiểm tra trước khi dùng và kiểm tra tại thời điểm biên dịch để lỗi lộ ra ngay lúc build thay vì lúc chạy:

```c
/* node có tồn tại trong cây không? */
DT_NODE_EXISTS(DT_NODELABEL(uart1))          /* 1 */
DT_NODE_EXISTS(DT_NODELABEL(uart9))          /* 0 - không có node này */

/* node có tồn tại VÀ status = "okay" không? */
DT_NODE_HAS_STATUS(DT_NODELABEL(uart1), okay)      /* 1 */
DT_NODE_HAS_STATUS(DT_NODELABEL(uart1), disabled)  /* 0 */
```

Mẫu chặn lỗi kinh điển, đặt ở đầu file:

```c
#define UART DT_NODELABEL(uart1)

#if !DT_NODE_HAS_STATUS(UART, okay)
#error "uart1 chua duoc bat - kiem tra status trong devicetree"
#endif
```

`BUILD_ASSERT` cũng làm được điều tương tự và cho thông báo đẹp hơn:

```c
BUILD_ASSERT(DT_NODE_HAS_STATUS(UART, okay), "uart1 phai okay");
```

### 4.3. Đọc property

API dùng để đọc property là macro `DT_PROP(node, property)`. Ví dụ, để đọc giá trị property `current_speed` ở ví dụ trên :

```c
int speed = DT_PROP(DT_NODELABEL(uart1), current_speed);    /* 115200 */
```

Với property kiểu chuỗi:

```c
const char *lbl = DT_PROP(DT_NODELABEL(green_led), label);   /* "Green LED" */
```

:::warning Chú ý
Tên property trong C/C++ cần chuyển đổi các ký tự đặc biết thành dấu `_` và được chuyển thành chữ thường
:::

Các API thường dùng khi không chắc property có tồn tại:

```c
/* property có được khai báo trong node không? */
DT_NODE_HAS_PROP(UART, current_speed)        /* 1 */
DT_NODE_HAS_PROP(UART, parity)               /* 0 - node không có property này */

/* đọc thuộc tính, nếu không có thì lấy giá trị mặc định do ta chỉ định */
int par = DT_PROP_OR(UART, parity, 0);       /* 0 vì node không có 'parity' */
```

Phân biệt `DT_PROP_OR` với `default:` trong binding: `default:` nằm ở phía binding áp dụng cho mọi node, còn `DT_PROP_OR` là giá trị riêng áp dụng cho đoạn code này.

### 4.4. Đọc property `reg`

Property `reg` xuất hiện ở hầu hết node phần cứng nên có macro riêng, tiện hơn `DT_PROP(node, reg)`:

```c
uintptr_t base = DT_REG_ADDR(UART);          /* 0x40013800 */
size_t    size = DT_REG_SIZE(UART);          /* 0x400      */

/* với node trên bus I2C, reg là địa chỉ slave */
uint8_t addr = DT_REG_ADDR(DT_NODELABEL(bme280));   /* 0x76 */
```

Khi node có nhiều vùng `reg`, dùng bản `_BY_IDX` như sau:

```c
for (size_t i = 0; i < DT_NUM_REGS(node_id); i++) {
    size_t addr = DT_REG_ADDR_BY_IDX(node_id, i);
}
```

### 4.5. Đọc interrupt

```c
int irq  = DT_IRQN(UART);                    /* 37 - số IRQ */
int prio = DT_IRQ(UART, priority);           /* 0  - độ ưu tiên */
```

Bộ macro `IRQ_CONNECT` khi cần đăng ký ISR thường kết hợp với các macro này:

```c
IRQ_CONNECT(DT_IRQN(UART), DT_IRQ(UART, priority),uart_isr, DEVICE_DT_GET(UART), 0);
```

### 4.6. Property kiểu mảng và danh sách

Với property nhiều phần tử thì có bộ macro đếm độ dài và truy cập theo chỉ số. Giả sử một node có `offsets = <0x100 0x200 0x300>;`:

```c
int n  = DT_PROP_LEN(NODE, offsets);              /* 3 */
int v0 = DT_PROP_BY_IDX(NODE, offsets, 0);        /* 0x100 */
int v2 = DT_PROP_BY_IDX(NODE, offsets, 2);        /* 0x300 */

/* duyệt toàn bộ phần tử mảng lúc biên dịch */
#define PRINT_ONE(idx, node) printk("%d\n", DT_PROP_BY_IDX(node, offsets, idx));
DT_FOREACH_PROP_ELEM(NODE, offsets, PRINT_ONE)
```

### 4.7. Quan hệ giữa các node

```c
/* node cha */
DT_PARENT(DT_NODELABEL(bme280))       /* node i2c1 */

/* node bus mà thiết bị nối vào (khác parent khi có nhiều tầng) */
DT_BUS(DT_NODELABEL(bme280))          /* node i2c1 */

/* duyệt mọi node con đang okay của một node */
#define HANDLE_CHILD(child) /* ... làm gì đó với child ... */
DT_FOREACH_CHILD_STATUS_OKAY(DT_NODELABEL(i2c1), HANDLE_CHILD)
```

### 4.8. Đọc phandle và specifier

Đây là phần dễ nhầm nhất. Với `gpios = <&gpioa 5 GPIO_ACTIVE_HIGH>;`, ba cách đọc:

```c
#define LED DT_NODELABEL(green_led)

/* lấy node controller được trỏ tới */
DT_GPIO_CTLR(LED, gpios)              /* node gpioa */

/* lấy từng ô specifier - tên ô do gpio-cells trong binding quy định */
DT_GPIO_PIN(LED, gpios)              /* 5 */
DT_GPIO_FLAGS(LED, gpios)           /* GPIO_ACTIVE_HIGH */

/* dạng tổng quát cho phandle-array bất kỳ */
DT_PHA(LED, gpios, pin)             /* 5 - "pin" là tên ô trong binding */
DT_PHA_BY_IDX(LED, gpios, 0, flags)
```

Nhớ lại mục 3.7: `pin` và `flags` là tên các ô khai báo trong `gpio-cells` của binding node `gpioa`. Chính nhờ binding mà `DT_GPIO_PIN` biết ô thứ nhất tên là `pin`.

### 4.9. Tổng hợp macro hay dùng

| Macro | Trả về |
|---|---|
| `DT_NODELABEL(lbl)` / `DT_ALIAS(a)` / `DT_PATH(...)` / `DT_CHOSEN(c)` | node identifier |
| `DT_NODE_EXISTS(n)` | node có trong cây không |
| `DT_NODE_HAS_STATUS(n, okay)` | node tồn tại và đang bật |
| `DT_PROP(n, p)` | giá trị property |
| `DT_PROP_OR(n, p, d)` | property hoặc giá trị dự phòng |
| `DT_NODE_HAS_PROP(n, p)` | property có được khai báo không |
| `DT_REG_ADDR(n)` / `DT_REG_SIZE(n)` | địa chỉ / kích thước từ `reg` |
| `DT_IRQN(n)` / `DT_IRQ(n, priority)` | số IRQ / độ ưu tiên |
| `DT_PROP_LEN(n, p)` / `DT_PROP_BY_IDX(n, p, i)` | độ dài / phần tử mảng |
| `DT_PARENT(n)` / `DT_BUS(n)` | node cha / node bus |
| `DT_GPIO_PIN(n, p)` / `DT_GPIO_FLAGS(n, p)` | ô của phandle GPIO |

## 5. Lấy con trỏ struct device từ node devicetree

Ở mục trước, các macro `DT_PROP`, `DT_REG_ADDR`... cho ta dữ liệu mô tả phần cứng: địa chỉ ở đâu, baud rate bao nhiêu, dùng chân GPIO nào. Nhưng dữ liệu không tự làm gì cả. Muốn thực sự gửi một byte qua UART hay bật một chân GPIO, ta cần gọi hàm của driver và mọi hàm driver trong Zephyr đều cần nhận tham số đầu tiên là một `const struct device *`.

`struct device` là đối tượng runtime đại diện cho một thiết bị đã được driver khởi tạo. Khác hẳn node identifier:

| | Node identifier | `struct device` |
|---|---|---|
| Bản chất | Token lúc biên dịch | Đối tượng thật trong bộ nhớ (flash/RAM) |
| Tồn tại lúc | Chỉ khi build | Cả lúc chạy |
| Chứa gì | Không chứa gì, chỉ là toạ độ | Con trỏ tới hàm API của driver, vùng config, vùng data |
| Dùng để | Làm tham số cho macro `DT_*` | Làm tham số cho hàm driver `uart_*`, `gpio_*`... |

Nói cách khác: node identifier là địa chỉ nhà trên bản đồ, còn `struct device` là ngôi nhà có người ở, có điện nước. Cầu nối giữa hai thế giới này là macro `DEVICE_DT_GET`:

```c
static const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart1));
```

Macro này nhận node identifier và trả về con trỏ tới `struct device` tương ứng. Đây vẫn là hằng số biên dịch: nó không tìm kiếm thiết bị lúc chạy mà preprocessor thay thẳng bằng địa chỉ của struct do driver tạo ra. Vì vậy gọi nó không tốn chi phí runtime và có thể gán cho biến `static const`.

Điều kiện để `DEVICE_DT_GET` hoạt động thì cần phải thoả mãn hai điều kiện sau:

1. Node trong devicetree có `status = "okay"`.
2. Driver tương ứng được bật trong Kconfig và có gọi macro định nghĩa device (Xem bài **Viết driver**).

`DEVICE_DT_GET` chỉ trả về con trỏ, nó không đảm bảo thiết bị đã khởi tạo thành công hay chưa, ví dụ init đọc thanh ghi lỗi. Vì vậy trước khi dùng phải kiểm tra `device_is_ready` lúc runtime:

```c
static const struct device *uart = DEVICE_DT_GET(DT_NODELABEL(uart1));

int main(void)
{
    if (!device_is_ready(uart)) {
        printk("UART chua san sang\n");
        return -ENODEV;
    }

    /* giờ mới an toàn gọi API của driver */
    uart_poll_out(uart, 'A');
    return 0;
}
```


Vì `DEVICE_DT_GET` nhận node identifier, ta có thể ghép nó với bất kỳ cách lấy node nào ở mục 4.1:

```c
/* thiết bị được /chosen trỏ tới */
const struct device *console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

/* controller của bus mà cảm biến nối vào */
const struct device *i2c = DEVICE_DT_GET(DT_BUS(DT_NODELABEL(bme280)));
```

Trường hợp `DT_BUS` rất hay dùng khi viết code cho thiết bị trên bus: từ node cảm biến, ta lấy được ngay con trỏ device của bus I2C để gọi `i2c_write`, `i2c_read`.

Nếu không chắc node có tồn tại, dùng bản có hậu tố để tránh lỗi biên dịch:

```c
/* trả về con trỏ nếu node okay, ngược lại trả NULL */
const struct device *dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(my_sensor));

if (dev == NULL || !device_is_ready(dev)) {
    return -ENODEV;
}
```

`DEVICE_DT_GET_ANY(compat)` lấy instance đầu tiên đang okay của một `compatible`, tiện khi chỉ có đúng một thiết bị loại đó:

```c
const struct device *rtc = DEVICE_DT_GET_ANY(maxim_ds3231);
```

Các lỗi thường gặp:

**`undefined reference to __device_dts_ord_XX`** (lỗi lúc *link*). Đây là dấu hiệu kinh điển: node đang `okay` nên `DEVICE_DT_GET` yêu cầu một `struct device`, nhưng driver chưa được bật nên cấu trúc đó không tồn tại. Cách xử lý:

1. Kiểm tra đã bật `CONFIG_<driver>=y` trong `prj.conf` chưa (ví dụ `CONFIG_I2C=y`, `CONFIG_SENSOR=y`).
2. Kiểm tra node có `status = "okay"` chưa.
3. Kiểm tra `compatible` có khớp binding không (sai `compatible` thì Kconfig `DT_HAS_..._ENABLED` không bật, driver không được biên dịch).

**`device_is_ready` trả về false lúc chạy.** Node và driver đều đúng, nhưng hàm init của driver thất bại, thường do phần cứng không phản hồi, sai địa chỉ I2C hoặc thiết bị phụ thuộc (regulator, clock) chưa lên. Bật log của driver để xem nguyên nhân cụ thể.

## 6. Devicetree spec

### 6.1. Vấn đề của cách làm thủ công

Xét lại chân LED trong devicetree mẫu:

```dts
green_led: led_0 {
    gpios = <&gpioa 5 GPIO_ACTIVE_HIGH>;
};
```

Để bật LED này bằng API GPIO cấp thấp, ta cần các thông tin rời rạc và phải tự lấy từng cái:

```c
/* 1. con trỏ device của controller gpioa */
const struct device *port = DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(green_led), gpios));
/* 2. số pin  */
gpio_pin_t pin = DT_GPIO_PIN(DT_NODELABEL(green_led), gpios);
/* 3. flags   */
gpio_flags_t flags = DT_GPIO_FLAGS(DT_NODELABEL(green_led), gpios);

gpio_pin_configure(port, pin, GPIO_OUTPUT_ACTIVE | flags);
gpio_pin_set(port, pin, 1);
```

Cách này dài dòng và dễ sai: rất dễ lấy `pin` của node này nhưng lỡ truyền `port` của node khác hoặc quên `| flags`. Compiler không bắt được những lỗi đó vì cả ba đều chỉ là số/con trỏ hợp lệ.

### 6.2. Giải pháp

Zephyr gói cả ba vấn đề trên vào một struct duy nhất gọi là devicetree spec. Với GPIO là `struct gpio_dt_spec`:

```c
struct gpio_dt_spec {
    const struct device *port;   /* controller */
    gpio_pin_t           pin;    /* số pin      */
    gpio_dt_flags_t      dt_flags;
};
```

Ta điền toàn bộ struct này chỉ bằng một macro `GPIO_DT_SPEC_GET` và mọi hàm có hậu tố `_dt` nhận thẳng con trỏ tới struct đó, không phải bóc tách từng trường:

```c
#include <zephyr/drivers/gpio.h>

#define LED0 DT_ALIAS(led0)
#define SW0  DT_ALIAS(sw0)

/* một macro điền cả port + pin + flags */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0, gpios);
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(SW0, gpios);

int main(void)
{
    /* kiểm tra controller sẵn sàng, thay cho device_is_ready(port) */
    if (!gpio_is_ready_dt(&led)) {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);   /* flags trong spec tự được áp */
    gpio_pin_configure_dt(&btn, GPIO_INPUT);

    while (1) {
        gpio_pin_toggle_dt(&led);
        k_msleep(500);
    }
}
```

So sánh hai cách viết cùng một việc:

| Thủ công | Dùng spec |
|---|---|
| `gpio_pin_configure(port, pin, GPIO_OUTPUT_ACTIVE \| flags)` | `gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE)` |
| `gpio_pin_toggle(port, pin)` | `gpio_pin_toggle_dt(&led)` |
| `device_is_ready(port)` | `gpio_is_ready_dt(&led)` |

Vì `&led` mang theo cả `port` lẫn `pin`, ta không thể truyền nhầm pin của controller này sang controller khác. Đây là lý do tài liệu Zephyr khuyến nghị dùng `_dt_spec` cho mọi code ứng dụng.

### 6.3. Các họ spec khác

Mỗi subsystem có bộ ba tương tự: một struct, một macro `_GET` và một loạt hàm hậu tố `_dt`.

| Subsystem | Struct | Macro lấy spec |
|---|---|---|
| GPIO | `struct gpio_dt_spec` | `GPIO_DT_SPEC_GET(node, gpios)` |
| PWM | `struct pwm_dt_spec` | `PWM_DT_SPEC_GET(node)` |
| ADC | `struct adc_dt_spec` | `ADC_DT_SPEC_GET(node)` |
| SPI | `struct spi_dt_spec` | `SPI_DT_SPEC_GET(node, cfg, delay)` |
| I2C | `struct i2c_dt_spec` | `I2C_DT_SPEC_GET(node)` |

```c
struct pwm_dt_spec  pwm = PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0));
struct adc_dt_spec  adc = ADC_DT_SPEC_GET(DT_PATH(zephyr_user));
struct spi_dt_spec  spi = SPI_DT_SPEC_GET(DT_NODELABEL(mydev), SPI_WORD_SET(8), 0);
struct i2c_dt_spec  i2c = I2C_DT_SPEC_GET(DT_NODELABEL(bme280));
```

Với I2C chẳng hạn, `i2c_dt_spec` gói sẵn con trỏ device của bus và địa chỉ slave (`0x76`), nên đọc thanh ghi cảm biến gọn lại còn:

```c
uint8_t reg = 0xD0, val;
i2c_write_read_dt(&i2c, &reg, 1, &val, 1);   /* không phải truyền địa chỉ 0x76 riêng */
```

### 6.4. Node `zephyr,user`

Đôi khi ứng dụng cần trỏ tới một chân GPIO hay một kênh ADC nhưng không thuộc thiết bị chuẩn nào đã có sẵn binding. Thay vì viết binding riêng, Zephyr cho sẵn node `zephyr,user` như một chỗ chứa đồ hợp lệ:

```dts
/ {
    zephyr,user {
        io-channels = <&adc 0>;
        relay-gpios = <&gpiob 3 GPIO_ACTIVE_LOW>;
    };
};
```

Rồi lấy spec từ chính node đó:

```c
#define APP DT_PATH(zephyr_user)

static const struct gpio_dt_spec relay = GPIO_DT_SPEC_GET(APP, relay_gpios);
static const struct adc_dt_spec   ch0   = ADC_DT_SPEC_GET(APP);
```

Đây là cách nhanh gọn nhất để nối dây một vài chân phần cứng cho ứng dụng nhỏ mà không phải đụng tới binding.

## 7. Luồng build

Các mục trên đã mô tả từng mảnh ghép riêng lẻ. Ở mục này, ta sẽ ghép tất cả lại thành một chuỗi liên tục thông qua việc theo dõi một node BME280 duy nhất và ở mỗi bước sẽ chỉ rõ file nào được sinh ra, nội dung thực tế là gì.

Ví dụ về node mà ta sẽ bám theo trong suốt quá trình:

```dts
&i2c1 {
    status = "okay";
    bme280: bme280@76 {
        compatible = "bosch,bme280";
        reg = <0x76>;
    };
};
```

### 7.1. CMake gom mọi nguồn DTS

Đầu tiên CMake xác định danh sách các file DTS theo thứ tự ưu tiên rồi ghép lại:

```
1. boards/arm/my_board/my_board.dts         <- board DTS
    └── #include <myvendor/mychip100.dtsi>  <- SoC DTS
        └── #include <arm/armv7-m.dtsi>     <- Arch DTS
2. + app.overlay                            <- Node bme280 của ta nằm đây
3. + boards/my_board.overlay                <- overlay riêng board
```

Toàn bộ được đưa qua C preprocessor (chứ chưa phải qua C compiler) để xử lý `#include` và thay các hằng số như `GPIO_ACTIVE_HIGH`, `I2C_BITRATE_FAST`. Kết quả là một file phẳng, đã ghép hết các file trên:

```
build/zephyr/zephyr.dts.pre     (cây devicetree hợp nhất, dạng text)
```

Đây là lúc overlay chồng lên node gốc: `status` chuyển thành `okay`, node `bme280@76` được thêm vào dưới `i2c1`.

### 7.2. Tìm binding YAML theo compatible

Script `gen_defines.py` đọc `zephyr.dts.pre` cùng với toàn bộ file YAML trong các thư mục `dts/bindings/`. Với mỗi node, nó:

1. Lấy `compatible` + bus của node cha -> tra ra file binding. Ở đây: `bosch,bme280` trên bus `i2c` -> `bosch,bme280-i2c.yaml`.
2. Validate node theo binding
3. Gán cho node một số thứ tự phụ thuộc gọi là *dependency ordinal* - một số nguyên duy nhất, ví dụ node bme280 nhận ordinal `27`.

Kết quả được dựng thành cây EDT (Enhanced DeviceTree) lưu ở `build/zephyr/edt.pickle`.

### 7.3. Sinh macro cho node

Từ EDT, script `gen_defines.py` generate một file header:

```
build/zephyr/include/generated/zephyr/devicetree_generated.h
```

Trong đó, với node bme280 thì sẽ có các macro như sau (rút gọn):

```c
/* định danh node theo path */
#define DT_N_S_soc_S_i2c_40005400_S_bme280_76_P_reg        0x76
#define DT_N_S_soc_S_i2c_40005400_S_bme280_76_P_status     "okay"

#define DT_N_NODELABEL_bme280   DT_N_S_soc_S_i2c_40005400_S_bme280_76

/* đăng ký node vào danh sách instance của compatible bosch,bme280 */
#define DT_N_INST_0_bosch_bme280            DT_N_S_soc_S_i2c_40005400_S_bme280_76
#define DT_N_INST_bosch_bme280_NUM_OKAY     1
#define DT_COMPAT_HAS_OKAY_bosch_bme280     1
#define DT_FOREACH_OKAY_INST_bosch_bme280(fn)   fn(0)
```

Mọi macro `DT_*` chỉ là nối chuỗi rồi tra bảng này. Ví dụ `DT_PROP(DT_NODELABEL(bme280), reg)`:

```
DT_NODELABEL(bme280)  ->  DT_N_NODELABEL_bme280
                      ->  DT_N_S_soc_S_i2c_40005400_S_bme280_76
DT_PROP nối "_P_reg"  ->  DT_N_S_soc_S_i2c_40005400_S_bme280_76_P_reg
                      ->  0x76
```

Đây là lý do quy tắc đổi `-` thành `_` là bắt buộc: gõ sai một ký tự là ra một tên macro không tồn tại.

### 7.4. Sinh symbol Kconfig từ devicetree

Song song, build system sinh một file Kconfig từ chính devicetree:

```
build/zephyr/Kconfig/Kconfig.dts
```

Với mỗi `compatible` có ít nhất một node `okay`, nó tạo symbol `DT_HAS_<COMPAT>_ENABLED=y`:

```conf
config DT_HAS_BOSCH_BME280_ENABLED
    def_bool y
```

### 7.5. Kconfig tự bật driver

File `Kconfig` của driver BME280 khai báo phụ thuộc vào symbol này:

```conf
config BME280
    bool "BME280 sensor"
    default y
    depends on DT_HAS_BOSCH_BME280_ENABLED
```

Và CMake:

```cmake
zephyr_library_sources_ifdef(CONFIG_BME280 bme280.c)
```

Ta có chuỗi nhân quả:

```
DTS có node okay
   -> DT_HAS_BOSCH_BME280_ENABLED = y
      -> CONFIG_BME280 = y
         -> bme280.c được đưa vào build
```

Ngược lại, nếu node BME `status = "disabled"` thì file `.c` không được compile chút nào.

### 7.6. Cách tự kiểm chứng từng bước

| Muốn xem | Mở file |
|---|---|
| Cây sau khi ghép overlay | `build/zephyr/zephyr.dts` |
| Node có match binding không, macro property nào | `build/zephyr/include/generated/zephyr/devicetree_generated.h` |
| Symbol `DT_HAS_*` sinh ra | `build/zephyr/Kconfig/Kconfig.dts` |
| Driver có được bật không | `build/zephyr/.config` (tìm `CONFIG_BME280`) |
| Ordinal của một node | tìm `_ORD` trong `devicetree_generated.h` |

## 8. Dependency ordinal

Sau khi ghép xong toàn bộ DTS và overlay, script `gen_defines.py` dựng một **đồ thị phụ thuộc** giữa các node. Node A được gọi là phụ thuộc vào node B nếu:
- B là cha của A
- B là bus mà A kết nối
- A trỏ tới B qua một phandle bất kỳ (`gpios`, `clocks`, `pinctrl-0`...).

Đồ thị này được sắp xếp topo rồi mỗi node nhận một số nguyên duy nhất theo thứ tự đó, gọi là *dependency ordinal*. Node gốc `/` nhận số 0.

Tính chất quan trọng nhất: **node phụ thuộc luôn có ordinal nhỏ hơn node phụ thuộc vào nó**. Với ví dụ BME280:

```
/                 -> ord 0
  soc             -> ord 3
    i2c1          -> ord 12      <- bus
      bme280@76   -> ord 27      <- nằm trên i2c1 nên ordinal lớn hơn
```

Trong `devicetree_generated.h`, mỗi node có ba macro liên quan:

```c
#define DT_N_S_soc_S_i2c_40005400_S_bme280_76_ORD             47
#define DT_N_S_soc_S_i2c_40005400_S_bme280_76_REQUIRES_ORDS   3, 20
#define DT_N_S_soc_S_i2c_40005400_S_bme280_76_SUPPORTS_ORDS   /* rỗng */
```

Đọc chúng từ code bằng:

```c
DT_DEP_ORD(DT_NODELABEL(bme280))             /* 47 */
DT_REQUIRES_DEP_ORDS(DT_NODELABEL(bme280))   /* 3, 20  — những node nó cần */
DT_SUPPORTS_DEP_ORDS(DT_NODELABEL(bme280))   /* những node cần tới nó */
```

Nhưng ta hiếm khi gọi trực tiếp. Chúng chủ yếu phục vụ ba việc bên trong Zephyr:

**Đặt tên cho device symbol.** Macro `DEVICE_DT_GET` ở mục 5 thực chất chỉ là lấy địa chỉ của biến có tên ghép từ ordinal:

```c
DEVICE_DT_GET(DT_NODELABEL(bme280))   ->   &__device_dts_ord_27
```

Đây chính là lý do lỗi link ở cuối mục 5 luôn hiện ra dưới dạng một con số vô nghĩa `undefined reference to __device_dts_ord_27`. Muốn biết `27` là node nào thì tìm chuỗi `_ORD 27` trong `devicetree_generated.h`, tên macro đứng trước nó là path của node.

**Thứ tự khởi tạo.** Hai device cùng init level và cùng priority thì xếp theo ordinal. Vì ordinal đến từ sắp xếp topo nên device luôn được init sau những thứ nó phụ thuộc, kể cả khi lập trình viên quên chỉnh priority:

```
i2c1   (ord 12) init trước
bme280 (ord 27) init sau
```

Cơ chế này chỉ cứu được trường hợp cùng level và priority. Nếu đặt cảm biến ở `POST_KERNEL` priority 40 còn driver I2C ở priority 50 thì cảm biến vẫn init trước và `i2c_is_ready_dt()` sẽ trả về false.

**Mô tả quan hệ phụ thuộc lúc runtime.** Khi bật `CONFIG_DEVICE_DEPS`, danh sách `REQUIRES_ORDS` được nhúng thẳng vào `struct device` để subsystem power management biết thứ tự bật/tắt thiết bị.

:::warning Ordinal không phải là hằng số cố định
Thêm một node vào bất kỳ đâu trong cây cũng có thể đánh số lại toàn bộ, kể cả việc một node `/omit-if-no-ref/` được tham chiếu hay không cũng làm ordinal xê dịch. Ordinal khác nhau giữa các board, giữa các cấu hình build và giữa các phiên bản Zephyr nên đừng bao giờ hard code nó vào code hay script.
:::

## 9. Debug

Khi có gì đó không hoạt động, đi theo thứ tự sau:

**1. Xem cây sau khi ghép.** Mở `build/zephyr/zephyr.dts`, tìm node của mình, kiểm tra `status` và các property có đúng như mong đợi không. Overlay có thể đã không được nạp.

**2. Kiểm tra overlay có được dùng không.** Đầu output của `west build` in ra danh sách file devicetree đã dùng. Nếu không thấy tên overlay của mình thì tên file hoặc vị trí đặt sai.

**3. Xem macro đã sinh.** Tìm trong `build/zephyr/include/generated/zephyr/devicetree_generated.h`. Nếu không có macro nào cho node, gần như chắc chắn là thiếu binding hoặc `compatible` viết sai chính tả.

**4. Lỗi link `undefined reference to __device_dts_ord_NN`.** Node okay nhưng driver chưa được bật trong Kconfig. Kiểm tra lại `build/zephyr/.config`.

**5. Build sạch khi đổi devicetree.** Devicetree không phải lúc nào cũng được theo dõi dependency đầy đủ:

```bash
west build -b <board> -p always .
```

Ngoài ra có công cụ tra cứu trực quan:

```bash
west build -t hardware-map        # liệt kê thiết bị
west build -t initlevels          # thứ tự khởi tạo device
```
