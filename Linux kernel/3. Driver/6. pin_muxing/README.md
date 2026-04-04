# Pin muxing

## 1. Bản chất của pin muxing

Trên các vi xử lý SoC (như TI AM335x, i.MX6, STM32MP1,…), mỗi chân pin có thể đảm nhiệm nhiều chức năng khác nhau.

Ví dụ trên AM335x (BeagleBone Black): Chân C18 có thể là:
- gpio1_28
- mmc1_dat0
- uart1_ctsn
- spi0_cs1
- ...

Tùy cấu hình mode (Mode 0 → 7) mà phần cứng sẽ chọn chức năng tương ứng.

$\rightarrow$ Vì vậy pinmux là cơ chế phần cứng cho phép chọn chức năng đầu ra của mỗi pin.

Trong Linux, pinmux được quản lý bởi subsystem pinctr. Subsystem này đảm nhiệm:
- Thiết lập pin cho đúng chức năng.
- Thiết lập pull-up/pull-down.
- Cho phép chuyển trạng thái pin theo chế độ (default, sleep,…).

## 2. Cấu trúc pin muxing trong device tree

```
/ {
    pinctrl: pinctrl@addr {           // (1) Pin controller node
        compatible = "...";
        reg = <...>;
        #address-cells = <1>;
        #size-cells = <0>;

        uart1_pins: pinmux_uart1_pins {   // (2) Pin configuration group
            pinctrl-single,pins = <
                0x180 (PIN_OUTPUT_PULLDOWN | MUX_MODE0)
                0x184 (PIN_INPUT_PULLUP | MUX_MODE0)
            >;
        };
    };

    &uart1 {                           // (3) Device node using that pin config
        pinctrl-names = "default";
        pinctrl-0 = <&uart1_pins>;
        status = "okay";
    };
};
```

### 3. Thuộc tính `pinctrl-single,pins`

Là danh sách các offset và giá trị cấu hình cho từng pin. Mỗi phần tử có dạng `<offset value>`.

**Ví dụ:**

```
pinctrl-single,pins = <
    0x180 (PIN_OUTPUT_PULLDOWN | MUX_MODE0)
    0x184 (PIN_INPUT_PULLUP | MUX_MODE0)
>;
```

Trong đó:
- `0x180, 0x184`: offset thanh ghi control register của từng pin.
- `(PIN_OUTPUT_PULLDOWN | MUX_MODE0)`: giá trị ghi vào thanh ghi tương ứng (thường được định nghĩa trong `include/dt-bindings/pinctrl/*.h`).

### 4. Thuộc tính `pinctrl-names`

Khi kernel điều khiển các thiết bị (I2C, UART, GPIO, SPI,...), mỗi thiết bị có thể hoạt động trong nhiều trạng thái khác nhau.

$\rightarrow$ Mỗi trạng thái đó có thể cần cấu hình pin khác nhau. Ví dụ khi ngủ thì cần disable pin để tránh rò điện.

$\rightarrow$ `pinctrl-names` sẽ xác định các trạng thái pin mà device có thể dùng. Thông thường có các trạng thái như "default", "sleep", "idle", "active".

**Ví dụ**

```
pinctrl-names = "default", "sleep";
```

→ Kernel biết rằng device này có hai cấu hình pin:
- default: khi hoạt động bình thường.
- sleep: khi vào trạng thái suspend.
 
### 5. Thuộc tính `pinctrl-0`, `pinctrl-1`, ...

Mỗi thuộc tính này trỏ tới một nhóm pin được định nghĩa trước thông qua `phandle`.

Các thuộc tính này và `pinctrl-names` luôn đi đôi với nhau. Ví dụ:

```
pinctrl-names = "default", "sleep";
pinctrl-0 = <&uart1_pins_default>;
pinctrl-1 = <&uart1_pins_sleep>;
```

| Tên trạng thái | Nhóm cấu hình           | Ý nghĩa |
|----------------|-------------------------|---------|
| default        | `<&uart1_pins_default>` |cấu hình pin khi thiết bị hoạt động bình thường |
| sleep          | `<&uart1_pins_sleep>`   | cấu hình pin khi suspend hoặc shutdown |

## 6. Ví dụ PWM

- Copy file dts [am335x-boneblack](./example/led_dimmy/am335x-boneblack.dts) vào thư mục dts trong source kernel.
- Trở về thư mục `KERNEL` và build dts.
  ```bash
  make -j2 ARCH=arm LOCALVERSION=-bone69 CROSS_COMPILE=$CC dtbs
  ```
- Copy file dtb vừa được build ra vào bbb
- Thực hiện md5sum file dtb trong bbb trước khi copy nó sang thư mục dts trong bbb để verify.
- Khi md5sum của hai file dtb same nhau thì thực hiện reboot
- Khi reboot, ta có thể vào thư mục `/sys/class/pwm/pwmchipX`
- Thực hiện export channel PWM mong muốn
  ```bash
  echo 0 > /sys/class/pwm/pwmchipX/export
  ```
- Sau đó sẽ xuất hiện thư mục `/sys/class/pwm/pwmchipX/pwm0/`.
- Cấu hình tần số và duty cycle, tất cả đơn vị đều tính bằng nano giây.
  ```bash
  echo 5000000 > /sys/class/pwm/pwmchipX/period        # 5 ms chu kỳ
  echo 2500000 > /sys/class/pwm/pwmchipX/duty_cycle    # 50% duty
  ```
- Bật PWM
  ```bash
  echo 1 > /sys/class/pwm/pwmchipX/enable
  ```

## 7. Debug

### 7.1. Kiểm tra device tree runtime

Muốn xem các pinmux hiện có trong device tree lúc runtime ta sử dụng:

``` bash
find /sys/firmware/devicetree/base -name "pinmux@*"
# Output: /sys/firmware/devicetree/base/ocp/interconnect@44c00000/segment@200000/target-module@10000/scm@0/pinmux@800

cd /sys/firmware/devicetree/base/ocp/interconnect@44c00000/segment@200000/target-module@10000/scm@0/pinmux@800
ls
```

Ví dụ output:

![Debug devicetree](img/debug-devicetree2.png)

Từ đây, ta có thể kiểm tra pinmux mong muốn, ví dụ:

```bash
hexdump -C pinmux_spi0_pins/pinctrl-single,pins
```

### 7.2. Xem mỗi chân hiện đang được mux thành chức năng gì?

```bash
grep PINX /sys/kernel/debug/pinctrl/44e10800.pinmux-pinctrl-single/pinmux-pins
```

Ví dụ:

```bash
grep PIN0 /sys/kernel/debug/pinctrl/44e10800.pinmux-pinctrl-single/pinmux-pins
```

-> Output:

```bash
pin 0 (PIN0): 481d8000.mmc (GPIO UNCLAIMED) function pinmux_emmc_pins group pinmux_emmc_pins
```

-> Cách này giúp xác định xem chân ta muốn dùng đang bị peripheral nào claim hay chưa.

### 7.3. Xem raw register của từng pin

Lệnh:

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

Với AM335x pin register layout như sau:

```
bit 5: Pull-disable
bit 4: Pull-up/down select (0=pulldown, 1=pullup)
bit 3: Receiver enable (input enabled)
bit 2: Slew fast/slow
bit 0-2: Mode (0-7)
```

-> Từ đây, có thể xác định chân pin có được config đúng như mình mong muốn hay không.