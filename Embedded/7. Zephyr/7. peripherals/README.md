Ba bài trước đã tách riêng từng công cụ: Kconfig quyết định phần mềm nào được biên dịch, devicetree mô tả phần cứng có gì, CMake gom tất cả lại thành firmware. Bài này ghép cả ba vào việc cụ thể nhất mà ai học Zephyr cũng phải làm: điều khiển ngoại vi.

Mỗi ngoại vi được trình bày theo cùng một khuôn: liệt kê đầy đủ các file của project, sau đó giải thích từng bước tại sao mỗi dòng phải viết như vậy. Board dùng làm ví dụ là **STM32 F4VE** (STM32F407VET6, tên board trong Zephyr là `black_f407ve`) vì nó có sẵn LED, nút bấm và bus I2C đã kéo ra chân. Toàn bộ code C đều viết theo alias nên chạy được trên board khác chỉ bằng cách đổi overlay.

:::warning Luôn kiểm tra lại devicetree của board
Tên label, số chân và alias trong bài lấy theo file `.dts` của `black_f407ve`, nó có thể khác giữa các phiên bản Zephyr. Trước khi copy overlay, mở file devicetree của board (mục 1.3) để đối chiếu tên label thực tế.
:::

## 1. Khung chung của một ứng dụng

### 1.1. Bốn file bắt buộc

Mọi ví dụ trong bài đều có cùng bộ khung sau, khác nhau chỉ ở nội dung:

```
my_app/
├── CMakeLists.txt      # khai báo project + file source
├── prj.conf            # bật driver (Kconfig)
├── app.overlay         # mô tả phần cứng (devicetree)
└── src/
    └── main.c          # code ứng dụng
```

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(my_app)

target_sources(app PRIVATE src/main.c)
```
::: explain [Giải thích CMakeLists.txt]
- `cmake_minimum_required`: Zephyr yêu cầu CMake từ 3.20 trở lên.
- `find_package(Zephyr ...)`: kéo toàn bộ build system của Zephyr vào, phải đứng trước `project()` vì nó chạy Kconfig và devicetree rồi mới tạo thư viện `app`.
- `HINTS $ENV{ZEPHYR_BASE}`: chỉ cho CMake biết tree source Zephyr nằm ở đâu, biến này do `west` hoặc script `zephyr-env.sh` đặt.
- `target_sources(app PRIVATE ...)`: thêm file source của ta vào thư viện `app` mà Zephyr đã tạo sẵn.
:::

```conf [prj.conf]
CONFIG_GPIO=y
```
::: explain [Giải thích prj.conf]
Đây là nơi bật driver: Mỗi ngoại vi có một symbol tổng ở dạng `CONFIG_<SUBSYS>=y`: `CONFIG_I2C`, `CONFIG_PWM`, `CONFIG_ADC`...Symbol này bật lớp API, còn driver cụ thể cho SoC thường tự bật nhờ cơ chế `depends on DT_HAS_<compat>_ENABLED` đã nói ở bài devicetree.
:::

```dts [app.overlay]
/ {
    /* nội dung tuỳ ví dụ */
};
```
::: explain [Giải thích app.overlay]
Nội dung trong file được ghép chồng lên devicetree của board. Tên `app.overlay` là tên mặc định: CMake tự nhặt khi file nằm cạnh `CMakeLists.txt`, không cần khai báo gì thêm.
:::

```c [src/main.c]
#include <zephyr/kernel.h>

int main(void)
{
    return 0;
}
```
::: explain [Giải thích src/main.c]
- `<zephyr/kernel.h>`: header gốc, kéo theo các API kernel cơ bản: `k_msleep`, thread, đồng bộ...
-  Từ Zephyr 3.4 trở đi, `main()` có kiểu `int` và trả về 0.  Trả về từ `main()` không kết thúc chương trình, chỉ kết thúc thread main.
:::
::::

### 1.2. Ba câu hỏi trước khi dùng bất kỳ ngoại vi nào

Khi một ngoại vi không hoạt động, phần lớn trường hợp là do quên một trong ba bước sau. Đây là checklist nên chạy qua mỗi lần thêm ngoại vi mới:

| Bước | Câu hỏi | Kiểm tra ở đâu |
|---|---|---|
| 1. Devicetree | Node có tồn tại và `status = "okay"` chưa? Chân đã được pinctrl cấu hình chưa? | `build/zephyr/zephyr.dts` |
| 2. Kconfig | Driver đã được biên dịch vào firmware chưa? | `build/zephyr/.config` |
| 3. Code | Đã lấy đúng spec, đã `*_is_ready_dt()` trước khi dùng chưa? | `src/main.c` |

Thứ tự này cũng là thứ tự nhân quả: devicetree bật driver qua Kconfig, Kconfig sinh ra `struct device`, code lấy `struct device` ra dùng. Sai bước 1 thì bước 2 và 3 không thể đúng.

### 1.3. Xem board có sẵn những gì

Trước khi viết overlay, nên xem board đã khai báo sẵn cái gì để khỏi viết lại. Ba nguồn thông tin:

```bash
# 1. File devicetree gốc của board
$ZEPHYR_BASE/boards/others/black_f407ve/black_f407ve.dts

# 2. Cây devicetree cuối cùng sau khi ghép overlay (chính xác nhất)
build/zephyr/zephyr.dts

# 3. Danh sách device và thứ tự khởi tạo
west build -t initlevels
```

Phần `aliases` trong file `.dts` của board là thứ đáng xem nhất, nó cho biết board có sẵn `led0`, `sw0`, `pwm-led0`... hay không:

```dts
aliases {
    led0 = &blue_led_1;      /* LED1 - PA6 */
    led1 = &blue_led_2;      /* LED2 - PA7 */
    sw0  = &user_button_k0;  /* nút K0 - PE4 */
};
```

Đây chính là lý do các sample của Zephyr chạy được trên hàng trăm board mà không sửa code: chúng chỉ dùng `DT_ALIAS(led0)`, còn việc alias đó trỏ vào chân nào là chuyện của board.

### 1.4. Build và flash

```bash
west build -p always -b black_f407ve .
west flash
```

Flag `-p always` xoá sạch thư mục build trước khi build lại. Với devicetree và Kconfig, dependency không phải lúc nào cũng được theo dõi đầy đủ nên khi sửa `.overlay` hoặc `prj.conf` mà thấy kết quả không đổi thì hãy build lại với cờ này.

## 2. GPIO output: blink led

Ví dụ kinh điển nhất, nhưng ta sẽ làm kỹ hơn `samples/basic/blinky` một chút: chu kỳ blink được đưa thành một symbol Kconfig riêng của ứng dụng.

### 2.1. Toàn bộ project

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blinky)

target_sources(app PRIVATE src/main.c)
```
::: explain [Giải thích CMakeLists.txt]
- Giống hệt khung ở mục 1.1, chỉ đổi tên `project(blinky)`.
- `target_sources`: khai báo file nguồn duy nhất của ứng dụng.
:::

```conf [Kconfig]
mainmenu "Blinky Application"

config APP_BLINK_PERIOD_MS
    int "Chu kỳ blink LED (ms)"
    range 50 5000
    default 500
    help
      Khoảng thời gian giữa hai lần đảo trạng thái LED.

source "Kconfig.zephyr"
```
::: explain [Giải thích Kconfig]
- `mainmenu`: tiêu đề hiện ở đầu giao diện `menuconfig`.
- `config APP_BLINK_PERIOD_MS`: symbol riêng của ứng dụng, kiểu `int`.
- `range 50 5000`: chặn giá trị ngoài khoảng; `default 500` dùng khi `prj.conf` không đặt gì.
- `help`: nội dung hiện khi bấm `?` trong menuconfig.
- `source "Kconfig.zephyr"`: bắt buộc ở cuối file, thiếu dòng này là mất toàn bộ symbol của Zephyr.
- Symbol `APP_BLINK_PERIOD_MS` được generate vào `autoconf.h` nên dùng trong C như mọi `CONFIG_` khác và đổi được ngay từ dòng lệnh mà không sửa code:

```bash
west build -b black_f407ve . -- -DCONFIG_APP_BLINK_PERIOD_MS=100
```
:::

