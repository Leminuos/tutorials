## Giới thiệu

Topic này hướng dẫn sử dụng các API I2C trong ESP-IDF. Driver mới cung cấp hai chế độ hoạt động:
- I2C Master: ESP32 điều khiển bus I2C, giao tiếp với các thiết bị slave
- I2C Slave: ESP32 đóng vai trò thiết bị slave, nhận lệnh từ master

:::warning Phiên bản yêu cầu
Các API sau chỉ áp dụng đối với các ESP-IDF phiên bản 5.4 trở lên.
:::

:::tip Thread safety
Driver mới này hỗ trợ thread safe. Ta có thể gọi các API truyền nhận từ nhiều task khác nhau trên cùng một bus mà không cần dùng mutex bên ngoài, driver sẽ tự quản lý việc này.
:::

## 1. I2C master

Trong chế độ master, ESP32 chủ động điều khiển giao tiếp I2C. Quy trình sử dụng gồm 3 bước:
- Khởi tạo I2C bus - Cấu hình phần cứng I2C
- Thêm device vào bus - Đăng ký các thiết bị slave cần giao tiếp
- Truyền/nhận dữ liệu - Thực hiện các giao dịch I2C

### 1.1. Tạo/huỷ I2C bus

#### 1.1.1. Khởi tạo bus I2C

**Prototype của API**:

```c
i2c_new_master_bus(
  const i2c_master_bus_config_t *bus_config,
  i2c_master_bus_handle_t *ret_bus_handle
)
```

**Chức năng:** Cấp phát và khởi tạo một bus I2C master gắn với 1 controller hoặc port cụ thể.

**Tham số:**
- `bus_config`: Cấu hình bus (port I2C, chân gpio, clock source, pullup,...)
- `ret_bus_handle` (out): Con trỏ để nhận handle của bus sau khi khởi tạo thành công.

**Giá trị trả về:**
- `ESP_OK`: Khởi tạo thành công
- `ESP_ERR_NOT_FOUND`: Không còn I2C port khả dụng
- `ESP_ERR_INVALID_ARG`: Tham số không hợp lệ

**Cách sử dụng:** Gọi một lần khi khởi tạo hệ thống, lưu `bus_handle` để thực hiện add device hoặc thực hiện transaction.

**Cấu trúc `i2c_master_bus_config_t`**:

```c
typedef struct {
    int i2c_port;
    gpio_num_t sda_io_num;
    gpio_num_t scl_io_num;
    i2c_clock_source_t clk_source;
    uint32_t glitch_ignore_cnt;
    struct {
        uint32_t enable_internal_pullup:1;
    } flags;
} i2c_master_bus_config_t;
```

- `i2c_port`: chọn I2C controller phần cứng
  - `-1`: ESP-IDF auto chọn port rảnh
  - `I2C_NUM_0`, `I2C_NUM_1`
- `sda_io_num`, `scl_io_num`: Chân gpio cho SDA/SCL
- `clk_source`: nguồn clock cho I2C peripheral
  - `I2C_CLK_SRC_DEFAULT`: IDF sẽ tự chọn nguồn clock phù hợp nhất cho I2C ở chế độ hiện tại.
  - `I2C_CLK_SRC_APB`: APB clock.
- `glitch_ignore_cnt`: số chu kỳ clock dùng để lọc các xung nhiễu trên đường đây.
- `flags.enable_internal_pullup`: Bật/tắt điện trở pull-up nội

Ví dụ:

```c
i2c_master_bus_config_t bus_config = {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = -1, // Tự động chọn port
    .scl_io_num = GPIO_NUM_22,
    .sda_io_num = GPIO_NUM_21,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true,
};
i2c_master_bus_handle_t bus_handle;
ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
```

:::warning Chú ý
Dù trong code có `enable_internal_pullup`, nhưng trở kéo nội của ESP32 rất yếu (khoảng 45kΩ). Với chế độ fast mode, bắt buộc ta phải có trở kéo ngoài (khoảng 2.2k đến 4.7k).
:::

