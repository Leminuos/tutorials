## Mục lục

- [Tổng quan](#1-tổng-quan)
- [Các kiến thức wifi cơ bản](#2-các-kiến-thức-wi-fi-cơ-bản)
- [Kiến trúc hệ thống](#3-kiến-trúc-hệ-thống)
- [Cơ chế hoạt động](#4-cơ-chế-hoạt-động)
- [Event ID reference](#5-event-id-reference)
- [Threading model](#6-threading-model)
- [Memory management](#7-memory-management)
- [Quy trình khởi tạo wifi chuẩn](#8-quy-trình-khởi-tạo-wi-fi-chuẩn)
- [Power management](#9-power-management)
- [Wifi scanning](#10-wi-fi-scanning)

## 1. Tổng quan

Wi-Fi subsystem trong ESP-IDF là giải pháp tuân thủ chuẩn IEEE 802.11 b/g/n, được thiết kế để vận hành trên kiến trúc vi điều khiển của Espressif. Khác với các thư viện Arduino đơn giản hóa, ESP-IDF Wi-Fi stack cung cấp quyền kiểm soát chi tiết xuống tận lớp vật lý (PHY) và lớp liên kết dữ liệu (Data Link Layer).

Hệ thống hoạt động thông qua sự phối hợp chặt chẽ giữa ba thành phần cốt lõi:
- Wi-Fi driver: Quản lý phần cứng RF và giao thức 802.11 (Binary blobs).
- ESP-NETIF: Lớp trừu tượng hóa giao diện mạng (Network Interface Abstraction).
- LwIP (Lightweight IP): TCP/IP Stack tiêu chuẩn công nghiệp cho hệ thống nhúng.

Để sử dụng hiệu quả stack này, cần nắm vững ba đặc tính kiến trúc nền tảng sau:

### 1.1. Kiến trúc event-driven

ESP-IDF sử dụng mô hình lập trình hướng sự kiện thay vì lập trình tuần tự. Thay vì liên tục kiểm tra trạng thái kết nối trong vòng lặp `while(1)` gây lãng phí chu kỳ CPU, hệ thống hoạt động dựa trên cơ chế post và handle event:
- Khi có thay đổi trạng thái (kết nối thành công, mất kết nối, nhận địa chỉ IP), driver sẽ phát một sự kiện vào **Default Event Loop**.
- Cơ chế này cho phép ứng dụng phản ứng tức thì với các thay đổi mạng mà không cần tiêu tốn tài nguyên để giám sát liên tục.

### 1.2. Cơ chế bất đồng bộ

Hầu hết các API điều khiển Wi-Fi trong ESP-IDF đều là non-blocking.

Ví dụ: Khi gọi hàm `esp_wifi_connect()`, hàm này trả về `ESP_OK` ngay lập tức để xác nhận lệnh đã được gửi đi. Tuy nhiên, quá trình kết nối thực tế như scan, auth, handshake sẽ diễn ra ngầm trong một task riêng biệt.

Do đó, không thể biết ngay kết quả kết nối tại thời điểm gọi hàm. Phải đợi phản hồi thông qua event Loop. Cơ chế này đảm bảo luồng chính không bao giờ bị treo khi mạng không ổn định.

### 1.3. Tối ưu hóa đa lõi

ESP32 là vi điều khiển dual-core và Wi-Fi stack được thiết kế để tận dụng triệt để sức mạnh này nhằm đảm bảo độ ổn định cao:
- Core 0 (PRO_CPU - Protocol CPU): Xử lý toàn bộ tác vụ của hệ thống mạng, bao gồm Wi-Fi driver, xử lý ngắt RF và ngăn xếp TCP/IP (LwIP).
- Core 1 (APP_CPU - Application CPU): Dành riêng cho logic ứng dụng của người dùng, xử lý cảm biến, màn hình.

Việc phân chia này đảm bảo rằng ngay cả khi ứng dụng gặp sự cố hoặc xử lý tính toán nặng, kết nối Wi-Fi vẫn được duy trì ổn định ở core còn lại.

## 2. Các kiến thức Wi-Fi cơ bản

### 2.1. Wi-Fi channel

Giao thức WiFi (chuẩn 802.11 b/g/n trên ESP32) hoạt động ở băng tần 2.4GHz. Ta có thể hình dung băng tần này giống như một con đường cao tốc được chia thành nhiều làn đường nhỏ, gọi là các channel. Tuy nhiên, các làn đường này không hoàn toàn tách biệt mà thường xuyên bị chồng lấn lên nhau, gây ra hiện tượng giao thoa sóng làm giảm tốc độ và độ ổn định của mạng.
- Dải kênh: Băng tần 2.4GHz được chia thành 14 kênh (channel 1-14), mỗi kênh cách nhau 5MHz. Tại Việt Nam và đa số các nước, chúng ta thường dùng từ kênh 1 đến 13.
- Độ rộng kênh (Bandwidth): Mỗi kênh WiFi thường chiếm độ rộng 20MHz hoặc 40MHz.

:::warning Chú ý
Khi cấu hình ESP32 làm SoftAP mode, ta nên thiết lập cố định vào một trong 3 kênh 1-6-11 để tín hiệu đạt hiệu suất tốt nhất, tránh nhiễu từ các router nhà mạng (thường để chế độ Auto channel).
:::

### 2.2 Beacon frame

Để một thiết bị station nhìn thấy và kết nối được với mạng WiFi, AP phải liên tục thông báo sự hiện diện của nó. Việc này được thực hiện thông qua một loại gói tin quản lý đặc biệt gọi là beacon frame, được phát broadcast ra môi trường xung quanh theo chu kỳ.
- Beacon interval: Là khoảng thời gian giữa 2 lần phát gói beacon, mặc định thường là 100ms.
  - Tăng thời gian này: Giúp thiết bị AP tiết kiệm năng lượng hơn, nhưng làm thiết bị STA chậm tìm thấy mạng hơn.
  - Giàm thời gian này: Giúp kết nối nhanh hơn nhưng chiếm dụng đường truyền nhiều hơn.
- SSID (Service Set Identifier): Là tên mạng Wifi mà người dùng nhìn thấy (ví dụ: "Wifi_Nha_Toi").
- BSSID (Basic Service Set Identifier): Là địa chỉ MAC vật lý của AP. Đây là định danh duy nhất của AP. Trong các hệ thống nhiều router có cùng tên SSID (như Mesh wifi) thì esp32 sẽ dựa vào BSSID để phân biệt và quyết định kết nối vào router nào, thường là cái ở gần nhất.

### 2.3. Cường độ tín hiệu (RSSI)

RSSI (Received Signal Strength Indicator) là chỉ số quan trọng nhất để đánh giá chất lượng liên kết giữa esp32 và router. Khác với các đại lượng đo lường thông thường, RSSI được biểu diễn bằng số âm và đơn vị là `dBm`. Giá trị càng lớn tức là càng gần số 0 thì tín hiệu càng mạnh.
- -30 dBm: Tín hiệu cực mạnh.
- -50 đến -70 dBm: Tín hiệu tốt và ổn định, lý tưởng cho việc truyền dữ liệu liên tục.
- -80 dBm: Tín hiệu yếu, có thể xảy ra hiện tượng mất gói tin.
- -90 dBm: Tín hiệu cực yếu, rất khó kết nối hoặc chập chờn.

:::tip
Khi thực hiện scan, ta nên lọc bỏ các AP có RSSI thấp hơn -85dBm để đảm bảo ESP32 không cố gắng kết nối vào các mạng quá yếu.
:::

### 2.4. Probe request & probe response

Trong quá trình tìm kiếm mạng hoặc khi kết nối vào một mạng wifi, esp32 và router sẽ thực hiện một cuộc hội thoại ngắn thông qua hai loại gói tin quản lý quan trọng sau.

**Probe request**

- Được gửi bởi các thiết bị station. Ví dụ: ESP32.
- Phân loại:
  - Broadcast: Gửi gói tin với SSID rỗng. Ý nghĩa: "Có ai ở quanh đây không? Hãy lên tiếng!". Tất cả Router nghe thấy sẽ trả lời.
  - Directed: Gửi gói tin kèm theo tên SSID cụ thể. Ý nghĩa: Hỏi đích danh "Mạng `Wifi_Nha_Toi` có ở đây không?". Đây là cách duy nhất để kết nối vào mạng ẩn (Hidden SSID). Vì router ẩn không phát tên trong gói beacon, esp32 buộc phải hỏi đúng tên mạng để router đó nhận ra và trả lời.

**Probe response**

- Được gửi bởi access point (route).
- Gói tin này chứa các thông tin cấu hình mạng tương tự như gói beacon: Tốc độ hỗ trợ, chuẩn bảo mật WPA2/WPA3, các tiện ích mở rộng...
- Router gửi gói này ngay lập tức sau khi nhận được gói probe request hợp lệ, giúp station nhận diện mạng nhanh chóng mà không cần chờ đến chu kỳ beacon tiếp theo.

## 3. Kiến trúc hệ thống

### 3.1. Tầng 1: Wi-Fi driver (Lớp vật lý & liên kết)

Tầng thấp nhất, tương tác trực tiếp với phần cứng radio (RF).
- Thành phần chính: Bao gồm các thư viện đóng (binary blobs - `.a` files) như `libnet80211.a`, `libpp.a`, `libcore.a`. Espressif không công khai source code phần này để bảo vệ bản quyền công nghệ RF và các chứng chỉ Wi-Fi Alliance.
- Nhiệm vụ:
  - Điều khiển phần cứng RF (bật/tắt radio, chuyển kênh).
  - Xử lý giao thức 802.11 (bắt tay 4 bước, mã hóa WPA2/WPA3).
  - Quản lý các gói tin management (Management frames: Beacon, Probe Request/Response).
- Cơ chế hoạt động: Chạy trên một task có độ ưu tiên rất cao để đảm bảo tính realtime. Task này xử lý các ngắt phần cứng từ bộ thu phát RF và thực hiện các tác vụ realtime của giao thức 802.11 như gửi beacon, xử lý ACK, quản lý năng lượng.

### 3.2. Tầng 2: ESP-NETIF (Lớp trừu tượng mạng)

Từ phiên bản ESP-IDF v4.1 trở đi, Espressif giới thiệu ESP-NETIF - lớp trung gian quan trọng.

Trước đây, Wi-Fi driver tương tác trực tiếp với TCP/IP stack. Điều này gây khó khăn khi cần thay đổi stack hoặc sử dụng nhiều network interface đồng thời (Wi-Fi, Ethernet, 4G).

Nhiệm vụ:
- Đóng vai trò trung gian, chuyển đổi dữ liệu từ Wi-Fi driver sang định dạng mà TCP/IP stack sử dụng và ngược lại.
- Quản lý vòng đời của interface (up/down/link status).
- Cung cấp cấu hình cho IP tĩnh, DNS server, hostname.

### 3.3. Tầng 3: LwIP (Lớp mạng & giao vận)

Thành phần trung tâm kết nối internet trên vi điều khiển. ESP-IDF sử dụng phiên bản LwIP đã được tùy biến để hỗ trợ multi-threading.

Lớp này được quản lý bởi task `tcpip_thread` có độ ưu tiên thấp hơn wifi driver nhưng cao hơn application task, với nhiệm vụ:
- Nhận dữ liệu thô từ tầng dưới, chuyển đổi thành các gói tin IP/TCP/UDP.
- Quản lý các socket, timeout của kết nối TCP.
- Xử lý DHCP, DNS resolution.

:::warning Lưu ý
LwIP có bộ nhớ riêng để chứa gói tin. Cấu hình không phù hợp lượng RAM cho LwIP thường dẫn đến lỗi "Out of memory" khi mạng không ổn định hoặc lưu lượng lớn.
:::

### 3.4. Tầng 4: Lớp ứng dụng

Lớp này là nơi code của người dùng (file `main.c`/`main.cpp`) hoạt động.

Ứng dụng không tương tác trực tiếp với driver mà thông qua:
- Socket API: `socket()`, `bind()`, `connect()`, `send()`, `recv()`.
- Event loop: Để lắng nghe trạng thái (khi nào có mạng, khi nào mất mạng).

### 3.5 Luồng dữ liệu

Để hiểu rõ hơn, hãy theo dõi hành trình của một gói tin khi tải một trang web:

**Luồng gửi (TX Path)**

- Application: Gọi `send(sock, data, ...)` trong code.
- LwIP: Dữ liệu được sao chép vào bộ đệm của LwIP, đóng gói thêm TCP header, IP header.
- ESP-NETIF: Nhận gói tin IP, chuyển xuống driver tương ứng (Wi-Fi Station interface).
- Wi-Fi driver: Thêm 802.11 MAC header, mã hóa gói tin (nếu dùng WPA2), đưa vào hàng đợi phần cứng (TX Queue).
- Hardware: Phát tín hiệu qua antenna.

**Luồng nhận (RX Path)**

- Hardware: Antenna thu tín hiệu, phần cứng RF giải mã sóng thành bit.
- Wi-Fi driver (ISR): Ngắt xảy ra, driver lấy dữ liệu từ phần cứng, kiểm tra CRC, giải mã WPA2. Nếu gói tin hợp lệ, chuyển lên lớp trên.
- ESP-NETIF: Nhận frame ethernet giả lập, chuyển thành gói tin IP.
- LwIP: Kiểm tra IP header, TCP port. Nếu đúng port đang lắng nghe, đẩy dữ liệu vào socket receive buffer.
- Application: Hàm `recv()` đang chờ sẽ được đánh thức và nhận dữ liệu.

## 4. Cơ chế hoạt động

Nếu kiến trúc hệ thống là khung xương, thì cơ chế hoạt động là nhịp tim và hệ thần kinh. Để phát triển ứng dụng Wi-Fi ổn định trên ESP-IDF, cần tư duy theo hai mô hình song song: state machine và event loop.

### 4.1. State machine

Wi-Fi driver của ESP-IDF được thiết kế dựa trên một finite state machine nghiêm ngặt. Mỗi hành động gọi API chỉ hợp lệ trong một trạng thái cụ thể. Việc vi phạm quy tắc chuyển trạng thái sẽ dẫn đến lỗi hệ thống hoặc hành vi không xác định.

Chu trình hoạt động chuẩn bao gồm 5 phase:

**1. Initialization phase**

- Trạng thái: `Uninitialized` -> `Init`.
- API: `esp_wifi_init()`.
- Đặc điểm: Driver cấp phát tài nguyên RAM, khởi tạo cấu trúc dữ liệu điều khiển. Lúc này, RF vẫn TẮT, dòng tiêu thụ thấp.

**2. Configuration phase**

- Trạng thái: `Init`.
- API: `esp_wifi_set_mode()`, `esp_wifi_set_config()`.
- Quy tắc: Chỉ được phép cấu hình khi driver đã `Init` nhưng chưa `Start`.

**3. Start phase**

- Trạng thái: `Init` -> `Started`.
- API: `esp_wifi_start()`.
- Đặc điểm: Bật nguồn cho RF, nạp calibration data từ NVS. Driver bắt đầu hoạt động, nhưng chưa kết nối.
- Sự kiện: `WIFI_EVENT_STA_START`.

**4. Connection phase**

- Trạng thái: `Started` -> `Connected`.
- API: `esp_wifi_connect()`.
- Chi tiết: Scan SSID, Bắt tay 4 bước.
- Sự kiện:
  - Thành công: `WIFI_EVENT_STA_CONNECTED`.
  - Thất bại: `WIFI_EVENT_STA_DISCONNECTED`.

**5. Network phase**

- Trạng thái: `Connected` -> `Got IP`.
- Tự động: ESP-NETIF kích hoạt DHCP client ngay khi kết nối.
- Kết quả: Nhận địa chỉ IP. Đây là trạng thái cần đạt được để truy cập internet.
- Sự kiện: `IP_EVENT_STA_GOT_IP`

### 4.2. Kiến trúc event loop

Do tính chất bất đồng bộ, ESP-IDF sử dụng mô hình publisher-subscriber để thông báo trạng thái thay vì yêu cầu ứng dụng phải polling liên tục. Hệ thống sử dụng thư viện `esp_event` làm xương sống.

Quy trình xử lý một sự kiện diễn ra như sau:

- Posting: Wi-Fi driver phát hiện kết nối thành công -> Gửi sự kiện vào queue.
- Queuing: Sự kiện nằm trong Queue chờ xử lý. Driver tiếp tục làm việc khác ngay lập tức.
- Dispatching: Task `sys_evt` lấy sự kiện ra, tra cứu hàm nào đã đăng ký lắng nghe sự kiện này.
- Handling: Task `sys_evt` gọi hàm `event_handler`. Lưu ý: Hàm này chạy trong context của task `sys_evt`, không phải task main.

ESP-IDF cho phép tạo nhiều event loop:
- Default event loop: Được tạo bởi hệ thống `esp_event_loop_create_default()`. Dùng chung cho Wi-Fi, Ethernet, IP events. Đây là lựa chọn phổ biến nhất.
- User event loop: Có thể tự tạo `esp_event_loop_create()` để xử lý các sự kiện riêng của ứng dụng. Ví dụ: sự kiện từ cảm biến nhiệt độ, nút nhấn nhằm tránh làm tắc nghẽn Loop hệ thống.

Bên trong, event loop quản lý một danh sách liên kết các handler:
- Mỗi node chứa: Event Base, Event ID, và con trỏ hàm Callback.
- Khi gọi `esp_event_handler_instance_register()`, đang thêm một node vào danh sách này.

:::warning Stack overflow
Hàm `event_handler` chạy trong ngữ cảnh của task hệ thống (`sys_evt`) với stack mặc định thường là 4KB.

- Rủi ro: Khai báo biến cục bộ lớn (ví dụ: `char buffer[4096]`) ngay trong handler sẽ gây crash chip lập tức.
- Giải pháp: Sử dụng cấp phát động hoặc biến toàn cục cho các bộ đệm lớn.
:::

:::tip Tối ưu hóa queue
Trong trường hợp ứng dụng sinh ra quá nhiều sự kiện cùng lúc khiến sự kiện quan trọng bị drop, có thể tăng kích thước queue tại:
`idf.py menuconfig` -> `Component config` -> `Event Loop Library` -> `Event loop queue size`
:::

## 5. Event ID reference

Để xử lý logic chính xác, cần hiểu ý nghĩa của từng sự kiện. ESP-IDF chia sự kiện thành hai nhóm lớn:
- `WIFI_EVENT` (Tầng vật lý/MAC)
- `IP_EVENT` (Tầng mạng)

### 5.1. WIFI_EVENT

| Event ID | Điều kiện kích hoạt | Dữ liệu kèm theo | Hành động khuyến nghị |
| -------- | ------------------- | ---------------- | --------------------- |
| `WIFI_EVENT_STA_START` | Driver Wi-Fi đã khởi động thành công | Không | Gọi `esp_wifi_connect()` |
| `WIFI_EVENT_STA_CONNECTED` | Đã kết nối thành công đến AP ở tầng MAC | `wifi_event_sta_connected_t` (BSSID, channel, authmode) | Đợi nhận IP |
| `WIFI_EVENT_STA_DISCONNECTED` | Mất kết nối (do timeout, AP tắt, hoặc lỗi auth) | `wifi_event_sta_disconnected_t` (reason code) | Phân tích reason code để quyết định có kết nối lại hay không |
| `WIFI_EVENT_SCAN_DONE` | Quá trình scan kết thúc | `wifi_event_sta_scan_done_t` | Gọi `esp_wifi_scan_get_ap_records()` để lấy kết quả |
| `WIFI_EVENT_AP_START` | SoftAP đã khởi động | Không | Không cần xử lý |
| `WIFI_EVENT_AP_STACONNECTED` | Có client kết nối vào SoftAP | `wifi_event_ap_staconnected_t` (MAC address) | Log hoặc cấp phát tài nguyên |
| `WIFI_EVENT_AP_STADISCONNECTED` | Client ngắt kết nối | `wifi_event_ap_stadisconnected_t` (MAC address) | Giải phóng tài nguyên |

### 5.2. IP_EVENT

| Event ID | Điều kiện kích hoạt | Dữ liệu kèm theo | Hành động khuyến nghị |
| -------- | ------------------- | ---------------- | --------------------- |
| `IP_EVENT_STA_GOT_IP` | DHCP client đã nhận được địa chỉ IP | `ip_event_got_ip_t` (IP, netmask, gateway) | Bắt đầu các tác vụ mạng (HTTP, MQTT) |
| `IP_EVENT_STA_LOST_IP` | Mất địa chỉ IP (thường do DHCP lease hết hạn) | Không | Dừng các kết nối mạng, chờ renew |
| `IP_EVENT_GOT_IP6` | Nhận được IPv6 address | `ip_event_got_ip6_t` | Chỉ cần thiết nếu sử dụng IPv6 |

### 5.3. Reason code quan trọng

Khi nhận được `WIFI_EVENT_STA_DISCONNECTED`, cần kiểm tra trường `reason` để xác định nguyên nhân:

| Reason Code | Ý nghĩa | Xử lý khuyến nghị |
| ----------- | ------- | ----------------- |
| `WIFI_REASON_AUTH_FAIL` (2) | Sai mật khẩu | Không kết nối lại tự động, yêu cầu nhập lại password |
| `WIFI_REASON_NO_AP_FOUND` (201) | Không tìm thấy AP | Có thể kết nối lại sau delay |
| `WIFI_REASON_ASSOC_LEAVE` (8) | ESP32 tự ngắt kết nối | Đây là hành động chủ động, không cần xử lý |
| `WIFI_REASON_BEACON_TIMEOUT` (200) | Mất tín hiệu AP (đi ra ngoài phạm vi) | Kết nối lại tự động |
| `WIFI_REASON_HANDSHAKE_TIMEOUT` (204) | Lỗi trong quá trình handshake 4-way | Kết nối lại, có thể do nhiễu |

## 6. Threading model

ESP-IDF có một mô hình đa luồng phức tạp. Hiểu rõ thread nào chạy code nào sẽ giúp tránh deadlock và race condition.

### 6.1. Các task chính trong Wi-Fi stack

| Task name | Core | Độ ưu tiên | Nhiệm vụ |
| --------- | ---- | ---------- | -------- |
| `wifi` | 0 | 23 | Xử lý ngắt Wi-Fi, gửi/nhận frame 802.11 |
| `tcpip_task` | 0 | 18 | Xử lý gói tin IP/TCP/UDP (LwIP) |
| `sys_evt` | 0 | 20 | Dispatch events (Default Event Loop) |
| `main` | 1 | 1 | Task khởi động, thường là nơi gọi `app_main()` |

### 6.2. Quy tắc gọi API từ các task

**1. Wifi API có thể gọi từ bất kỳ task nào**

Các hàm như `esp_wifi_connect()`, `esp_wifi_disconnect()`, `esp_wifi_scan_start()` đều thread-safe. ESP-IDF sử dụng mutex nội bộ để bảo vệ.

**2. Không gọi blocking API trong event handler**

Event handler chạy trong context của `sys_evt` task. Nếu block task này, toàn bộ hệ thống event sẽ bị đóng băng.

Ví dụ sai:
```c
static void event_handler(void* arg, esp_event_base_t event_base, 
                          int32_t event_id, void* event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // TUYỆT ĐỐI KHÔNG ĐƯỢC LÀM VIỆC NÀY
    }
}
```

Cách làm đúng:
```c
static TaskHandle_t network_task_handle = NULL;

static void event_handler(void* arg, esp_event_base_t event_base, 
                          int32_t event_id, void* event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP) {
        xTaskNotifyGive(network_task_handle); // Chỉ set cờ, không xử lý logic
    }
}

void network_task(void *pvParameters)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // Chờ tín hiệu
        // Xử lý logic nặng tại đây
    }
}
```

**3. Các LwIP API không phải lúc nào cũng thread safe**

Một số hàm socket như `send()`, `recv()` là thread-safe. Nhưng các hàm như `setsockopt()`, `bind()` không được đảm bảo. Khuyến nghị là chỉ gọi các API LwIP từ một task duy nhất hoặc sử dụng mutex để bảo vệ.

## 7. Memory management

RAM là tài nguyên khan hiếm trên ESP32. Wi-Fi stack tiêu tốn một lượng đáng kể bộ nhớ.

### 7.1. Phân bổ RAM tĩnh

Khi gọi `esp_wifi_init()`, driver cấp phát các vùng nhớ sau:

| Thành phần | Kích thước ước tính | Mục đích |
| ---------- | ------------------- | -------- |
| TX buffers | ~14 KB | Lưu trữ gói tin đang chờ gửi |
| RX buffers | ~8 KB | Lưu trữ gói tin vừa nhận |
| Control structures | ~5 KB | State machine, counters, timers |
| NVS cache | ~4 KB | Cache cấu hình Wi-Fi để đọc/ghi nhanh |

Tổng cộng: **~31-35 KB RAM** chỉ riêng cho Wi-Fi driver (chưa tính LwIP).

### 7.2. Phân bổ RAM động

LwIP sử dụng một heap riêng biệt gọi là **LWIP HEAP**. Kích thước mặc định là 32 KB. Nếu ứng dụng mở nhiều socket đồng thời hoặc xử lý lưu lượng lớn, cần tăng:

```
idf.py menuconfig
-> Component config
  -> LWIP
    -> TCP
      -> Maximum number of listening TCP connections (tăng từ 16 lên 32)
    -> LWIP_MEM_SIZE (tăng từ 32KB lên 48KB)
```

### 7.3. Chiến lược tối ưu bộ nhớ

**1. Giảm số lượng TX/RX buffer**

```c
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
cfg.static_rx_buf_num = 4;  // Giảm từ 10 xuống 4
cfg.dynamic_rx_buf_num = 8; // Giảm từ 32 xuống 8
cfg.static_tx_buf_num = 0;  // Chuyển sang dùng dynamic
esp_wifi_init(&cfg);
```

Lưu ý: Giảm quá mức sẽ dẫn đến mất gói tin khi lưu lượng tăng đột biến.

**2. Disable features không dùng**

Nếu không cần IPv6:
```
idf.py menuconfig -> Component config -> LWIP -> Enable IPv6 [Disable]
```

**3. Sử dụng PSRAM cho buffer lớn**

ESP32 có thể mở rộng RAM bằng PSRAM ngoài (lên tới 8MB). Tuy nhiên, tốc độ truy cập PSRAM chậm hơn RAM nội (80MHz vs 240MHz), nên chỉ nên dùng cho buffers không cần realtime.

```c
char *large_buffer = (char *)heap_caps_malloc(100*1024, MALLOC_CAP_SPIRAM);
```

## 8. Quy trình khởi tạo Wi-Fi chuẩn

Đây là flow chuẩn mực được khuyến nghị bởi Espressif:

**Phase 1: Khởi tạo nền tảng**

1. Khởi tạo NVS Flash:
   Wi-Fi driver lưu trữ thông tin calibration và cấu hình Wi-Fi vào NVS.
   
   ```c
   esp_err_t ret = nvs_flash_init();
   if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
       ESP_ERROR_CHECK(nvs_flash_erase());
       ret = nvs_flash_init();
   }
   ```
2. Khởi tạo network interface:
   :::code-group
   ```c [sta]
   ESP_ERROR_CHECK(esp_netif_init());
   esp_netif_create_default_wifi_sta();
   ```
   ```c [soft-ap]
   ESP_ERROR_CHECK(esp_netif_init());
   esp_netif_create_default_wifi_ap();
   ```
   :::
4. Tạo Default Event Loop:
   ```c
   ESP_ERROR_CHECK(esp_event_loop_create_default());
   ```

**Phase 2: Cấu hình Wi-Fi**

4. Khởi tạo Wi-Fi driver:
   Sử dụng cấu hình mặc định để cấp phát tài nguyên cho driver.
   
   ```c
   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
   ESP_ERROR_CHECK(esp_wifi_init(&cfg));
   ```
5. Register event handlers: Đăng ký hàm callback để xử lý các sự kiện WIFI_EVENT và IP_EVENT.
   ```c
   esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL);
   esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, NULL);
   ```
6. Set Wi-Fi mode & configuration:
   - Chọn mode (STA/AP).
   - Nạp thông tin SSID, password vào struct `wifi_config_t`.

   :::code-group
   ```c [sta]
   wifi_config_t wifi_config = {
       .sta = {
           .ssid = "your_ssid",
           .password = "your_password",
       },
   };
   ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
   ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
   ```

   ```c [soft-ap]
   wifi_config_t wifi_config = {
       .ap = {
           .ssid = "ESP32_AP",
           .ssid_len = strlen("ESP32_AP"),
           .password = "12345678",
           .max_connection = 4,
           .authmode = WIFI_AUTH_WPA_WPA2_PSK
       },
   };

   ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
   ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
   ```
   :::

**Phase 3: Kích hoạt**

Start Wi-Fi: Gọi `esp_wifi_start()`.

**Phase 4: Xử lý event**

Ví dụ cơ bản:
:::code-group
```c [sta]
static void wifi_event_handler(void* arg,
                                esp_event_base_t event_base,
                                int32_t event_id,
                                void* event_data)
{
    switch (event_id)
    {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        
        case WIFI_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            break;

        default:
            break;
    }
}
```
```c [soft-ap]
static void wifi_event_handler(void* arg,
                                esp_event_base_t event_base,
                                int32_t event_id,
                                void* event_data)
{
    switch (event_id)
    {
        case WIFI_EVENT_AP_STACONNECTED:
            wifi_event_ap_staconnected_t *ev = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "STA joined: " MACSTR ", AID=%d", MAC2STR(ev->mac), ev->aid);
            break;

        default:
            break;
    }
}
```
:::

## 9. Power management

Wi-Fi là thành phần tiêu thụ năng lượng lớn nhất trên ESP32.

| Trạng thái                  | Dòng tiêu thụ ước tính    |
| --------------------------- | ------------------------- |
| Idle (CPU nhẹ)              | ~20–40mA                  |
| Wi-Fi hoạt động bình thường | ~80–120mA                 |
| TX peak                     | có thể lên tới ~200–240mA |

Nếu dùng pin mà không cấu hình power save thì pin sẽ cạn rất nhanh.

ESP-IDF cung cấp cơ chế modem sleep dựa trên chuẩn 802.11 legacy power save.

### 9.1. Nguyên lý hoạt động

Trong Wi-Fi, AP phát một gói gọi là beacon theo chu kỳ (thường mỗi 100ms).

Trong beacon có một trường gọi là DTIM (Delivery Traffic Indication Message).

Cơ chế hoạt động như sau:
- ESP32 báo với router rằng nó sẽ ngủ và chỉ thức dậy theo chu kỳ để nhận beacon.
- esp32 tắt RF để tiết kiệm điện.
- Đến chu kỳ DTIM, ESP32 thức dậy ngắn hạn để:
  - Nghe beacon
  - Kiểm tra xem router có dữ liệu gửi cho nó không
- Nếu router có dữ liệu cần gửi cho ESP32, nó sẽ báo hiệu trong gói tin beacon. Khi đó, ESP32 sẽ thức dậy hoàn toàn để nhận dữ liệu.
- Nếu không có dữ liệu, ESP32 tắt RF và ngủ tiếp.

### 9.2. Các chế độ power save

Sử dụng API `esp_wifi_set_ps(wifi_ps_type_t type)` để cấu hình:

| Chế độ | Mô tả | Ưu điểm | Nhược điểm |
| ------ | ----- | ------- | ---------- |
| `WIFI_PS_MIN_MODEM` (Mặc định) | Thức dậy theo mỗi chu kỳ DTIM (thường là 1 hoặc 3 beacon intervals). | Cân bằng tốt giữa điện năng và độ trễ. Giảm dòng trung bình xuống ~20-30mA. | Độ trễ mạng tăng nhẹ. |
| `WIFI_PS_MAX_MODEM` | Thức dậy theo chu kỳ DTIM * Listen Interval (có thể cấu hình). | Tiết kiệm điện tối đa. | Độ trễ cao. Có thể rớt gói tin Broadcast nếu router không buffer tốt. |
| `WIFI_PS_NONE` | Radio luôn bật. | Độ trễ thấp nhất, băng thông cao nhất. | Tiêu thụ năng lượng lớn (~100-240mA liên tục). Chỉ dùng khi cắm nguồn DC. |

## 10. Wi-Fi scanning

Scanning là quá trình Wi-Fi driver thực hiện tìm kiếm các AP xung quanh. Hiểu rõ scanning giúp tối ưu thời gian kết nối và chọn channel sạch.

Trong quá trình scan, driver sẽ quét các channel tùy thuộc vào cấu hình quét toàn bộ channel (default) hoặc một channel cụ thể tùy thuộc vào struct `wifi_scan_config_t`.

Sau khi scan xong thì driver sẽ lưu danh sách AP tìm được trong bộ nhớ động của Wi-Fi driver. Danh sách này sẽ được lấy ra bằng API `esp_wifi_scan_get_ap_records()` và được giải phóng bằng `esp_wifi_clear_ap_list()`.

:::warning Chú ý
Thực hiện scan khi đã được connect thì có thể làm kết nối kém ổn định nếu thời gian scan lớn -> Cần giới hạn thời gian scan.
:::

### 10.1. Các loại scan

- Active scan:
  - ESP32 gửi gói tin probe request trên từng channel.
  - Các AP nghe thấy sẽ trả lời bằng probe response chứa SSID, RSSI, Auth Mode.
  - Ưu điểm: Nhanh, tìm được các mạng ẩn (Hidden SSID).
  - Nhược điểm: Gây nhiễu môi trường sóng, tốn năng lượng hơn.
- Passive scan:
  - ESP32 chỉ im lặng nhảy qua từng channel và lắng nghe gói tin beacon định kỳ của router.
  - Ưu điểm: Tiết kiệm điện, không làm lộ diện thiết bị.
  - Nhược điểm: Chậm (phải đợi hết chu kỳ beacon ~100ms), không tìm được hidden SSID.

### 10.2. Chiến lược scan

**Blocking scan**

Thực hiện scan và chờ cho đến khi scan hoàn tất. Trong quá trình này, nó sẽ block task hiện tại.

```c
esp_wifi_scan_start(&scan_config, true); 
// Code tại đây chỉ chạy sau khi scan xong (~1-2 giây)
```

Khuyến nghị: Chỉ dùng trong giai đoạn khởi tạo hoặc CLI command.

**Non-blocking scan**

Đây là cách làm chuyên nghiệp.

- Gọi lệnh scan và trả về ngay lập tức
   ```c
   esp_wifi_scan_start(&scan_config, false);
   ```
- Chờ sự kiện `WIFI_EVENT_SCAN_DONE` trong event handler.

**Fast scan**

Để giảm thời gian connect (từ 2s xuống <500ms), có thể giới hạn phạm vi quét:
- Specific Channel: Chỉ quét channel đã lưu trong NVS.
- Specific SSID: Chỉ gửi probe request cho SSID cụ thể.

### 10.3. Ví dụ

```c
if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count); // Lấy số lượng AP tìm thấy

    if (ap_count > 0) {
        wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
        esp_wifi_scan_get_ap_records(&ap_count, ap_list);
        
        ESP_LOGI(TAG, "Found %d APs:", ap_count);
        for (int i = 0; i < ap_count; i++) {
            ESP_LOGI(TAG, "SSID: %s | RSSI: %d | Auth: %d", 
                     ap_list[i].ssid, ap_list[i].rssi, ap_list[i].authmode);
        }
        free(ap_list); // Đừng quên giải phóng bộ nhớ
    }
}
```

### 10.4. Scan configuration

Cấu hình qua struct `wifi_scan_config_t`:

| Field | Ý nghĩa |
| ----- | ------- |
| ssid | Nếu khác `NULL`, chỉ tìm AP có SSID này. |
| bssid | Nếu khác `NULL`, chỉ tìm AP có MAC address này. |
| channel | Nếu set (1-13), chỉ quét duy nhất channel này (cực nhanh). Nếu 0, quét toàn bộ. |
| show_hidden | true để hiện các mạng ẩn. |
| scan_type | `WIFI_SCAN_TYPE_ACTIVE` hoặc `WIFI_SCAN_TYPE_PASSIVE`. |
| scan_time | Cấu hình thời gian dừng lại ở mỗi channel. |