```conf [prj.conf]
CONFIG_GPIO=y
CONFIG_APP_BLINK_PERIOD_MS=250
```
::: explain [Giải thích prj.conf]
- `CONFIG_GPIO=y`: bật driver GPIO.
- `CONFIG_APP_BLINK_PERIOD_MS=250`: ghi đè khai báo `default 500` trong `Kconfig`.
:::

```dts [app.overlay]
/ {
    aliases {
        /* board black_f407ve đã có sẵn alias led0,
           dòng này chỉ cần khi board của ta chưa có */
        led0 = &blue_led_1;
    };
};
```
::: explain [Giải thích app.overlay]
Khai báo alias `led0` trỏ tới node LED của board. F4VE đã có sẵn alias này nên khối trên chỉ cần khi chuyển sang board chưa có.

Node được trỏ tới nằm trong `.dts` của board:

```dts
leds {
    compatible = "gpio-leds";
    blue_led_1: led_1 {
        gpios = <&gpioa 6 GPIO_ACTIVE_LOW>;
        label = "User LD1";
    };
    blue_led_2: led_2 {
        gpios = <&gpioa 7 GPIO_ACTIVE_LOW>;
        label = "User LD2";
    };
};
```

Ba thông tin cần cho việc bật LED nằm gọn trong property `gpios`: controller (`&gpioa`), số chân (`6`) và cực tính (`GPIO_ACTIVE_LOW`). Cực tính là thứ đáng chú ý nhất. Trên F4VE, hai LED nối anode lên 3V3 và cathode xuống chân MCU nên chân phải kéo xuống mức thấp thì LED mới sáng, đó là lý do ghi `GPIO_ACTIVE_LOW`. Board nào nối LED xuống GND thì ghi `GPIO_ACTIVE_HIGH`.
:::

```c [src/main.c]
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define LED_NODE DT_ALIAS(led0)

#if !DT_NODE_HAS_STATUS(LED_NODE, okay)
#error "Alias led0 is missing or the node is disabled"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);

int main(void)
{
    int ret;

    if (!gpio_is_ready_dt(&led)) {
        printk("GPIO controller is not ready\n");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Failed to configure LED pin: %d\n", ret);
        return ret;
    }

    printk("Blinking every %d ms on %s pin %d\n",
           CONFIG_APP_BLINK_PERIOD_MS, led.port->name, led.pin);

    while (1) {
        gpio_pin_toggle_dt(&led);
        k_msleep(CONFIG_APP_BLINK_PERIOD_MS);
    }

    return 0;
}
```
::: explain [Giải thích src/main.c]
- `#define LED_NODE DT_ALIAS(led0)`: lấy node identifier từ alias, code không phụ thuộc tên label của board.
- Khối `#if ... #error`: chặn lỗi ngay lúc biên dịch nếu board không có alias, thay cho một đống lỗi macro khó đọc. Đây là mẫu nên có ở đầu mọi file dùng devicetree.
- `GPIO_DT_SPEC_GET`: Macro nhận hai tham số: node identifier và tên property chứa phandle GPIO (ở đây là `gpios`). Kết quả là một struct `gpio_dt_spec` gồm `port`, `pin`, `dt_flags`. Vì mọi giá trị đều là hằng số biên dịch nên biến này khai báo được `static const`, nó nằm ở flash và không tốn RAM. Nếu tự lấy ba thứ đó rời rạc thì rất dễ lấy `pin` của node này nhưng truyền `port` của node khác, mà compiler không bắt được vì cả ba đều là số hoặc con trỏ hợp lệ.
- Nhờ `dt_flags` mang theo `GPIO_ACTIVE_LOW`/`HIGH`, toàn bộ code phía sau làm việc với mức logic: `gpio_pin_set_dt(&led, 1)` luôn có nghĩa là bật LED bất kể phần cứng nối kiểu gì.
- `gpio_is_ready_dt(&led)`: thực chất là `device_is_ready(led.port)`. Với GPIO on chip thì gần như luôn thành công, nhưng với GPIO expander qua I2C (PCF8574, MCP23017...) đây là chốt chặn thật sự vì chip có thể không phản hồi.
- `gpio_pin_configure_dt(..., GPIO_OUTPUT_ACTIVE)`: Tham số thứ hai là thêm vào cờ đã có trong devicetree chứ không thay thế. Các giá trị hay dùng:

    | Cờ | Ý nghĩa |
    |---|---|
    | `GPIO_OUTPUT` | Output, trạng thái ban đầu không xác định |
    | `GPIO_OUTPUT_ACTIVE` | Output, khởi tạo ở mức active (LED sáng) |
    | `GPIO_OUTPUT_INACTIVE` | Output, khởi tạo ở mức inactive (LED tắt) |
    | `GPIO_INPUT` | Input |
    | `GPIO_INPUT \| GPIO_PULL_UP` | Input có điện trở pullup |
    | `GPIO_OUTPUT \| GPIO_OPEN_DRAIN` | Output open drain |

    Luôn ưu tiên `GPIO_OUTPUT_ACTIVE`/`INACTIVE` thay vì `GPIO_OUTPUT`, vì hai cờ này đặt luôn trạng thái ban đầu trong cùng một thao tác, tránh việc LED nhấp nháy một cái lúc khởi động.

- `gpio_pin_toggle_dt()` + `k_msleep(CONFIG_APP_BLINK_PERIOD_MS)`: vòng lặp blink, chu kỳ lấy thẳng từ Kconfig.
:::
::::

### 2.2. Trường hợp board không có alias `led0`

Đây là tình huống rất hay gặp với board tự thiết kế hoặc board tối giản như `stm32_min_dev@blue` (Blue Pill). Ta tự khai báo node LED trong overlay:

:::: code-group
```dts [app.overlay]
/ {
    leds {
        compatible = "gpio-leds";

        status_led: led_0 {
            gpios = <&gpioc 13 GPIO_ACTIVE_LOW>;
            label = "Status LED";
        };
    };

    aliases {
        led0 = &status_led;
    };
};
```
::: explain [Ba điểm cần nhớ]
1. Node cha phải có `compatible = "gpio-leds"`. Binding `gpio-leds.yaml` có `child-binding:` khai báo property `gpios` cho các node con, không có nó thì `GPIO_DT_SPEC_GET` không tìm được macro.
2. `GPIO_ACTIVE_LOW` vì LED trên Blue Pill nối từ nguồn xuống chân PC13, kéo chân xuống mức thấp thì LED sáng. Khai báo đúng ở đây giúp code C không phải đảo logic.
3. Alias khai báo trong overlay bổ sung cho phần `aliases` của board, nhờ vậy `src/main.c` không cần sửa một dòng nào.
:::
::::

Nếu ứng dụng chạy trên nhiều board với cấu hình khác nhau, thay vì sửa `app.overlay` mỗi lần, tạo thư mục `boards/` và đặt tên file theo tên board:

```
my_app/
├── boards/
│   ├── black_f407ve.overlay
│   └── stm32_min_dev_blue.overlay
└── app.overlay          # phần dùng chung
```

CMake tự chọn đúng file theo tham số `-b`. Cả `app.overlay` lẫn `boards/<board>.overlay` đều được nạp, phần riêng của board được ghép sau nên có quyền ghi đè.

### 2.3. Lỗi thường gặp

| Triệu chứng | Nguyên nhân |
|---|---|
| Lỗi biên dịch `DT_N_ALIAS_led0 undefined` | Board không có alias `led0`, chưa khai báo trong overlay |
| Link error `undefined reference to __device_dts_ord_NN` | Thiếu `CONFIG_GPIO=y` |
| LED sáng ngược (tắt khi lẽ ra phải sáng) | Sai `GPIO_ACTIVE_LOW`/`GPIO_ACTIVE_HIGH` trong devicetree |
| `gpio_pin_configure_dt` trả về `-EINVAL` | Chân không hỗ trợ flag được yêu cầu, ví dụ xin pull-up trên chân không có |
| LED nhấp nháy một cái lúc boot | Dùng `GPIO_OUTPUT` thay vì `GPIO_OUTPUT_INACTIVE` |

## 3. GPIO input: đọc nút nhấn bằng polling