#### 1.1.2. Giải phóng bus I2C

**Prototype của API:**

```c
i2c_del_master_bus(i2c_master_bus_handle_t bus_handle)
```

**Chức năng:** Giải phóng bus và tài nguyên driver.

**Cách sử dụng:** Gọi khi chắc chắn không còn task nào sử dụng bus.

#### 1.1.3. Reset bus I2C

**Prototype của API:**

```c
i2c_master_bus_reset(i2c_master_bus_handle_t bus_handle)
```

**Chức năng:** Reset bus I2C.

**Khi nào cần reset:**
- Bus bị kẹt (hang)
- SDA bị giữ ở mức thấp
- Lỗi giao tiếp liên tục
- Cần khôi phục trạng thái ban đầu

### 1.2. Quản lý device trên bus

Sau khi tạo bus, cần đăng ký các thiết bị slave sẽ giao tiếp.

#### 1.2.1. Thêm device vào bus

**Prototype của API:**

```c
i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle, const i2c_device_config_t *dev_config, i2c_master_dev_handle_t *ret_dev_handle)
```

**Chức năng:** Đăng ký một thiết bị slave cụ thể vào bus đã tạo.
**Tham số:**
- `bus_handle`: Handle của bus (đã tạo bằng `i2c_new_master_bus`)
- `dev_config`: Cấu hình thiết bị (địa chỉ slave, tốc độ clock riêng cho thiết bị đó, flag,...).
- `ret_dev_handle` (output): Con trỏ nhận handle của thiết bị.

**Cấu trúc `i2c_device_config_t`:**

```c
typedef struct {
    i2c_addr_bit_len_t dev_addr_length;
    uint16_t device_address;
    uint32_t scl_speed_hz;
    uint32_t scl_wait_us;
    struct {
        uint32_t disable_ack_check:      1;
    } flags;
} i2c_device_config_t;
```

- `dev_addr_length`: Độ dài địa chỉ I2C, thường là `I2C_ADDR_BIT_LEN_7`. Một số thiết bị cũ dùng 10-bit nhưng rất hiếm.
- `device_address`: Địa chỉ slave (Tra datasheet của linh kiện).
- `scl_wait_us`: Thời gian chờ clock stretching. Thường để mặc định.
- `disable_ack_check`: Nếu bit này là false thì có nghĩa là ack check sẽ được enable, nếu detect được nack thì transaction sẽ stop và API sẽ trả về error.
- `scl_speed_hz`: Tốc độ SCL cho riêng device này
  - Standard Mode: 100,000 (100kHz)
  - Fast Mode: 400,000 (400kHz)
  - Fast Mode Plus: 1,000,000 (1MHz) - tùy dòng chip ESP32 hỗ trợ.

Ví dụ:

```c
i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x68, // Ví dụ: dịa chỉ MPU6050
    .scl_speed_hz = 400000, // 400kHz
};
i2c_master_dev_handle_t dev_handle;
ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
```

#### 1.2.2. Xoá device khỏi bus

**Prototype của API:**

```c
i2c_master_bus_rm_device(i2c_master_dev_handle_t dev_handle)
```

**Chức năng:** Gỡ device khỏi bus và giải phóng tài nguyên.

### 1.3. Truyền nhận dữ liệu

Sau khi khởi tạo bus và add device, ta có thể thực hiện các giao dịch I2C.

#### 1.3.1. Ghi dữ liệu

**Prototype của API:**

```c
i2c_master_transmit(i2c_master_dev_handle_t dev, const uint8_t *write_buffer, size_t write_size, int timeout_ms)
```

**Chức năng:** Master gửi một chuỗi byte tới slave.

**Tham số:**
- `dev`: Handle của device (đã add vào bus)
- `write_buffer`: Mảng dữ liệu cần gửi
- `write_size`: Số byte cần gửi
- `timeout_ms`: Thời gian timeout (ms), `-1` = chờ vô hạn

