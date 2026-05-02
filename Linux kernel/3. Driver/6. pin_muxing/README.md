
# Pin Muxing trong Linux Embedded

## 1. Tổng quan về Pin Muxing

### 1.1. Vấn đề

Trên các vi xử lý SoC hiện đại như TI AM335x, NXP i.MX6, ST STM32MP1,...số lượng ngoại vi (UART, SPI, I2C, PWM, CAN, Ethernet,...) luôn nhiều hơn số lượng chân vật lý của chip. Việc đưa tất cả tín hiệu ra ngoài cùng lúc là không khả thi về mặt vật lý.

**Giải pháp: Pin Multiplexing**

Mỗi chân vật lý có thể được kết nối tới nhiều khối ngoại vi khác nhau thông qua một bộ chuyển mạch (multiplexer). Tại một thời điểm, chỉ một chức năng được kích hoạt trên mỗi chân.

### 1.2. Cơ chế hoạt động

Mỗi chân pin trên SoC được kết nối với một **pad cell** (còn gọi là I/O cell). Pad cell này chứa:
- **Multiplexer (MUX)**: chọn 1 trong nhiều tín hiệu nội bộ để đưa ra/nhận vào chân vật lý.
- **Pull-up/Pull-down resistor**: điện trở kéo lên/kéo xuống nội bộ.
- **Input buffer (Receiver)**: bộ đệm đầu vào, có thể bật/tắt.
- **Slew rate control**: điều khiển tốc độ chuyển mức của tín hiệu đầu ra.
- **Drive strength**: độ mạnh dòng điện đầu ra (trên một số SoC).

**Ví dụ trên AM335x (BeagleBone Black):** Chân vật lý D17 có thể được cấu hình thành:

```
                    ┌─── Mode 0: uart1_rtsn
                    ├─── Mode 1: timer5
Pin D17 ──── MUX ── ├─── Mode 2: dcan0_rx
(P9_19)             ├─── Mode 3: I2C2_SCL
                    ├─── Mode 4: spi1_cs1
                    ├─── Mode 5: pr1_uart0_rts_n
                    ├─── Mode 6: (reserved)
                    └─── Mode 7: gpio0_13
```

$\rightarrow$ Tại một thời điểm, chỉ một mode được chọn. Ví dụ đặt `MUX_MODE = 0` thì chân D17 hoạt động như `uart1_rtsn`, đặt `MUX_MODE = 7` thì chân D17 trở thành `gpio0_13`.

:::warning Pin conflict
Mode 7 luôn là GPIO trên AM335x. Việc chọn mode sai hoặc hai ngoại vi cùng claim một chân sẽ gây xung đột phần cứng.
:::

## 2. Control Module và Pad Control Register

### 2.1. Control Module là gì?

Trên AM335x, Control Module là khối phần cứng trung tâm chịu trách nhiệm quản lý cấu hình toàn bộ hệ thống SoC. Nó nằm trong vùng nhớ L4_WKUP với địa chỉ cơ sở:

```
Base address:  0x44E1_0000
Size:          0x1FFF (8KB)
```

Control Module không chỉ quản lý pin muxing mà còn bao gồm nhiều nhóm thanh ghi chức năng khác nhau. Theo TRM (Chapter 9), Control Module được chia thành 7 nhóm chức năng chính:

| Nhóm | Tên | Chức năng |
|------|-----|-----------|
| 1 | System Identity | Thông tin boot config, SYSBOOT, revision SoC |
| 2 | Power & Clock | Quản lý nguồn, clock gating cho Control Module |
| 3 | Device ID & Bus | Nhận dạng silicon, cấu hình bus nội bộ (L3/L4) |
| 4 | Peripheral Control | Cấu hình riêng cho USB PHY, Ethernet MAC, JTAG,… |
| 5 | **Pad Control** | **Chọn chức năng (mux mode), pull-up/down, input/output cho từng pin** |
| 6 | DDR PHY | Cấu hình impedance, slew rate, timing cho bộ nhớ DDR |
| 7 | EDMA & Misc | Crossbar sự kiện EDMA, và các thanh ghi cấu hình khác |

:::warning Write protection
Pad Control Registers được bảo vệ bởi phần cứng — chỉ CPU ở chế độ supervisor mới có quyền ghi.Trong Linux, việc cấu hình pin mux thường được thực hiện bởi kernel thông qua device tree lúc boot.
:::