Trước khi sang ngắt, làm bản polling để thấy rõ vấn đề mà ngắt giải quyết.

**Toàn bộ project**

:::: code-group
```conf [prj.conf]
CONFIG_GPIO=y
```
::: explain [Giải thích prj.conf]
- Chỉ cần `CONFIG_GPIO=y`, ví dụ dùng `printk` nên chưa cần bật hệ thống log.
:::

```dts [app.overlay]
/ {
    buttons {
        compatible = "gpio-keys";

        user_btn: button_0 {
            /* nút K0 trên F4VE nối chân PE4 xuống GND */
            gpios = <&gpioe 4 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
            label = "User button K0";
            zephyr,code = <INPUT_KEY_0>;
        };
    };

    aliases {
        sw0 = &user_btn;
    };
};
```
::: explain [Giải thích app.overlay]
- `compatible = "gpio-keys"`: binding chuẩn cho nút bấm, tương ứng với `gpio-leds` của LED.
- `(GPIO_ACTIVE_LOW | GPIO_PULL_UP)`: Hai cờ này mô tả hai chuyện khác nhau:
  - `GPIO_ACTIVE_LOW` là thông tin logic: mạch nối nút xuống GND, nhấn thì chân xuống mức thấp. Nhờ nó mà `gpio_pin_get_dt()` trả về `1` khi nhấn.
  - `GPIO_PULL_UP` là thông tin cấu hình phần cứng: yêu cầu bật điện trở kéo lên bên trong SoC. Mạch đã có điện trở kéo lên ngoài thì không cần cờ này.

  Bắt buộc phải có dấu ngoặc đơn bao quanh biểu thức logic. Devicetree preprocessor cần nó, thiếu ngoặc sẽ báo lỗi cú pháp khó hiểu.

- `zephyr,code`: mã phím theo chuẩn input subsystem, chỉ cần khi ta dùng subsystem `input` còn với GPIO API thuần thì có hay không đều được..
- `aliases`: gán `sw0` để code C gọi bằng `DT_ALIAS(sw0)`.
:::

```c [src/main.c]
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#define LED_NODE DT_ALIAS(led0)
#define BTN_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(BTN_NODE, gpios);

int main(void)
{
    int prev = 0;

    if (!gpio_is_ready_dt(&led) || !gpio_is_ready_dt(&btn)) {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&btn, GPIO_INPUT);

    while (1) {
        int cur = gpio_pin_get_dt(&btn);   /* 1 = đang nhấn, 0 = nhả */

        if (cur < 0) {
            printk("Failed to read pin: %d\n", cur);
            return cur;
        }

        if (cur != prev) {
            printk("Button %s\n", cur ? "pressed" : "released");
            gpio_pin_set_dt(&led, cur);    /* nhấn thì sáng, nhả thì tắt */
            prev = cur;
        }

        k_msleep(10);
    }

    return 0;
}
```
::: explain [Giải thích src/main.c]
- Hai spec `led` và `btn` lấy từ hai alias, cùng một cách viết.
- `gpio_pin_configure_dt(&btn, GPIO_INPUT)`: đặt chân thành đầu vào.
- `gpio_pin_get_dt()`: trả về mức logic (1 nghĩa là đang nhấn) nhờ `GPIO_ACTIVE_LOW` trong devicetree. Muốn đọc mức điện thật thì dùng `gpio_pin_get_raw()`, nhưng ứng dụng bình thường gần như không cần bản `raw`. Hàm trả số âm khi lỗi nên `if (cur)` là cách viết sai kinh điển: `-EIO` cũng khác 0 nên bị hiểu thành đang nhấn. Luôn kiểm tra `< 0` trước.
- `if (cur != prev)`: chỉ xử lý khi trạng thái đổi, tránh in liên tục.
:::
::::

**Vì sao polling không ổn**

Vòng `while` có 3 vấn đề:
  - **Tốn CPU**: cứ 10 ms lại dậy một lần dù chẳng có gì xảy ra, chặn hệ thống vào chế độ ngủ sâu.
  - **Trễ**: nhấn rồi nhả nhanh hơn 10 ms thì mất luôn sự kiện.
  - **Không mở rộng được**: thêm cảm biến, thêm nút thì `k_msleep(10)` của việc này làm chậm việc kia.

Đây chính là lý do phải chuyển sang ngắt.

## 4. GPIO interrupt: nhấn nút bật/tắt LED

### 4.1. Toàn bộ project

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(button_irq)

target_sources(app PRIVATE src/main.c)
```
::: explain [Giải thích CMakeLists.txt]
- Khung chuẩn, đổi tên `project(button_irq)`.
:::

```conf [Kconfig]
mainmenu "Button Interrupt Application"

config APP_DEBOUNCE_MS
    int "Thời gian chống rung phím (ms)"
    range 5 200
    default 40
    help
      Sau khi có cạnh đầu tiên, mọi cạnh trong khoảng thời gian này
      đều bị bỏ qua. Nút có chất lượng kém cần giá trị lớn hơn.

source "Kconfig.zephyr"
```
::: explain [Giải thích Kconfig]
- `APP_DEBOUNCE_MS`: cho phép chỉnh thời gian chống rung mà không phải sửa code, tiện khi đổi loại nút.
- `range 5 200`: chặn các giá trị vô lý ngay từ khâu cấu hình.
:::

```conf [prj.conf]
CONFIG_GPIO=y
CONFIG_APP_DEBOUNCE_MS=40

# In log ra console
CONFIG_LOG=y
CONFIG_LOG_MODE_DEFERRED=y
```
::: explain [Giải thích prj.conf]
- `CONFIG_GPIO=y`: bật driver GPIO.
- `CONFIG_APP_DEBOUNCE_MS=40`: giá trị chống rung dùng cho lần build này.
- `CONFIG_LOG=y` + `CONFIG_LOG_MODE_DEFERRED=y`: log được đẩy sang thread riêng nên việc in không chặn ngữ cảnh gọi.
:::

```dts [app.overlay]
/ {
    aliases {
        /* black_f407ve đã có sẵn cả hai alias này,
           khai báo lại chỉ để ví dụ chạy được trên board khác */
        led0 = &blue_led_1;
        sw0  = &user_button_k0;
    };
};
```
::: explain [Giải thích app.overlay]
- Khai báo hai alias `led0` và `sw0`; F4VE đã có sẵn cả hai, khối này chỉ cần khi chạy trên board khác.
:::

```c [src/main.c]
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define LED_NODE DT_ALIAS(led0)
#define BTN_NODE DT_ALIAS(sw0)

#if !DT_NODE_HAS_STATUS(LED_NODE, okay) || !DT_NODE_HAS_STATUS(BTN_NODE, okay)
#error "Both led0 and sw0 aliases are required in the devicetree"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED_NODE, gpios);
static const struct gpio_dt_spec btn = GPIO_DT_SPEC_GET(BTN_NODE, gpios);

static struct gpio_callback btn_cb;
static bool led_on;

/* Chạy trong ngữ cảnh thread của system workqueue, không phải ISR */
static void debounce_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    /* Sau khoảng chống rung, đọc lại chân để bỏ qua nhiễu */
    if (gpio_pin_get_dt(&btn) != 1) {
        return;
    }

    led_on = !led_on;
    gpio_pin_set_dt(&led, led_on);
    LOG_INF("Button pressed -> LED %s", led_on ? "ON" : "OFF");
}

static K_WORK_DELAYABLE_DEFINE(debounce_work, debounce_handler);

/* Chạy trong ngữ cảnh ngắt */
static void button_isr(const struct device *port,
                       struct gpio_callback *cb,
                       gpio_port_pins_t pins)
{
    ARG_UNUSED(port);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    k_work_reschedule(&debounce_work, K_MSEC(CONFIG_APP_DEBOUNCE_MS));
}

