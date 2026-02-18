## 1. Overview

NVS là thư viện lưu trữ dạng **key–value** trên flash của ESP32 trong ESP‑IDF. NVS được thiết kế để lưu trữ các dữ liệu cấu hình, tham số hệ thống và trạng thái cần được bảo toàn sau reset hoặc mất nguồn, mà không cần sử dụng filesystem.

Về mặt kiến trúc, NVS hoạt động trực tiếp trên các flash partition có type `data` và subtype `nvs`. Mỗi partition NVS hoạt động như một cơ sở dữ liệu nhỏ, có khả năng chịu lỗi mất điện và tối ưu cho đặc tính ghi/xoá của flash.

Ví dụ:

```
Name   Type SubType Offset   Size
nvs    data nvs     0x9000   0x5000
```

Đặc điểm:
- NVS không phải file system
- ESP-IDF quản lý toàn bộ layout bên trong
- Ứng dụng không truy cập flash trực tiếp

## 2. Tại sao cần sử dụng NVS?

Trong hệ thống nhúng, một số dữ liệu cần được lưu lại sau khi mất nguồn hoặc reset. Ví dụ như cấu hình Wi-Fi, tham số calibration, hoặc trạng thái hệ thống. Nếu không có cơ chế lưu trữ phù hợp, các dữ liệu này sẽ bị mất mỗi khi thiết bị khởi động lại.

NVS được thiết kế để giải quyết bài toán này theo cách đơn giản và an toàn:
- Dữ liệu được ghi theo kiểu ghi nối tiếp (ghi thêm bản ghi mới, không ghi đè trực tiếp lên dữ liệu cũ)
- Flash không bị xoá/ghi lặp lại liên tục tại cùng một địa chỉ, giúp tăng tuổi thọ flash
- Nếu mất điện trong lúc đang ghi, dữ liệu đã ghi trước đó vẫn an toàn
- Dev chỉ làm việc với key–value, không cần quản lý sector hay địa chỉ flash

Nhờ các đặc điểm này, NVS rất phù hợp để lưu cấu hình và trạng thái hệ thống trong firmware ESP32.

## 3. Các khái niệm cốt lõi

### 3.1 Key và value

NVS lưu dữ liệu dưới dạng cặp **key–value**:

- Key là chuỗi ASCII, tối đa 15 ký tự
- Value có thể là số nguyên (8/16/32/64 bit), string hoặc blob (binary data)

### 3.2 Namespace

Key trong NVS luôn nằm trong một **namespace**. Namespace giúp phân tách dữ liệu giữa các module trong firmware, tránh xung đột tên key. Mỗi namespace được mở thông qua một handle và mọi thao tác đọc/ghi đều thông qua handle này.

### 3.3 Commit

Các hàm `nvs_set_*()` hoặc `nvs_erase_*()` chỉ thay đổi dữ liệu trong RAM cache của NVS. Dữ liệu chỉ thực sự được ghi xuống flash khi gọi `nvs_commit()`.

Điều này cho phép gom nhiều thay đổi và commit một lần để giảm số lần ghi flash.

## 4. Cách NVS lưu dữ liệu trong flash

NVS không ghi đè dữ liệu trực tiếp lên flash. Thay vào đó, mỗi lần một key được cập nhật, giá trị mới sẽ được ghi thêm vào vùng nhớ trống, còn giá trị cũ được đánh dấu là không còn hiệu lực.

Bạn có thể hình dung NVS giống như một cuốn sổ:
- Mỗi lần cập nhật là viết thêm một dòng mới
- Dòng cũ không bị xoá ngay, chỉ được đánh dấu là không dùng nữa
- Khi cuốn sổ gần hết chỗ trống, hệ thống sẽ dọn dẹp các dòng cũ để lấy chỗ trống

Flash partition của NVS được chia thành các page (mỗi page tương ứng một sector flash). Tại một thời điểm chỉ có một page được dùng để ghi dữ liệu mới. Khi page này đầy, NVS sẽ chuyển sang page khác và tự động dọn dẹp page cũ.

Cách làm này giúp:
- Tránh ghi đè trực tiếp lên flash
- Giảm số lần xoá sector
- Đảm bảo dữ liệu không bị hỏng khi mất điện giữa chừng

## 5. Khởi tạo NVS

NVS cần được khởi tạo trước khi sử dụng. Trong thực tế, hai lỗi thường gặp khi khởi tạo là:
- Không còn page trống
- Phiên bản NVS không tương thích (sau khi thay đổi partition table hoặc ESP‑IDF)

:::warning Thứ tự khởi tạo
NVS phải được khởi tạo trước khi bất kỳ module nào (Wi‑Fi, BLE, app config) sử dụng API NVS. Việc gọi `nvs_open()` khi chưa `nvs_flash_init()` sẽ dẫn tới lỗi runtime khó debug.
:::