**Ví dụ:**
```c
// Gửi lệnh reset tới MPU6050
uint8_t cmd[] = {0x6B, 0x80};
esp_err_t ret = i2c_master_transmit(mpu6050_handle, cmd, sizeof(cmd), 1000);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "I2C transmit failed");
}
```

#### 1.3.2. Đọc dữ liệu

**Prototype của API:**

```c
i2c_master_receive(i2c_master_dev_handle_t dev, uint8_t *read_buffer, size_t read_size, int timeout_ms)
```

**Chức năng:** Master đọc một chuỗi byte từ slave.

**Tham số:**
- `dev`: Handle của device
- `read_buffer` (output): Mảng để chứa dữ liệu đọc được
- `read_size`: Số byte cần đọc
- `timeout_ms`: Thời gian timeout (ms)

**Ví dụ:**
```c
// Đọc 6 byte từ slave
uint8_t data[6];
esp_err_t ret = i2c_master_receive(mpu6050_handle, data, 6, 1000);
if (ret == ESP_OK) {
    // Xử lý dữ liệu
}
```

#### 1.3.3. Ghi sau đó đọc

**Prototype của API:**

```c
i2c_master_transmit_receive(i2c_master_dev_handle_t dev, const uint8_t *write_buffer, size_t write_size, uint8_t *read_buffer, size_t read_size, int timeout_ms)
```

**Chức năng:** Gửi địa chỉ thanh ghi, sau đó thực hiện Repeated Start để đọc dữ liệu từ thanh ghi đó.

**Khi nào dùng:** Đây là thao tác phổ biến nhất khi đọc thanh ghi từ sensor/IC.

**Quy trình:**
1. Master gửi START
2. Master gửi địa chỉ slave + Write bit
3. Master gửi địa chỉ thanh ghi cần đọc
4. Master gửi REPEATED START (không STOP)
5. Master gửi địa chỉ slave + Read bit
6. Master đọc dữ liệu
7. Master gửi STOP

**Ví dụ:**
```c
// Đọc WHO_AM_I register (0x75) của MPU6050
uint8_t reg_addr = 0x75;
uint8_t who_am_i;

esp_err_t ret = i2c_master_transmit_receive(
    mpu6050_handle,
    &reg_addr, 1,      // Gửi địa chỉ thanh ghi
    &who_am_i, 1,      // Đọc 1 byte
    1000
);

if (ret == ESP_OK) {
    ESP_LOGI(TAG, "WHO_AM_I = 0x%02X", who_am_i);  // Kỳ vọng: 0x68
}
```

## 2. I2C slave

Trong chế độ slave, ESP32 đóng vai trò thiết bị slave, đáp ứng yêu cầu từ master.

:::warning Chú ý
Các phiên bản IDF 5.x có hỗ trợ I2C slave v2, có thể sử dụng bằng cách bật config `CONFIG_I2C_ENABLE_SLAVE_DRIVER_VERSION_2`. Trang idf cũng nói rõ v2 sẽ là hướng bảo trì chính, v1 sẽ bị bỏ ở phiên bản IDF 6.0. Cho nên topic này sẽ tập trung vào I2C slave v2.
:::

### 2.1. Tạo/huỷ slave device

#### 2.1.1. Khởi tạo slave device

**Prototype của API:**

```c
i2c_new_slave_device(const i2c_slave_config_t *slave_config, i2c_slave_dev_handle_t *ret_handle)
```

**Chức năng:** Khởi tạo I2C controller ở chế độ slave.

**Tham số:**
- `slave_config`: Cấu hình slave (port, GPIO pins, địa chỉ slave, buffer depth,...)
- `ret_handle` (output): Con trỏ nhận handle của slave device

