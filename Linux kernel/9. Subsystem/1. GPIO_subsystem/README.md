# GPIO Subsystem

## Mở đầu

GPIO (General Purpose Input/Output) là các chân số đa dụng của SoC, có thể cấu hình làm input (đọc mức logic) hoặc output (xuất mức logic). Trên board nhúng, GPIO được dùng để điều khiển LED, đọc nút nhấn, reset thiết bị ngoại vi, chip-select cho SPI, nhận ngắt từ cảm biến...

Vấn đề: mỗi SoC có cách điều khiển GPIO khác nhau (thanh ghi khác nhau, số bank khác nhau). Nếu mỗi driver tự truy cập thanh ghi thì:
- Không có sự thống nhất giữa các SoC.
- Không thể chia sẻ / tránh xung đột khi hai driver cùng dùng một chân.
- Ứng dụng user-space không có API chung.

GPIO subsystem (`drivers/gpio/`) ra đời để giải quyết: cung cấp một lớp trừu tượng chung, quản lý việc cấp phát chân, và xuất ra API thống nhất cho cả kernel-space lẫn user-space.

## Kiến trúc tổng thể

```
                    ┌─────────────────────────────────────┐
   User space       │  App (libgpiod / gpioset / Python)   │
                    └────────────────┬────────────────────┘
                                     │ ioctl()
                    ─────────────────┼──────────────────────
                                     │
                    ┌────────────────▼────────────────────┐
                    │  Character device: /dev/gpiochipN   │  ← gpiolib-cdev.c
                    └────────────────┬────────────────────┘
                                     │
   Kernel space     ┌────────────────▼────────────────────┐
                    │            GPIOLIB (core)           │  ← gpiolib.c
                    │  - quản lý gpio_desc, request/free  │
                    │  - phân giải DT/ACPI → gpio_desc    │
                    │  - API: gpiod_get(), gpiod_set_value│
                    └───┬─────────────────────────┬───────┘
                        │                         │
       consumer drivers │                         │ struct gpio_chip
     (leds-gpio,        │                         │
      gpio-keys, spi…)  │                         ▼
                        │      ┌──────────────────────────────┐
                        │      │  GPIO controller driver      │
                        │      │  (gpio-sunxi, gpio-pl061…)   │
                        │      └──────────────┬───────────────┘
                        │                     │ read/write register
                        │                     ▼
                        │            ┌────────────────┐
                        └───────────►│  SoC hardware  │
                                     └────────────────┘
```

Ba tầng chính:

| Tầng | Vai trò | Vị trí code |
| --- | --- | --- |
| **GPIO controller driver** (provider) | Hiện thực `struct gpio_chip`, đọc/ghi thanh ghi thật của SoC | `drivers/gpio/gpio-*.c` |
| **gpiolib** (core) | Đăng ký chip, cấp phát chân, phân giải Device Tree, chống xung đột | `drivers/gpio/gpiolib*.c` |
| **Consumer** | Driver kernel khác hoặc ứng dụng user-space xin dùng chân GPIO | `drivers/leds/leds-gpio.c`, libgpiod... |

### Quan hệ với pinctrl

GPIO và pinmux là hai việc khác nhau nhưng liên quan chặt:
- **pinctrl** quyết định một chân vật lý hoạt động ở *function* nào (UART TX? I2C SDA? hay GPIO?), và các thuộc tính điện (pull-up/down, drive strength).
- **GPIO** chỉ điều khiển được chân khi pinctrl đã mux chân đó về function GPIO.

Trên nhiều SoC (Allwinner, i.MX, STM32...) một driver duy nhất làm cả hai: vừa `gpiochip_add_data()` vừa `pinctrl_register()`. Cầu nối giữa hai bên là callback `.request` của `gpio_chip` → gọi `pinctrl_gpio_request()`.

## Provider: struct gpio_chip

Đây là interface mà driver controller phải hiện thực:

```c
struct gpio_chip {
    const char *label;
    struct device *parent;

    int  ngpio;          /* số chân của chip này */
    int  base;           /* số hiệu global bắt đầu (legacy, nên để -1) */

    int  (*request)(struct gpio_chip *gc, unsigned offset);
    void (*free)(struct gpio_chip *gc, unsigned offset);

    int  (*get_direction)(struct gpio_chip *gc, unsigned offset);
    int  (*direction_input)(struct gpio_chip *gc, unsigned offset);
    int  (*direction_output)(struct gpio_chip *gc, unsigned offset, int value);

    int  (*get)(struct gpio_chip *gc, unsigned offset);
    void (*set)(struct gpio_chip *gc, unsigned offset, int value);

    int  (*set_config)(struct gpio_chip *gc, unsigned offset,
                       unsigned long config);   /* pull-up, open-drain… */

    int  (*to_irq)(struct gpio_chip *gc, unsigned offset);

    struct gpio_irq_chip irq;   /* nếu chip có khả năng sinh interrupt */
};
```

Đăng ký trong hàm probe:

```c
static int my_gpio_probe(struct platform_device *pdev)
{
    struct my_gpio *priv;

    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->base = devm_platform_ioremap_resource(pdev, 0);
    if (IS_ERR(priv->base))
        return PTR_ERR(priv->base);

    priv->gc.label            = "my-gpio";
    priv->gc.parent           = &pdev->dev;
    priv->gc.owner            = THIS_MODULE;
    priv->gc.base             = -1;     /* để kernel tự cấp số */
    priv->gc.ngpio            = 32;
    priv->gc.direction_input  = my_direction_input;
    priv->gc.direction_output = my_direction_output;
    priv->gc.get              = my_get;
    priv->gc.set              = my_set;

    return devm_gpiochip_add_data(&pdev->dev, &priv->gc, priv);
}
```

Mỗi `gpio_chip` được đăng ký sẽ tạo ra một node `/dev/gpiochipN` và một entry trong `/sys/bus/gpio/devices/`.

Lưu ý về **offset vs global number**: bên trong một chip, chân được đánh số `0..ngpio-1` (offset). Cách đánh số global (`base + offset`) là di sản của sysfs API cũ, đã deprecated — code mới luôn dùng cặp `(gpiochip, offset)`.

## Consumer trong kernel: gpiod API

API hiện đại thao tác trên `struct gpio_desc *`, lấy được từ Device Tree:

```c
#include <linux/gpio/consumer.h>

struct gpio_desc *led;

/* "led" khớp với property "led-gpios" trong DT node của device */
led = devm_gpiod_get(dev, "led", GPIOD_OUT_LOW);
if (IS_ERR(led))
    return PTR_ERR(led);

gpiod_set_value(led, 1);          /* theo logic, có tính GPIO_ACTIVE_LOW */
gpiod_set_raw_value(led, 1);      /* bỏ qua active-low, ghi thẳng mức điện */

val = gpiod_get_value(button);

irq = gpiod_to_irq(button);       /* lấy IRQ number để request_irq() */
```

| Hàm | Ý nghĩa |
| --- | --- |
| `devm_gpiod_get(dev, con_id, flags)` | Xin 1 chân, tự free khi driver unbind |
| `devm_gpiod_get_optional()` | Trả `NULL` nếu DT không khai báo (không coi là lỗi) |
| `devm_gpiod_get_array()` | Xin cả nhóm chân (`data-gpios` nhiều phần tử) |
| `gpiod_direction_input/output()` | Đổi hướng lúc runtime |
| `gpiod_get_value_cansleep()` | Bản dùng được khi chip nằm sau I2C/SPI (có thể ngủ) |

Flags khi request: `GPIOD_IN`, `GPIOD_OUT_LOW`, `GPIOD_OUT_HIGH`, `GPIOD_ASIS`.

**Quan trọng**: `gpiod_get_value()` chỉ được gọi trong atomic context nếu chip là memory-mapped. Với GPIO expander qua I2C/SPI (PCF8574, MCP23017...), phải dùng biến thể `_cansleep()`.

## Khai báo trong Device Tree

Provider node phải có `gpio-controller` và `#gpio-cells`:

```dts
pio: pinctrl@1c20800 {
    compatible = "allwinner,suniv-f1c100s-pinctrl";
    reg = <0x01c20800 0x400>;
    gpio-controller;
    #gpio-cells = <3>;        /* <&pio bank pin flags> */
    interrupt-controller;
    #interrupt-cells = <3>;
};
```

Consumer node tham chiếu tới:

```dts
leds {
    compatible = "gpio-leds";

    led-status {
        label = "status";
        /* bank E (=4), pin 6, active high */
        gpios = <&pio 4 6 GPIO_ACTIVE_HIGH>;
        default-state = "on";
    };
};

my-device {
    compatible = "vendor,my-device";
    reset-gpios = <&pio 1 2 GPIO_ACTIVE_LOW>;
    irq-gpios   = <&pio 1 3 GPIO_ACTIVE_HIGH>;
};
```

Quy ước tên: property phải kết thúc bằng `-gpios` (hoặc `-gpio`), và `con_id` truyền vào `gpiod_get()` là phần đứng trước. Ví dụ `reset-gpios` → `gpiod_get(dev, "reset", ...)`. Nếu chỉ có property tên `gpios` thì truyền `con_id = NULL`.

Số cell (`#gpio-cells`) tuỳ SoC: Allwinner dùng 3 (`bank, pin, flags`), đa số SoC khác dùng 2 (`pin, flags`).

Flags thông dụng (`include/dt-bindings/gpio/gpio.h`): `GPIO_ACTIVE_HIGH`, `GPIO_ACTIVE_LOW`, `GPIO_OPEN_DRAIN`, `GPIO_PULL_UP`, `GPIO_PULL_DOWN`.

## Interface phía user-space

### sysfs (`/sys/class/gpio`) — DEPRECATED

```bash
echo 200 > /sys/class/gpio/export
echo out > /sys/class/gpio/gpio200/direction
echo 1   > /sys/class/gpio/gpio200/value
echo 200 > /sys/class/gpio/unexport
```

Interface này đã bị deprecated từ kernel 4.8 và đang bị gỡ dần (cần bật `CONFIG_GPIO_SYSFS` mới có). Nhược điểm:

| Vấn đề | Mô tả |
| --- | --- |
| Global number không ổn định | Số hiệu thay đổi theo thứ tự probe, khác nhau giữa các phiên bản kernel |
| Không có ownership | Process chết → chân vẫn export, không ai dọn |
| Không atomic | Không set/get nhiều chân cùng lúc |
| Đọc interrupt vụng về | Phải `poll()` trên file `value` |
| Không có metadata | Không biết chân đang được ai dùng, tên là gì |

### Character device (`/dev/gpiochipN`) — cách làm hiện tại

Từ kernel 4.8, mỗi gpiochip có một char device. Ứng dụng mở nó và dùng `ioctl()` để:
- Truy vấn thông tin chip và từng chân (`GPIO_GET_CHIPINFO_IOCTL`, `GPIO_V2_GET_LINEINFO_IOCTL`).
- Xin một **line request** cho một hoặc nhiều chân (`GPIO_V2_GET_LINE_IOCTL`) → nhận về một file descriptor riêng.
- Đọc/ghi giá trị qua fd đó (`GPIO_V2_LINE_GET_VALUES_IOCTL` / `SET_VALUES`).
- Nhận sự kiện cạnh lên/xuống bằng `read()` hoặc `poll()` trên fd.

Ưu điểm quyết định: **quyền sở hữu gắn với file descriptor**. Process chết → fd đóng → chân tự động được giải phóng. Ngoài ra hỗ trợ đọc/ghi nhiều chân atomic và timestamp sự kiện chính xác do kernel gắn.

Có hai thế hệ ABI: **v1** (kernel 4.8+, `gpiohandle_request` / `gpioevent_request`) và **v2** (kernel 5.10+, `gpio_v2_line_request`, gộp value và event vào một request, hỗ trợ debounce). Code mới nên dùng v2.

## libgpiod

Viết ioctl trần rất dài dòng, nên kernel community cung cấp **libgpiod** — thư viện C bọc lại character device ABI, kèm một bộ công cụ dòng lệnh.

