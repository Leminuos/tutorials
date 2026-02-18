Trong USB, mọi thao tác cấu hình và điều khiển thiết bị đều được thực hiện thông qua Control Transfer trên endpoint 0. Nội dung của các control transfer này được chuẩn hóa thành một tập hợp các Standard Requests. Đây là các yêu cầu bắt buộc mà mọi thiết bị USB phải hiểu và phản hồi đúng theo specification.

Các request này được gửi trong setup stage của Control Transfer, với cấu trúc gói setup data luôn dài 8 byte như sau:

| Offset | Field           | Kích thước | Mô tả |
| ------ | --------------- | ---------- | ----- |
| 0      | `bmRequestType` | 1 byte     | Xác định hướng, loại request và đối tượng đích |
| 1	     | `bRequest`	   | 1 byte     | Mã request cụ thể |
| 2–3	 | `wValue`	       | 2 byte     | Tham số chính của request |
| 4–5	 | `wIndex`	       | 2 byte     | Tham số phụ hoặc index |
| 6–7	 | `wLength`       | 2 byte     | Số byte dữ liệu trong data stage |

Trường `bmRequestType` được chia thành các bit ý nghĩa như sau:

| Bit   | Ý nghĩa	     | Giá trị |
| ----- | -------------- | ------- |
| 7	    | Hướng truyền	 | - 0: Host → Device <br>- 1: Device → Host |
| 6..5  | Loại request	 | - 00: Standard <br>- 01: Class <br>- 10: Vendor <br>- Reserved |
| 4..0	| Đối tượng đích | - 00000: Device <br>- 00001: Interface <br>- 00010: Endpoint <br>- 00011: Other |

Post này sẽ chỉ tập trung vào Standard Requests, tương ứng `bmRequestType` có Type = 00.

Các Standard Requests bắt buộc theo USB 2.0 Specification:

| `bRequest` | Tên Request          | Hướng         | Mục đích |
| -------- | -------------------- | ------------- | -------- |
| 0x00     | `GET_STATUS`         | Device → Host | Đọc trạng thái của device/interface/endpoint |
| 0x01     | `CLEAR_FEATURE`      | Host → Device | Xóa một tính năng hoặc trạng thái đặc biệt |
| 0x03     | `SET_FEATURE`        | Host → Device | Thiết lập một tính năng hoặc trạng thái đặc biệt |
| 0x05     | `SET_ADDRESS`        | Host → Device | Gán địa chỉ mới cho thiết bị |
| 0x06     | `GET_DESCRIPTOR`     | Device → Host | Đọc descriptor từ thiết bị |
| 0x07     | `SET_DESCRIPTOR`     | Host → Device | Ghi descriptor (hiếm khi sử dụng) |
| 0x08     | `GET_CONFIGURATION`  | Device → Host | Đọc cấu hình hiện tại |
| 0x09     | `SET_CONFIGURATION`  | Host → Device | Chọn cấu hình hoạt động |
| 0x0A     | `GET_INTERFACE`      | Device → Host | Đọc interface đang hoạt động |
| 0x0B     | `SET_INTERFACE`      | Host → Device | Chọn alternate interface |
| 0x0C     | `SYNCH_FRAME`        | Device → Host | Đồng bộ frame (dùng cho isochronous endpoint) |

**GET_DESCRIPTOR (bRequest = 0x06)**

Đây là request quan trọng nhất trong toàn bộ giao thức USB. Host sử dụng `GET_DESCRIPTOR` để đọc các descriptor từ thiết bị, bắt đầu bằng Device Descriptor ở địa chỉ mặc định 0. Từ các descriptor này, host xác định loại thiết bị, số lượng cấu hình, số lượng interface và các endpoint cần kích hoạt.

Trong đó:
- `wValue`: Trường 16 bit này được chia thành hai byte:
  - Byte cao: Descriptor Type
    
    | Value | Descript type |
    | ----- | ------------- |
    | 1     | Device Descriptor |
    | 2     | Configuration Descriptor |
    | 3     | String Descriptor |
    | 4     | Interface Descriptor |
    | 5     | Endpoint Descriptor |
    | 6     | Device Qualifier Descriptor |
    | 7     | Other Speed Configuration Descriptor |

  - Byte thấp: Descriptor Index
    - Device Descriptor: luôn bằng 0
    - Configuration Descriptor: số thứ tự cấu hình (0,1,2...)
    - String Descriptor: số thứ tự chuỗi

- `wIndex`:
  - Với hầu hết descriptor: `wIndex = 0`
  - Với String Descriptor: `wIndex = Language ID` (ví dụ 0x0409 cho English-US)

- `wLength`: Số byte tối đa host muốn đọc. Thiết bị chỉ được gửi tối đa bằng giá trị này.

:::tip
Request này luôn có data stage với dữ liệu trả về từ device sang host.
:::

**SET_ADDRESS (bRequest = 0x05)**

Sau khi đọc Device Descriptor lần đầu, host gửi `SET_ADDRESS` để gán một địa chỉ mới cho thiết bị. Ngay sau status stage hoàn tất, thiết bị bắt đầu phản hồi ở địa chỉ mới này.

Trong đó:
- `wValue`: Chứa địa chỉ mới (7 bit thấp có giá trị từ 1 đến 127).
- `wIndex`: Luôn bằng 0.
- `wLength`: Luôn bằng 0.

:::tip
Request này không có data stage.
:::

**SET_CONFIGURATION (bRequest = 0x09)**

Host gửi `SET_CONFIGURATION` để chọn một cấu hình cụ thể mà thiết bị hỗ trợ.

Trong đó:
- `wValue`: Chứa configuration value tương ứng với `bConfigurationValue` trong Configuration Descriptor.
- `wIndex`: Luôn bằng 0.
- `wLength`: Luôn bằng 0.

Khi hoàn tất request này, thiết bị chuyển sang trạng thái configured và các endpoint ngoài endpoint 0 bắt đầu hoạt động.

:::tip
Request này không có data stage.
:::

**GET_STATUS, SET_FEATURE, CLEAR_FEATURE**

Ba request này được dùng để quản lý trạng thái thiết bị và endpoint. Ví dụ, `CLEAR_FEATURE` được sử dụng để xóa trạng thái HALT của một endpoint sau khi xảy ra STALL.

Các request này đảm bảo host có thể điều khiển trạng thái logic của device một cách chuẩn hóa.