Trong tài liệu này, chúng ta tập trung vào **nhóm 5 — Pad Control**, vì đây là nhóm thanh ghi quyết định trực tiếp việc pin muxing.

### 2.2. Cấu trúc Pad Control Register (AM335x)

Mỗi chân pin có một thanh ghi cấu hình riêng gọi là `conf_<module>_<pin>`, nằm trong dải offset `0x800 – 0xA34` của Control Module. Thanh ghi này có độ rộng 32 bit, nhưng chỉ 7 bit thấp được sử dụng:

| Bit     | Tên          | Mô tả chi tiết                                                                                        |
|---------|--------------|-------------------------------------------------------------------------------------------------------|
| Bit 0–2 | `MUX_MODE`   | Chọn chức năng cho pin (Mode 0–7). Mode 0 = chức năng mặc định, Mode 7 = GPIO.                        |
| Bit 3   | `PULLUDEN`   | Pull-Up/Down Enable. 0 = bật internal pullup, 1 = tắt (floating).                                     |
| Bit 4   | `PULLTYPESEL`| Pull Type Select. 0 = pull-down, 1 = pull-up. Chỉ có tác dụng khi PULLUDEN = 0.                       |
| Bit 5   | `RXACTIVE`   | Receiver Enable. 0 = chỉ output, 1 = cho phép input. Bắt buộc bật cho tín hiệu đầu vào (RX, SDA,...). |
| Bit 6   | `SLEWCTRL`   | Slew Rate Control. 0 = fast slew, 1 = slow slew. Slow slew giảm nhiễu EMI nhưng hạn chế tốc độ.       |

**Ví dụ giải mã giá trị thanh ghi**

Giả sử đọc được giá trị thanh ghi là `0x31` (nhị phân: `0b00110001`):

```
0x31 = 0b 0 0 1 1 0 001
          | | | | |  |
          | | | | |  +- MUX_MODE    = 001 (Mode 1)
          | | | | +---- PULLUDEN    = 0   (pull enabled)
          | | | +------ PULLTYPESEL = 1   (pull-up)
          | | +-------- RXACTIVE    = 1   (input enabled)
          | +---------- SLEWCTRL    = 0   (fast)
          +------------ Reserved
```

$\rightarrow$ Pin này đang ở Mode 1, bật pull-up, cho phép nhận tín hiệu đầu vào, tốc độ slew nhanh.

### 2.3. Các macro thường dùng trong kernel

Linux kernel định nghĩa các macro trong file `include/dt-bindings/pinctrl/am33xx.h` và `omap.h` để dễ đọc hơn:

| Macro                  | Giá trị | Ý nghĩa                        |
|------------------------|---------|--------------------------------|
| `MUX_MODE0` – `MUX_MODE7` | 0x0 – 0x7 | Chọn chức năng Mode 0–7   |
| `PIN_INPUT`            | 0x20    | Bật RXACTIVE (cho phép input)  |
| `PIN_INPUT_PULLUP`     | 0x30    | Input + Pull-up                |
| `PIN_INPUT_PULLDOWN`   | 0x20    | Input + Pull-down              |
| `PIN_OUTPUT`           | 0x00    | Output, không input            |
| `PIN_OUTPUT_PULLDOWN`  | 0x00    | Output + Pull-down             |
| `PIN_OUTPUT_PULLUP`    | 0x10    | Output + Pull-up               |

### 2.4. Tính toán offset cho Device Tree

Trong device tree của AM335x, offset được sử dụng trong `pinctrl-single,pins` là địa chỉ của thanh ghi trong Control Module **trừ đi 0x800**. Ví dụ:

```
Thanh ghi conf_uart1_txd nằm tại offset: 0x984 (trong Control Module)
-> Offset trong device tree: 0x984 - 0x800 = 0x184
```

Vì vậy khi ta thấy `0x184` trong device tree, nó tương ứng với thanh ghi Control Module tại địa chỉ tuyệt đối `0x44E1_0000 + 0x984 = 0x44E1_0984`.

### 2.5. Nhóm Peripheral Control

> Tại sao cần biết thêm nhóm Peripheral Control?