**Cấu trúc `i2c_slave_config_t`:**
```c
typedef struct {
    int i2c_port;                  // Port I2C
    gpio_num_t sda_io_num;         // Chân SDA
    gpio_num_t scl_io_num;         // Chân SCL
    i2c_clock_source_t clk_source; // Nguồn clock
    uint8_t slave_addr;            // Địa chỉ slave của ESP32
    size_t send_buf_depth;         // Độ sâu buffer gửi
    size_t receive_buf_depth;      // Độ sâu buffer nhận
} i2c_slave_config_t;
```

Ví dụ:

```c
i2c_slave_config_t slave_cfg = {
    .i2c_port = I2C_NUM_0,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .slave_addr = 0x58, // Địa chỉ ESP32 đóng vai Slave
    .send_buf_depth = 256, // Độ dài buffer gửi
    .receive_buf_depth = 256, // Độ dài buffer nhận
};
i2c_slave_dev_handle_t slave_handle;
ESP_ERROR_CHECK(i2c_new_slave_device(&slave_cfg, &slave_handle));
```

:::tip Kích thước Buffer
Chọn `send_buf_depth` và `receive_buf_depth` đủ lớn để xử lý dữ liệu mà master có thể gửi/yêu cầu trong một giao dịch. Nếu buffer quá nhỏ, dữ liệu có thể bị mất.
:::

#### 2.1.2. Xoá slave device

**Prototype của API:**

```c
i2c_del_slave_device(i2c_slave_dev_handle_t handle)
```

**Chức năng:** Gỡ slave device và giải phóng tài nguyên.

### 2.2. Nhận dữ liệu từ master

Trong bản V2, slave sẽ đợi dữ liệu từ master thông qua API `i2c_slave_receive`.

**Prototype của API:**

```c
esp_err_t i2c_slave_receive(i2c_slave_dev_handle_t i2c_slave, uint8_t *data, size_t buffer_size)
```

**Chức năng:** Đăng ký buffer để driver tự động điền dữ liệu khi master ghi vào.

**Tham số:**
- `i2c_slave`: Handle của slave device
- `data`: Mảng để chứa dữ liệu nhận được
- `buffer_size`: Kích thước mảng (byte)

**Cơ chế hoạt động:**
1. Khi gọi API này, nó trả về `ESP_OK` ngay lập tức (non-blocking)
2. Driver đăng ký buffer của ta vào hàng đợi nhận
3. Khi master gửi dữ liệu, driver tự động điền vào buffer
4. Callback `on_receive` được gọi để thông báo hoàn tất

:::warning Buffer Size
Kích thước buffer rất quan trọng. Nếu master gửi dữ liệu nhanh hơn khả năng xử lý của slave, dữ liệu sẽ bị ghi đè hoặc mất nếu buffer quá nhỏ.
:::

**Ví dụ:**
```c
uint8_t recv_buffer[128];

// Đăng ký buffer nhận
esp_err_t ret = i2c_slave_receive(slave_handle, recv_buffer, sizeof(recv_buffer));
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to register receive buffer");
}

// Dữ liệu sẽ được điền tự động khi master ghi
// Sử dụng callback on_receive để biết khi nào nhận xong
```

### 2.3. Gửi dữ liệu cho master

Để master có thể đọc dữ liệu, slave phải chuẩn bị sẵn dữ liệu vào buffer gửi trước khi master thực hiện lệnh đọc.

**Prototype của API:**

```c
esp_err_t i2c_slave_transmit(i2c_slave_dev_handle_t i2c_slave, const uint8_t *data, int size, int xfer_timeout_ms)
```

**Chức năng:** Đẩy dữ liệu vào hàng đợi gửi (TX FIFO). Khi master yêu cầu đọc, dữ liệu này sẽ tự động được gửi đi.

**Tham số:**
- `i2c_slave`: Handle của slave device
- `data`: Mảng dữ liệu cần gửi
- `size`: Số byte cần gửi
- `xfer_timeout_ms`: Timeout cho việc chuyển dữ liệu

**Lưu ý:** Cần chuẩn bị dữ liệu trước khi master yêu cầu đọc.

