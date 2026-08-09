Các topic trước đã trình bày mọi thứ dưới góc nhìn của người viết application: ta biết trước mình cần node nào và gọi thẳng `DT_NODELABEL(bme280)` cho đúng cái đó.

Khi viết driver thì ngược lại, ta không biết trước hệ thống sẽ gắn bao nhiêu con cảm biến, gắn ở bus nào, địa chỉ bao nhiêu... nên driver phải phục vụ được mọi node match `compatible` của nó, dù là 0, 1 hay 5 node.

Bài này trình bày cách viết một driver như vậy, theo mạch sau:
1. Driver gắn với node devicetree bằng cách nào? |
2. Nhiều node cùng loại thì phân biệt ra sao? |
3. Mỗi thiết bị lấy bộ nhớ riêng ở đâu? |
4. Gói tất cả lại thành một device object như thế nào? |
5. Làm sao lặp việc đó cho mọi node? |
6. Device object sinh ra có những gì bên trong? |
7. Nó được khởi tạo vào lúc nào trong quá trình boot? |
8. Trường hợp một driver phục vụ nhiều loại chip |
9. Tra cứu macro |

## Ví dụ

Toàn bộ bài dùng chung một ví dụ như sau: một board có ba node BME280, trong đó node thứ ba bị tắt.

```dts
&i2c1 {
    status = "okay";

    bme280_in: bme280@76 {          /* cảm biến trong nhà */
        compatible = "bosch,bme280";
        reg = <0x76>;
        label = "SENSOR_INDOOR";
        sampling-rate = <10>;
        int-gpios = <&gpioa 5 GPIO_ACTIVE_HIGH>;
        status = "okay";
    };
};

&i2c2 {
    status = "okay";

    bme280_out: bme280@77 {         /* cảm biến ngoài trời */
        compatible = "bosch,bme280";
        reg = <0x77>;
        sampling-rate = <1>;
        status = "okay";
    };
};

&i2c3 {
    bme280@76 {
        compatible = "bosch,bme280";
        reg = <0x76>;
        status = "disabled";
    };
};
```

Giả sử `i2c1` nằm ở địa chỉ `0x40005400` và `i2c2` ở `0x40005800`.

## 1. Driver match node device tree như thế nào?

Trước tiên, ta cần phải hiểu là làm thế nào để driver match được với node device tree?

Câu trả lời nằm ở macro `DT_DRV_COMPAT`. Macro này cho biết driver xử lý mọi node có compatible nào và nó phải nằm trước mọi include header `#include`.

Ví dụ:

```c
#define DT_DRV_COMPAT bosch_bme280

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
```

### 1.1. Quy tắc chuyển đổi tên compatible

```
DTS:    compatible = "bosch,bme280"
                        ↓
    lowercase + thay mọi ký tự không phải [a-z0-9] bằng '_'
                        ↓
Driver: #define DT_DRV_COMPAT bosch_bme280
```

Vài ví dụ thực tế trong source Zephyr:

```c
"nordic,nrf-uarte"     -> #define DT_DRV_COMPAT nordic_nrf_uarte
"st,stm32-usart"       -> #define DT_DRV_COMPAT st_stm32_usart
"bosch,bme280"         -> #define DT_DRV_COMPAT bosch_bme280
"espressif,esp32-uart" -> #define DT_DRV_COMPAT espressif_esp32_uart
```

### 1.2. Vì sao macro `DT_DRV_COMPAT` phải đứng trước `#include`?

Trong file `devicetree.h`, nó định nghĩa các macro `DT_INST_*` dựa trên `DT_DRV_COMPAT`:

```c
#define DT_INST(inst, compat)   UTIL_CAT(DT_N_INST, _##inst##_##compat)
#define DT_DRV_INST(inst)       DT_INST(inst, DT_DRV_COMPAT)

#define DT_INST_REG_ADDR(inst)  DT_REG_ADDR(DT_DRV_INST(inst))
#define DT_INST_PROP(inst, p)   DT_PROP(DT_DRV_INST(inst), p)
#define DT_INST_IRQN(inst)      DT_IRQN(DT_DRV_INST(inst))
```

Do đó, nếu đặt sau thì preprocessor sẽ báo lỗi kiểu `DT_DRV_COMPAT` undefined hoặc âm thầm sai.

Ta thấy các macro trên đều cần một tham số `inst` — đó là viết tắt của instance và ý nghĩa của nó được giải thích ngay sau đây.