int main(void)
{
    int ret;

    if (!gpio_is_ready_dt(&led) || !gpio_is_ready_dt(&btn)) {
        LOG_ERR("GPIO controller is not ready");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        return ret;
    }

    ret = gpio_pin_configure_dt(&btn, GPIO_INPUT);
    if (ret < 0) {
        return ret;
    }

    /* 1. Bật ngắt trên cạnh chuyển sang trạng thái active */
    ret = gpio_pin_interrupt_configure_dt(&btn, GPIO_INT_EDGE_TO_ACTIVE);
    if (ret < 0) {
        LOG_ERR("Pin %d does not support interrupts: %d", btn.pin, ret);
        return ret;
    }

    /* 2. Khai báo callback và chân nào kích hoạt nó */
    gpio_init_callback(&btn_cb, button_isr, BIT(btn.pin));

    /* 3. Gắn callback vào controller */
    ret = gpio_add_callback_dt(&btn, &btn_cb);
    if (ret < 0) {
        return ret;
    }

    LOG_INF("Ready, press the button to toggle the LED");

    /* Không còn vòng lặp bận rộn, thread main ngủ vĩnh viễn */
    k_sleep(K_FOREVER);
    return 0;
}
```
::: explain [Giải thích src/main.c]

**Chọn kiểu ngắt**

```c
gpio_pin_interrupt_configure_dt(..., GPIO_INT_EDGE_TO_ACTIVE)
```

Các kiểu ngắt chọn được:

| Cờ | Kích hoạt khi |
|---|---|
| `GPIO_INT_EDGE_TO_ACTIVE` | Chân chuyển sang trạng thái active (theo cực tính trong DT) |
| `GPIO_INT_EDGE_TO_INACTIVE` | Chân rời trạng thái active |
| `GPIO_INT_EDGE_BOTH` | Cả hai chiều |
| `GPIO_INT_EDGE_RISING` | Cạnh lên về điện áp, bỏ qua cực tính |
| `GPIO_INT_EDGE_FALLING` | Cạnh xuống về điện áp |
| `GPIO_INT_LEVEL_ACTIVE` | Giữ ngắt suốt thời gian chân ở mức active |
| `GPIO_INT_DISABLE` | Tắt ngắt trên chân này |

- Cặp `TO_ACTIVE`/`TO_INACTIVE` nên dùng trong ứng dụng vì chúng tự thích nghi theo cực tính trong devicetree: đổi board có nút nối ngược lại thì chỉ cần sửa overlay. Cặp `RISING`/`FALLING` chỉ dùng khi thực sự quan tâm mức điện, ví dụ đọc encoder.

- `GPIO_INT_LEVEL_*` thì cần cẩn thận, ngắt kích hoạt liên tục suốt thời gian giữ mức, ISR không xử lý nguồn gây ngắt là hệ thống treo trong bão ngắt.

**Đăng ký callback**

```c
static struct gpio_callback btn_cb;

gpio_init_callback(&btn_cb, button_isr, BIT(btn.pin));
gpio_add_callback_dt(&btn, &btn_cb);
```

Tham số thứ ba của `gpio_init_callback` là bit mask, không phải số chân. Nhờ vậy một callback phục vụ được nhiều chân trên cùng controller:

```c
gpio_init_callback(&cb, multi_isr, BIT(btn1.pin) | BIT(btn2.pin));
```

Trong ISR, tham số `pins` cho biết chân nào vừa kích hoạt:

```c
if (pins & BIT(btn1.pin)) { /* nút 1 */ }
if (pins & BIT(btn2.pin)) { /* nút 2 */ }
```

Biến `btn_cb` không được là biến cục bộ, driver giữ con trỏ tới nó trong danh sách liên kết. Để trên stack của `main()` thì danh sách trỏ vào vùng nhớ rác.

`gpio_add_callback_dt(&btn, &btn_cb)`: cách viết gọn của `gpio_add_callback(btn.port, &btn_cb)`.

**ISR**

`button_isr()` chạy trong ngữ cảnh ngắt, nghĩa là:

| Được phép | Cấm |
|---|---|
| `k_sem_give()` | `k_sleep()`, `k_msleep()` |
| `k_work_submit()`, `k_work_reschedule()` | `k_mutex_lock()` |
| `k_msgq_put(..., K_NO_WAIT)` | Bất kỳ hàm nào có thể block |
| Đọc/ghi GPIO | Cấp phát bộ nhớ động, in log dài |
| Đặt cờ `volatile`, thao tác `atomic_*` | Gọi API I2C/SPI dạng blocking |

Lý do: Ngắt không phải một thread, không có gì để scheduler chuyển sang khi nó bị block, gọi `k_sleep()` trong ISR làm kernel panic:

Template chuẩn là defer: ISR ghi nhận sự kiện rồi đẩy phần việc sang thread. Ba cách phổ biến

```c
/* Cách 1: workqueue - đơn giản nhất, dùng trong ví dụ này */
k_work_submit(&my_work);

/* Cách 2: semaphore - khi có thread riêng đang chờ */
k_sem_give(&my_sem);

/* Cách 3: message queue - khi cần truyền kèm dữ liệu */
k_msgq_put(&my_msgq, &event, K_NO_WAIT);
```

**Chống rung phím**

Nút cơ khí không tạo ra một cạnh duy nhất mà tạo ra hàng chục cạnh trong vài mili giây. Không xử lý thì một lần nhấn có thể toggle LED nhiều lần và kết quả nhìn như ngẫu nhiên.

Cách chống rung dùng ở đây gọn và rất hiệu quả:

```c
static void button_isr(...)
{
    k_work_reschedule(&debounce_work, K_MSEC(CONFIG_APP_DEBOUNCE_MS));
}
```

`k_work_reschedule` đặt lại hẹn giờ: nếu công việc đang chờ thì nó huỷ hẹn cũ và hẹn lại từ đầu. Vì vậy trong lúc tiếp điểm còn rung, mỗi cạnh mới đều đẩy mốc hẹn ra xa thêm 40 ms. Chỉ khi tín hiệu đã ổn định trọn 40 ms thì `debounce_handler` mới thực sự chạy và chạy đúng một lần.

Trong `debounce_handler`, nó đọc lại chân trước khi đảo LED:

```c
if (gpio_pin_get_dt(&btn) != 1) {
    return;
}
```

Nếu sau 40 ms mà chân không còn ở trạng thái active thì đó là nhiễu chứ không phải cú nhấn thật $\rightarrow$ bỏ qua.

**Chia sẽ biến giữa ISR và thread**

Trong ví dụ này, `led_on` chỉ được đọc/ghi trong `debounce_handler` chạy ở thread workqueue nên an toàn. Nếu ISR cũng ghi vào một biến mà thread đọc, biến đó phải khai báo `volatile` và nếu thao tác gồm nhiều bước thì phải bảo vệ bằng `k_spin_lock()` hoặc dùng kiểu `atomic`:

```c
static atomic_t press_count;

atomic_inc(&press_count);                    /* trong ISR    */
atomic_val_t n = atomic_clear(&press_count); /* trong thread */
```

**Thread main sleep**

`k_sleep(K_FOREVER)` khiến thread main ngủ, CPU vào idle. Toàn bộ hoạt động do ngắt và workqueue đảm nhiệm, tiêu thụ điện giảm mạnh so với bản polling. Đây là hình dạng điển hình của một firmware hướng sự kiện.
:::
::::

### 4.2. Lỗi thường gặp

| Triệu chứng | Nguyên nhân |
|---|---|
| `gpio_pin_interrupt_configure_dt` trả `-ENOTSUP` | Chân/controller không hỗ trợ ngắt hoặc SoC giới hạn số line ngắt |
| Callback không bao giờ chạy | Truyền `btn.pin` thay vì `BIT(btn.pin)` vào `gpio_init_callback` |
| Callback chạy loạn xạ | Chưa chống rung hoặc chân input đang thả nổi vì thiếu `GPIO_PULL_UP` |
| Kernel panic ngay lần nhấn đầu | Gọi hàm có thể block (`k_msleep`, `k_mutex_lock`) trong ISR |
| Callback chạy một lần rồi thôi | `struct gpio_callback` khai báo là biến cục bộ |

:::tip Xung đột EXTI trên STM32
STM32 dùng chung một line EXTI cho các chân cùng số trên mọi port: PA13, PB13, PC13 dùng chung `EXTI13`. Vì vậy không thể bật ngắt đồng thời trên PA13 và PC13. Khi thiết kế mạch, chọn số chân khác nhau cho các nguồn ngắt.
:::

## 5. I2C: đọc cảm biến SHT30

SHT30 là cảm biến nhiệt độ/độ ẩm của Sensirion, địa chỉ I2C `0x44` (chân ADDR nối GND) hoặc `0x45` (nối VDD). Trong mục này, ta làm hai cách đọc cảm biến SHT30 để thấy rõ sự khác nhau giữa việc tự viết binding và việc dùng driver có sẵn.

### 5.1. Cách 1: tự viết binding, giao tiếp bằng I2C API

Đây là cách cần dùng khi Zephyr chưa có driver cho con chip của ta, đồng thời là bài tập tổng hợp tốt nhất về binding.

:::: code-group
```cmake [CMakeLists.txt]
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sht30_raw)