| Phiên bản | Ghi chú |
| --- | --- |
| libgpiod v1.x | API `gpiod_chip_open()`, `gpiod_line_request_output()`... Rất phổ biến, còn nhiều trên các distro cũ |
| libgpiod v2.x | Viết lại API quanh `line_request` + `line_config`, chỉ dùng ABI v2, cần kernel ≥ 5.10 |

Hai API **không tương thích ngược**. Kiểm tra phiên bản bằng `gpiodetect --version`.

### Công cụ dòng lệnh

```bash
# Liệt kê các gpiochip trong hệ thống
$ gpiodetect
gpiochip0 [1c20800.pinctrl] (224 lines)

# Xem chi tiết từng line: tên, hướng, ai đang giữ
$ gpioinfo gpiochip0
gpiochip0 - 224 lines:
        line   0:  "PA0"  unused   input  active-high
        ...
        line 134:  "PE6"  "status" output active-high [used]

# Tìm line theo tên
$ gpiofind PE6
gpiochip0 134

# Đọc giá trị
$ gpioget gpiochip0 134            # v1
$ gpioget -c gpiochip0 134         # v2

# Ghi giá trị rồi giữ 2 giây (thoát là chân được release!)
$ gpioset -m time -s 2 gpiochip0 134=1     # v1
$ gpioset -t 2s -c gpiochip0 134=1         # v2

# Theo dõi sự kiện cạnh (nút nhấn)
$ gpiomon -f -n 5 gpiochip0 10             # v1: 5 sự kiện cạnh xuống
$ gpiomon -e falling -c gpiochip0 10       # v2
```

Bẫy hay gặp: `gpioset` giải phóng chân ngay khi thoát, nên mức output trở về mặc định. Muốn giữ mức phải giữ tiến trình sống (`-m wait`, `-t 0`, hoặc viết chương trình C).

### Lập trình C — libgpiod v1

```c
#include <gpiod.h>
#include <unistd.h>

int main(void)
{
    struct gpiod_chip *chip;
    struct gpiod_line *line;

    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip)
        return 1;

    line = gpiod_chip_get_line(chip, 134);
    if (!line)
        goto close_chip;

    /* consumer name sẽ hiện lên trong gpioinfo */
    if (gpiod_line_request_output(line, "blinky", 0) < 0)
        goto close_chip;

    for (int i = 0; i < 10; i++) {
        gpiod_line_set_value(line, i & 1);
        sleep(1);
    }

    gpiod_line_release(line);
close_chip:
    gpiod_chip_close(chip);
    return 0;
}
```

Đọc input và chờ sự kiện:

```c
struct gpiod_line_event ev;
struct timespec ts = { 5, 0 };            /* timeout 5s */

gpiod_line_request_falling_edge_events(line, "button");

while (1) {
    int ret = gpiod_line_event_wait(line, &ts);
    if (ret < 0)  break;
    if (ret == 0) continue;               /* timeout */

    gpiod_line_event_read(line, &ev);
    printf("edge %s at %ld.%09ld\n",
           ev.event_type == GPIOD_LINE_EVENT_RISING_EDGE ? "rising" : "falling",
           ev.ts.tv_sec, ev.ts.tv_nsec);
}
```

### Lập trình C — libgpiod v2

```c
#include <gpiod.h>
#include <unistd.h>

int main(void)
{
    struct gpiod_chip *chip;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *lcfg;
    struct gpiod_request_config *rcfg;
    struct gpiod_line_request *req;
    unsigned int offset = 134;

    chip = gpiod_chip_open("/dev/gpiochip0");

    settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);

    lcfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(lcfg, &offset, 1, settings);

    rcfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(rcfg, "blinky");

    req = gpiod_chip_request_lines(chip, rcfg, lcfg);

    for (int i = 0; i < 10; i++) {
        gpiod_line_request_set_value(req, offset,
              (i & 1) ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
        sleep(1);
    }

    gpiod_line_request_release(req);
    gpiod_request_config_free(rcfg);
    gpiod_line_config_free(lcfg);
    gpiod_line_settings_free(settings);
    gpiod_chip_close(chip);
    return 0;
}
```