Khi lập trình pin mux trên Linux, nhóm 5 (Pad Control) là đủ cho hầu hết trường hợp. Các nhóm còn lại đã được kernel driver tự xử lý. Tuy nhiên, có một số ngoại vi mà chỉ cấu hình pad mux thôi là chưa đủ — chúng cần thêm thanh ghi ở nhóm 4 (Peripheral Control) để hoạt động đúng.

**Ví dụ điển hình — Ethernet (CPSW):**

AM335x có module Ethernet CPSW (Common Platform Switch) hỗ trợ 2 port, mỗi port có thể chạy ở chế độ MII, RMII, hoặc RGMII. Để Ethernet hoạt động, ta cần cấu hình hai tầng:

**Tầng 1 — Pad Control (nhóm 5):** Mux các chân vật lý sang đúng mode cho Ethernet. Ví dụ với RMII1:

```dts
cpsw_default: cpsw_default {
    pinctrl-single,pins = <
        AM33XX_IOPAD(0x910, PIN_INPUT_PULLUP | MUX_MODE1)    /* rmii1_refclk */
        AM33XX_IOPAD(0x914, PIN_OUTPUT_PULLDOWN | MUX_MODE1)  /* rmii1_txen */
        AM33XX_IOPAD(0x918, PIN_OUTPUT_PULLDOWN | MUX_MODE1)  /* rmii1_txd1 */
        AM33XX_IOPAD(0x91c, PIN_OUTPUT_PULLDOWN | MUX_MODE1)  /* rmii1_txd0 */
        AM33XX_IOPAD(0x920, PIN_INPUT_PULLUP | MUX_MODE1)     /* rmii1_rxd1 */
        AM33XX_IOPAD(0x924, PIN_INPUT_PULLUP | MUX_MODE1)     /* rmii1_rxd0 */
    >;
};
```

**Tầng 2 — Peripheral Control (nhóm 4):** Cho SoC biết port đang chạy chế độ gì, qua thanh ghi `GMII_SEL`:

```
GMII_SEL (offset 0x650 trong Control Module):
  Bit 1:0 — Chọn chế độ cho port 1: 00 = GMII/MII, 01 = RMII, 10 = RGMII
  Bit 3:2 — Chọn chế độ cho port 2
  Bit 4   — RMII1 clock external (1) hay internal (0)
  Bit 5   — RMII2 clock external (1) hay internal (0)
```

$\rightarrow$ Nếu ta chỉ mux pin (tầng 1) mà không set `GMII_SEL` (tầng 2), SoC không biết Ethernet đang chạy RMII hay MII $\righarow$ Ethernet sẽ không hoạt động dù các chân đã đúng mode.

Lý do cần thanh ghi `GMII_SEL` là vì mỗi chế độ Ethernet có đặc điểm khác nhau về tốc độ, số chân, và cách truyền dữ liệu:

| Chế độ | GMII_SEL | Tốc độ tối đa | Số chân dữ liệu | Số chân tổng | Đặc điểm |
|--------|----------|----------------|-----------------|--------------|----------|
| MII    | `00`     | 100 Mbps       | 4 TX + 4 RX     | ~16 chân     | Chuẩn truyền thống, tốn nhiều chân nhất |
| RMII   | `01`     | 100 Mbps       | 2 TX + 2 RX     | ~9 chân      | Giảm một nửa số chân so với MII, dùng clock 50 MHz |
| RGMII  | `10`     | 1000 Mbps      | 4 TX + 4 RX (DDR) | ~12 chân   | Hỗ trợ Gigabit, dùng DDR để tăng tốc |

$\righarow$ Cùng một bộ chân vật lý, việc SoC xử lý tín hiệu sẽ khác nhau hoàn toàn tùy vào chế độ. `GMII_SEL` chính là thanh ghi cho SoC biết cần xử lý tín hiệu theo cách nào.

Nói cách khác: **Pad Control** quyết định *chân nào nối với ngoại vi nào*, còn **Peripheral Control** quyết định *ngoại vi đó hoạt động ở chế độ nào*.

**Một số thanh ghi Peripheral Control đáng biết khác:**

- `USB_CTRLx`: bật/tắt USB PHY, chọn chế độ cho từng cổng USB.
- `MAC_ID0_LO/HI`: chứa MAC address cho Ethernet.