target_sources(app PRIVATE src/main.c)
```
::: explain [Giải thích CMakeLists.txt]
Template chuẩn. Không cần khai báo gì cho thư mục `dts/bindings/`, build system tự quét khi nó nằm cạnh `CMakeLists.txt`.

Thư mục `dts/bindings/` đặt cạnh `CMakeLists.txt` được build system tự quét, không cần khai báo gì trong CMake. Nếu muốn dùng chung binding cho nhiều project thì đặt nó ở nơi khác rồi trỏ tới bằng `DTS_ROOT`:

```cmake
list(APPEND DTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../common)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
```

Dòng `list(APPEND DTS_ROOT ...)` phải đứng trước `find_package` vì devicetree được xử lý ngay bên trong lệnh này:
:::

```yaml [dts/bindings/myapp,sht30.yaml]
description: |
  Cảm biến nhiệt độ / độ ẩm SHT30 được ứng dụng truy cập trực tiếp
  bằng I2C API (không dùng sensor subsystem).

  Ví dụ:

      &i2c1 {
          sht30: sht30@44 {
              compatible = "myapp,sht30";
              reg = <0x44>;
              measure-interval-ms = <2000>;
          };
      };

compatible: "myapp,sht30"

include: [i2c-device.yaml]

properties:
  measure-interval-ms:
    type: int
    default: 1000
    description: Chu kỳ lấy mẫu, tính bằng mili giây.
```
::: explain [Giải thích dts/bindings/myapp,sht30.yaml]
Không có file YAML này thì node `sht30@44` vẫn nằm trong cây nhưng không sinh macro property nào, `I2C_DT_SPEC_GET` sẽ lỗi biên dịch. Hai dòng quan trọng nhất trong file binding:

```yaml
compatible: "myapp,sht30"     # sợi dây nối YAML với node
include: [i2c-device.yaml]    # kế thừa: reg bắt buộc + on-bus: i2c
```

`include: [i2c-device.yaml]` làm hai việc: bắt buộc property reg (địa chỉ slave) và khai báo on-bus: i2c. Vế thứ hai quan trọng vì nó buộc node phải nằm dưới một node có bus: i2c, đồng thời cho phép `DT_BUS()` trả về đúng controller.

Property tự định nghĩa `measure-interval-ms` có `default: 1000`, nghĩa là node không khai báo thì vẫn đọc được bằng `DT_PROP(...)`. Trong `app.overlay` ta ghi đè thành 2000.
:::

```conf [prj.conf]
CONFIG_I2C=y
CONFIG_LOG=y

CONFIG_CRC=y

CONFIG_SHELL=y
CONFIG_I2C_SHELL=y
```
::: explain [Giải thích prj.conf]
- `CONFIG_I2C=y`: bật lớp API I2C.
- `CONFIG_CRC=y`: lấy hàm `crc8()` có sẵn của Zephyr để kiểm tra dữ liệu.
- `CONFIG_SHELL` + `CONFIG_I2C_SHELL`: có lệnh `i2c scan` để dò thiết bị trên bus, đây là công cụ debug quan trọng nhất khi làm việc với I2C:

```
uart:~$ i2c scan i2c@40005400
     0 1 2 3 4 5 6 7 8 9 a b c d e f
00:             -- -- -- -- -- -- -- --
40: -- -- -- -- 44 -- -- -- -- -- -- -- -- -- -- --
1 devices found on i2c@40005400
```

- Không thấy `44` ở đây thì vấn đề nằm ở phần cứng (dây, nguồn, điện trở kéo lên) hoặc pinctrl, không phải ở code. Đừng debug code C trước khi lệnh này chạy đúng.
:::

```dts [app.overlay]
&i2c1 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_STANDARD>;

    sht30: sht30@44 {
        compatible = "myapp,sht30";
        reg = <0x44>;
        measure-interval-ms = <2000>;
    };
};
```
::: explain [Giải thích app.overlay]

Node `i2c1` đã có sẵn trong file `.dtsi` của SoC nhưng thường ở trạng thái `disabled`, ta chỉ cần bật lên bằng cú pháp `&label`:
- `clock-frequency`: `I2C_BITRATE_STANDARD` là 100 kHz, `I2C_BITRATE_FAST` là 400 kHz. SHT30 chịu được tới 1 MHz nhưng dây dài thì nên giữ ở 100 kHz.
- `sht30@44` và `reg = <0x44>`: địa chỉ tại tên node và trong `reg` phải trùng nhau.
- `measure-interval-ms = <2000>`: ghi đè giá trị `default` khai báo trong binding.

Nếu board chưa cấu hình chân cho `i2c1` thì cần thêm pinctrl:

```
&i2c1 {
    pinctrl-0 = <&i2c1_scl_pb6 &i2c1_sda_pb7>;
    pinctrl-names = "default";
    status = "okay";
};
```

Tên như `i2c1_scl_pb6` do file pinctrl của SoC định nghĩa, tra trong `zephyr/dts/bindings/pinctrl/` hoặc file `<soc>-pinctrl.dtsi`. Mở `build/zephyr/zephyr.dts` để biết chắc board đã khai báo sẵn hay chưa.
:::

```c [src/main.c]
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sht30, LOG_LEVEL_INF);

#define SHT30_NODE DT_NODELABEL(sht30)

#if !DT_NODE_HAS_STATUS(SHT30_NODE, okay)
#error "The sht30 node is not enabled in the devicetree"
#endif

#define SHT30_CMD_MEASURE_HIGH_REP  0x2400
#define SHT30_MEASURE_TIME_MS       20
#define SHT30_INTERVAL_MS           DT_PROP(SHT30_NODE, measure_interval_ms)

static const struct i2c_dt_spec sht30 = I2C_DT_SPEC_GET(SHT30_NODE);

static bool crc_ok(const uint8_t *data, uint8_t expected)
{
    return crc8(data, 2, 0x31, 0xFF, false) == expected;
}

static int sht30_measure(int32_t *temp_mc, int32_t *hum_mpct)
{
    uint8_t cmd[2] = { SHT30_CMD_MEASURE_HIGH_REP >> 8,
                       SHT30_CMD_MEASURE_HIGH_REP & 0xFF };
    uint8_t rx[6];
    int ret;

    ret = i2c_write_dt(&sht30, cmd, sizeof(cmd));
    if (ret < 0) {
        LOG_ERR("Failed to send measurement command: %d", ret);
        return ret;
    }

    k_msleep(SHT30_MEASURE_TIME_MS);

    ret = i2c_read_dt(&sht30, rx, sizeof(rx));
    if (ret < 0) {
        LOG_ERR("Failed to read measurement result: %d", ret);
        return ret;
    }

    if (!crc_ok(&rx[0], rx[2]) || !crc_ok(&rx[3], rx[5])) {
        LOG_ERR("CRC mismatch, data is not reliable");
        return -EIO;
    }

    uint16_t raw_t = sys_get_be16(&rx[0]);
    uint16_t raw_h = sys_get_be16(&rx[3]);

    /* T  = -45 + 175 * raw / 65535   (độ C)
       RH = 100 * raw / 65535         (%)      */
    *temp_mc  = -45000 + (int32_t)((175000LL * raw_t) / 65535);
    *hum_mpct = (int32_t)((100000LL * raw_h) / 65535);

    return 0;
}

