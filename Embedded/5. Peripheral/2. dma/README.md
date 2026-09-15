## 1. Tổng quan

Direct Memory Access (DMA) là một khối phần cứng cho phép dữ liệu được chuyển trực tiếp giữa ngoại vi và bộ nhớ hoặc giữa hai vùng nhớ mà không cần CPU tham gia vào từng byte.

Trong các hệ thống nhúng như audio streaming, điều khiển màn hình LCD, thu thập dữ liệu ADC tốc độ cao hoặc truyền thông SPI/Ethernet, lượng dữ liệu cần xử lý thường lớn và liên tục. Nếu CPU phải tham gia vào từng lần truyền, hệ thống sẽ nhanh chóng bị quá tải. DMA được thiết kế để giải quyết chính bài toán này.

Xuyên suốt tài liệu, ta dùng hình ảnh một nhà hàng:
- **CPU** là bếp trưởng: người phụ trách những việc cần chất xám (xử lý logic, thuật toán).
- **Ngoại vi** (ADC, UART, SPI,...) là quầy ra món: nơi dữ liệu được tạo ra hoặc cần được đưa tới.
- **DMA** là phụ bếp chuyên vận chuyển: chỉ làm một việc là bưng bê dữ liệu từ chỗ này sang chỗ khác.

Nếu không có phụ bếp, bếp trưởng phải tự tay bưng từng đĩa (copy từng byte), không còn thời gian nấu món chính. Có phụ bếp, bếp trưởng chỉ cần giao việc một lần: "Chuyển 100 đĩa từ quầy sang bàn số 5, xong thì báo tôi", rồi quay lại nấu ăn.

## 2. Tại sao cần DMA

Để thấy rõ vai trò của DMA, ta so sánh ba cách đưa dữ liệu từ ngoại vi vào RAM: polling, interrupt và DMA.

Xét cùng một bài toán: thu ADC ở tần số 1 MHz, độ phân giải 16-bit.

$$1\,000\,000 \text{ mẫu/giây} \times 2 \text{ byte} = 2 \text{ MB/giây}$$

### 2.1. Polling

CPU liên tục kiểm tra cờ sẵn sàng dữ liệu và đọc thanh ghi:

```c
while (1) {
    if (ADC_DR_READY) {
        buffer[i++] = ADC_DR;
    }
}
```

Nhược điểm:
- CPU luôn bận chờ dữ liệu, không có thời gian cho tác vụ khác.
- Không phù hợp với hệ thống đa nhiệm.
- Tiêu thụ năng lượng cao do CPU không bao giờ được nghỉ.

Với lưu lượng 2 MB/giây, CPU gần như bị chiếm dụng hoàn toàn chỉ để sao chép dữ liệu.

### 2.2. Interrupt

Để CPU không phải bận chờ, ta cấu hình ngoại vi phát interrupt mỗi khi có dữ liệu mới:

```c
void ADC_IRQHandler(void) {
    buffer[i++] = ADC_DR;
}
```

CPU đã được rảnh giữa các mẫu, nhưng vấn đề chuyển sang chỗ khác: với 1 MHz, hệ thống nhận 1000000 interrupt mỗi giây. Mỗi lần ngắt, CPU phải lưu ngữ cảnh, đọc thanh ghi, ghi vào RAM rồi khôi phục ngữ cảnh. Chi phí vào/ra ngắt lớn hơn nhiều so với chính thao tác copy 2 byte.

Hậu quả: CPU vẫn quá tải, tốn năng lượng, độ trễ ngắt tăng và có thể bỏ lỡ các tác vụ quan trọng khác.

### 2.3. DMA

Nhận xét chung của hai cách trên: CPU tham gia vào từng mẫu dữ liệu. DMA loại bỏ đúng điểm này:

1. CPU cấu hình DMA một lần: địa chỉ nguồn, địa chỉ đích, số lượng dữ liệu, chế độ (chi tiết ở [Phần 4](#4-cấu-hình-một-kênh-dma)).
2. DMA tự động chuyển dữ liệu mỗi khi ngoại vi sẵn sàng.
3. Khi chuyển xong cả khối, DMA phát một interrupt.
4. ISR báo cho task xử lý dữ liệu.

Với ví dụ ADC ở trên, nếu buffer 1000 mẫu thì CPU chỉ nhận khoảng 1000 ngắt/giây thay vì 1 000 000.

Lợi ích:
- **Giải phóng CPU:** Trong khi DMA vận chuyển dữ liệu, CPU có thể vào sleep mode để tiết kiệm điện hoặc chạy các thuật toán như bộ lọc số, PID, xử lý ảnh,...
- **Tăng thông lượng:** DMA controller được thiết kế chuyên cho việc copy dữ liệu, không phải fetch/decode lệnh hay lưu ngữ cảnh, nên nhanh hơn nhiều so với CPU chạy lặp lại các lệnh `LDR`/`STR`.
- **Đảm bảo tính thời gian thực:** Với các ngoại vi tốc độ cao như audio (I2S), camera (DCMI) hay ADC lấy mẫu nhanh, DMA là cách thực tế để theo kịp tốc độ dữ liệu mà không làm treo hệ thống.

Vậy làm sao DMA tự đọc/ghi bộ nhớ khi không có CPU và làm sao nó biết lúc nào ngoại vi có dữ liệu? Đó là nội dung của phần 3.

## 3. Cơ chế hoạt động

DMA cần hai khả năng để làm việc độc lập với CPU:
1. **Truy cập bộ nhớ và ngoại vi** $\rightarrow$ DMA là một bus master ([3.1](#31-dma-là-một-bus-master)).
2. **Biết thời điểm cần chuyển dữ liệu** $\rightarrow$ DMA nhận tín hiệu request từ ngoại vi ([3.2](#32-dma-request)).

### 3.1. DMA là một bus master

Trên vi điều khiển, CPU không phải thiết bị duy nhất được quyền ra lệnh trên bus. DMA cũng là một bus master: nó có thể tự phát địa chỉ và đọc/ghi SRAM, Flash hay thanh ghi ngoại vi giống như CPU.

```
  ┌──────┐
  │ CPU  │──┐
  └──────┘  │   ┌─────────────┐
            ├──►│ Bus Matrix  │──► SRAM / Flash
  ┌──────┐  │   └─────────────┘
  │ DMA  │──┘          │
  └──────┘             └──► APB/AHB Peripherals
     ▲
     │ DMA request
┌────┴────┐
│ADC/UART │
└─────────┘
```

Bus matrix đóng vai trò trọng tài (bus arbitration). Khi DMA muốn đọc/ghi, nó yêu cầu bus matrix cấp quyền truy cập tới slave tương ứng.

:::warning Cycle stealing
DMA không hoàn toàn chạy song song với CPU. Khi CPU và DMA cùng truy cập một slave tại cùng thời điểm, bus matrix chỉ cho một bên đi qua, bên còn lại phải chờ - hiện tượng này gọi là cycle stealing. Tuy nhiên, mỗi lần truyền DMA chỉ chiếm bus trong vài chu kỳ, nên ảnh hưởng tới CPU là rất nhỏ so với việc CPU tự copy dữ liệu.
:::

### 3.2. DMA request

Một hiểu lầm phổ biến là DMA giống như camera giám sát, liên tục nhìn vào ngoại vi để xem có dữ liệu hay không. Thực tế, DMA không biết gì về trạng thái bên trong ngoại vi:
- DMA không biết khi nào ADC chuyển đổi xong.
- DMA không biết khi nào UART nhận được byte mới.
- DMA không biết khi nào timer tràn.

Nếu DMA tự ý đọc liên tục thanh ghi dữ liệu, nó sẽ đọc phải dữ liệu cũ chưa được cập nhật, và còn gây tranh chấp bus không cần thiết. Vì vậy, DMA cần một cơ chế đồng bộ: hardware request signal.

Quay lại về ví dụ nhà hàng:
- **Không có request (polling):** Phụ bếp cứ vài giây lại chạy tới quầy hỏi "Có món chưa?". Phần lớn các lần đều về tay không - tốn sức và làm rối bếp.
- **Có request:** Phụ bếp đứng chờ (idle). Khi quầy có món, quầy bấm chuông (request). Nghe chuông, phụ bếp mới tới lấy đúng đĩa đó mang đi. Lấy xong, chuông tắt.

Tương ứng trong phần cứng:
- Ngoại vi được bật chức năng DMA (ví dụ bit `DMAR`/`DMAT` của UART, bit `DMA` của ADC).
- Khi có sự kiện (UART `RXNE`, ADC `EOC`, timer update,...), thay vì (hoặc bên cạnh việc) bật cờ ngắt cho CPU, ngoại vi kéo đường request tới kênh DMA tương ứng.
- DMA thực hiện một lần truyền, và thao tác đọc/ghi thanh ghi dữ liệu thường tự xóa request.

:::warning Lưu ý
Mỗi ngoại vi chỉ được nối tới một số kênh/stream DMA cố định (hoặc chọn qua bộ ghép kênh request mux). Cần tra bảng mapping trong reference manual của chip để chọn đúng kênh.
:::

### 3.3. Luồng một phiên truyền

Ghép hai cơ chế trên lại, một phiên truyền DMA diễn ra như sau:

1. CPU cấu hình kênh DMA (nguồn, đích, số lượng, chế độ) và bật DMA trên cả ngoại vi lẫn DMA controller.
2. Ngoại vi có dữ liệu mới $\rightarrow$ phát DMA request.
3. DMA xin quyền dùng bus (bus arbitration).
4. DMA đọc một đơn vị dữ liệu từ địa chỉ nguồn $\rightarrow$ ghi vào địa chỉ đích.
5. DMA giảm bộ đếm, tăng con trỏ (nếu bật increment).
6. Lặp lại bước 2-5 đến khi bộ đếm về 0 $\rightarrow$ sinh ngắt Transfer Complete (TC).
7. Tùy chế độ: dừng lại hoặc tự nạp lại và chạy tiếp ([Phần 5](#5-các-chế-độ-hoạt-động)).

Mỗi bước trên tương ứng với một nhóm tham số cấu hình, được trình bày ở phần tiếp theo.

## 4. Cấu hình một kênh DMA

Trước khi DMA chạy, CPU phải "giao việc" cho nó. Mỗi kênh (channel/stream) DMA được cấu hình bởi các nhóm tham số sau:

| Nhóm tham số | Trả lời câu hỏi | Mục |
|---|---|---|
| Direction & trigger | Chuyển theo hướng nào, khi nào chuyển? | [4.1](#41-direction--trigger) |
| Source / Destination | Lấy ở đâu, ghi vào đâu, mỗi lần bao nhiêu bit? | [4.2](#42-source--destination) |
| Transfer size | Chuyển bao nhiêu đơn vị dữ liệu? | [4.3](#43-transfer-size) |
| Priority | Ai được ưu tiên khi nhiều kênh cùng tranh chấp? | [4.4](#44-priority) |
| Mode | Chuyển xong thì làm gì? | [Phần 5](#5-các-chế-độ-hoạt-động) |

### 4.1. Direction & trigger

**Direction** xác định vai trò của nguồn và đích:
- **Peripheral-to-Memory (P2M):** Phổ biến nhất. Thu dữ liệu từ ADC, UART, SPI vào bộ đệm RAM.
- **Memory-to-Peripheral (M2P):** Đẩy dữ liệu từ RAM ra ngoại vi, ví dụ phát âm thanh qua I2S, gửi gói tin qua UART.
- **Memory-to-Memory (M2M):** Sao chép giữa hai vùng nhớ, giống `memcpy` nhưng bằng phần cứng.

**Trigger** xác định thời điểm mỗi đơn vị dữ liệu được chuyển:
- **Hardware request:** Tín hiệu từ ngoại vi như đã mô tả ở [3.2](#32-dma-request). Đây là chế độ mặc định cho P2M và M2P, vì tốc độ truyền phải khớp với tốc độ của ngoại vi.
- **Software trigger:** Không chờ request, DMA chạy liên tục với tốc độ tối đa ngay khi được bật. Chỉ dùng cho M2M, vì bộ nhớ luôn "sẵn sàng".

### 4.2. Source & destination

Nguồn là nơi DMA lấy dữ liệu, đích là nơi DMA ghi dữ liệu. Cả hai đều được mô tả bằng cùng 3 tham số:

**Base address (địa chỉ cơ sở)**
- Với ngoại vi: địa chỉ thanh ghi dữ liệu, ví dụ `&USART1->DR`, `&ADC1->DR`, `&DAC->DHR12R1`.
- Với bộ nhớ: địa chỉ mảng, ví dụ `rxBuffer` hoặc `&rxBuffer[0]`.

**Data width (độ rộng dữ liệu)**: mỗi đơn vị dữ liệu rộng bao nhiêu bit.
- Tùy chọn: Byte (8-bit), Half-word (16-bit), Word (32-bit).
- Độ rộng phía ngoại vi phải khớp với thanh ghi. Truy cập 32-bit vào một thanh ghi 8-bit có thể đọc sai dữ liệu hoặc gây bus fault.
- Độ rộng phía bộ nhớ phải khớp với kiểu phần tử của mảng (`uint8_t`, `uint16_t`, `uint32_t`).
- Nếu nguồn và đích khác độ rộng, DMA vẫn chuyển **theo từng item** (cắt bớt hoặc thêm bit 0 tùy hãng) chứ không gom nhiều byte thành một word. Nên cấu hình hai bên giống nhau để tránh bất ngờ.

**Address increment (tăng địa chỉ)**: sau mỗi item, con trỏ có tự tăng không.
- **Enable:** Dùng cho bộ nhớ. Ghi xong `buffer[0]` thì chuyển sang `buffer[1]`.
- **Disable:** Dùng cho ngoại vi. Thanh ghi dữ liệu nằm cố định tại một địa chỉ, nên con trỏ phải đứng yên.

Quy tắc nhớ nhanh theo direction:

| Direction | Increment nguồn | Increment đích |
|---|---|---|
| P2M (ADC $\rightarrow$ RAM) | Disable | Enable |
| M2P (RAM $\rightarrow$ DAC) | Enable | Disable |
| M2M (Flash $\rightarrow$ RAM) | Enable | Enable |

### 4.3. Transfer size

Transfer size là số lượng item DMA cần chuyển trong một phiên.
- **Thanh ghi quản lý:** thường là `NDTR`/`CNDTR` (Number of Data to Transfer) hoặc `LEN`.
- **Cơ chế đếm:** Mỗi item chuyển xong, giá trị này tự động giảm 1. Đọc thanh ghi này bất kỳ lúc nào sẽ biết còn bao nhiêu item chưa chuyển - đây là cơ sở để tính vị trí ghi hiện tại trong buffer (dùng ở [Phần 6](#6-ví-dụ-nhận-dữ-liệu-uart-bằng-dma)).
- **Khi bộ đếm về 0:** cờ TC (Transfer Complete) được bật để báo CPU. Việc DMA dừng hẳn hay tự nạp lại phụ thuộc vào mode ([Phần 5](#5-các-chế-độ-hoạt-động)).
- Ngoài TC, DMA thường có cờ HT (Half Transfer) khi chuyển được một nửa, và TE (Transfer Error) khi có lỗi bus.

:::warning Lưu ý
Size là số lượng **item**, không phải số byte. Nếu data width là 16-bit (2 byte) và size = 10 thì tổng dung lượng là 20 byte. Buffer phía bộ nhớ phải đủ lớn tương ứng.
:::

### 4.4. Priority

Khi nhiều kênh DMA cùng có request, DMA controller dùng priority (Low / Medium / High / Very High, hoặc theo số thứ tự kênh tùy chip) để quyết định kênh nào được phục vụ trước. Kênh gắn với ngoại vi có tốc độ cao hoặc dễ mất dữ liệu (ADC, I2S RX) nên được ưu tiên cao hơn kênh gửi dữ liệu không gấp (UART TX log).

## 5. Các chế độ hoạt động

Tham số cuối cùng trong bảng ở [Phần 4](#4-cấu-hình-một-kênh-dma) trả lời câu hỏi: **khi bộ đếm về 0 thì DMA làm gì?** Lựa chọn phụ thuộc vào tính chất dữ liệu (một khối rời rạc hay một luồng liên tục) và cách CPU xử lý dữ liệu.

### 5.1. Normal mode

Chế độ cơ bản nhất, hoạt động theo kiểu one-shot.

**Cơ chế:** DMA chuyển dữ liệu cho đến khi bộ đếm về 0 thì dừng hẳn, phát ngắt TC và bỏ qua mọi request tiếp theo từ ngoại vi. Muốn chạy tiếp, CPU phải tắt kênh, nạp lại transfer size (và địa chỉ nếu cần) rồi bật lại.

**Ứng dụng:** Phù hợp cho các khối dữ liệu có kích thước biết trước, hoặc cần CPU can thiệp sau mỗi lần truyền.
- **Memory-to-Memory:** Copy bảng cấu hình từ Flash sang RAM khi khởi động.
- **Đọc cảm biến định kỳ:** Mỗi phút lấy một khối mẫu điện áp pin. Đọc xong DMA dừng, hệ thống quay lại sleep.
- **Gửi lệnh:** Gửi một chuỗi AT command tới module SIM/WiFi qua UART TX.

### 5.2. Circular mode

Được thiết kế cho luồng dữ liệu liên tục, hoạt động như một ring buffer phần cứng.

**Cơ chế:** Khi bộ đếm về 0, DMA phát ngắt TC nhưng **không dừng**. Nó tự nạp lại transfer size ban đầu và đưa con trỏ về đầu buffer, rồi tiếp tục phục vụ request. Việc này diễn ra hoàn toàn trong phần cứng, CPU không cần can thiệp.

**Ứng dụng:** Xương sống của các ứng dụng xử lý tín hiệu liên tục.
- **ADC continuous sampling:** Thu liên tục dòng điện/nhiệt độ để giám sát hệ thống.
- **Tạo dạng sóng:** Đẩy lặp lại một bảng mẫu (sine, tam giác) ra DAC, hoặc bảng giá trị duty cycle vào thanh ghi compare (CCR) của timer.
- **UART RX không biết trước độ dài:** Buffer vòng nhận liên tục, CPU tự theo dõi vị trí ghi (xem [Phần 6](#6-ví-dụ-nhận-dữ-liệu-uart-bằng-dma)).

:::warning Rủi ro ghi đè
Vì DMA không bao giờ dừng, nếu CPU xử lý chậm hơn tốc độ DMA ghi, dữ liệu mới sẽ đè lên dữ liệu cũ chưa kịp xử lý.

Kỹ thuật phổ biến là dùng cả hai ngắt HT và TC để chia buffer làm hai nửa:
- Ngắt **HT**: DMA vừa ghi xong nửa đầu và đang ghi nửa sau $\rightarrow$ CPU xử lý nửa đầu.
- Ngắt **TC**: DMA vừa ghi xong nửa sau và quay lại ghi nửa đầu $\rightarrow$ CPU xử lý nửa sau.

Cách này chỉ an toàn nếu CPU xử lý xong mỗi nửa trước khi DMA quay lại vùng đó. Nếu không đảm bảo được, cần tách bạch hẳn vùng đọc và vùng ghi - đó là ý tưởng của double buffer mode.
:::

### 5.3. Double buffer mode (ping-pong)

Kỹ thuật nâng cao, loại bỏ triệt để xung đột truy cập giữa CPU và DMA, thường dùng trong các hệ thống băng thông cao.

**Vì sao cần:** Kỹ thuật HT/TC ở trên vẫn dùng chung **một** vùng nhớ: khi CPU đang đọc một nửa thì DMA đang ghi ngay nửa bên cạnh, và chỉ cần CPU chậm một chút là hai bên chạm nhau. Double buffer dùng **hai vùng nhớ tách biệt** để CPU và DMA không bao giờ làm việc trên cùng một buffer.

**Cơ chế:** Hoạt động như hai người chơi bóng bàn:
1. DMA điền dữ liệu vào buffer A. CPU rảnh hoặc xử lý dữ liệu cũ.
2. Khi buffer A đầy, DMA chuyển sang ghi buffer B, đồng thời phát ngắt báo CPU "buffer A đã xong".
3. Trong khi DMA ghi buffer B, CPU xử lý dữ liệu tĩnh tại buffer A mà không sợ bị ghi đè.
4. Buffer B đầy $\rightarrow$ DMA quay lại ghi buffer A, CPU xử lý buffer B. Cứ thế đảo chiều liên tục.

**Cách hiện thực:**
- Một số DMA controller hỗ trợ sẵn trong phần cứng (có thanh ghi cho hai địa chỉ buffer, ví dụ `DBM` trên STM32F4/F7).
- Nếu chip không hỗ trợ, có thể làm bằng phần mềm: dùng normal mode, trong ngắt TC đổi địa chỉ đích sang buffer còn lại rồi bật lại kênh. Nhược điểm là có khoảng hở nhỏ giữa hai phiên, phải đảm bảo ngoại vi không mất dữ liệu trong khoảng đó.
- Với DMA dạng descriptor, chỉ cần một vòng liên kết gồm 2 descriptor trỏ vào nhau là có ping-pong không cần CPU (xem [Phần 7](#7-dma-descriptor--scatter-gather)).

**Ứng dụng:**
- **Audio processing (I2S):** Một buffer nhận mẫu mới từ micro, buffer kia để CPU chạy FFT hoặc lọc nhiễu.
- **Graphic display:** Một buffer để render khung hình tiếp theo, buffer kia được đẩy ra LCD.
- **High-speed logging:** Ghi log ra thẻ SD. Tốc độ ghi thẻ không đều và thường chậm hơn tốc độ thu thập, double buffer giúp không mất dữ liệu trong lúc chờ.

### 5.4. So sánh

| | Normal | Circular | Double buffer |
|---|---|---|---|
| Khi bộ đếm về 0 | Dừng | Nạp lại, ghi đè từ đầu | Chuyển sang buffer còn lại |
| CPU can thiệp | Sau mỗi phiên | Không (chỉ đọc dữ liệu) | Không (chỉ đọc dữ liệu) |
| Rủi ro ghi đè | Không | Có | Không (nếu CPU xử lý kịp một buffer) |
| Bộ nhớ | 1 buffer | 1 buffer | 2 buffer |
| Phù hợp | Khối dữ liệu rời rạc | Luồng liên tục, xử lý nhẹ | Luồng liên tục, xử lý nặng |

## 6. Ví dụ: nhận dữ liệu UART bằng DMA

Phần này ghép lại toàn bộ kiến thức ở trên. Bài toán: nhận dữ liệu từ module GPS qua UART ở tốc độ cao, các câu NMEA đến liên tục.

### 6.1. Cấu hình

Áp dụng từng mục ở [Phần 4](#4-cấu-hình-một-kênh-dma):

| Tham số | Giá trị | Lý do |
|---|---|---|
| Kênh DMA | Kênh nối với UART RX | Tra bảng mapping ([3.2](#32-dma-request)) |
| Direction | P2M | Dữ liệu đi từ UART vào RAM ([4.1](#41-direction--trigger)) |
| Trigger | Hardware request (`RXNE`) | Chỉ chuyển khi có byte mới |
| Nguồn | `&UART->DR`, 8-bit, increment **disable** | Thanh ghi cố định ([4.2](#42-source--destination)) |
| Đích | `uint8_t rxBuffer[100]`, 8-bit, increment **enable** | Ghi lần lượt vào mảng |
| Transfer size | 100 | Kích thước buffer ([4.3](#43-transfer-size)) |
| Mode | Normal hoặc Circular | Xem [6.3](#63-chọn-chế-độ) |

### 6.2. Luồng hoạt động

Tương ứng với các bước ở [3.3](#33-luồng-một-phiên-truyền):

1. **Khởi tạo:** CPU cấu hình kênh DMA theo bảng trên.
2. **Kích hoạt:** CPU bật bit cho phép DMA RX trong UART (ví dụ `DMAR`) và bật kênh DMA.
3. **Vận hành:**
   - Một byte đến UART $\rightarrow$ `RXNE` bật.
   - Thay vì gọi ngắt CPU, sự kiện này tạo DMA request.
   - DMA lấy quyền bus, copy byte từ `UART->DR` vào `rxBuffer[0]` (việc đọc `DR` cũng xóa `RXNE`).
   - DMA giảm bộ đếm còn 99, con trỏ đích tăng lên `rxBuffer[1]`.
   - Lặp lại cho mỗi byte tiếp theo.
4. **Kết thúc:** Khi đủ 100 byte, DMA phát ngắt TC để báo CPU: "Đã nhận đủ, mời xử lý".

Suốt quá trình nhận 100 byte, CPU không bị ngắt lần nào.

### 6.3. Chọn chế độ

- **Normal mode** phù hợp khi mỗi gói có độ dài cố định và biết trước. Trong ngắt TC, CPU xử lý buffer rồi khởi động lại DMA ([5.1](#51-normal-mode)).
- Tuy nhiên câu NMEA có độ dài thay đổi, nên chờ đủ 100 byte là không thực tế: một câu ngắn có thể nằm trong buffer rất lâu. Giải pháp phổ biến là **circular mode kết hợp ngắt IDLE line** của UART ([5.2](#52-circular-mode)):
  - Mỗi khi đường truyền rảnh (hết một cụm dữ liệu) hoặc có ngắt HT/TC, CPU tính vị trí ghi hiện tại: `pos = 100 - NDTR`.
  - Dữ liệu mới nằm trong khoảng `[old_pos, pos)` (có thể vòng qua cuối buffer).
  - CPU xử lý đoạn đó rồi cập nhật `old_pos = pos`.

```c
#define RX_BUF_SIZE 100
static uint8_t  rxBuffer[RX_BUF_SIZE];
static uint16_t old_pos = 0;

/* Gọi trong ngắt UART IDLE, DMA HT và DMA TC */
void uart_rx_check(void) {
    uint16_t pos = RX_BUF_SIZE - DMA_GET_NDTR();

    if (pos == old_pos) {
        return;
    }

    if (pos > old_pos) {
        process(&rxBuffer[old_pos], pos - old_pos);
    } else {
        /* DMA đã vòng lại đầu buffer */
        process(&rxBuffer[old_pos], RX_BUF_SIZE - old_pos);
        process(&rxBuffer[0], pos);
    }

    old_pos = (pos == RX_BUF_SIZE) ? 0 : pos;
}
```

:::warning Lưu ý
Nếu trong một chu kỳ xử lý, UART nhận nhiều hơn `RX_BUF_SIZE` byte thì dữ liệu cũ sẽ bị ghi đè (rủi ro đã nêu ở [5.2](#52-circular-mode)). Cần chọn kích thước buffer đủ lớn so với baudrate và độ trễ xử lý.
:::

## 7. DMA descriptor & scatter-gather

Mọi thứ từ đầu tới giờ đều là DMA **dựa trên thanh ghi** (register-based): CPU ghi địa chỉ và kích thước vào thanh ghi, DMA chạy cho **một khối dữ liệu liền mạch**. Mô hình này có hai giới hạn:
- Dữ liệu phải nằm liền nhau trong bộ nhớ.
- Muốn chuyển nhiều khối khác nhau liên tiếp (hoặc đổi buffer như double buffer ở [5.3](#53-double-buffer-mode-ping-pong)), CPU phải thức dậy cấu hình lại sau mỗi phiên.

Các DMA controller cao cấp hơn (thường gặp trong Ethernet MAC, USB, SDIO hoặc SoC) giải quyết bằng **descriptor**.

### 7.1. Khái niệm descriptor

Thay vì nạp tham số trực tiếp vào thanh ghi, CPU tạo một **danh sách công việc** nằm trong RAM. Mỗi công việc là một descriptor - thực chất là một struct chứa đúng các tham số đã học ở [Phần 4](#4-cấu-hình-một-kênh-dma), cộng thêm một con trỏ:

```c
typedef struct DMA_Descriptor {
    uint32_t src_addr;                  /* Lấy ở đâu */
    uint32_t dst_addr;                  /* Ghi vào đâu */
    uint32_t control;                   /* Số lượng, độ rộng, cho phép ngắt,... */
    struct DMA_Descriptor *next;        /* Công việc tiếp theo */
} DMA_Descriptor_t;
```

CPU chỉ cần ghi địa chỉ của descriptor đầu tiên vào thanh ghi DMA. Khi xong một descriptor, DMA tự đọc trường `next` để lấy việc tiếp theo, tạo thành một **danh sách liên kết do phần cứng duyệt**.

### 7.2. Luồng hoạt động

Ở chế độ descriptor, DMA controller hoạt động giống một bộ xử lý nhỏ với chu trình 3 bước:

1. **FETCH:** DMA đọc descriptor hiện tại từ RAM vào các thanh ghi nội bộ (shadow register). Ví dụ: "Copy 100 byte từ UART sang buffer A".
2. **EXECUTE:** DMA chuyển dữ liệu theo đúng chỉ dẫn, giảm bộ đếm dần về 0 - giống hệt một phiên truyền ở [3.3](#33-luồng-một-phiên-truyền).
3. **LOAD NEXT:** Khi xong item cuối, DMA nhìn vào trường `next`:
   - `next != NULL`: nhảy tới descriptor đó và quay lại bước FETCH, ví dụ "Copy 50 byte từ SPI sang buffer B".
   - `next == NULL`: dừng và phát ngắt (tương đương normal mode).
   - `next` trỏ về descriptor đầu tiên: danh sách thành vòng lặp (tương đương circular mode; vòng 2 descriptor chính là double buffer).

Việc chuyển giữa hai descriptor chỉ mất vài chu kỳ clock, nên luồng dữ liệu gần như không bị gián đoạn và CPU không cần can thiệp.

:::warning Memory alignment
Descriptor được DMA đọc trực tiếp từ RAM, nên nó phải nằm ở vùng nhớ mà DMA truy cập được và thỏa yêu cầu căn chỉnh (alignment) của phần cứng, thường là địa chỉ chia hết cho 4 hoặc lớn hơn tùy controller.

Nếu đặt descriptor lệch căn chỉnh (ví dụ trong một struct `packed`), DMA có thể đọc sai trường hoặc gây bus fault. Nên dùng attribute để ép compiler căn chỉnh:

```c
__attribute__((aligned(32))) DMA_Descriptor_t my_desc_list[10];
```

Trên các chip có cache (Cortex-M7, Cortex-A), cũng cần lưu ý đặt descriptor và buffer ở vùng nhớ không bị cache hoặc flush cache trước khi DMA đọc.
:::

### 7.3. Kỹ thuật scatter-gather

Đây là ứng dụng mạnh nhất của descriptor, đặc biệt trong giao thức mạng (Ethernet, USB) và lưu trữ (SD card). Nó giải quyết trực tiếp giới hạn "dữ liệu phải liền mạch" nêu ở đầu phần 7.

**Scatter (nhận: một luồng $\rightarrow$ nhiều vùng nhớ)**

Bài toán: nhận một frame Ethernet dài 1500 byte, gồm Ethernet header (14 byte), IP header (20 byte) và payload.

- Cách thường: nhận toàn bộ vào một mảng lớn `rx_buffer`, sau đó `memcpy` để tách header và payload $\rightarrow$ tốn CPU và tốn gấp đôi bộ nhớ.
- Cách scatter: tạo 3 descriptor nối tiếp, cùng nguồn là ngoại vi:
  - Desc 1: nhận 14 byte $\rightarrow$ ghi vào `Eth_Header`.
  - Desc 2: nhận 20 byte $\rightarrow$ ghi vào `IP_Header`.
  - Desc 3: nhận phần còn lại $\rightarrow$ ghi vào `Payload`.

Kết quả: khi DMA chạy xong, dữ liệu đã nằm đúng ở các biến cần dùng, không cần copy thêm.

**Gather (gửi: nhiều vùng nhớ $\rightarrow$ một luồng)**

Bài toán: gửi một gói tin có dạng

```
[Header cố định] + [Dữ liệu cảm biến] + [Checksum]
```

trong đó ba phần nằm rải rác ở ba vùng nhớ khác nhau.

- Cách thường: ghép cả ba vào một buffer tạm rồi mới gửi.
- Cách gather: tạo 3 descriptor trỏ tới 3 vùng nhớ, cùng đích là thanh ghi dữ liệu của ngoại vi (hoặc TX FIFO của MAC). DMA tự nối chúng thành một luồng liên tục đẩy ra ngoài.