Trong thực tế, các kernel driver (cpsw, usb,…) sẽ tự ghi các thanh ghi Peripheral Control này. Ta không cần thao tác trực tiếp, nhưng hiểu chúng sẽ giúp debug khi ngoại vi không hoạt động dù pin mux đã đúng.

## 3. Pinctrl Subsystem trong Linux Kernel

### 3.1. Kiến trúc tổng quan

Trong Linux kernel, toàn bộ việc quản lý pin muxing được xử lý bởi pinctrl subsystem. Subsystem này cung cấp một framework thống nhất cho việc:

- Quản lý danh sách tất cả các pin trên SoC (**pin enumeration**).
- Nhóm các pin lại với nhau (**pin grouping**), ví dụ: nhóm 4 pin cho SPI.
- Chọn chức năng cho từng pin hoặc nhóm pin (**pin muxing**).
- Cấu hình điện học: pull-up/down, drive strength, slew rate (**pin configuration**).
- Hỗ trợ chuyển đổi trạng thái pin theo nhu cầu (default, sleep, idle).

### 3.2. Các thành phần chính

#### 3.2.1. Pin Controller Driver

Mỗi SoC có một pin controller driver riêng (ví dụ: `pinctrl-single` cho TI AM335x, `pinctrl-stm32` cho STM32). Driver này đăng ký với pinctrl subsystem và cung cấp các callback để:
- Liệt kê tất cả pin và nhóm pin.
- Thực hiện việc ghi thanh ghi mux.
- Cấu hình điện học cho pin.

#### 3.2.2. Pin Configuration Consumer

Các device driver (UART, SPI, I2C,…) là consumer của pinctrl. Khi một device driver được probe, kernel tự động:
- Tìm cấu hình pin tương ứng trong device tree (qua `pinctrl-0`, `pinctrl-1`,…).
- Áp dụng trạng thái `"init"` (nếu có) trước khi gọi `probe()`.
- Chuyển sang trạng thái `"default"` sau khi `probe()` hoàn thành.

#### 3.2.3. Mối quan hệ với GPIO Subsystem

Pin controller và GPIO controller thường quản lý cùng một tập chân vật lý. Khi một pin được yêu cầu làm GPIO, pinctrl subsystem sẽ được gọi để mux pin đó sang chế độ GPIO (thường là Mode 7 trên AM335x). Cơ chế này là gpio-ranges được định nghĩa trong device tree.

:::warning Strict mode
Nếu một pin đã được claim bởi một peripheral (ví dụ UART), việc yêu cầu pin đó làm GPIO sẽ bị từ chối nếu driver đặt cờ "strict".
:::

## 4. Cấu hình Pin Muxing trong Device Tree

### 4.1. Cấu trúc tổng thể

Việc cấu hình pin muxing trong device tree gồm ba phần chính:

1. **Pin controller node**: định nghĩa khối pin controller của SoC.
2. **Pin configuration group**: định nghĩa cấu hình cụ thể cho từng nhóm pin.
3. **Device node**: liên kết device với cấu hình pin tương ứng.

**Ví dụ đầy đủ:**

```dts
/* (1) Pin controller node */
am33xx_pinmux: pinmux@44e10800 {
    compatible = "ti,am437-padconf", "pinctrl-single";
    reg = <0x44e10800 0x31c>;
    #address-cells = <1>;
    #size-cells = <0>;
    #pinctrl-cells = <2>;

    /* (2) Pin configuration group */
    uart1_pins_default: pinmux_uart1_pins {
        pinctrl-single,pins = <
            AM33XX_IOPAD(0x980, PIN_INPUT_PULLUP | MUX_MODE0)    /* uart1_rxd */
            AM33XX_IOPAD(0x984, PIN_OUTPUT_PULLDOWN | MUX_MODE0)  /* uart1_txd */
        >;
    };

    uart1_pins_sleep: pinmux_uart1_sleep {
        pinctrl-single,pins = <
            AM33XX_IOPAD(0x980, PIN_INPUT_PULLDOWN | MUX_MODE7)
            AM33XX_IOPAD(0x984, PIN_INPUT_PULLDOWN | MUX_MODE7)
        >;
    };
};

/* (3) Device node */
&uart1 {
    pinctrl-names = "default", "sleep";
    pinctrl-0 = <&uart1_pins_default>;
    pinctrl-1 = <&uart1_pins_sleep>;
    status = "okay";
};
```