int main(void)
{
    if (!i2c_is_ready_dt(&sht30)) {
        LOG_ERR("I2C bus is not ready");
        return -ENODEV;
    }

    LOG_INF("SHT30 at address 0x%02x on bus %s",
            sht30.addr, sht30.bus->name);

    while (1) {
        int32_t t, h;

        if (sht30_measure(&t, &h) == 0) {
            LOG_INF("T = %d.%02d C, RH = %d.%02d %%",
                    t / 1000, abs(t % 1000) / 10,
                    h / 1000, abs(h % 1000) / 10);
        }

        k_msleep(SHT30_INTERVAL_MS);
    }

    return 0;
}
```
::: explain [Giải thích src/main.c]
`I2C_DT_SPEC_GET` gói con trỏ device của bus và địa chỉ slave vào một struct. Nhờ nó, mọi hàm `_dt` không cần truyền địa chỉ nữa và ta không thể lỡ tay gửi lệnh của cảm biến này tới địa chỉ của cảm biến khác.

Bộ hàm hay dùng:

| Hàm | Giao tiếp trên bus |
|---|---|
| `i2c_write_dt(&spec, buf, len)` | START + addr(W) + data + STOP |
| `i2c_read_dt(&spec, buf, len)` | START + addr(R) + data + STOP |
| `i2c_write_read_dt(&spec, wbuf, wlen, rbuf, rlen)` | START + addr(W) + wbuf + RESTART + addr(R) + rbuf + STOP |
| `i2c_reg_read_byte_dt(&spec, reg, &val)` | Đọc một thanh ghi 8 bit |
| `i2c_reg_write_byte_dt(&spec, reg, val)` | Ghi một thanh ghi 8 bit |
| `i2c_burst_read_dt(&spec, reg, buf, len)` | Đọc nhiều byte liên tiếp từ một thanh ghi |

Với cảm biến kiểu thanh ghi (MPU6050, BME280...) ta dùng `i2c_write_read_dt` vì chip trả lời ngay còn SHT30 thì cần tới 15 ms để đo xong $\rightarrow$ `sht30_measure()` ghi lệnh `0x2400` $\rightarrow$ `k_msleep(20)` $\rightarrow$ đọc 6 byte.

Cảm biến có hai chế độ:
- **Clock stretching** (lệnh `0x2C06`): SHT30 giữ SCL ở mức thấp cho tới khi đo xong. Cách này chỉ hoạt động nếu controller hỗ trợ và nhiều controller sẽ báo timeout.
- **Không clock stretching** (lệnh `0x2400`): SHT30 trả NACK nếu ta đọc quá sớm, master phải tự chờ.

Ta chọn cách thứ hai vì nó chạy được trên mọi controller, đổi lại phải tự `k_msleep(20)`. Con số 20 ms lấy từ datasheet (tối đa 15 ms cho độ phân giải cao) cộng biên an toàn.

SHT30 gửi về 6 byte gồm 2 byte nhiệt độ + 1 byte CRC, rồi 2 byte độ ẩm + 1 byte CRC. Bỏ qua CRC nghĩa là chấp nhận đọc ra số rác khi dây nhiễu, mà số rác vẫn nằm trong dải hợp lệ nên rất khó phát hiện. Zephyr có sẵn hàm `crc8()` trong `<zephyr/sys/crc.h>`, chỉ cần `CONFIG_CRC=y`

```c
crc8(data, 2, 0x31, 0xFF, false)   /* đa thức 0x31, init 0xFF, không reverse */
```

Công thức trong datasheet dùng số thực, nhưng in `float` trên embedded cần `CONFIG_CBPRINTF_FP_SUPPORT=y` và tốn thêm vài KB flash. Cách gọn hơn là nhân trước chia sau với đơn vị nhỏ hơn:

```c
*temp_mc = -45000 + (int32_t)((175000LL * raw_t) / 65535);
```

`175000LL` ép phép nhân sang 64 bit, tránh tràn khi `raw_t` gần 65535 (175000 × 65535 xấp xỉ 1.1×10^10, vượt xa giới hạn 32 bit). Kết quả là nhiệt độ tính theo milli-độ C, in ra bằng cách chia và lấy dư.

Chú ý `abs(t % 1000)` khi in: với nhiệt độ âm, phần dư cũng âm, không lấy trị tuyệt đối sẽ in ra dạng `-5.-30`.
:::
::::

### 5.2. Cách 2: dùng driver có sẵn qua Sensor API

Zephyr đã có driver cho SHT3x với `compatible = "sensirion,sht3xd"`. Dùng nó thì không phải viết binding, không phải nhớ mã lệnh và code ứng dụng giống hệt nhau cho mọi loại cảm biến.

:::: code-group
```conf [prj.conf]
CONFIG_I2C=y
CONFIG_SENSOR=y
CONFIG_LOG=y

CONFIG_SHT3XD=y
```
::: explain [Giải thích prj.conf]
- `CONFIG_SENSOR=y`: bắt buộc, đây là lớp API mà driver phụ thuộc vào.
- `CONFIG_SHT3XD=y`: thực ra là thừa. File `Kconfig` của driver khai báo:

    ```conf
    config SHT3XD
        bool "SHT3xD sensor"
        default y
        depends on DT_HAS_SENSIRION_SHT3XD_ENABLED
    ```
    
    Chỉ cần node có `status = "okay"` là symbol `DT_HAS_SENSIRION_SHT3XD_ENABLED` được sinh ra và driver tự động được build. Ghi ra `prj.conf` chỉ để người đọc dễ theo dõi.
:::

```dts [app.overlay]
&i2c1 {
    status = "okay";
    clock-frequency = <I2C_BITRATE_FAST>;

    sht30: sht3xd@44 {
        compatible = "sensirion,sht3xd";
        reg = <0x44>;
    };
};
```
::: explain [Giải thích app.overlay]
- Chỉ đổi `compatible` sang `sensirion,sht3xd` là dùng được binding và driver có sẵn trong cây nguồn Zephyr.
- Không còn thư mục `dts/bindings/` của ứng dụng.
:::

```c [src/main.c]
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void)
{
    const struct device *const dev = DEVICE_DT_GET(DT_NODELABEL(sht30));
    struct sensor_value temp, hum;
    int ret;

    if (!device_is_ready(dev)) {
        LOG_ERR("%s is not ready", dev->name);
        return -ENODEV;
    }

    while (1) {
        ret = sensor_sample_fetch(dev);
        if (ret < 0) {
            LOG_ERR("Failed to fetch sample: %d", ret);
            k_msleep(1000);
            continue;
        }

        sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
        sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &hum);

        LOG_INF("T = %d.%06d C, RH = %d.%06d %%",
                temp.val1, abs(temp.val2),
                hum.val1, abs(hum.val2));

        k_msleep(2000);
    }

    return 0;
}
```
::: explain [Giải thích src/main.c]
- `DEVICE_DT_GET`: lấy con trỏ `struct device` do driver tạo ra, khác hẳn `i2c_dt_spec` ở cách 1. Ở cách 1 node chỉ là dữ liệu và ứng dụng tự nói chuyện với chip; ở đây node sinh ra một device thật với bảng hàm API.
- `sensor_sample_fetch()`: driver đọc chip một lần và lưu vào vùng data của nó.
- `sensor_channel_get()`: lấy giá trị đã quy đổi theo từng kênh, không cần biết thanh ghi nào.
- `struct sensor_value` là số thực dạng hai phần, giá trị thật là `val1 + val2 / 1000000`:

    ```c
    struct sensor_value {
        int32_t val1;   /* phần nguyên */
        int32_t val2;   /* phần lẻ, đơn vị một-phần-triệu */
    };
    ```
    
    Ví dụ 25.375 độ C được biểu diễn thành `val1 = 25, val2 = 375000`. Cách này giữ độ chính xác cao mà không cần dấu phẩy động. Muốn đổi ra `double` thì dùng `sensor_value_to_double(&temp)`, nhưng in ra lại cần `CONFIG_CBPRINTF_FP_SUPPORT=y`.
    
    Với giá trị âm, cả `val1` và `val2` đều âm nên khi in phải lấy `abs(val2)`.
:::
::::

### 5.3. So sánh hai cách

| | Cách 1 | Cách 2 |
|---|---|---|
| Phải viết binding | Có | Không |
| Phải đọc datasheet | Toàn bộ | Không |
| Kiểm soát chi tiết (chế độ đo, heater...) | Đầy đủ | Chỉ những gì driver hỗ trợ |
| Đổi sang cảm biến khác | Viết lại toàn bộ | Đổi `compatible`, code C giữ nguyên |
| Kích thước firmware | Nhỏ hơn | Lớn hơn (kéo theo subsystem) |
| Hỗ trợ trigger/ngắt của chip | Tự làm | Có sẵn qua `sensor_trigger_set()` |

Nguyên tắc chọn: chip đã có driver trong Zephyr thì luôn dùng driver, kể cả khi chỉ cần một tính năng nhỏ. Chỉ tự viết khi chip chưa được hỗ trợ, và khi đó nên viết hẳn thành driver theo mẫu `DT_INST_FOREACH_STATUS_OKAY` ở bài devicetree thay vì để code nằm trong `main.c`.

## 6. PWM: điều chỉnh độ sáng LED

GPIO chỉ bật/tắt được LED. Muốn chỉnh độ sáng thì cần PWM, và đây cũng là ví dụ tốt để thấy overlay phải đụng tới cả timer lẫn pinctrl.

Trên F4VE, LED1 nằm ở PA6 và chân này cũng chính là TIM3_CH1, nên ta điều khiển được độ sáng LED có sẵn mà không cần nối thêm gì.

### 6.1. Toàn bộ project

:::: code-group
```conf [prj.conf]
CONFIG_PWM=y
CONFIG_LOG=y
```
::: explain [Giải thích prj.conf]
- `CONFIG_PWM=y`: bật lớp API PWM.
:::

```dts [app.overlay]
/ {
    pwmleds {
        compatible = "pwm-leds";

        pwm_led0: pwm_led_0 {
            pwms = <&pwm3 1 PWM_MSEC(1) PWM_POLARITY_INVERTED>;
            label = "PWM LED1";
        };
    };

    aliases {
        pwm-led0 = &pwm_led0;
    };
};