Ví dụ khởi tạo an toàn:

```c
void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
}
```

## 6. Một số ví dụ sử dụng cơ bản

### 6.1 Lưu biến đếm số lần boot

```c
nvs_handle_t handle;
uint32_t boot_count = 0;

nvs_open("app", NVS_READWRITE, &handle);

if (nvs_get_u32(handle, "boot", &boot_count) == ESP_ERR_NVS_NOT_FOUND) {
    boot_count = 0;
}

boot_count++;
nvs_set_u32(handle, "boot", boot_count);
nvs_commit(handle);
nvs_close(handle);
```

Mỗi lần cập nhật `boot_count`, NVS sẽ ghi một bản ghi mới thay vì ghi đè lên dữ liệu cũ.

:::tip Boot counter
Các biến như `boot_count` chỉ nên được cập nhật một lần khi boot, không nên ghi trong loop hoặc task chạy định kỳ để tránh làm tăng tần suất ghi flash.
:::

### 6.2 Lưu struct cấu hình bằng blob

```c
typedef struct {
    uint32_t version;
    uint32_t baudrate;
    uint8_t mode;
} app_cfg_t;

static esp_err_t cfg_save(const app_cfg_t *cfg)
{
    nvs_handle_t h;
    ESP_ERROR_CHECK(nvs_open("app", NVS_READWRITE, &h));

    esp_err_t err = nvs_set_blob(h, "cfg", cfg, sizeof(*cfg));
    if (err == ESP_OK) err = nvs_commit(h);

    nvs_close(h);
    return err;
}

void cfg_init(void)
{
    app_cfg_t cfg = {
        .version  = APP_CFG_VERSION,
        .baudrate = 115200,
        .mode     = 1,
    };
    ESP_ERROR_CHECK(cfg_save(&cfg));
}
```

Struct được lưu dưới dạng blob. Trường `version` giúp kiểm soát tương thích khi firmware được nâng cấp.

## 7. Sử dụng nhiều partition NVS

### 7.1 Vì sao cần nhiều partition NVS?

Trong các hệ thống ESP32 phức tạp, không phải mọi dữ liệu NVS đều có cùng tính chất. Một số dữ liệu rất quan trọng và gần như không thay đổi (factory data), trong khi một số khác thay đổi thường xuyên trong quá trình vận hành (user config, runtime state).

Nếu tất cả dữ liệu này dùng chung một partition NVS, sẽ phát sinh các vấn đề:
- Dữ liệu thay đổi thường xuyên làm tăng tần suất dọn dẹp (GC), ảnh hưởng tới dữ liệu quan trọng
- Khó kiểm soát quyền ghi (factory data vô tình bị ghi đè)
- Khó thiết kế chiến lược OTA và factory reset

Việc sử dụng nhiều partition NVS cho phép tách biệt rõ ràng vòng đời và mục đích của từng loại dữ liệu.

### 7.2 Các kiểu partition NVS thường dùng

Trong thực tế, một thiết kế phổ biến là chia NVS thành các partition sau:
- **NVS factory**: lưu dữ liệu provisioning tại nhà máy (serial, calibration, key cố định)
- **NVS config**: lưu cấu hình người dùng (Wi‑Fi, mode, endpoint)
- **NVS runtime**: lưu trạng thái tạm thời (counter, flag, statistics nhẹ)

Mỗi partition có kích thước và chính sách sử dụng khác nhau, phù hợp với tần suất ghi của dữ liệu bên trong.

### 7.3 Định nghĩa nhiều partition NVS trong partition table

Ví dụ một đoạn partition table có nhiều partition NVS:

```csv
# Name,     Type, SubType, Offset,  Size
nvs_factory,data, nvs,     0x9000,  0x4000
nvs_config, data, nvs,     0xD000,  0x6000
nvs_runtime,data, nvs,     0x13000, 0x4000
```

Mỗi partition được định danh bằng **label**, label này sẽ được dùng khi mở NVS từ firmware.

### 7.4 Mở NVS theo partition cụ thể trong firmware

Thay vì dùng `nvs_open()`, firmware sẽ sử dụng `nvs_open_from_partition()`:

```c
nvs_handle_t h;

nvs_open_from_partition("nvs_factory", "factory", NVS_READONLY, &h);
```

Điều này đảm bảo:
- Firmware chỉ đọc dữ liệu factory
- Không có rủi ro ghi nhầm vào partition quan trọng

Tương tự, các module khác có thể mở partition `nvs_config` hoặc `nvs_runtime` với quyền phù hợp.

:::warning Quyền truy cập partition
Luôn mở `nvs_factory` ở chế độ `NVS_READONLY` trong firmware thông thường. Chỉ cho phép ghi trong chế độ factory hoặc provisioning mode đặc biệt.
:::