### 4.2. Thuộc tính `pinctrl-single,pins`

Thuộc tính này chứa danh sách các cặp `<offset value>` cho từng pin trong nhóm:

- **offset**: vị trí thanh ghi Pad Control Register trong Control Module (trừ 0x800 nếu không dùng macro `AM33XX_IOPAD`).
- **value**: giá trị ghi vào thanh ghi, kết hợp các cờ MUX_MODE, PIN_INPUT, PULLUP,…

**Ví dụ 1: Dùng macro (khuyến nghị):**

```dts
pinctrl-single,pins = <
    AM33XX_IOPAD(0x980, PIN_INPUT_PULLUP | MUX_MODE0)   /* uart1_rxd */
    AM33XX_IOPAD(0x984, PIN_OUTPUT_PULLDOWN | MUX_MODE0) /* uart1_txd */
>;
```

Macro `AM33XX_IOPAD(addr, val)` tự động chuyển đổi địa chỉ Control Module sang offset phù hợp (trừ 0x800). Ở đây `0x980 - 0x800 = 0x180`.

**Ví dụ 2: Dùng offset trực tiếp (một số board cũ):**

```dts
pinctrl-single,pins = <
    0x180 (PIN_INPUT_PULLUP | MUX_MODE0)    /* uart1_rxd: 0x980 - 0x800 */
    0x184 (PIN_OUTPUT_PULLDOWN | MUX_MODE0) /* uart1_txd: 0x984 - 0x800 */
>;
```

Hai ví dụ trên cho kết quả giống nhau. Ví dụ 1 dễ đọc hơn vì ta có thể tra trực tiếp offset `0x980` trong TRM mà không cần tính toán.

### 4.3. Thuộc tính `pinctrl-names`

Xác định danh sách các trạng thái pin mà device có thể sử dụng. Các trạng thái chuẩn được kernel hỗ trợ:

| Trạng thái | Mô tả                                                                        |
|------------|------------------------------------------------------------------------------|
| `default`  | Cấu hình khi device hoạt động bình thường. Kernel tự động áp dụng sau `probe()`. |
| `init`     | Cấu hình trước khi `probe()`. Nếu không định nghĩa, kernel dùng `default`.     |
| `sleep`    | Cấu hình khi device vào chế độ suspend. Thường disable pin để tiết kiệm điện.  |
| `idle`     | Cấu hình khi device tạm nghỉ (runtime idle).                                   |

**Ví dụ — UART với 2 trạng thái:**

Khi hệ thống hoạt động bình thường, UART cần các chân TX/RX ở đúng mode. Khi suspend, ta muốn chuyển các chân sang GPIO pull-down để tránh rò điện:

```
/* Trạng thái hoạt động: pin ở Mode 0 (UART) */
uart1_pins_default: pinmux_uart1_default {
    pinctrl-single,pins = <
        AM33XX_IOPAD(0x980, PIN_INPUT_PULLUP | MUX_MODE0)    /* uart1_rxd */
        AM33XX_IOPAD(0x984, PIN_OUTPUT_PULLDOWN | MUX_MODE0)  /* uart1_txd */
    >;
};

/* Trạng thái sleep: pin chuyển sang GPIO input pull-down */
uart1_pins_sleep: pinmux_uart1_sleep {
    pinctrl-single,pins = <
        AM33XX_IOPAD(0x980, PIN_INPUT_PULLDOWN | MUX_MODE7)
        AM33XX_IOPAD(0x984, PIN_INPUT_PULLDOWN | MUX_MODE7)
    >;
};
```

Trong device node, liên kết hai trạng thái này:

```
&uart1 {
    pinctrl-names = "default", "sleep";
    pinctrl-0 = <&uart1_pins_default>;
    pinctrl-1 = <&uart1_pins_sleep>;
    status = "okay";
};
```

$\rightarrow$ Kernel sẽ tự động chuyển pin giữa `default` và `sleep` khi hệ thống suspend/resume mà driver không cần xử lý gì thêm.

### 4.4. Thuộc tính `pinctrl-0`, `pinctrl-1`, ...