&timers3 {
    status = "okay";
    st,prescaler = <1000>;

    pwm3: pwm {
        status = "okay";
        pinctrl-0 = <&tim3_ch1_pa6>;
        pinctrl-names = "default";
    };
};
```
::: explain [Giải thích app.overlay]

Hiểu 4 cell trong property `pwms`:

```
pwms = <&pwm3 1 PWM_MSEC(1) PWM_POLARITY_INVERTED>
```

| Cell | Giá trị | Ý nghĩa |
|---|---|---|
| 1 | `&pwm3` | Phandle tới PWM controller (TIM3) |
| 2 | `1` | Số kênh, ở đây là TIM3 channel 1 |
| 3 | `PWM_MSEC(1)` | Chu kỳ, tính bằng nano giây (1 ms tương đương 1 kHz) |
| 4 | `PWM_POLARITY_INVERTED` | Đảo cực tính xung |

Tên các ô do `pwm-cells:` trong binding của node `pwm3` quy định, đúng cơ chế specifier cells ở bài devicetree.

Chu kỳ 1 ms đủ cao để mắt không thấy nhấp nháy. Với LED khoảng 500 Hz đến 20 kHz đều ổn, điều khiển động cơ thì phải cân nhắc kỹ hơn.

Vì sao là `PWM_POLARITY_INVERTED`? LED trên F4VE sáng khi chân ở mức thấp. Với cực tính thường, xung càng rộng thì thời gian ở mức cao càng nhiều, tức LED càng tối. Đảo cực tính để xung rộng = sáng hơn, đúng với vòng lặp trong `main()`. Board có LED active-high thì dùng `PWM_POLARITY_NORMAL`.

Trên STM32, PWM là một chức năng của timer nên overlay phải bật hai node:

```
&timers3 {
    status = "okay";         /* bật khối timer TIM3 */
    st,prescaler = <1000>;   /* chia clock đầu vào */

    pwm3: pwm {
        status = "okay";     /* bật chức năng PWM của timer đó */
        pinctrl-0 = <&tim3_ch1_pa6>;
        pinctrl-names = "default";
    };
};
```

Quên `status = "okay"` ở node `timers3` là lỗi kinh điển: build vẫn qua nhưng `pwm_is_ready_dt` trả về false hoặc tệ hơn là link error.

`st,prescaler` quyết định độ phân giải. TIM3 trên F407 nằm trên APB1 và chạy ở 84 MHz. Với prescaler 1000, tần số đếm còn 84 kHz, tức một chu kỳ 1 ms chỉ đếm được 84 nấc, vẫn đủ mượt cho mắt. Muốn nhiều mức sáng hơn thì giảm prescaler xuống (ví dụ <100> cho 840 nấc). Prescaler quá lớn thì độ sáng thay đổi giật cục, quá nhỏ thì không đạt được chu kỳ dài.

`pinctrl-0 = <&tim3_ch1_pa6>` nghĩa là đưa TIM3 kênh 1 đưa ra chân PA6. Tên này được định nghĩa sẵn trong file pinctrl của SoC. Không phải chân nào cũng nối được với kênh nào, bảng ánh xạ nằm trong datasheet của MCU (mục Alternate function mapping).

Vì PA6 vừa là `led0` (GPIO) vừa là `pwm_led0` (PWM) nên không được dùng ví dụ này cùng lúc với ví dụ mục 2: hai driver sẽ tranh nhau cấu hình chân. Muốn giữ cả hai thì chuyển PWM sang LED2 (PA7 = TIM3_CH2, `&tim3_ch2_pa7`, kênh `2`).
:::

```c [src/main.c]
#include <zephyr/kernel.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define PWM_LED_NODE DT_ALIAS(pwm_led0)

static const struct pwm_dt_spec pwm_led = PWM_DT_SPEC_GET(PWM_LED_NODE);

#define STEP_COUNT  50U
#define STEP_MS     30U

int main(void)
{
    uint32_t step;
    int ret;

    if (!pwm_is_ready_dt(&pwm_led)) {
        LOG_ERR("PWM controller is not ready");
        return -ENODEV;
    }

    LOG_INF("PWM period = %u ns", pwm_led.period);

    while (1) {
        /* Sáng dần */
        for (step = 0; step <= STEP_COUNT; step++) {
            ret = pwm_set_pulse_dt(&pwm_led,
                                   pwm_led.period * step / STEP_COUNT);
            if (ret < 0) {
                LOG_ERR("Failed to set pulse width: %d", ret);
                return ret;
            }
            k_msleep(STEP_MS);
        }

        /* Tối dần */
        for (step = STEP_COUNT; step > 0; step--) {
            pwm_set_pulse_dt(&pwm_led,
                             pwm_led.period * step / STEP_COUNT);
            k_msleep(STEP_MS);
        }
    }

    return 0;
}
```
::: explain [Giải thích src/main.c]
```c
struct pwm_dt_spec {
    const struct device *dev;
    uint32_t channel;
    uint32_t period;      /* lấy từ ô thứ 3 trong DTS */
    pwm_flags_t flags;
};
```

Vì `period` đã nằm trong spec nên ta chỉ cần đặt độ rộng xung:

```c
pwm_set_pulse_dt(&pwm_led, pwm_led.period * step / STEP_COUNT);
```

Viết `pwm_led.period * step / STEP_COUN`T (nhân trước chia sau) chứ không phải `pwm_led.period / STEP_COUNT * step` vì phép chia số nguyên làm mất phần lẻ, chia trước sẽ khiến các mức sáng bị dồn cục.

Muốn đổi cả chu kỳ lẫn độ rộng thì dùng `pwm_set_dt(&spec, period_ns, pulse_ns)`.

Mọi API PWM của Zephyr dùng nano giây. Các macro hỗ trợ:

```c
PWM_NSEC(n)     /* n nano giây  */
PWM_USEC(n)     /* n micro giây */
PWM_MSEC(n)     /* n mili giây  */
PWM_SEC(n)      /* n giây       */
PWM_HZ(f)       /* chu kỳ ứng với tần số f */
PWM_KHZ(f)
```

Ví dụ điều khiển servo chu kỳ 20 ms, độ rộng xung 1-2 ms:

```c
pwm_set_dt(&servo, PWM_MSEC(20), PWM_USEC(1500));   /* vị trí giữa */
```
:::
::::

### 6.2. Lỗi thường gặp

| Triệu chứng | Nguyên nhân |
|---|---|
| `pwm_is_ready_dt` false | Quên `status = "okay"` ở node timer cha |
| `pwm_set_pulse_dt` trả `-EINVAL` | Độ rộng xung lớn hơn chu kỳ, hoặc chu kỳ vượt khả năng của timer với prescaler hiện tại |
| LED sáng ngược (kéo lên lại tối đi) | Sai cực tính, đổi giữa `PWM_POLARITY_NORMAL` và `PWM_POLARITY_INVERTED` |
| LED sáng hết cỡ hoặc tắt hẳn, không chỉnh được | Chân bị driver GPIO chiếm (node `leds` vẫn đang dùng PA6), hoặc pinctrl trỏ sai kênh |
| Độ sáng chỉ có vài mức | `st,prescaler` quá lớn |
| LED nhấp nháy thấy được | Chu kỳ quá dài, giảm xuống dưới `PWM_MSEC(5)` |

## 7. ADC: đọc biến trở

ADC là ví dụ điển hình cho node `zephyr,user`: ta cần một kênh đo cho ứng dụng, nhưng nó không thuộc thiết bị chuẩn nào để có binding riêng.

Ví dụ dùng chân PA1 = ADC1_IN1 trên hàng chân của F4VE, nối với con chạy của một biến trở 10 kΩ (hai đầu còn lại nối 3V3 và GND). Tránh dùng PA0 vì chân đó đã là nút WK_UP của board.

### 7.1. Toàn bộ project

:::: code-group
```conf [prj.conf]
CONFIG_ADC=y
CONFIG_LOG=y
```
::: explain [Giải thích prj.conf]
- `CONFIG_ADC=y`: bật lớp API ADC.
:::

```dts [app.overlay]
/ {
    zephyr,user {
        io-channels = <&adc1 1>;
    };
};