### 7.5 Chiến lược OTA và factory reset với nhiều partition NVS

Sử dụng nhiều partition NVS giúp chiến lược update rõ ràng hơn:
- **OTA firmware update**: không ảnh hưởng tới bất kỳ partition NVS nào
- **Factory reset**: chỉ xoá `nvs_config` và `nvs_runtime`, giữ nguyên `nvs_factory`
- **Re‑provisioning**: chỉ tác động lên `nvs_factory` trong chế độ đặc biệt

Cách làm này giúp tránh mất serial, calibration hoặc key quan trọng sau khi reset thiết bị.

### 7.6 Lưu ý thiết kế quan trọng

- Không nên dùng một partition NVS cho cả factory và runtime data
- Partition ghi thường xuyên nên có kích thước đủ lớn để giảm tần suất GC
- Factory partition nên mở ở chế độ read‑only trong firmware bình thường
- Khi dùng NVS Encryption, tất cả partition NVS đều dùng chung cơ chế mã hoá

Sử dụng nhiều partition NVS là một quyết định kiến trúc, không phải chỉ là cấu hình tiện lợi.

## 8. Garbage Collection

### 8.1 Garbage Collection là gì?

Do NVS luôn ghi thêm dữ liệu mới thay vì ghi đè, theo thời gian sẽ xuất hiện nhiều bản ghi cũ không còn sử dụng. GC là quá trình tự động dọn dẹp các bản ghi này để giải phóng không gian lưu trữ.

Nói đơn giản:
- Bản ghi mới được ghi thêm
- Bản ghi cũ bị bỏ qua
- Khi sắp hết chỗ trống, hệ thống dọn dẹp để lấy chỗ ghi tiếp

### 8.2 GC hoạt động như thế nào?

Vùng flash NVS được chia thành nhiều page. Khi page đang ghi bị đầy, hệ thống sẽ:
1. Chuyển sang page mới để tiếp tục ghi
2. Sao chép các dữ liệu còn hợp lệ từ page cũ sang page mới
3. Xoá page cũ để tái sử dụng

Quá trình này được thiết kế để nếu mất điện giữa chừng, dữ liệu đã ghi trước đó vẫn không bị ảnh hưởng.

### 8.3 Lỗi thường gặp liên quan đến GC

- `ESP_ERR_NVS_NO_FREE_PAGES`: không còn page trống để tiếp tục ghi dữ liệu
- `ESP_ERR_NVS_NOT_ENOUGH_SPACE`: không đủ chỗ cho dữ liệu dạng string hoặc blob

Những lỗi này thường cho thấy partition NVS quá nhỏ hoặc firmware ghi dữ liệu quá thường xuyên.

:::warning Dấu hiệu thiết kế sai
Nếu hệ thống thường xuyên gặp lỗi `ESP_ERR_NVS_NO_FREE_PAGES`, đây không phải lỗi runtime, mà có thể là dấu hiệu partition NVS quá nhỏ hoặc firmware ghi dữ liệu quá thường xuyên.
:::

## 9. Bảo mật: NVS Encryption

### 9.1 NVS Encryption dùng để làm gì?

Bình thường, dữ liệu NVS được lưu trực tiếp trên flash và có thể đọc được nếu trích xuất flash từ bên ngoài. Điều này không an toàn đối với các dữ liệu riêng tư.

NVS Encryption cho phép mã hoá dữ liệu trước khi ghi xuống flash, nhằm:
- Bảo vệ Wi-Fi password
- Bảo vệ token, secret key
- Ngăn chỉnh sửa dữ liệu cấu hình quan trọng

:::warning Phiên bản yêu cầu
NVS Encryption chỉ khả dụng trên các ESP‑IDF phiên bản hỗ trợ `CONFIG_NVS_ENCRYPTION` (ESP‑IDF 4.x trở lên). Tài liệu này giả định ESP‑IDF 5.x.
:::

### 9.2 Cách bật NVS Encryption

Việc bật NVS Encryption được thực hiện **ở mức project**, không phải ở mức code ứng dụng.

Các bước điển hình:

- Mở menu cấu hình:

```
idf.py menuconfig
```

- Vào mục:

```
Component config → NVS
```

- Bật tuỳ chọn:

```
[*] Enable NVS Encryption
```

- Đảm bảo partition table có thêm partition `nvs_keys`, ví dụ:

```csv
nvs_keys, data, nvs_keys, 0x8000, 0x2000
```

Sau khi bật, ESP‑IDF sẽ tự động:
- Sinh khoá mã hoá
- Lưu khoá vào partition `nvs_keys`
- Mã hoá toàn bộ dữ liệu ghi vào các partition NVS

Firmware không cần thay đổi API khi đọc/ghi NVS.

:::warning Lưu ý quan trọng
Nếu partition `nvs_keys` bị xoá hoặc thay đổi, toàn bộ dữ liệu NVS đã mã hoá sẽ không thể phục hồi.
:::