`pinctrl`-0, `pinctrl`-1,... là các pinctrl state — mỗi số tương ứng một trạng thái hoạt động khác nhau của device, mỗi state định nghĩa cách các chân pin được cấu hình trong trạng thái đó.

`pinctrl-N` và `pinctrl-names` luôn đi cùng nhau:

```dts
pinctrl-names = "default", "sleep";
pinctrl-0 = <&uart1_pins_default>;  /* state 0 = "default" */
pinctrl-1 = <&uart1_pins_sleep>;    /* state 1 = "sleep" */
```

Mapping rất đơn giản — theo thứ tự index:

```
pinctrl-names = "default",          "sleep"
                    ↕                   ↕ 
                pinctrl-0           pinctrl-1
```

**Ví dụ — Device chỉ cần 1 trạng thái (trường hợp phổ biến nhất):**

```dts
&spi0 {
    pinctrl-names = "default";
    pinctrl-0 = <&spi0_pins>;
    status = "okay";
};
```

**Ví dụ — Kết hợp nhiều nhóm pin trong 1 trạng thái:**

Khi một device cần pin từ nhiều nhóm khác nhau (ví dụ SPI có thêm GPIO cho chip select phụ), ta có thể tham chiếu nhiều phandle:

```dts
&spi0 {
    pinctrl-names = "default";
    pinctrl-0 = <&spi0_pins>, <&spi0_cs1_pin>;
    /*           └─── group 1   └─── group 2    */
    /*           tất cả được apply cùng lúc     */
    status = "okay";
};
```

$\rightarrow$ Khi kernel apply state "default", nó apply cả 2 group cùng lúc — tức là cấu hình tất cả các chân trong cả 2 group.

:::warning Nhiều phandle
Một `pinctrl-N` có thể tham chiếu nhiều phandle nếu cần cấu hình pin từ nhiều nhóm khác nhau. Kernel sẽ áp dụng tất cả các nhóm theo thứ tự.
:::

### 4.5. Vấn đề pin conflict

Một chân vật lý trên SoC có thể được mux thành nhiều function khác nhau, nhưng tại một thời điểm chỉ dùng được một function:

```
PAD_UART1_TX (chân vật lý)
    ├── function 0: UART1_TX    ← chỉ chọn được 1
    ├── function 1: GPIO1_IO16  ←
    └── function 2: PWM1_OUT    ←
```

Vấn đề pin conflict xảy ra khi 2 node cùng claim một chân:

```
/* Node UART */
&uart1 {
    pinctrl-0 = <&pinctrl_uart1>;  /* dùng PAD_UART1_TX */
    status = "okay";
};

/* Node GPIO */
&gpio_leds {
    pinctrl-0 = <&pinctrl_gpio>;   /* cũng dùng PAD_UART1_TX */
    status = "okay";
};
```

Kernel không tự động phát hiện conflict lúc parse DTS. Conflict chỉ xảy ra lúc runtime khi driver probe:

```
uart1 probe trước   → request pin PAD_UART1_TX → thành công
gpio_leds probe sau → request cùng pin → CONFLICT → probe thất bại
```

Ta sẽ thấy lỗi trong dmesg:

```bash
dmesg | grep -i "pin\|conflict"

# Output:
[    2.134] pin 45 (PAD_UART1_TX): device 2020000.serial 
            already used by 209c000.gpio (GPIO)
[    2.135] imx-pinctrl 20e0000.iomuxc: pin PAD_UART1_TX 
            already requested by 209c000.gpio
[    2.136] gpio-leds: Error applying setting, reverse things back
```

Cách xử lý pin conflict là disable node không dùng:

```
/* Chỉ dùng UART, không dùng GPIO trên chân đó */
&uart1 {
    pinctrl-0 = <&pinctrl_uart1>;
    status = "okay";      /* enable */
};

&gpio_leds {
    status = "disabled";  /* disable → không request pin */
};
```

## 5. Ví dụ thực tế: Cấu hình PWM

### 5.1. Cấu hình trong Device Tree

Thêm nhóm pin PWM vào pin controller:

```dts
&am33xx_pinmux {
	pwm1a_pins: pinmux_pwa1a_pins {
		pinctrl-single,pins = <
			AM33XX_PADCONF(AM335X_PIN_GPMC_A2, PIN_OUTPUT, MUX_MODE6) /* P9_14: EHRPWM1A */
		>;
	};
};

&epwmss1 {
	status = "okay";
};

&ehrpwm1 {
	pinctrl-names = "default";
	pinctrl-0 = <&pwm1a_pins>;
	status = "okay";
};
```

### 5.2. Bật subsystem PWM

Toàn bộ PWM subsystem trong kernel đang tắt, nên khi boot vào kernel sẽ không thấy có `/sys/class/pwm/`, driver ehrpwm không tồn tại nên DTS dù đúng cũng không có gì bind vào. Cần bật thêm các symbol sau:

```
CONFIG_PWM=y
CONFIG_PWM_SYSFS=y
CONFIG_PWM_TIEHRPWM=y    # cho P9_14 / P9_16 (EHRPWM1A/B)
CONFIG_PWM_TIECAP=y      # tuỳ chọn, nếu sau dùng ECAP
```

### 5.3. Build và deploy

- Copy file DTS vào thư mục `arch/arm/boot/dts/` trong source kernel.
- Build device tree blob:

```bash
make -j2 ARCH=arm LOCALVERSION=-bone69 CROSS_COMPILE=$CC dtbs
```

- Copy file `.dtb` sang board và verify bằng `md5sum`.
- Reboot board.

### 5.4. Test PWM từ userspace

```bash
# Export PWM channel
echo 0 > /sys/class/pwm/pwmchipX/export

# Cấu hình period (5ms = 5,000,000 ns)
echo 5000000 > /sys/class/pwm/pwmchipX/pwm0/period

# Cấu hình duty cycle (50% = 2,500,000 ns)
echo 2500000 > /sys/class/pwm/pwmchipX/pwm0/duty_cycle

# Bật PWM
echo 1 > /sys/class/pwm/pwmchipX/pwm0/enable

# Tắt PWM
echo 0 > /sys/class/pwm/pwmchipX/pwm0/enable
```

:::warning PWM sysfs
Thay `X` bằng số pwmchip tương ứng. Đơn vị của period và duty_cycle là nano giây. `duty_cycle` phải nhỏ hơn hoặc bằng `period`.
:::

## 6. Debug

### 6.1. Xem chân đang được mux thành chức năng gì

Lệnh này cho biết mỗi pin đang được peripheral nào sử dụng:

```bash
grep PINX /sys/kernel/debug/pinctrl/44e10800.pinmux-pinctrl-single/pinmux-pins
```

Ví dụ:

```bash
grep PIN0 /sys/kernel/debug/pinctrl/44e10800.pinmux-pinctrl-single/pinmux-pins
```

Output mẫu:

```bash
pin 0 (PIN0): 481d8000.mmc (GPIO UNCLAIMED) function pinmux_emmc_pins group pinmux_emmc_pins
```

$\rightarrow$ Từ output trên, ta biết PIN0 đang được mmc claim với cấu hình `pinmux_emmc_pins`.

$\rightarrow$ **Cách này giúp xác định xem chân ta muốn dùng đang bị peripheral nào claim hay chưa.**

### 6.2. Xem giá trị raw của thanh ghi

Kiểm tra giá trị thực tế được ghi vào thanh ghi Pad Control:

```bash
grep PINX /sys/kernel/debug/pinctrl/44e10800.pinmux-pinctrl-single/pins
```

Ví dụ:

```bash
grep PIN0 /sys/kernel/debug/pinctrl/44e10800.pinmux-pinctrl-single/pins
```

-> Output:

```bash
pin 0 (PIN0) 0:gpio-0-31 44e10800 00000031 pinctrl-single
```

Giá trị `0x31` có thể được giải mã theo cấu trúc thanh ghi đã mô tả ở phần 2.3.

$\rightarrow$ **Từ đây, có thể xác định chân pin có được config đúng như mình mong muốn hay không.**

### 6.3. Kiểm tra các nhóm pin đã đăng ký

```bash
# Xem tất cả nhóm pin đã đăng ký
cat /sys/kernel/debug/pinctrl/44e10800.pinmux-pinctrl-single/pingroups

# Xem các function đã đăng ký
cat /sys/kernel/debug/pinctrl/44e10800.pinmux-pinctrl-single/pinmux-functions
```