> Một driver phục vụ nhiều compatible cùng lúc thì không dùng được `DT_DRV_COMPAT`. Trường hợp đó xem [mục 8](#8-khi-driver-không-có-dt_drv_compat).

## 2. Instance là gì?

Instance là thứ tự của các node có cùng compatible và có status `okay`, được script `gen_defines.py` đánh số 0, 1, 2, ...

Với devicetree ở ví dụ đầu bài, script sinh ra các macro sau trong file `devicetree_generated.h`:

```c
/* Chuẩn hoá tên compatible: "bosch,bme280" -> bosch_bme280 */

#define DT_N_INST_0_bosch_bme280    DT_N_S_soc_S_i2c_40005400_S_bme280_76
#define DT_N_INST_1_bosch_bme280    DT_N_S_soc_S_i2c_40005800_S_bme280_77
/* node trên i2c3 có status = "disabled" -> Không được đánh instance */

#define DT_N_INST_bosch_bme280_NUM_OKAY   2
#define DT_COMPAT_HAS_OKAY_bosch_bme280   1

/* Danh sách instance */
#define DT_FOREACH_OKAY_INST_bosch_bme280(fn)   fn(0) fn(1)
```

Nhìn vào file ta sẽ thấy:
- Node `bme280_in` thành instance 0
- Node `bme280_out` thành instance 1
- Node trên `i2c3` không có instance nào.

:::warning Lưu ý
Instance chỉ được đánh cho node `okay` và số thứ tự này không phải là một giá trị cố định: thêm/bớt/tắt một node trong devicetree là thứ tự có thể đổi. Vì vậy driver không bao giờ được hard code ý nghĩa cho từng số instance.
:::

Từ các macro này ta có thể lấy được property của node. Ví dụ với `DT_INST_REG_ADDR(0)`, nó bung ra từng lớp như sau:

```
DT_INST_REG_ADDR(0)
  -> DT_REG_ADDR( DT_DRV_INST(0) )
  -> DT_REG_ADDR( DT_INST(0, bosch_bme280) )                   <- DT_DRV_COMPAT được thay vào
  -> DT_REG_ADDR( DT_N_INST_0_bosch_bme280 )                   <- nối chuỗi bằng UTIL_CAT
  -> DT_REG_ADDR( DT_N_S_soc_S_i2c_40005400_S_bme280_76 )
  -> DT_N_S_soc_S_i2c_40005400_S_bme280_76_REG_IDX_0_VAL_ADDRESS
  -> 0x76
```

Tại đây, ta biết được instance số 0 có địa chỉ slave trên bus I2C là `0x76`, đúng bằng `reg` của node `bme280_in`.

## 3. Mỗi instance cần vùng config và data riêng

Một driver phục vụ nhiều thiết bị nên không được dùng chung biến toàn cục. Do đó, mỗi instance phải có bộ nhớ riêng.

Quy ước Zephyr tách làm hai:

- `config`: dữ liệu read only, biết từ lúc compile (địa chỉ bus, chân GPIO, tần số...). Đặt `const`, nằm ở flash.
- `data`: trạng thái thay đổi lúc chạy (giá trị đọc gần nhất, cờ, buffer...). Nằm ở RAM.

Với ví dụ của ta, driver khai báo:

```c
struct bme280_config {
    struct i2c_dt_spec  bus;            /* bus + địa chỉ slave */
    struct gpio_dt_spec int_gpio;       /* chân báo ngắt */
    uint16_t            sampling_rate;  /* Hz */
};

struct bme280_data {
    struct bme280_calib  calib;     /* hệ số calib đọc từ chip lúc init */
    int32_t              temp;      /* nhiệt độ, đơn vị 0.01 °C */
    uint32_t             press;     /* áp suất Pa */
    struct k_sem         lock;      /* bảo vệ truy cập từ nhiều thread */
    struct gpio_callback int_cb;    /* callback khi có ngắt */
};
```

Ranh giới giữa hai struct là câu hỏi: giá trị này có biết được lúc build không? Địa chỉ slave `0x76` thì biết, nên nó thuộc `config`. Còn hệ số calib thì mỗi con chip xuất xưởng sẽ có hệ số riêng ghi trong ROM của nó, chỉ đọc được lúc chạy, nên nó thuộc `data`.

Trong các hàm của driver, lấy chúng ra từ con trỏ `dev`:

```c
static int bme280_init(const struct device *dev)
{
    const struct bme280_config *cfg = dev->config;   /* vùng const */
    struct bme280_data *data = dev->data;            /* vùng RAM   */

    if (!i2c_is_ready_dt(&cfg->bus)) {
        return -ENODEV;
    }

    k_sem_init(&data->lock, 1, 1);

    return bme280_read_calib(dev);
}
```

Chi tiết về hai trường này nằm ở [mục 6.2](#62-trường-config) và [mục 6.3](#63-trường-data).

## 4. Đăng ký device với `DEVICE_DT_INST_DEFINE`

Ta viết một macro nhận `inst` rồi tạo đủ bộ config + data + đăng ký device cho instance đó.

Dấu `##inst` nối số thứ tự vào tên biến để mỗi instance có biến riêng:

```c
#define BME280_DEFINE(inst)                                                      \
    static const struct bme280_config bme280_config_##inst = {                   \
        .bus           = I2C_DT_SPEC_INST_GET(inst),                             \
        .int_gpio      = GPIO_DT_SPEC_INST_GET_OR(inst, int_gpios, {0}),         \
        .sampling_rate = DT_INST_PROP(inst, sampling_rate),                      \
    };                                                                           \
                                                                                 \
    static struct bme280_data bme280_data_##inst;                                \
                                                                                 \
    DEVICE_DT_INST_DEFINE(inst,                                                  \
                  bme280_init,                  /* hàm init      */              \
                  NULL,                         /* pm_device     */              \
                  &bme280_data_##inst,          /* con trỏ data  */              \
                  &bme280_config_##inst,        /* con trỏ config*/              \
                  POST_KERNEL,                  /* init level    */              \
                  CONFIG_SENSOR_INIT_PRIORITY,  /* độ ưu tiên    */              \
                  &bme280_api);                 /* bảng hàm API  */
```

`DEVICE_DT_INST_DEFINE` chính là thứ tạo ra `struct device` mà người dùng lấy bằng `DEVICE_DT_GET` ở bài **Device tree**.

Tham số thứ ba không phải là một hàm mà là con trỏ tới một object `struct pm_device`. Driver có hỗ trợ power management sẽ khai báo object đó rồi truyền vào:

```c
PM_DEVICE_DT_INST_DEFINE(inst, bme280_pm_action);   /* tạo object PM */

DEVICE_DT_INST_DEFINE(inst,
              bme280_init,
              PM_DEVICE_DT_INST_GET(inst),   /* thay cho NULL */
              ...);
```

Cặp `POST_KERNEL` + `CONFIG_SENSOR_INIT_PRIORITY` quyết định thứ tự khởi tạo và là nguyên nhân của rất nhiều lỗi khó hiểu lúc boot. Toàn bộ [mục 7](#7-thứ-tự-khởi-tạo-init-level-và-priority) dành cho hai tham số này.

## 5. Sinh device cho mọi instance

Macro ở [mục 4](#4-đăng-ký-device-với-device_dt_inst_define) mới chỉ tạo được một instance. Để áp nó lên mọi node match, đặt dòng sau ở cuối file driver:

```c
DT_INST_FOREACH_STATUS_OKAY(BME280_DEFINE)
```

Macro này duyệt qua mọi instance của `DT_DRV_COMPAT` có status `okay` và gọi `BME280_DEFINE(inst)` cho từng cái. Với ví dụ của ta, nó bung ra thành:

```c
BME280_DEFINE(0)
BME280_DEFINE(1)
```

Và mỗi lần gọi lại sinh ra một device object hoàn chỉnh. Với `BME280_DEFINE(0)` nó sẽ bung ra thành:

```c
static const struct bme280_config bme280_config_0 = {
    .bus           = { .bus = DEVICE_DT_GET(i2c1), .addr = 0x76 },
    .int_gpio      = { .port = DEVICE_DT_GET(gpioa), .pin = 5, .dt_flags = GPIO_ACTIVE_HIGH },
    .sampling_rate = 10,
};

static struct bme280_data bme280_data_0;
static struct device_state __devstate_dts_ord_47;

static const struct device __device_dts_ord_47
    __attribute__((__section__("._device.static.47_47")))
    = {
    .name   = "SENSOR_INDOOR",
    .config = &bme280_config_0,
    .data   = &bme280_data_0,
    .api    = &bme280_api,
    .state  = &__devstate_dts_ord_47,
    .ops    = { .init = bme280_init },
    .flags  = 0,
};

static const struct init_entry __init___device_dts_ord_47
    __attribute__((__section__(".z_init_POST_KERNEL_90_47_")))
    = {
    .init_fn = NULL,                      /* chỉ dùng cho SYS_INIT */
    .dev     = &__device_dts_ord_47,
};
```

Trong đó `90` là giá trị của `CONFIG_SENSOR_INIT_PRIORITY`, còn `47` là *ordinal* — một số nguyên do build system gán cho node, không liên quan gì tới số instance. Chi tiết về nó xem bài **Device tree**.

Điểm cần chú ý ở đây là hàm init không được gọi trực tiếp mà đi qua hai section khác nhau:

- `struct init_entry` nằm ở section `.z_init_<level>_<prio>_<ord>_`. Linker gom mọi entry vào một mảng liên tục và sắp xếp theo `(init_level, priority, ordinal)`. Lúc boot, kernel duyệt tuần tự mảng này mà không cần biết device nào là gì. Đây chính là cơ chế đứng sau hai tham số `POST_KERNEL` và `CONFIG_SENSOR_INIT_PRIORITY`.
- `struct device` nằm ở section riêng (`._device.*`) để kernel/shell duyệt được danh sách mọi device.

Với entry của một device, trường `init_fn` để `NULL` và kernel lấy hàm init từ `dev->ops.init` ([mục 6.6](#66-trường-ops)). Trường `init_fn` chỉ được dùng cho `SYS_INIT()` - loại init không gắn với device nào.

**Khi devicetree không có node nào match**

Macro `DT_INST_FOREACH_STATUS_OKAY` bung thành rỗng: driver vẫn biên dịch nhưng không tạo instance nào. Đây là lý do một driver có trong source nhưng `DEVICE_DT_GET` vẫn báo thiếu lúc link - đơn giản là chưa có node nào trong devicetree.

Để không biên dịch vô ích trong trường hợp đó, đầu file driver thường có một chốt chặn:

```c
#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
/* ... toàn bộ driver ... */
#endif
```

## 6. Bên trong `struct device`

[Mục 5](#5-sinh-device-cho-mọi-instance) đã cho thấy device object được sinh ra trông như thế nào. Mục này giải thích từng trường trong đó dùng để làm gì.

Định nghĩa của nó nằm ở `include/zephyr/device.h`:

```c
struct device {
    const char *name;               /* tên thiết bị */
    const void *config;             /* cấu hình read-only, do driver định nghĩa */
    const void *api;                /* bảng con trỏ hàm của subsystem */
    struct device_state *state;     /* trạng thái init, kernel ghi vào */
    void *data;                     /* dữ liệu runtime, do driver định nghĩa */
    struct device_ops ops;          /* con trỏ hàm init/deinit */
    device_flags_t flags;           /* cờ: deferred-init... */

#if defined(CONFIG_DEVICE_DEPS)
    device_handle_t *deps;          /* danh sách device mà nó phụ thuộc */
#endif
#if defined(CONFIG_PM_DEVICE)
    struct pm_device_base *pm_base; /* trạng thái power management */
#endif
#if defined(CONFIG_DEVICE_DT_METADATA)
    const struct device_dt_metadata *dt_meta;  /* nodelabel từ devicetree */
#endif
};
```

Bản thân `struct device` là `const` nằm trong flash. Nó không chứa dữ liệu thay đổi được mà chỉ chứa con trỏ tới những vùng thay đổi được nằm ở RAM. Nhờ vậy hàng chục device object gần như không tốn RAM, chỉ vùng data thật sự cần mới tốn.

```
        FLASH (const)                            RAM
 ┌──────────────────────────┐
 │ struct device            │
 │  .name  = "SENSOR_INDOOR"│
 │  .config  ───────────────┼──┐
 │  .api     ───────────────┼──┼──┐        ┌────────────────────┐
 │  .data    ───────────────┼──┼──┼───────>│ struct bme280_data │
 │  .state   ───────────────┼──┼──┼───┐    └────────────────────┘
 └──────────────────────────┘  │  │   │    ┌────────────────────┐
 ┌──────────────────────────┐  │  │   └───>│ struct device_state│
 │ struct bme280_config     │<─┘  │        │  .init_res    = 0  │
 │  .bus = { i2c1, 0x76 }   │     │        │  .initialized = 1  │
 └──────────────────────────┘     │        └────────────────────┘
 ┌──────────────────────────┐     │
 │ struct sensor_driver_api │<────┘
 │  .sample_fetch = ...     │
 │  .channel_get  = ...     │
 └──────────────────────────┘
```

Hai instance của ta mang giá trị như sau:

| Trường | Instance 0 (`bme280_in`) | Instance 1 (`bme280_out`) |
|---|---|---|
| `name` | `"SENSOR_INDOOR"` (từ `label`) | `"bme280@77"` (không có `label`) |
| `config` | `&bme280_config_0` | `&bme280_config_1` |
| `data` | `&bme280_data_0` | `&bme280_data_1` |
| `api` | `&bme280_api` | `&bme280_api` |
| `state` | `&__devstate_dts_ord_47` | `&__devstate_dts_ord_52` |
| `ops.init` | `bme280_init` | `bme280_init` |

Trong đó `api` và `ops.init` chỉ có một bản duy nhất trong flash, còn `config`, `data`, `state` thì mỗi instance một bản riêng.

### 6.1. Trường `name`

Chuỗi tên thiết bị, do macro `DEVICE_DT_NAME` quyết định:

```c
#define DEVICE_DT_NAME(node_id)  DT_PROP_OR(node_id, label, DT_NODE_FULL_NAME(node_id))
```

Nghĩa là lấy property `label` nếu node có, không thì lấy tên đầy đủ của node:

```c
DEVICE_DT_NAME(DT_NODELABEL(bme280_in))   /* -> "SENSOR_INDOOR"  (có label) */
DEVICE_DT_NAME(DT_NODELABEL(bme280_out))  /* -> "bme280@77"      (không có label) */
```

Trường `name` phục vụ hai việc. Thứ nhất là hiển thị trong log và shell:

```
uart:~$ device list
devices:
- i2c@40005400 (READY)
- SENSOR_INDOOR (READY)
- bme280@77 (READY)
```

Thứ hai là tra cứu device theo tên lúc runtime:

```c
const struct device *dev = device_get_binding("SENSOR_INDOOR");
```

`device_get_binding` duyệt tuần tự toàn bộ danh sách device rồi so sánh chuỗi nên chậm và không kiểm tra được lúc biên dịch. Gõ sai tên thì trả về `NULL` lúc chạy chứ không báo lỗi build:

```c
device_get_binding("SENSOR_INDOR");   /* thiếu chữ O -> NULL, build vẫn qua */
device_get_binding("bme280@76");      /* NULL, vì node này có label nên name là SENSOR_INDOOR */
```

Ngoài ra hàm này còn trả về `NULL` khi device tồn tại nhưng init thất bại nên `NULL` có hai nghĩa khác nhau và ta không phân biệt được.

Vì vậy trong code, ta nên dùng `DEVICE_DT_GET`, khi gõ sai nodelabel thì lỗi ngay lúc biên dịch:

```c
const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(bme280_in));   /* an toàn */
```

Chỉ dùng `device_get_binding` khi tên thiết bị chỉ biết được lúc chạy, ví dụ người dùng gõ vào shell.

### 6.2. Trường `config`

Con trỏ tới struct cấu hình riêng của từng driver, được đổ đầy lúc biên dịch bằng các macro `DT_INST_*` như đã thấy ở [mục 4](#4-đăng-ký-device-với-device_dt_inst_define). Đây là nơi chứa mọi thứ biết trước lúc build và không bao giờ đổi: địa chỉ bus, chân GPIO, số IRQ, tần số clock, các option lấy từ property.

Hai instance của ta cho ra hai biến const có giá trị khác nhau:

```c
/* instance 0 - bme280_in trên i2c1, có chân ngắt */
static const struct bme280_config bme280_config_0 = {
    .bus           = { .bus = DEVICE_DT_GET(i2c1), .addr = 0x76 },
    .int_gpio      = { .port = DEVICE_DT_GET(gpioa), .pin = 5, .dt_flags = GPIO_ACTIVE_HIGH },
    .sampling_rate = 10,
};

/* instance 1 - bme280_out trên i2c2, không có int-gpios nên lấy giá trị mặc định {0} */
static const struct bme280_config bme280_config_1 = {
    .bus           = { .bus = DEVICE_DT_GET(i2c2), .addr = 0x77 },
    .int_gpio      = { 0 },
    .sampling_rate = 1,
};
```

Vì kiểu khai báo trong `struct device` là `const void *` nên các hàm driver phải gán về đúng struct rồi mới dùng:

```c
static int bme280_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    const struct bme280_config *cfg = dev->config;
    uint8_t buf[6];

    return i2c_burst_read_dt(&cfg->bus, BME280_REG_PRESS_MSB, buf, sizeof(buf));
}
```

Cùng một dòng `i2c_burst_read_dt(&cfg->bus, ...)`:

- Gọi với `dev` của `bme280_in` thì nó đọc `0x76` trên `i2c1`
- Gọi với `dev` của `bme280_out` thì nó đọc `0x77` trên `i2c2`.

Driver không hề biết cũng không cần biết sự khác nhau đó.

Một ví dụ khác cho thấy `config` quyết định cả luồng chạy chứ không chỉ tham số. Trường `int_gpio` rỗng ở instance 1, driver dựa vào đó để bỏ qua phần cài đặt ngắt:

```c
static int bme280_init(const struct device *dev)
{
    const struct bme280_config *cfg = dev->config;

    if (!i2c_is_ready_dt(&cfg->bus)) {
        return -ENODEV;
    }

    /* chỉ instance nào khai báo int-gpios mới đi vào nhánh này */
    if (cfg->int_gpio.port != NULL) {
        gpio_pin_configure_dt(&cfg->int_gpio, GPIO_INPUT);
        gpio_pin_interrupt_configure_dt(&cfg->int_gpio, GPIO_INT_EDGE_TO_ACTIVE);
    }

    return bme280_read_calib(dev);
}
```

Do `config` nằm ở flash và là `const`, compiler biết chắc giá trị không đổi nên tối ưu rất mạnh. Nhánh `if (cfg->int_gpio.port != NULL)` ở trên có thể bị loại bỏ hoàn toàn ở instance 1 khi compiler thấy nó luôn sai.

### 6.3. Trường `data`

Con trỏ tới vùng RAM riêng của từng instance, chứa mọi trạng thái thay đổi lúc chạy: giá trị đo gần nhất, buffer DMA, semaphore, callback do application đăng ký, cờ trạng thái...

Khác với `config` được đổ đầy lúc build, `data` chỉ là một biến `static` chưa khởi tạo. Nội dung của nó do hàm init điền vào lúc boot:

```c
static struct bme280_data bme280_data_0;   /* nằm ở .bss, toàn số 0 lúc boot */

static int bme280_init(const struct device *dev)
{
    struct bme280_data *data = dev->data;

    k_sem_init(&data->lock, 1, 1);
    return bme280_read_calib(dev);   /* đọc hệ số calib từ chip vào data->calib */
}
```

Các hàm còn lại của driver đọc/ghi vùng này qua `dev->data`:

```c
static int bme280_channel_get(const struct device *dev,
                              enum sensor_channel chan,
                              struct sensor_value *val)
{
    struct bme280_data *data = dev->data;

    val->val1 = data->temp / 100;
    val->val2 = (data->temp % 100) * 10000;

    return 0;
}
```

Cặp `config`/`data` chính là thứ cho phép một bản code driver duy nhất phục vụ nhiều thiết bị. Hàm `bme280_sample_fetch` không hề có biến toàn cục nào, mọi thứ nó cần đều đi qua `dev`, nên gọi nó với `dev` khác nhau là thao tác trên phần cứng khác nhau.

Ta thấy rõ điều này khi application đọc cả hai cảm biến trong cùng một vòng lặp:

```c
static const struct device *const sensors[] = {
    DEVICE_DT_GET(DT_NODELABEL(bme280_in)),
    DEVICE_DT_GET(DT_NODELABEL(bme280_out)),
};

for (int i = 0; i < ARRAY_SIZE(sensors); i++) {
    sensor_sample_fetch(sensors[i]);
    sensor_channel_get(sensors[i], SENSOR_CHAN_AMBIENT_TEMP, &val);
    printk("%s: %d.%02d C\n", sensors[i]->name, val.val1, val.val2 / 10000);
}
```

Sau vòng lặp, RAM chứa hai bộ giá trị độc lập:

```
bme280_data_0 (SENSOR_INDOOR)      bme280_data_1 (bme280@77)
  .temp  = 2537   -> 25.37 C         .temp  = 3112   -> 31.12 C
  .press = 100821                    .press = 100795
  .calib = {hệ số của chip 0x76}     .calib = {hệ số của chip 0x77}
  .lock  = k_sem riêng               .lock  = k_sem riêng
```

Nếu driver lỡ dùng một biến `temp` toàn cục thay cho `data->temp`, hai cảm biến sẽ ghi đè lẫn nhau và cả hai cùng trả về giá trị của con nào vừa được `fetch` gần nhất. Đây là lỗi kinh điển khi chuyển code từ một dự án bare-metal chỉ có đúng một thiết bị sang Zephyr.

Tương tự, `data->lock` phải nằm trong `data` chứ không thể là semaphore toàn cục: hai cảm biến nằm trên hai bus khác nhau nên hoàn toàn có thể đọc song song từ hai thread, khoá chung chỉ làm chúng chờ nhau vô ích.

### 6.4. Trường `api`

Đây là con trỏ tới bảng hàm vtable của subsystem mà driver thuộc về. Mỗi subsystem định nghĩa sẵn một struct chuẩn, ví dụ với sensor:

```c
__subsystem struct sensor_driver_api {
    int (*attr_set)(const struct device *dev, ...);
    int (*sample_fetch)(const struct device *dev, enum sensor_channel chan);
    int (*channel_get)(const struct device *dev, ...);
    ...
};
```

Driver điền các hàm của mình vào đó qua macro `DEVICE_API`:

```c
static DEVICE_API(sensor, bme280_api) = {
    .sample_fetch = bme280_sample_fetch,
    .channel_get  = bme280_channel_get,
};
```

Và hàm inline public của subsystem chỉ là một lớp wrapper: lấy `api` ra rồi gọi xuyên qua con trỏ hàm.

```c
static inline int sensor_sample_fetch(const struct device *dev)
{
    return DEVICE_API_GET(sensor, dev)->sample_fetch(dev, SENSOR_CHAN_ALL);
}
```

Macro `DEVICE_API(sensor, bme280_api)` bung ra thành `const struct sensor_driver_api bme280_api` kèm thêm việc đặt bảng này vào một linker section riêng cho lớp `sensor`.

Linker gom mọi bảng cùng lớp nằm liền nhau:

```
section của lớp sensor
┌──────────────────┐ <- start
│ bme280_api       │
│ sht3xd_api       │
│ dht22_api        │
└──────────────────┘ <- end
```

Nhờ bố cục đó, chỉ cần so sánh địa chỉ là biết device có thuộc lớp không:

```c
/* -> dev->api có nằm trong khoảng [start, end) của section sensor không? */
DEVICE_API_IS(sensor, dev)
```

Và macro `DEVICE_API_GET(sensor, dev)` là ép kiểu `(const struct sensor_driver_api *)dev->api` và kèm một assert kiểm tra `dev` có đúng là sensor không khi bật `CONFIG_DEVICE_API_ASSERT=y`.

Đây là toàn bộ cơ chế đa hình của Zephyr. Application gọi `sensor_sample_fetch(dev)` mà không cần biết `dev` là BME280, SHT3X hay DHT22. Đổi cảm biến chỉ cần sửa devicetree, code application giữ nguyên.

Cụ thể, giả sử board có thêm một con SHT3X. Driver của nó là file khác, hàm khác nhưng điền vào cùng một kiểu struct:

```c
/* drivers/sensor/sht3xd/sht3xd.c */
static DEVICE_API(sensor, sht3xd_api) = {
    .sample_fetch = sht3xd_sample_fetch,
    .channel_get  = sht3xd_channel_get,
};
```

Application viết đúng một vòng lặp cho cả ba thiết bị:

```c
static const struct device *const sensors[] = {
    DEVICE_DT_GET(DT_NODELABEL(bme280_in)),
    DEVICE_DT_GET(DT_NODELABEL(bme280_out)),
    DEVICE_DT_GET(DT_NODELABEL(sht3xd_0)),
};

for (int i = 0; i < ARRAY_SIZE(sensors); i++) {
    sensor_sample_fetch(sensors[i]);
}
```

Và lời gọi hàm `sensor_sample_fetch` giống hệt nhau lại đi tới ba kết quả khác nhau:

```
sensor_sample_fetch(sensors[0])
  -> api = sensors[0]->api                 /* = &bme280_api  */
  -> api->sample_fetch(...)                /* -> bme280_sample_fetch()  đọc 0x76 trên i2c1 */

sensor_sample_fetch(sensors[1])
  -> api = sensors[1]->api                 /* = &bme280_api */
  -> api->sample_fetch(...)                /* -> bme280_sample_fetch()  đọc 0x77 trên i2c2 */

sensor_sample_fetch(sensors[2])
  -> api = sensors[2]->api                 /* = &sht3xd_api */
  -> api->sample_fetch(...)                /* -> sht3xd_sample_fetch()  driver hoàn toàn khác */
```

Hai lời gọi đầu cùng chung một hàm nhưng khác `dev` nên khác `config`/`data`. Lời gọi thứ ba vào một hàm khác hẳn. Application không phân biệt được ba trường hợp này và đó chính là mục đích.

Vì `api` chỉ là con trỏ tới một bảng nằm ở flash, mọi instance của cùng một driver dùng chung đúng một bảng. Thêm 10 con BME280 vào devicetree cũng không sinh thêm bản sao nào của `bme280_api`.

Driver không nhất thiết phải điền đủ mọi hàm trong struct. Trường không điền sẽ là `NULL` và subsystem thường kiểm tra trước khi gọi:

```c
static inline int sensor_attr_set(const struct device *dev, ...)
{
    const struct sensor_driver_api *api = DEVICE_API_GET(sensor, dev);

    if (api->attr_set == NULL) {
        return -ENOSYS;      /* driver không hỗ trợ đặt thuộc tính */
    }

    return api->attr_set(dev, ...);
}
```

Đây là lý do một số hàm của subsystem trả về `-ENOSYS` dù device hoàn toàn ready: đơn giản là driver đó không cài đặt tính năng tương ứng.

Cuối cùng, `api` hoàn toàn có thể là `NULL`. Những device không cung cấp dịch vụ gì cho lớp trên mà chỉ cần chạy một đoạn init lúc boot (bật regulator, nạp firmware cho chip khác, cấu hình pinmux) thì truyền `NULL` vào tham số cuối của `DEVICE_DT_INST_DEFINE`. Chúng vẫn là device đầy đủ: vẫn có `state`, vẫn nằm trong thứ tự init, vẫn được `deps` tham chiếu tới.

### 6.5. Trường `state`

Đây là trường duy nhất do kernel quản lý chứ không phải driver:

```c
struct device_state {
    uint8_t init_res;        /* mã lỗi mà hàm init trả về */
    bool initialized : 1;    /* hàm init đã được chạy chưa */
};
```

Luồng hoạt động lúc boot:

1. Kernel duyệt mảng `init_entry` theo thứ tự `(level, priority)`.
2. Với mỗi entry, gọi hàm init lấy từ `dev->ops.init`.
3. Ghi kết quả vào `dev->state`: đánh dấu `initialized = true` và lưu mã lỗi vào `init_res`.

Và `device_is_ready()` chỉ đơn giản là đọc hai trường này:

```c
bool z_device_is_ready(const struct device *dev)
{
    if (dev == NULL) {
        return false;
    }

    return dev->state->initialized && (dev->state->init_res == 0);
}
```

Điều này giải thích vì sao `DEVICE_DT_GET` luôn trả về con trỏ hợp lệ nhưng vẫn phải gọi `device_is_ready()`: con trỏ tồn tại từ lúc build còn việc phần cứng có phản hồi hay không thì chỉ biết sau khi hàm init chạy xong lúc boot.

Ví dụ cụ thể: ta rút dây con cảm biến ngoài trời rồi khởi động lại board. Devicetree không đổi, `DEVICE_DT_GET` vẫn cho con trỏ như cũ, nhưng `bme280_init` không nhận được ACK trên bus nên trả về `-EIO`:

```
[00:00:00.104] <err> bme280: bme280@77: khong doc duoc chip ID (-5)
```

Lúc này `state` của hai instance khác nhau:

```c
__devstate_dts_ord_47 = { .initialized = 1, .init_res = 0 }   /* SENSOR_INDOOR: ready      */
__devstate_dts_ord_52 = { .initialized = 1, .init_res = 5 }   /* bme280@77:     khong ready */
```

Chú ý `init_res = 5` chứ không phải `-5`: trường này là `uint8_t` nên kernel lưu giá trị tuyệt đối của mã lỗi. Hệ quả là ta chỉ biết có lỗi chứ không lấy lại được mã lỗi gốc từ `state`, muốn biết nguyên nhân phải xem log của driver.

Nhìn từ phía application:

```c
const struct device *out = DEVICE_DT_GET(DT_NODELABEL(bme280_out));

printk("%p\n", out);                  /* -> địa chỉ hợp lệ, không phải NULL */
printk("%d\n", device_is_ready(out)); /* -> 0 */

sensor_sample_fetch(out);             /* nếu bỏ qua kiểm tra: driver đọc bus lỗi,
                                       * data->temp giữ nguyên giá trị rác */
```

Còn shell hiển thị đúng khác biệt đó:

```
uart:~$ device list
devices:
- SENSOR_INDOOR (READY)
- bme280@77 (DISABLED)
```

Đây là lý do mọi ví dụ trong Zephyr đều mở đầu bằng `device_is_ready()`: nó là cách duy nhất để phân biệt thiết bị được định nghĩa trong firmware với thiết bị dùng được.

### 6.6. Trường `ops`

Chứa con trỏ tới hàm init và deinit nếu được bật của driver:

```c
struct device_ops {
    int (*init)(const struct device *dev);
#ifdef CONFIG_DEVICE_DEINIT_SUPPORT
    int (*deinit)(const struct device *dev);
#endif
};
```

Với ví dụ của ta, `ops` được điền từ tham số thứ hai của `DEVICE_DT_INST_DEFINE`:

```c
.ops = { .init = bme280_init },
```

Lúc boot, kernel duyệt mảng `init_entry` (mục 5), thấy `entry->dev != NULL` thì lấy hàm init ra từ đây:

```c
const struct device *dev = entry->dev;

if (dev != NULL) {
    int ret = dev->ops.init(dev);       /* gọi bme280_init(dev) */

    dev->state->init_res = (ret < 0) ? -ret : ret;
    dev->state->initialized = true;
}
```

Trường `deinit` chỉ tồn tại khi bật `CONFIG_DEVICE_DEINIT_SUPPORT`, dùng cho thiết bị cần gỡ bỏ hẳn lúc chạy (rút thẻ SD, tắt hoàn toàn một mạch ngoại vi để tiết kiệm điện). Driver muốn hỗ trợ thì dùng biến thể `*_DEINIT_*` của macro định nghĩa device:

```c
static int bme280_deinit(const struct device *dev)
{
    const struct bme280_config *cfg = dev->config;

    if (cfg->int_gpio.port != NULL) {
        gpio_pin_interrupt_configure_dt(&cfg->int_gpio, GPIO_INT_DISABLE);
    }

    return bme280_set_mode(dev, BME280_MODE_SLEEP);
}

DEVICE_DT_INST_DEINIT_DEFINE(inst, bme280_init, bme280_deinit, NULL,
                             &bme280_data_##inst, &bme280_config_##inst,
                             POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &bme280_api);
```

Application gọi `device_deinit(dev)` để chạy hàm này. Sau đó `state->initialized` về `false`, `device_is_ready()` trả về `false`, và muốn dùng lại thì phải gọi `device_init(dev)`.

:::warning Lưu ý
Đừng nhầm `deinit` với power management. `deinit` là gỡ bỏ hẳn, thiết bị trở về trạng thái như chưa từng được init. Còn suspend/resume ở phần `pm_base` bên dưới chỉ là tạm ngủ, mọi cấu hình và dữ liệu vẫn được giữ.
:::

### 6.7. Trường `flags`

Một byte cờ mô tả tính chất đặc biệt của device. Hiện chỉ có một cờ được định nghĩa:

```c
typedef uint8_t device_flags_t;

#define DEVICE_FLAG_INIT_DEFERRED  BIT(0)
```

Mặc định `flags = 0`, nghĩa là device được init tự động lúc boot theo đúng `(level, priority)`. Khi node devicetree khai báo property `zephyr,deferred-init`, build system bật cờ này lên và kernel sẽ bỏ qua device đó lúc boot:

```dts
bme280_out: bme280@77 {
    compatible = "bosch,bme280";
    reg = <0x77>;
    zephyr,deferred-init;      /* -> flags = DEVICE_FLAG_INIT_DEFERRED */
    status = "okay";
};
```

Application tự quyết định tại thời điểm init:

```c
const struct device *out = DEVICE_DT_GET(DT_NODELABEL(bme280_out));

printk("%d\n", device_is_ready(out));   /* -> 0, vì init chưa hề chạy */

/* ... cấp nguồn cho cảm biến ngoài trời, chờ nó ổn định ... */

int ret = device_init(out);             /* giờ mới gọi bme280_init() */
if (ret == 0) {
    /* device_is_ready(out) giờ trả về 1 */
}
```

Cơ chế này giải quyết mấy tình huống mà thứ tự init tự động không xử lý được: thiết bị cần nguồn ngoài chỉ bật sau khi application chạy, thiết bị cắm nóng hoặc thiết bị chậm mà ta không muốn nó kéo dài thời gian boot.

Điểm dễ nhầm: cờ này khác hẳn `status = "disabled"`. Node `disabled` thì không sinh ra device object nào cả, `DEVICE_DT_GET` lên nó gây lỗi lúc link. Còn node `deferred-init` vẫn có đủ device object, chỉ là chưa được init.

### 6.8. Trường `pm_base`

> Chỉ tồn tại khi bật `CONFIG_PM_DEVICE`.

Trỏ tới object `struct pm_device` chứa state machine power management của thiết bị: trạng thái hiện tại (`ACTIVE`, `SUSPENDED`, `OFF`), bộ đếm tham chiếu cho runtime PM và con trỏ tới hàm *action callback* mà driver cung cấp:

```c
static int bme280_pm_action(const struct device *dev, enum pm_device_action action)
{
    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        return bme280_set_mode(dev, BME280_MODE_SLEEP);
    case PM_DEVICE_ACTION_RESUME:
        return bme280_set_mode(dev, BME280_MODE_NORMAL);
    default:
        return -ENOTSUP;
    }
}

PM_DEVICE_DT_INST_DEFINE(inst, bme280_pm_action);
```

Khi application gọi `pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND)`, subsystem PM đọc `dev->pm_base`, kiểm tra chuyển trạng thái có hợp lệ không, rồi mới gọi callback của driver:

```c
const struct device *in = DEVICE_DT_GET(DT_NODELABEL(bme280_in));

pm_device_action_run(in, PM_DEVICE_ACTION_SUSPEND);
  /* -> PM đọc in->pm_base, thấy trạng thái hiện tại là ACTIVE
   *    -> hợp lệ, gọi bme280_pm_action(in, PM_DEVICE_ACTION_SUSPEND)
   *    -> chip vào chế độ sleep, ghi trạng thái mới SUSPENDED vào pm_base   */

pm_device_action_run(in, PM_DEVICE_ACTION_SUSPEND);
  /* -> PM thấy đã SUSPENDED rồi -> trả về -EALREADY, KHÔNG gọi driver */
```

Việc chặn ở tầng PM như trên có nghĩa driver không phải tự theo dõi trạng thái của mình: `bme280_pm_action` chỉ cần biết cách thực hiện một bước chuyển, còn "có được phép chuyển hay không" đã có `pm_base` lo.

Trong thực tế ta hiếm khi gọi `pm_device_action_run` trực tiếp mà dùng runtime PM — cơ chế đếm tham chiếu để thiết bị tự ngủ khi không ai dùng:

```c
pm_device_runtime_enable(in);       /* thường gọi trong bme280_init */

/* mỗi lần cần đo */
pm_device_runtime_get(in);          /* đếm 0 -> 1, PM tự gọi RESUME */
sensor_sample_fetch(in);
sensor_channel_get(in, SENSOR_CHAN_AMBIENT_TEMP, &val);
pm_device_runtime_put(in);          /* đếm 1 -> 0, PM tự gọi SUSPEND */
```

Bộ đếm này nằm trong `pm_base`. Nếu hai thread cùng `get` thì đếm lên 2 và thiết bị chỉ ngủ khi cả hai đã `put`, nên không thread nào tắt nhầm thiết bị mà thread kia đang dùng.

Nếu truyền `NULL` như ví dụ ở [mục 4](#4-đăng-ký-device-với-device_dt_inst_define) thì `pm_base` bằng `NULL`, thiết bị không tham gia PM và mọi lời gọi `pm_device_*` lên nó đều bị từ chối:

```c
pm_device_action_run(out, PM_DEVICE_ACTION_SUSPEND);   /* -> -ENOSYS */
```

### 6.9. Trường `deps`

> Chỉ tồn tại khi bật `CONFIG_DEVICE_DEPS`.

Mảng ordinal của những device mà thiết bị này phụ thuộc, suy ra tự động từ cấu trúc devicetree: node cha, bus mà nó nằm trên, các phandle nó trỏ tới (GPIO controller, clock, regulator...).

Ta không viết gì cả, build system tự đọc ra quan hệ này từ DTS. Với `bme280_in`:

```dts
&i2c1 {                                  /* <- node cha, cũng là bus */
    bme280_in: bme280@76 {
        int-gpios = <&gpioa 5 ...>;      /* <- phandle tới gpioa */
    };
};
```

Nó sinh ra:

```c
/* deps của bme280_in: i2c1 (ordinal 12) và gpioa (ordinal 8) */
static const device_handle_t __devicedeps_dts_ord_47[] = { 12, 8, DEVICE_HANDLE_ENDS };

/* deps của bme280_out: chỉ có i2c2, vì node này không dùng gpio */
static const device_handle_t __devicedeps_dts_ord_52[] = { 15, DEVICE_HANDLE_ENDS };
```

Quan hệ này được dùng theo cả hai chiều. Chiều xuôi là tôi cần những ai (`device_required_handles_get`), chiều ngược là những ai cần tôi (`device_supported_foreach`) — kernel dựng được chiều ngược bằng cách quét toàn bộ bảng `deps`.

Nhờ vậy subsystem PM biết không được suspend `i2c1` khi `bme280_in` còn active và power domain biết phải bật `i2c1` với `gpioa` trước rồi mới resume được cảm biến. Nếu không có thông tin này, thứ tự tắt/bật phải hard-code trong application.

:::warning Lưu ý
`deps` **không** được dùng để sắp thứ tự init lúc boot. Việc đó hoàn toàn do cặp (init level, priority) ở [mục 7](#7-thứ-tự-khởi-tạo-init-level-và-priority) quyết định.
:::

### 6.10. Trường `dt_meta`

> Chỉ tồn tại khi bật `CONFIG_DEVICE_DT_METADATA`.

Lưu danh sách nodelabel của node dưới dạng chuỗi, cho phép tra device theo nodelabel lúc runtime:

```c
const struct device *dev = device_get_by_dt_nodelabel("bme280_in");
```

Đây là cách tra cứu động đáng tin hơn `device_get_binding` vì nodelabel do người viết devicetree đặt và không phụ thuộc property `label`. Với ví dụ của ta, cả hai cách sau đều trỏ tới cùng một device:

```c
device_get_by_dt_nodelabel("bme280_in");   /* theo nodelabel  */
device_get_binding("SENSOR_INDOOR");       /* theo dev->name  */
```

Một node có thể mang nhiều nodelabel, khi đó tra bằng cái nào cũng được:

```dts
bme280_in: sensor0: bme280@76 { ... };
```

```c
device_get_by_dt_nodelabel("bme280_in");   /* cùng kết quả */
device_get_by_dt_nodelabel("sensor0");
```

Mặc định `CONFIG_DEVICE_DT_METADATA` tắt vì phải nhét mọi chuỗi nodelabel vào flash. Chỉ bật khi thật sự cần tra cứu động — chủ yếu là lệnh shell và script test, nơi tên thiết bị được gõ vào lúc chạy. Code bình thường luôn dùng `DEVICE_DT_GET(DT_NODELABEL(bme280_in))` để có kiểm tra lúc biên dịch và không tốn gì lúc chạy.

### 6.11. Tóm tắt

| Trường | Nằm ở | Ai ghi | Riêng/chung giữa các instance | Vai trò |
|---|---|---|---|---|
| `name` | flash | build system | riêng | Tên hiển thị, tra cứu bằng `device_get_binding` |
| `config` | flash | driver (giá trị từ devicetree) | riêng | Tham số cố định: địa chỉ, pin, tần số |
| `data` | RAM | driver (lúc chạy) | riêng | Trạng thái runtime riêng mỗi instance |
| `api` | flash | driver | chung | vtable để subsystem gọi xuống driver |
| `state` | RAM | kernel | riêng | Kết quả init, nguồn của `device_is_ready()` |
| `ops` | flash | driver | chung | Hàm init/deinit của driver |
| `flags` | flash | build system | riêng | Cờ `DEVICE_FLAG_INIT_DEFERRED` |
| `pm_base` | RAM | subsystem PM | riêng | State machine power management |
| `deps` | flash | build system | riêng | Quan hệ phụ thuộc suy từ devicetree |
| `dt_meta` | flash | build system | riêng | Nodelabel để tra cứu lúc runtime |

## 7. Thứ tự khởi tạo: init level và priority

Quay lại hai tham số đã tạm gác ở [mục 4](#4-đăng-ký-device-với-device_dt_inst_define). Chúng quyết định hàm init của device chạy vào lúc nào trong quá trình boot, và đặt sai là nguyên nhân của nhiều lỗi chỉ xuất hiện lúc chạy.

- [7.1. Init level](#71-init-level) — chạy ở giai đoạn nào của boot
- [7.2. Độ ưu tiên](#72-độ-ưu-tiên) — thứ tự bên trong một giai đoạn
- [7.3. Tổng kết hai tham số](#73-tổng-kết-hai-tham-số)

### 7.1. Init level

Init level trả lời câu hỏi: hàm init của device này chạy ở giai đoạn nào của quá trình boot. Có 6 level, chạy theo đúng thứ tự sau:

```
reset
  └─> z_cstart()
        ├─ z_sys_init_run_level(EARLY)             <───── level EARLY
        ├─ arch_kernel_init()                             (khởi tạo kiến trúc)
        ├─ z_device_state_init()
        ├─ soc_early_init_hook() / board_early_init_hook()
        ├─ z_sys_init_run_level(PRE_KERNEL_1)      <───── level PRE_KERNEL_1
        ├─ z_sys_init_run_level(PRE_KERNEL_2)      <───── level PRE_KERNEL_2
        ├─ khởi tạo đối tượng kernel, scheduler
        │
        └─> chuyển sang thread main -> bg_thread_main()
              ├─ z_sys_init_run_level(POST_KERNEL)   <─── level POST_KERNEL
              ├─ soc_late_init_hook() / board_late_init_hook()
              ├─ z_sys_init_run_level(APPLICATION)   <─── level APPLICATION
              ├─ z_init_static_threads()                  (thread K_THREAD_DEFINE bắt đầu chạy)
              ├─ z_smp_init()
              ├─ z_sys_init_run_level(SMP)           <─── level SMP
              └─ main()
```

| Level | Chạy khi nào | Ngữ cảnh | Dành cho |
|---|---|---|---|
| `EARLY` | Ngay dòng đầu của `z_cstart()`, trước cả khởi tạo kiến trúc | Chưa có gì cả | Code arch/SoC bắt buộc phải chạy trước mọi thứ |
| `PRE_KERNEL_1` | Sau khi arch init xong | Interrupt stack, chưa có kernel | Thiết bị không phụ thuộc ai: clock control, pinctrl, interrupt controller, GPIO, UART console |
| `PRE_KERNEL_2` | Ngay sau `PRE_KERNEL_1` | Như trên | Thiết bị phụ thuộc vào thiết bị ở `PRE_KERNEL_1` |
| `POST_KERNEL` | Trong thread main, kernel đã chạy | Thread đầy đủ | Đa số driver: bus I2C/SPI, cảm biến, màn hình |
| `APPLICATION` | Cuối cùng, ngay trước `main()` | Thread đầy đủ | Khởi tạo mức ứng dụng |
| `SMP` | Sau khi các core phụ đã lên | Thread đầy đủ | Chỉ có khi bật `CONFIG_SMP` |

**Ranh giới quan trọng nhất nằm giữa `PRE_KERNEL_2` và `POST_KERNEL`.**

Ba level đầu chạy khi kernel chưa sống: chưa có scheduler, chưa có thread, và code chạy trên interrupt stack. Trong ba level đó, hàm init không được:

- Gọi bất kỳ API nào có thể block: `k_sleep()`, `k_msleep()`, `k_sem_take()` với timeout khác `K_NO_WAIT`, `k_mutex_lock()`.
- Dùng system workqueue (`k_work_submit()`).
- Dùng nhiều stack, vì interrupt stack thường nhỏ.

Muốn chờ một khoảng thời gian thì phải busy-wait bằng `k_busy_wait(usec)` — hàm này quay vòng đếm chứ không nhường CPU nên dùng được ở mọi ngữ cảnh.

Từ `POST_KERNEL` trở đi thì mọi API kernel đều dùng được bình thường.

**Vì sao BME280 phải là `POST_KERNEL`**

Hàm `bme280_init()` gọi `i2c_burst_read_dt()` để đọc hệ số calib. Driver I2C bên dưới dùng ngắt báo truyền xong rồi `k_sem_take()` để chờ. Nếu đặt cảm biến ở `PRE_KERNEL_1`, lời gọi `k_sem_take()` đó xảy ra khi chưa có scheduler — hệ thống treo hoặc assert ngay lúc boot, và log thường chưa kịp in ra gì.

**Vì sao UART console lại là `PRE_KERNEL_1`**

Ngược lại, driver UART dùng cho console phải lên thật sớm để `printk()` và log hoạt động được ngay từ những dòng đầu của boot, kể cả để in ra lỗi của chính quá trình init phía sau. Vì vậy nó chỉ ghi thanh ghi trực tiếp, không chờ ngắt, không đụng tới kernel service nào.

Đối chiếu vài driver thật trong cây nguồn Zephyr:

| Driver | Level |
|---|---|
| `clock_stm32_ll_common.c` (clock control) | `PRE_KERNEL_1` |
| `uart_stm32.c` (UART) | `PRE_KERNEL_1` |
| `gpio_nrfx.c` (GPIO controller) | `PRE_KERNEL_1` |
| `i2c_nrfx_twim.c` (bus I2C) | `POST_KERNEL` |
| `spi_nrfx_spim.c` (bus SPI) | `POST_KERNEL` |
| `bme280.c` (cảm biến) | `POST_KERNEL` |

Quy tắc rút ra: thiết bị càng gần phần cứng thô, càng ít phụ thuộc thì level càng sớm. Thứ tự thông thường là clock/pinctrl $\rightarrow$ GPIO/UART $\rightarrow$ bus $\rightarrow$ thiết bị trên bus.

### 7.2. Độ ưu tiên

Level chỉ chia boot thành 6 giai đoạn lớn. Trong cùng một level, thứ tự do priority quyết định: một số nguyên từ 0 đến 999, số nhỏ chạy trước.

Đây không phải priority theo nghĩa của scheduler, nó chỉ là khoá sắp xếp lúc link. Như đã thấy ở [mục 5](#5-sinh-device-cho-mọi-instance), mỗi `init_entry` nằm trong section tên `.z_init_<LEVEL>_<PRIO>_<ORD>_` và linker sắp xếp theo chính cái tên đó. Vì vậy thứ tự cuối cùng là bộ ba:

```
(init level, priority, devicetree ordinal)
```

Ordinal chỉ đóng vai trò phá hoà khi hai device trùng cả level lẫn priority. Đừng bao giờ phụ thuộc vào nó: ordinal đổi khi devicetree đổi.

**Các hằng số Kconfig mặc định**

Zephyr định nghĩa sẵn một thang priority chung trong `kernel/Kconfig.device`:

| Kconfig | Giá trị | Dành cho |
|---|---|---|
| `CONFIG_KERNEL_INIT_PRIORITY_OBJECTS` | 30 | Đối tượng kernel |
| `CONFIG_KERNEL_INIT_PRIORITY_LIBC` | 35 | Khởi tạo libc |
| `CONFIG_KERNEL_INIT_PRIORITY_DEFAULT` | 40 | Mức tối thiểu mặc định của mỗi level |
| `CONFIG_KERNEL_INIT_PRIORITY_DEVICE` | 50 | Driver phụ thuộc thành phần chung (interrupt controller...) nhưng không phụ thuộc device khác |
| `CONFIG_APPLICATION_INIT_PRIORITY` | 90 | Driver mức ứng dụng, không có ai phụ thuộc vào nó |

Mỗi subsystem lại có hằng riêng, phần lớn trỏ về thang trên:

| Kconfig của subsystem | Giá trị thực tế |
|---|---|
| `CONFIG_CLOCK_CONTROL_INIT_PRIORITY` | 30 (`= KERNEL_INIT_PRIORITY_OBJECTS`) |
| `CONFIG_GPIO_INIT_PRIORITY` | 40 (`= KERNEL_INIT_PRIORITY_DEFAULT`) |
| `CONFIG_I2C_INIT_PRIORITY` | 50 (`= KERNEL_INIT_PRIORITY_DEVICE`) |
| `CONFIG_SPI_INIT_PRIORITY` | 50 (`= KERNEL_INIT_PRIORITY_DEVICE`) |
| `CONFIG_SERIAL_INIT_PRIORITY` | 50 (`= KERNEL_INIT_PRIORITY_DEVICE`) |
| `CONFIG_MFD_INIT_PRIORITY` | 80 |
| `CONFIG_DISPLAY_INIT_PRIORITY` | 85 |
| `CONFIG_SENSOR_INIT_PRIORITY` | 90 |

**Với ví dụ BME280**

Bus I2C và cảm biến cùng nằm ở `POST_KERNEL`, chỉ khác priority:

```
POST_KERNEL, 50   -> i2c1, i2c2            (CONFIG_I2C_INIT_PRIORITY)
POST_KERNEL, 90   -> bme280_in, bme280_out (CONFIG_SENSOR_INIT_PRIORITY)
```

50 nhỏ hơn 90 nên hai bus lên trước, và đó chính là lý do `i2c_is_ready_dt(&cfg->bus)` trong `bme280_init()` trả về `true`.

**Lỗi kinh điển**

Giả sử ta thấy cảm biến lên hơi chậm và tự ý sửa thành số trần cho "sớm hơn":

```c
DEVICE_DT_INST_DEFINE(inst, bme280_init, NULL,
              &bme280_data_##inst, &bme280_config_##inst,
              POST_KERNEL,
              40,              /* thay vì CONFIG_SENSOR_INIT_PRIORITY */
              &bme280_api);
```

40 nhỏ hơn 50, nên cảm biến init trước cả bus I2C. Kết quả:

```
[00:00:00.002] <err> bme280: bme280@76: bus khong san sang
```

`bme280_init()` trả về `-ENODEV`, `state->init_res` khác 0, và mọi lời gọi `device_is_ready()` sau này trả về `false` dù phần cứng hoàn toàn bình thường.

:::warning Lưu ý
Luôn dùng hằng Kconfig thay vì gõ số trần. Hằng Kconfig giữ cho driver của ta đi theo khi thang priority chung thay đổi, và cho phép người dùng chỉnh từ `prj.conf` mà không phải sửa code:

```
CONFIG_SENSOR_INIT_PRIORITY=95
```
:::

**Devicetree không tự sắp thứ tự init**

Đây là điểm hay bị hiểu nhầm. Ở [mục 6.9](#69-trường-deps) ta thấy trường `deps` ghi lại quan hệ phụ thuộc suy ra từ devicetree — cảm biến phụ thuộc bus I2C. Nhưng Zephyr **không** dùng thông tin đó để tự sắp xếp thứ tự init lúc boot. `deps` chỉ phục vụ power management và power domain.

Nói cách khác, việc "bus phải lên trước cảm biến" hoàn toàn do người viết driver đặt đúng cặp (level, priority). Đặt sai thì build vẫn qua, devicetree vẫn hợp lệ, và lỗi chỉ xuất hiện lúc chạy.

**Cách kiểm chứng thứ tự thật**

Mở `build/zephyr/zephyr.map` và tìm các section `.z_init_`. Chúng nằm đúng theo thứ tự sẽ được gọi lúc boot:

```
.z_init_POST_KERNEL_50_12_   0x0800a1c0   0x8   .../libdrivers__i2c.a(i2c_nrfx_twim.c.obj)
.z_init_POST_KERNEL_90_47_   0x0800a1c8   0x8   .../libdrivers__sensor.a(bme280.c.obj)
.z_init_POST_KERNEL_90_52_   0x0800a1d0   0x8   .../libdrivers__sensor.a(bme280.c.obj)
```

Đọc được ngay: I2C (priority 50) trước, rồi tới hai instance BME280 (priority 90, ordinal 47 và 52).

### 7.3. Tổng kết hai tham số

| | Init level | Priority |
|---|---|---|
| Kiểu | Một trong 6 tên cố định | Số nguyên 0–999 |
| Trả lời câu hỏi | Chạy ở *giai đoạn nào* của boot | Trong giai đoạn đó thì *trước hay sau* ai |
| Ràng buộc | Trước `POST_KERNEL` thì không được block | Không có, chỉ là khoá sắp xếp |
| Chọn thế nào | Càng ít phụ thuộc, càng đặt sớm | Dùng hằng Kconfig của subsystem |

## 8. Khi driver không có `DT_DRV_COMPAT`

Đọc source Zephyr ta sẽ gặp không ít driver hoàn toàn không có dòng `#define DT_DRV_COMPAT` nào, ví dụ họ driver màn hình ILI9xxx. Chúng vẫn match được node devicetree bình thường.

Lý do là macro `DT_DRV_COMPAT` không bắt buộc. Nó chỉ là một cách đơn giản để viết ngắn hơn: mọi macro `DT_INST_*` đều là bản rút gọn của một macro đầy đủ, trong đó compatible được truyền trực tiếp thay vì lấy ngầm từ `DT_DRV_COMPAT`.

| Có `DT_DRV_COMPAT` | Bản đầy đủ tương ứng |
|---|---|
| `DT_DRV_INST(inst)` | `DT_INST(inst, compat)` |
| `DT_INST_PROP(inst, p)` | `DT_PROP(node_id, p)` |
| `DT_INST_REG_ADDR(inst)` | `DT_REG_ADDR(node_id)` |
| `DEVICE_DT_INST_DEFINE(inst, ...)` | `DEVICE_DT_DEFINE(node_id, ...)` |
| `DT_INST_FOREACH_STATUS_OKAY(fn)` | `DT_FOREACH_STATUS_OKAY(compat, fn)` |
| `DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)` | `DT_HAS_COMPAT_STATUS_OKAY(compat)` |

Chú ý khác biệt ở cột phải: bản đầy đủ nhận **node identifier** chứ không phải số instance, và `DT_FOREACH_STATUS_OKAY` cũng truyền `node_id` cho `fn` chứ không phải `inst`.

Lý do khiến một driver phải bỏ `DT_DRV_COMPAT` là nó phục vụ nhiều compatible.

Ví dụ file `display_ili9xxx.c` phục vụ cùng lúc 5 compatible: `ilitek,ili9163c`, `ilitek,ili9340`, `ilitek,ili9341`, `ilitek,ili9342c`, `ilitek,ili9488`. Các chip này khác nhau ở bảng thanh ghi khởi tạo và độ phân giải còn luồng vẽ thì giống hệt.

`DT_DRV_COMPAT` chỉ mang được đúng một giá trị tại một thời điểm nên nó không dùng được ở đây. Cách viết thay thế:

```c
#define MYCHIP_DEFINE(node_id)                                      \
    static const struct mychip_config mychip_config_##node_id = {   \
        .bus = I2C_DT_SPEC_GET(node_id),                            \
    };                                                              \
    static struct mychip_data mychip_data_##node_id;                \
    DEVICE_DT_DEFINE(node_id, mychip_init, NULL,                    \
             &mychip_data_##node_id, &mychip_config_##node_id,      \
             POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &mychip_api);

DT_FOREACH_STATUS_OKAY(vendor_chip_a, MYCHIP_DEFINE)
DT_FOREACH_STATUS_OKAY(vendor_chip_b, MYCHIP_DEFINE)
```

Chú ý macro nhận `node_id` chứ không phải `inst`, nên bên trong dùng `I2C_DT_SPEC_GET` / `DT_PROP` chứ không phải bản `_INST_`.

:::warning Lưu ý
Trong file không có `DT_DRV_COMPAT` mà lỡ dùng một macro `DT_INST_*`, lỗi báo ra rất khó hiểu vì preprocessor nối chuỗi với một token rỗng rồi mới thất bại. Gặp lỗi kiểu `DT_N_INST_0__P_reg undeclared` thì hãy kiểm tra ngay xem file có thiếu `DT_DRV_COMPAT` không, hoặc có phải ta đang dùng nhầm bản `_INST_` trong driver viết theo `node_id` không.
:::

Ngoài ra, một file không có `DT_DRV_COMPAT` cũng có thể đơn giản là **không định nghĩa device nào**. Họ ILI9xxx chia thành nhiều file, trong đó `display_ili9341.c` chỉ chứa hàm nạp thanh ghi riêng của chip và nhận `dev` do file khác truyền vào, còn phần định nghĩa device nằm ở `display_ili9xxx.c` dùng chung cho cả họ.

Muốn biết một driver match compatible nào, hãy tìm file có `DEVICE_DT_*_DEFINE`.

## 9. Bảng macro `DT_INST` hay dùng

Các macro `DT_INST_*` là bản rút gọn của macro `DT_*` tương ứng, khác ở chỗ nhận số instance thay vì node identifier. Dưới đây là những cái hay dùng nhất khi viết driver:

| Macro | Ý nghĩa |
|---|---|
| `DT_INST_PROP(inst, p)` | Đọc property `p` của instance |
| `DT_INST_PROP_OR(inst, p, default)` | Như trên, có giá trị mặc định nếu property không tồn tại |
| `DT_INST_REG_ADDR(inst)` | Địa chỉ lấy từ property `reg` |
| `DT_INST_IRQN(inst)` | Số IRQ |
| `DT_INST_GPIO_PIN(inst, gpios)` | Pin của phandle GPIO |
| `I2C_DT_SPEC_INST_GET(inst)` | Lấy `struct i2c_dt_spec` cho instance |
| `GPIO_DT_SPEC_INST_GET_OR(inst, p, default)` | Lấy `struct gpio_dt_spec`, có giá trị mặc định |
| `PM_DEVICE_DT_INST_GET(inst)` | Lấy object PM của instance |
| `DT_HAS_COMPAT_STATUS_OKAY(compat)` | Có instance nào okay không? |
| `DT_NUM_INST_STATUS_OKAY(compat)` | Có bao nhiêu instance okay? |
| `DT_INST_FOREACH_STATUS_OKAY(fn)` | Gọi `fn(inst)` cho mọi instance okay |
