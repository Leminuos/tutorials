Partition table là một bảng mô tả cách bố trí vùng nhớ trong flash. Nó cho biết mỗi phân vùng nằm tại offset bao nhiêu trong flash, chứa cái gì (app, dữ liệu, NVS, OTA…), dung lượng bao nhiêu và các thuộc tính bảo mật của nó.

=> Bảng này có tác dụng quản lý nhiều phân vùng firmware, từ đó có thể chuyển đổi qua lại. Ngoài ra, nó cũng lưu dữ liệu liên quan đến cấu hình người dùng, dump crash và hệ thống file.

Ví dụ nội dung của partition table như sau:

```
# Name,     Type, SubType, Offset,  Size
nvs,        data, nvs,     0x9000,  0x6000
otadata,    data, ota,     0xf000,  0x2000
phy_init,   data, phy,     0x10000, 0x1000
factory,    app,  factory, 0x10000, 1M
ota_0,      app,  ota_0,   0x110000,1M
ota_1,      app,  ota_1,   0x210000,1M
spiffs,     data, spiffs,  0x310000,1M
```

## Ý nghĩa của các cột trong partition table

| Trường | Ý nghĩa |
| ------ | ------- |
| Name   | Tên phân vùng (tối đa 16 ký tự), dùng để truy xuất phân vùng từ code (ví dụ: "nvs", "storage"). |
| Type   | Loại phân vùng: thường là app (chứa code) hoặc data (chứa dữ liệu). |
| SubType | Chi tiết hơn về loại: nvs, ota, spiffs, fat, hoặc factory. |
| Offset | Địa chỉ bắt đầu của phân vùng trên flash. Nếu để trống, ESP-IDF sẽ tự tính toán nối tiếp phân vùng trước. |
| Size | Kích thước phân vùng. Có thể dùng đơn vị K (Kilobytes) hoặc M (Megabytes). Ví dụ: 1M. |
| Flags | Thường để trống. Flag encrypted dùng nếu bạn muốn mã hóa phân vùng đó. |

## Apply partition table

Đầu tiên, ta cần tạo một file csv nằm trong thư mục root của project:

```
your_project/partitions.csv
```

Tiếp theo, ta sẽ thực hiện theo các bước sau:

- Mở terminal tại thư mục dự án và chạy lệnh: `idf.py menuconfig`
- Di chuyển đến mục: **Partition Table**, nhấn Enter và chọn **Custom partition CSV file**.
- Tại dòng **Custom partition CSV file**, nhập tên file (ví dụ: `partitions.csv`).
- Nhấn S để lưu và Esc để thoát.

Nếu ta muốn cấu hình trực tiếp trong file `sdkconfig`, ta có thể thêm các dòng sau vào file `sdkconfig`:

```conf
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"
```

Sau khi cấu hình xong, ta tiến hành build lại dự án. ESP-IDF sẽ tự động biên dịch file `.csv` thành file nhị phân `.bin` để nạp vào chip.

```
idf.py build
idf.py flash
```

## Một số lưu ý khi cấu hình partition table

### Quy tắc Offset và Alignment

Đây là lỗi phổ biến nhất khiến ESP32 báo lỗi "Image invalid" hoặc không load được app.

- Phân vùng APP: Bắt buộc phải được căn chỉnh tại địa chỉ chia hết cho 64KB (0x10000). Ví dụ: 0x10000, 0x20000, v.v.

- Phân vùng DATA: Phải được căn chỉnh theo 4KB (0x1000).

:::tip
Cách an toàn nhất là để trống cột Offset. ESP-IDF sẽ tự động tính toán và căn chỉnh đúng quy chuẩn.
:::

### Dung lượng flash thực tế

Ta cần biết chính xác chip của mình có bao nhiêu flash (thường là 4MB trên ESP32-WROOM, nhưng có loại 2MB, 8MB hoặc 16MB).

Tổng size của các phân vùng cộng lại (bao gồm cả bootloader và partition table) không được vượt quá dung lượng vật lý của chip hoặc flash size được cấu hình trong `menuconfig`.

Ta có thể kiểm tra dung lượng của flash bằng lệnh:

```bash
esptool.py flash_id
```

Khi sử dụng lệnh này thì ta sẽ nhận được các thông tin liên quan đến flash như:
- Manufacturer: Nhà sản xuất chip Flash.
- Device: Mã thiết bị.
- Detected flash size: Đây chính là dung lượng flash thực tế (ví dụ: 4MB, 8MB, 16MB).

Sau khi biết dung lượng flash thực tế, ta có thể cấu hình trong project để ESP-IDF sử dụng đúng mức dung lượng đó (thường ESP-IDF để mặc định là 2MB).

- Chạy `idf.py menuconfig`.
- Vào Serial flasher config.
- Tại mục flash size, chọn đúng dung lượng vừa tìm thấy (ví dụ: 4MB).
- Lưu và Build lại.

### Phân vùng NVS và OTA Data

- NVS (Non-Volatile Storage): Luôn cần ít nhất một phân vùng nvs để lưu thông tin Wi-Fi, biến hệ thống. Kích thước tối thiểu khuyên dùng là 0x4000 (16KB).

- `otadata`: Nếu ta dùng phân vùng `ota_0` và `ota_1`, thì bắt buộc phải có phân vùng `otadata` (kiểu `data`, subtype `ota`). Nếu thiếu, chip sẽ không biết phải boot vào bản firmware mới nào sau khi cập nhật.

### Tên phân vùng (Name)

- Độ dài tối đa là 16 ký tự.

- Tên này cực kỳ quan trọng khi ta viết code. Ví dụ, nếu ta dùng `SPIFFS` và đặt tên trong CSV là storage, thì trong code ta phải gọi đúng:

```cpp
esp_vfs_spiffs_conf_t conf = { .partition_label = "storage", ... };
```

### Sự tương quan với Bootloader

Mặc định, Partition Table được lưu tại địa chỉ 0x8000.

- Nếu bảng phân vùng quá lớn (nhiều dòng), nó có thể đè lên vùng dữ liệu phía sau.

- Nếu bootloader quá nặng (do bật quá nhiều tính năng log), nó có thể đè lên chính bảng phân vùng tại 0x8000. Ta có thể thay đổi địa chỉ này trong menuconfig nếu cần.

### Factory vs OTA

- Factory partition: Là bản firmware gốc.

- Nếu ta cấu hình có OTA, chip sẽ ưu tiên boot từ OTA nếu có dữ liệu trong otadata. Nếu không dùng OTA để tiết kiệm diện tích, ta chỉ cần một phân vùng factory duy nhất cho app.

## Parse partition table sau khi build

Sau khi chạy `idf.py build`, một file binary của partition table sẽ được tạo ra:

```bash
build/partition_table/partition-table.bin
```

Ta có thể parse dữ liệu của binary `partition-table.bin` thành một file `.csv` như sau:

```bash
python $IDF_PATH/components/partition_table/gen_esp32part.py build/partition_table/partition-table.bin
```