**Ví dụ:**
```c
// Chuẩn bị dữ liệu sensor để master đọc
uint8_t sensor_data[4] = {0x12, 0x34, 0x56, 0x78};

esp_err_t ret = i2c_slave_transmit(slave_handle, sensor_data, sizeof(sensor_data), 1000);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to queue transmit data");
}

// Khi master đọc, driver sẽ tự động gửi dữ liệu này
```

### 2.4. Callback/event

Đây là điểm mạnh của V2, đó là thay vì polling, ta có thể đăng ký các hàm callback để hệ thống tự gọi khi có sự kiện.

Các sự kiện hỗ trợ:
- `on_receive`: Được gọi khi master đã gửi xong một gói dữ liệu cho slave.
- `on_request `: Được gọi khi slave đã hoàn tất việc gửi dữ liệu cho master.

Đăng ký callback bằng cách sử dụng API:

```c
esp_err_t i2c_slave_register_event_callbacks(
    i2c_slave_dev_handle_t i2c_slave,
    const i2c_slave_event_callbacks_t *cbs,
    void *user_data
)
```

**Tham số:**
- `i2c_slave`: Handle của slave device
- `cbs`: Cấu trúc chứa các con trỏ hàm callback
- `user_data`: Dữ liệu tùy chỉnh truyền vào callback

**Cấu trúc callbacks:**
```c
typedef struct {
    i2c_slave_received_callback_t on_receive;  // Callback khi nhận xong
    i2c_slave_transmitted_callback_t on_request ;  // Callback khi gửi xong
} i2c_slave_event_callbacks_t;
```

**Ví dụ đầy đủ:**
```c
// Hàm callback khi nhận dữ liệu xong
static bool IRAM_ATTR i2c_slave_recv_callback(i2c_slave_dev_handle_t handle,
                                                const i2c_slave_rx_done_event_data_t *evt,
                                                void *user_data)
{
    // evt->buffer: con trỏ tới buffer nhận
    // evt->size: số byte thực tế nhận được
    
    ESP_LOGI(TAG, "Received %d bytes", evt->size);
    
    // Xử lý dữ liệu...
    
    // Đăng ký buffer mới để nhận dữ liệu tiếp theo
    i2c_slave_receive(handle, recv_buffer, sizeof(recv_buffer));
    
    return false;  // false = không cần yield từ ISR
}

// Hàm callback khi gửi dữ liệu xong
static bool IRAM_ATTR i2c_slave_sent_callback(i2c_slave_dev_handle_t handle,
                                               const i2c_slave_tx_done_event_data_t *evt,
                                               void *user_data)
{
    ESP_LOGI(TAG, "Sent %d bytes", evt->size);
    
    // Chuẩn bị dữ liệu mới nếu cần...
    
    return false;
}

// Đăng ký callbacks
i2c_slave_event_callbacks_t cbs = {
    .on_receive = i2c_slave_recv_callback,
    .on_request = i2c_slave_sent_callback,
};

ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(slave_handle, &cbs, NULL));
```

:::tip IRAM_ATTR
Các hàm callback nên có thuộc tính `IRAM_ATTR` để chạy từ RAM, tránh cache miss khi được gọi từ ISR.
:::

## 3. Workflow Tổng Quan

### Master mode workflow

1. Khởi tạo bus
2. Thêm device vào bus
3. Thực hiện giao dịch:
   - Ghi dữ liệu (transmit)
   - Đọc dữ liệu (receive)
   - Ghi rồi đọc (transmit_receive)
4. [Tùy chọn] Xóa device
5. [Khi kết thúc] Xóa bus

### Slave mode workflow

1. Khởi tạo slave device
2. Đăng ký callbacks
3. Đăng ký buffer nhận (`i2c_slave_receive`)
4. Chuẩn bị dữ liệu gửi (`i2c_slave_transmit`)
5. Đợi master giao tiếp
6. Callback được gọi khi có sự kiện
7. Xử lý trong callback và đăng ký buffer mới

## Tham khảo

https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/i2c.html#