&adc1 {
    status = "okay";
    pinctrl-0 = <&adc1_in1_pa1>;
    pinctrl-names = "default";

    #address-cells = <1>;
    #size-cells = <0>;

    channel@1 {
        reg = <1>;
        zephyr,gain = "ADC_GAIN_1";
        zephyr,reference = "ADC_REF_INTERNAL";
        zephyr,acquisition-time = <ADC_ACQ_TIME_DEFAULT>;
        zephyr,resolution = <12>;
    };
};
```
::: explain [Giải thích app.overlay]

Node `zephyr,user` là node đặc biệt của Zephyr, nó không cần `compatible` cũng không cần binding nhưng vẫn sinh macro cho mọi property. Đây là nơi đúng để khai báo kết nối phần cứng của riêng ứng dụng.

```dts
zephyr,user {
    io-channels = <&adc1 1>;
};
```

`io-channels` là property chuẩn để trỏ tới một kênh ADC: phandle tới controller, rồi số kênh. Muốn nhiều kênh thì liệt kê tiếp và đặt tên:

```
zephyr,user {
    io-channels = <&adc1 1>, <&adc1 2>;
    io-channel-names = "pot", "battery";
};

```

Rồi lấy từng cái bằng `ADC_DT_SPEC_GET_BY_NAME(ADC_NODE, pot)` hoặc `ADC_DT_SPEC_GET_BY_IDX(ADC_NODE, 1)`:

Khác với GPIO, ADC không dùng được ngay khi bật controller. Mỗi kênh phải có một node con mô tả cách lấy mẫu:

| Property | Ý nghĩa |
|---|---|
| `reg` | Số kênh phần cứng, phải match với số trong `io-channels` |
| `zephyr,gain` | Hệ số khuếch đại đầu vào, `ADC_GAIN_1` là không khuếch đại |
| `zephyr,reference` | Điện áp tham chiếu, `ADC_REF_INTERNAL` hoặc `ADC_REF_VDD_1` tuỳ SoC |
| `zephyr,acquisition-time` | Thời gian lấy mẫu, nguồn trở kháng cao cần giá trị lớn |
| `zephyr,resolution` | Số bit, ADC của STM32F407 tối đa 12 bit |
| `zephyr,oversampling` | Số lần lấy mẫu để lấy trung bình (tuỳ chọn), giúp giảm nhiễu |

Tên node `channel@1`, `reg = <1>` và số kênh trong `io-channels` là ba chỗ phải match nhau. Đây là ba chỗ rất dễ sửa sót khi đổi chân.

Hai dòng `#address-cells = <1>` và `#size-cells = <0>` là bắt buộc để node con dùng được `reg` làm địa chỉ. Thiếu chúng thì build báo lỗi về `reg` không hợp lệ.

`zephyr,acquisition-time` cần chú ý trong thực tế: biến trở 10 kΩ trở lên khiến tụ lấy mẫu nạp không kịp trong thời gian mặc định, kết quả đọc bị lệch và trôi. Gặp hiện tượng đó thì tăng giá trị này lên, ví dụ `<ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 10)>`.
:::

```c [src/main.c]
#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define ADC_NODE DT_PATH(zephyr_user)

static const struct adc_dt_spec adc_ch = ADC_DT_SPEC_GET(ADC_NODE);

int main(void)
{
    int16_t buf;
    struct adc_sequence seq = {
        .buffer      = &buf,
        .buffer_size = sizeof(buf),
    };
    int ret;

    if (!adc_is_ready_dt(&adc_ch)) {
        LOG_ERR("ADC controller is not ready");
        return -ENODEV;
    }

    ret = adc_channel_setup_dt(&adc_ch);
    if (ret < 0) {
        LOG_ERR("Failed to set up channel: %d", ret);
        return ret;
    }

    ret = adc_sequence_init_dt(&adc_ch, &seq);
    if (ret < 0) {
        return ret;
    }

    while (1) {
        ret = adc_read_dt(&adc_ch, &seq);
        if (ret < 0) {
            LOG_ERR("Failed to read ADC: %d", ret);
            k_msleep(1000);
            continue;
        }

        int32_t mv = buf;

        ret = adc_raw_to_millivolts_dt(&adc_ch, &mv);
        if (ret < 0) {
            LOG_WRN("Cannot convert to mV (vref is unknown)");
            LOG_INF("Raw value: %d", buf);
        } else {
            LOG_INF("Raw value: %d, voltage: %d mV", buf, mv);
        }

        k_msleep(500);
    }

    return 0;
}
```
::: explain [Giải thích src/main.c]
- `ADC_DT_SPEC_GET(DT_PATH(zephyr_user))`: lấy kênh đầu tiên trong danh sách `io-channels`.
- `adc_channel_setup_dt()`: nạp cấu hình kênh xuống phần cứng, chỉ cần gọi một lần lúc khởi tạo.
- `struct adc_sequence` mô tả một lần đọc. ADC API phức tạp hơn GPIO vì nó hỗ trợ đọc nhiều kênh liên tiếp vào một buffer, đọc theo DMA, đọc lặp... Với trường hợp đơn giản, `adc_sequence_init_dt()` điền sẵn `channels`, `resolution`, `oversampling` từ devicetree, ta chỉ phải đưa buffer.
- Buffer khai báo `int16_t` chứ không phải `uint16_t` vì cấu hình đo vi sai có thể cho giá trị âm.
- `adc_raw_to_millivolts_dt()`: Hàm này lấy `vref`, `gain` và `resolution` trong spec để quy đổi giá trị thô ra điện áp thật. Hàm sửa đổi giá trị tại chỗ nên phải copy `buf` sang `int32_t mv` trước khi gọi.
    Hàm trả lỗi khi driver không biết điện áp tham chiếu là bao nhiêu, khi đó khai báo thêm trong overlay:

    ```dts
    &adc1 {
        vref-mv = <3300>;
    };
    ```

- `k_msleep(500)` trong vòng lặp chỉ để ví dụ cho gọn. Trong ứng dụng thật nên dùng timer hoặc workqueue để không giữ thread main:

    ```c
    static void adc_work_handler(struct k_work *work)
    {
        /* đọc ADC ở đây */
        k_work_reschedule(k_work_delayable_from_work(work), K_MSEC(500));
    }
    static K_WORK_DELAYABLE_DEFINE(adc_work, adc_work_handler);
    ```
:::
::::