Biên dịch:

```bash
gcc blinky.c -o blinky $(pkg-config --cflags --libs libgpiod)
```

Trên Yocto: thêm `libgpiod` (runtime), `libgpiod-dev` (header cho SDK), `libgpiod-tools` (các lệnh CLI). Recipe nằm ở `meta-openembedded/meta-oe/recipes-support/libgpiod/`.

### Python

```bash
pip install gpiod        # binding chính thức, version bám theo thư viện C
```

```python
import gpiod
from gpiod.line import Direction, Value

with gpiod.request_lines(
    "/dev/gpiochip0",
    consumer="blinky",
    config={134: gpiod.LineSettings(direction=Direction.OUTPUT)},
) as request:
    request.set_value(134, Value.ACTIVE)
```

## Cấu hình kernel

```
Device Drivers  --->
  [*] GPIO Support  (GPIOLIB)
      [*]   Character device (/dev/gpiochipN)  (GPIO_CDEV)
      [*]     Support GPIO ABI Version 1       (GPIO_CDEV_V1)
      [ ]   /sys/class/gpio (sysfs interface)  (GPIO_SYSFS)   ← deprecated
      Memory mapped GPIO drivers  --->
      I2C GPIO expanders  --->
      SPI GPIO expanders  --->
```

Driver consumer thường dùng:
- `CONFIG_LEDS_GPIO` — LED nối vào GPIO, xuất ra `/sys/class/leds/`.
- `CONFIG_KEYBOARD_GPIO` — nút nhấn → input event `/dev/input/eventN`.
- `CONFIG_GPIO_PCF857X`, `CONFIG_GPIO_MCP23S08` — GPIO expander qua I2C/SPI.

## Debug

```bash
# Danh sách chip và chân đang được sử dụng (cần CONFIG_DEBUG_FS)
cat /sys/kernel/debug/gpio

# Thông tin theo device
ls /sys/bus/gpio/devices/

# Xem pinmux hiện tại: chân nào đang thuộc function nào
cat /sys/kernel/debug/pinctrl/*/pinmux-pins
cat /sys/kernel/debug/pinctrl/*/pins

# Xem DT mà kernel đang áp dụng
ls /proc/device-tree/
```

Các lỗi thường gặp:

| Triệu chứng | Nguyên nhân thường gặp |
| --- | --- |
| `gpiod_get()` trả `-ENOENT` | Sai tên property trong DT, hoặc thiếu hậu tố `-gpios` |
| `-EBUSY` / `Device or resource busy` | Chân đã bị driver khác giữ (xem cột `[used]` trong `gpioinfo`) |
| Ghi giá trị nhưng chân không đổi | pinctrl chưa mux chân về function GPIO |
| Mức logic bị ngược | Nhầm `GPIO_ACTIVE_LOW`, hoặc dùng `gpiod_set_raw_value()` |
| `gpioset` xong chân về mức cũ | Tiến trình thoát → line request bị release |

## Tổng kết

- Kiến trúc 3 tầng: **controller driver** (`gpio_chip`) → **gpiolib** → **consumer** (driver kernel hoặc user-space).
- Trong kernel: dùng **gpiod API** (`gpiod_get`, `gpiod_set_value`) với mô tả lấy từ Device Tree, không dùng API số nguyên `gpio_request()` cũ.
- Ở user-space: dùng **character device + libgpiod**, không dùng `/sys/class/gpio`.
- Luôn tư duy theo cặp `(gpiochip, offset)` thay vì số hiệu global.
- GPIO phụ thuộc pinctrl: chân phải được mux về function GPIO trước.

## Tham khảo

- `Documentation/driver-api/gpio/` — tài liệu kernel cho provider và consumer.
- `Documentation/devicetree/bindings/gpio/gpio.txt` — quy ước Device Tree.
- `include/linux/gpio/consumer.h`, `include/linux/gpio/driver.h`.
- `include/uapi/linux/gpio.h` — định nghĩa ABI của character device.
- https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git — source libgpiod.