### 9.3 Cách NVS Encryption hoạt động

Khi bật NVS Encryption:
- Dữ liệu được mã hoá trước khi ghi xuống flash
- Khoá mã hoá được lưu ở một partition riêng (`nvs_keys`)
- Firmware vẫn dùng API NVS như bình thường, không cần tự mã hoá

Nói cách khác, NVS Encryption hoạt động âm thầm bên dưới và dev không cần thay đổi logic ứng dụng.

### 9.4 Lưu ý quan trọng

- Nếu mất partition `nvs_keys`, toàn bộ dữ liệu NVS đã mã hoá sẽ không thể đọc lại
- Không thể đọc dữ liệu NVS encrypted bằng các công cụ đọc flash thông thường
- NVS Encryption nên được xem là một phần của thiết kế bảo mật tổng thể

## 10. Factory Provisioning và NVS Partition Generator

### 10.1 Factory provisioning là gì?

Trong sản xuất thiết bị nhúng, factory provisioning là bước nạp các dữ liệu định danh và mặc định vào thiết bị ngay tại nhà máy, trước khi thiết bị được giao cho khách hàng.

Các dữ liệu này thường bao gồm:
- Serial number
- Model / SKU
- Region / country code
- Tham số calibration phần cứng
- Credential mặc định hoặc key dùng riêng cho từng thiết bị

Những dữ liệu này có đặc điểm chung:
- Được ghi một lần hoặc rất ít lần trong vòng đời thiết bị
- Cần tồn tại suốt đời thiết bị
- Không nên bị ghi đè bởi firmware update

NVS rất phù hợp cho factory provisioning vì cho phép lưu trữ key–value bền vững mà không cần firmware chạy lần đầu để ghi dữ liệu.

### 10.2 NVS Partition Generator dùng để làm gì?

ESP-IDF cung cấp NVS Partition Generator cho phép tạo sẵn một binary NVS offline (trên PC), sau đó flash trực tiếp vào partition NVS của thiết bị.

Điều này có nghĩa là:
- Firmware không cần code để ghi dữ liệu factory
- Dữ liệu đã tồn tại ngay từ lần boot đầu tiên
- Giảm rủi ro lỗi trong quá trình provisioning

Quy trình này rất phổ biến trong dây chuyền sản xuất.

### 10.3 Quy trình provisioning điển hình

Một quy trình factory provisioning thường diễn ra như sau:
1. Nhà máy chuẩn bị file CSV mô tả dữ liệu NVS (key–value)
2. Sử dụng công cụ NVS Partition Generator để tạo file binary
3. Flash binary này vào partition NVS tương ứng
4. Flash firmware ứng dụng
5. Firmware khi boot chỉ đọc dữ liệu factory, không ghi lại

Cách làm này giúp tách biệt rõ ràng giữa:
- Dữ liệu factory (ít thay đổi, quan trọng)
- Dữ liệu runtime / user config (thay đổi trong quá trình sử dụng)

:::tip Factory data là read‑only
Firmware runtime không nên có bất kỳ code nào ghi lại serial, calibration hoặc key factory. Điều này giúp tránh lỗi sản xuất và lỗi OTA về sau.
:::

### 10.4 Ví dụ file CSV cho NVS generator

Ví dụ một file CSV đơn giản dùng cho provisioning:

```
key,type,encoding,value
serial,string,,ESP32-000123
model,string,,PRO-V1
region,string,,VN
hw_rev,u8,,2
```

Sau khi generate, các key này sẽ xuất hiện trong NVS và có thể được đọc bằng API `nvs_get_*()` như dữ liệu bình thường.

### 10.5 Lưu ý thiết kế khi dùng factory provisioning

- Nên tách partition NVS factory và partition NVS user
- Không ghi đè dữ liệu factory trong firmware runtime
- Nếu cần cập nhật dữ liệu factory, phải có quy trình riêng (factory mode)
- Khi dùng NVS Encryption, provisioning cần thực hiện sau khi khoá mã hoá đã được thiết lập

Factory provisioning nên được xem là một phần của kiến trúc hệ thống, không phải chỉ là bước flash dữ liệu.

## 11. Tổng kết

NVS là thành phần cốt lõi trong ESP‑IDF để lưu trữ dữ liệu bền vững. Khi được sử dụng đúng cách, NVS mang lại sự đơn giản, an toàn và hiệu quả cho các hệ thống nhúng.

Thiết kế tốt với NVS bao gồm:

* Phân chia namespace hợp lý
* Kiểm soát tần suất ghi
* Có versioning cho dữ liệu phức tạp
* Kết hợp mã hoá khi cần bảo mật

Nắm vững NVS là bước quan trọng để xây dựng firmware ESP32 ổn định và chuyên nghiệp.