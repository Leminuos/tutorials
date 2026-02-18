## 1. Tổng quan
Trong kiến trúc máy tính và hệ thống nhúng, Direct Memory Access (DMA) là một tính năng phần cứng quan trọng cho phép các thiết bị ngoại vi truy cập trực tiếp vào bộ nhớ RAM hoặc từ bộ nhớ sang bộ nhớ mà không cần thông qua sự can thiệp liên tục của CPU.

Trong các hệ thống nhúng như audio streaming, điều khiển màn hình LCD, thu thập dữ liệu ADC tốc độ cao hoặc truyền thông SPI/Ethernet, lượng dữ liệu cần xử lý thường lớn và liên tục. Nếu CPU phải tham gia vào từng lần truyền dữ liệu, hệ thống sẽ nhanh chóng bị quá tải. DMA được thiết kế để giải quyết chính bài toán này.

Hãy tưởng tượng CPU là một bếp trưởng trong nhà hàng, còn DMA là một phụ bếp chuyên biệt.
- Nếu không có DMA: Bếp trưởng phải tự tay lấy từng củ hành, rửa từng cái bát (copy từng byte dữ liệu). Việc này lãng phí chất xám và thời gian, khiến bếp trưởng không thể tập trung nấu món chính (xử lý logic phức tạp).
- Nếu có DMA: Bếp trưởng chỉ cần ra lệnh "Hãy chuyển rổ rau này vào kho", sau đó quay lại nấu ăn. Phụ bếp (DMA) sẽ tự thực hiện việc vận chuyển và báo cáo lại khi hoàn tất.

## 2. Tại sao cần DMA

Để thấy rõ vai trò của DMA, cần phân tích hai cách truyền dữ liệu truyền thống bằng CPU: polling và interrupt.

Giả sử hệ thống thu ADC ở tần số 1 MHz, độ phân giải 16-bit:
1,000,000 mẫu/giây × 2 byte = 2 MB/giây

### 2.1. Sử dụng polling

CPU liên tục kiểm tra cờ sẵn sàng dữ liệu và đọc thanh ghi:

```c
while (1) {
    if (ADC_DR_READY) {
        buffer[i++] = ADC_DR;
    }
}
```

Đặc điểm của polling:
- CPU luôn bận chờ dữ liệu.
- Không có thời gian xử lý các tác vụ khác.
- Không phù hợp với hệ thống đa nhiệm.
- Tiêu thụ năng lượng cao do CPU không được nghỉ.

Với lưu lượng 2 MB/giây, CPU gần như bị chiếm dụng hoàn toàn chỉ để sao chép dữ liệu.

### 2.2 Sử dụng interrupt

Thay vì polling, ta cấu hình peripheral phát interrupt mỗi khi có dữ liệu mới:

```c
void ADC_IRQHandler(void) {
    buffer[i++] = ADC_DR;
}
```

So với polling, CPU không còn bận chờ. Tuy nhiên, với tần số 1 MHz, hệ thống sẽ nhận 1,000,000 interrupt mỗi giây. Mỗi lần ngắt xảy ra, CPU thực hiện context switching: lưu ngữ cảnh, đọc thanh ghi dữ liệu, ghi vào RAM, rồi khôi phục ngữ cảnh.

Hậu quả: CPU bị quá tải, tiêu tốn năng lượng và có thể bỏ lỡ các tác vụ quan trọng khác.

### 2.3. Khi sử dụng DMA

Luồng xử lý sẽ thay đổi:
1. CPU cấu hình DMA:
   - Source address
   - Destination address
   - Transfer size
   - Mode
2. DMA tự động thực hiện truyền dữ liệu
3. Khi hoàn tất, DMA phát interrupt
4. ISR báo cho task xử lý

CPU chỉ tham gia ở đầu và cuối transaction. Điều này giúp:
- **Giải phóng CPU**: CPU chỉ cần cấu hình ban đầu. Trong khi DMA vận chuyển dữ liệu, CPU có thể chuyển sang chế độ sleep mode để tiết kiệm điện hoặc xử lý các thuật toán phức tạp như bộ lọc số, PID, xử lý ảnh,...
- **Tăng băng thông**: DMA controller được thiết kế chuyên biệt cho việc copy dữ liệu, nên tốc độ chuyển đổi thường nhanh hơn nhiều so với việc CPU thực hiện lệnh `LDR` (Load) và `STR` (Store) lặp đi lặp lại.
- **Đảm bảo tính thời gian thực**: Với các ngoại vi tốc độ cao như Audio (I2S), Camera (DCMI) hay ADC lấy mẫu nhanh, chỉ có DMA mới đáp ứng kịp tốc độ dữ liệu vào/ra mà không làm treo hệ thống.

## 3. Kiến trúc DMA controller

Để một DMA controller hoạt động, nó cần kiểm soát được bus hệ thống. Trong vi điều khiển, DMA hoạt động dựa trên cấu hình 4 yếu tố chính:
- Data Size: Số lượng dữ liệu cần chuyển (bao nhiêu byte/word).
- Trigger (Sự kiện kích hoạt): DMA không tự chạy, nó cần một "cò súng". Đó có thể là tín hiệu từ ngoại vi (như "UART Data Ready") hoặc lệnh từ phần mềm.

## 3.1. Source (Nguồn dữ liệu)

Đây là nơi DMA sẽ lấy dữ liệu. Để cấu hình đúng, ta cần trả lời 3 câu hỏi:
- Base address (Địa chỉ cơ sở): Dữ liệu nằm ở đâu? Ví dụ: Địa chỉ thanh ghi `&UART->DR` hoặc địa chỉ mảng `&buffer[0]`.
- Data width (Độ rộng dữ liệu): Đọc bao nhiêu bit một lần?
  - Tùy chọn: Byte (8-bit), Half-word (16-bit), hoặc Word (32-bit).
  - Lưu ý: Độ rộng này phải khớp với thanh ghi ngoại vi. Đọc 32-bit từ một thanh ghi 8-bit có thể gây lỗi bus.
- Address increment (Tăng địa chỉ): Sau khi đọc xong, con trỏ có tự động tăng lên không?
  - Enable: Dùng cho bộ nhớ (Mảng, Buffer). Đọc xong byte 0 nhảy sang byte 1.
  - Disable: Dùng cho ngoại vi. Vì thanh ghi dữ liệu của ngoại vi (DR) nằm cố định tại một địa chỉ, nên con trỏ phải luôn đứng yên tại đó.

## 3.2. Destination (Đích)

Đây là nơi DMA sẽ ghi dữ liệu. Tương tự như Source, ta cần cấu hình 3 tham số:
- Base address: Ghi vào đâu? (Thường là địa chỉ RAM hoặc thanh ghi ngoại vi).
- Data Width: Ghi bao nhiêu bit?DMA có khả năng tự động chuyển đổi độ rộng. Ví dụ: Đọc 8-bit từ source nhưng ghi 32-bit vào destination (gom 4 byte lại rồi ghi 1 lần).
- Address increment: Ghi xong có tăng con trỏ không?
  - Enable: Nếu đích là RAM buffer.
  - Disable: Nếu đích là thanh ghi ngoại vi, ví dụ nạp dữ liệu vào thanh ghi `DAC_DHR` để phát nhạc.

## 3.3. Transfer size (Khối lượng truyền tải)

Đây là số lượng đơn vị dữ liệu (Data items) mà DMA cần vận chuyển trong một phiên làm việc.
- Thanh ghi quản lý: Thường ký hiệu là CNDTR (Number of Data to Transfer) hoặc LEN.
- Cơ chế đếm: Mỗi khi một đơn vị dữ liệu được chuyển thành công từ source sang dest, giá trị này tự động giảm đi 1.
- Điểm kết thúc:
  - Khi bộ đếm về 0, DMA dừng lại đối với normal mode hoặc tự động nạp lại giá trị ban đầu đối với circular mode.
  - Khi về 0, cờ ngắt TC (Transfer Complete) sẽ được bật lên để báo cho CPU.

  :::warning Lưu ý
  Size ở đây là số lượng item, không phải số byte. Nếu cấu hình data width là 16-bit (2 byte) và Size = 10 thì tổng dung lượng truyền tải sẽ là 20 byte.
  :::

## 3.4. Trigger & control

Đây là yếu tố phức tạp nhất, quyết định khi nào và như thế nào DMA hoạt động.
- Direction (Hướng):
  - Memory-to-Memory (M2M): Sao chép dữ liệu từ vùng nhớ này sang vùng nhớ khác như hàm memcpy nhưng bằng phần cứng.
  - Peripheral-to-Memory (P2M): Phổ biến nhất. Thu thập dữ liệu cảm biến (ADC, UART, SPI) lưu vào bộ đệm RAM.
  - Memory-to-Peripheral (M2P): Đẩy dữ liệu từ RAM ra ngoại vi (như phát nhạc qua I2S, gửi gói tin qua UART).
- Priority (Độ ưu tiên):Low / Medium / High / Very High.Dùng để arbitration khi nhiều luồng DMA stream cùng tranh chấp bus.
- Trigger source (Nguồn kích hoạt):
  - Hardware request: Tín hiệu từ ngoại vi như timer Update, UART RXNE, ADC EOC,... Đây là chế độ mặc định cho P2M và M2P.
  - Software trigger: Kích hoạt bằng bit phần mềm. Chế độ này thường ép DMA chạy hết tốc lực, chỉ dùng cho M2M.

## 4. Các chế độ hoạt động

Hiểu rõ chế độ hoạt động là chìa khóa để vận dụng DMA hiệu quả. Tùy thuộc vào tính chất của dữ liệu (rời rạc hay liên tục) và yêu cầu xử lý của CPU, ta sẽ chọn một trong ba chế độ dưới đây.

### 4.1. Normal Mode

Đây là chế độ cơ bản nhất, hoạt động theo cơ chế one shot.

**Cơ chế hoạt động**: Khi được kích hoạt, DMA sẽ thực hiện chuyển dữ liệu cho đến khi bộ đếm dữ liệu giảm về 0. Ngay tại thời điểm bộ đếm đạt 0, quá trình truyền tải dừng lại hoàn toàn. DMA sẽ ngắt tín hiệu yêu cầu từ ngoại vi và không thực hiện thêm bất kỳ thao tác nào cho đến khi được CPU cấu hình và kích hoạt lại.

**Ứng dụng thực tế**: Normal mode phù hợp cho các tác vụ chuyển dữ liệu không tuần hoàn hoặc cần sự can thiệp logic sau mỗi lần truyền.
- Memory-to-Memory: Copy một mảng cấu hình từ Flash sang RAM khi khởi động hệ thống.
- Sensor Reading: Đọc giá trị battery mỗi phút một lần. Sau khi đọc xong, DMA dừng lại để tiết kiệm năng lượng.
- Command sending: Gửi một chuỗi lệnh AT command cấu hình module SIM/Wifi qua UART.

### 4.2. Circular mode

Circular mode được thiết kế cho các luồng dữ liệu liên tục, hoạt động như một ring buffer phần cứng.

**Cơ chế hoạt động**: Điểm khác biệt cốt lõi nằm ở việc xử lý khi bộ đếm dữ liệu về 0. Thay vì dừng lại, DMA controller sẽ tự động nạp lại giá trị số lượng dữ liệu ban đầu vào thanh ghi bộ đếm và reset địa chỉ con trỏ về vị trí khởi tạo. Quá trình này diễn ra ngay lập tức ở cấp độ phần cứng mà không cần CPU can thiệp.

**Ứng dụng thực tế**: Chế độ này là xương sống cho các ứng dụng xử lý tín hiệu thời gian thực.
- ADC continuous sampling: Thu thập dữ liệu cảm biến nhiệt độ/dòng điện liên tục để giám sát hệ thống.
- PWM generation: Tạo các dạng sóng tuần hoàn bằng cách đẩy dữ liệu liên tục vào thanh ghi capture của timer.

:::warning Lưu ý
Trong circular mode, rủi ro lớn nhất là ghi đè dữ liệu. Nếu CPU xử lý dữ liệu chậm hơn tốc độ DMA ghi vào, dữ liệu mới sẽ đè lên dữ liệu cũ chưa kịp xử lý. Để giải quyết vấn đề này, người ta thường sử dụng ngắt half transfer và transfer complete để chia đôi vùng đệm xử lý.
:::

### 4.3. Double buffer mode (ping-pong)

Đây là kỹ thuật nâng cao giúp giải quyết triệt để vấn đề xung đột truy cập giữa CPU và DMA, thường được sử dụng trong các hệ thống yêu cầu băng thông cao.

Tại sao cần double buffer? Trong circular mode đơn thuần, khi CPU đang đọc dữ liệu thì DMA có thể đang ghi vào ngay sát đó. Việc này gây nguy hiểm về tính toàn vẹn dữ liệu. Double buffer giải quyết bằng cách sử dụng hai vùng nhớ riêng biệt.

**Cơ chế hoạt động**: Cơ chế này hoạt động như hai người chơi bóng bàn:
- Giai đoạn 1: DMA điền dữ liệu vào buffer 0. Trong lúc này, CPU rảnh rỗi hoặc xử lý dữ liệu cũ.
- Giai đoạn 2: Khi buffer 0 đầy, DMA tự động chuyển hướng ghi sang buffer 1. Cùng lúc này, DMA kích hoạt ngắt báo cho CPU biết "buffer 0 đã xong, hãy xử lý đi".
- Giai đoạn 3: Trong khi DMA đang miệt mài fill vào buffer 1, CPU an tâm xử lý dữ liệu tĩnh tại buffer 0 mà không sợ bị ghi đè.

Quá trình này cứ thế đảo chiều liên tục.

Ứng dụng thực tế:
- Audio processing (I2S): Một buffer dùng để nạp mẫu âm thanh mới từ micro, buffer kia dùng để CPU thực hiện FFT hoặc lọc nhiễu.
- Graphic display: Một buffer dùng để render khung hình tiếp theo, buffer kia được đẩy ra màn hình LCD.
- High-speed logging: Ghi dữ liệu log ra thẻ nhớ SD (tốc độ ghi thẻ thường chậm hơn tốc độ thu thập, nên cần double buffer để không mất dữ liệu).

## 5. Cơ chế DMA request

Một sai lầm phổ biến là nghĩ rằng DMA như một camera giám sát, liên tục nhìn vào ngoại vi để xem có dữ liệu hay không. Thực tế không phải như vậy.

DMA về bản chất là một thiết bị thụ động trong việc khởi tạo giao dịch ngoại vi. Nó không biết gì về trạng thái dữ liệu bên trong ngoại vi.
- DMA không biết khi nào ADC chuyển đổi xong.
- DMA không biết khi nào gói tin UART được gửi đến.
- DMA không biết khi nào timer tràn.

Nếu DMA tự ý đọc liên tục thanh ghi dữ liệu của ngoại vi, nó sẽ đọc phải dữ liệu rác (dữ liệu cũ chưa được cập nhật) hoặc gây xung đột bus không cần thiết. Do đó, DMA cần một cơ chế đồng bộ hóa và đó chính là **hardware request signal**.

Hãy tưởng tượng trong một nhà hàng:
- Đầu bếp (Ngoại vi): Người tạo ra món ăn (dữ liệu).
- Người phục vụ (DMA): Người bưng món ăn từ bếp ra bàn khách (RAM).

**Kịch bản dùng polling**: Người phục vụ cứ 1 giây lại chạy vào bếp hỏi: "Xong chưa?". Nếu chưa xong, anh ta mất công chạy vào rồi lại chạy ra tay không. Việc này gây tốn sức và làm rối loạn bếp.

**Kịch bản dùng Request**: Người phục vụ đứng yên ở ngoài (Idle). Khi Đầu bếp nấu xong, ông ta bấm chuông (Request Signal). Nghe tiếng chuông, người phục vụ mới lao vào, lấy đúng đĩa thức ăn đó mang đi. Sau khi lấy xong, tiếng chuông tắt.

## 6. Ví dụ

Giả sử ta cần nhận dữ liệu từ module GPS qua cổng UART với tốc độ cao, các gói tin đến liên tục.

Kịch bản cấu hình:
- **Nguồn**: Địa chỉ thanh ghi `UART_RX_DR` (Data Register). Vì địa chỉ này cố định, ta cấu hình DMA không tăng địa chỉ nguồn.
- **Đích**: Mảng `uint8_t rxBuffer[100]` trong RAM. Ta cấu hình DMA tự động tăng địa chỉ đích sau mỗi lần nhận 1 byte.
- **Kích thước**: 100 bytes.
- **Chế độ**: Circular hoặc Normal.

Luồng hoạt động (Workflow):
- **Khởi tạo**: CPU thiết lập các thông số trên cho kênh DMA tương ứng với UART.
- **Kích hoạt**: CPU bật bit `DMA_Enable` trong UART và DMA controller.
- **Vận hành**:
  - Khi 1 byte đến UART, cờ RXNE bật lên.
  - Thay vì gọi ngắt CPU, tín hiệu này kích hoạt DMA.
  - DMA lấy quyền kiểm soát bus, copy byte đó từ `UART_RX_DR` vào `rxBuffer[0]`.
  - DMA tăng con trỏ đích lên `rxBuffer[1]`.
  - Quá trình lặp lại cho đến khi đủ 100 bytes.
- **Kết thúc**: Khi đủ 100 bytes, DMA gửi ngắt transfer complete interrupt để báo cho CPU: "Đã nhận xong gói tin, mời xử lý".

:::warning Memory alignment
Phần cứng DMA thường yêu cầu địa chỉ bắt đầu phải được alignment, tức là chia hết cho 4, 8 hoặc 32 bytes.

Nếu khai báo struct tự do, compiler có thể đặt nó ở địa chỉ lẻ, ví dụ như 0x20000001. DMA sẽ báo lỗi bus fault ngay lập tức.

-> Nên sử dụng `attribute` để ép compiler căn chỉnh.

```c
__attribute__((aligned(32))) DMA_Descriptor_t my_desc_list[10];
```

:::

## 7. DMA Descriptors & Scatter-Gather

Ở các dòng vi điều khiển cơ bản, DMA thường hoạt động dựa trên thanh ghi (Register-based): Bạn cấu hình địa chỉ, kích thước vào thanh ghi và nó chạy một lần cho một khối dữ liệu liền mạch.

Tuy nhiên, trong các hệ thống phức tạp, dữ liệu thường không nằm liền mạch trong bộ nhớ hoặc cần thực hiện chuỗi các tác vụ chuyển đổi khác nhau liên tiếp mà không muốn CPU phải thức dậy để cấu hình lại sau mỗi lần. Đây là lúc cần DMA descriptor.

### 7.1. Khái niệm Descriptor

Thay vì CPU nạp trực tiếp thông số vào thanh ghi của DMA controller, CPU sẽ tạo ra một danh sách công việc nằm ngay trong bộ nhớ RAM. Mỗi công việc này được gọi là một Descriptor.

Một descriptor thực chất là một struct trong C, chứa đầy đủ thông tin cho một lần chuyển đổi:
- Source Address: Lấy ở đâu.
- Destination Address: Gửi đến đâu.
- Data Count/Control: Bao nhiêu byte, cấu hình ngắt thế nào.
- Next Descriptor Pointer: Địa chỉ của công việc tiếp theo.

Khi DMA hoàn thành descriptor hiện tại, thay vì dừng lại, nó sẽ tự động nhìn vào trường Next Descriptor Pointer để tải cấu hình tiếp theo và tiếp tục chạy. Cơ chế này tạo thành một danh sách liên kết phần cứng.

### 7.2. Luồng hoạt động

Khi kích hoạt chế độ descriptor, DMA Controller hoạt động như một bộ vi xử lý nhỏ với quy trình 3 bước lặp đi lặp lại:

- FETCH (Nạp):
  - DMA đọc descriptor hiện tại từ RAM vào các thanh ghi nội bộ (Shadow Registers).
  - Lúc này, thanh ghi nội bộ của DMA chứa thông tin: "Copy 100 byte từ UART sang Buffer A".
- EXECUTE (Thực thi):
  - DMA tiến hành chuyển dữ liệu theo đúng chỉ dẫn vừa nạp.
  - Trong quá trình này, nó giảm bộ đếm byte dần về 0.

- UPDATE / LOAD NEXT (Cập nhật):
  - Ngay khi byte cuối cùng được chuyển xong, thay vì kích hoạt ngắt và dừng lại, DMA nhìn vào trường NextDescriptor.
  - Nếu `Next != NULL`: DMA nhảy tới địa chỉ RAM đó, thực hiện lại bước FETCH với cấu hình mới. Ví dụ: "Copy 50 byte từ SPI sang Buffer B").

Quá trình chuyển đổi giữa 2 descriptor diễn ra cực nhanh (chỉ vài chu kỳ clock), đảm bảo luồng dữ liệu gần như không bị đứt quãng.

### 7.3. Kỹ thuật Scatter-Gather

Đây là ứng dụng quyền năng nhất của Descriptor, đặc biệt trong các giao thức mạng (Ethernet, USB) hoặc lưu trữ (SD Card).

**Scatter**

Bài toán: Ta nhận một gói tin mạng ethernet dài 1500 bytes. Cấu trúc gói tin gồm: Header (14 bytes), IP Header (20 bytes), và Data Payload (còn lại).

Cách thường: Nhận toàn bộ vào một mảng lớn `rx_buffer`, sau đó dùng `memcpy` để tách header và data ra xử lý riêng -> Tốn CPU.

Cách Scatter: Ta tạo 3 descriptors nối tiếp:
- Desc 1: Nhận 14 bytes -> Lưu vào biến `Eth_Header`.
- Desc 2: Nhận 20 bytes -> Lưu vào biến `IP_Header`.
- Desc 3: Nhận phần còn lại -> Lưu vào mảng Payload.

Kết quả: Ngay khi DMA chạy xong, dữ liệu đã nằm gọn gàng ở đúng các biến cấu trúc bạn cần.

**Gather**

Bài toán: Ta cần gửi một gói tin gồm:
[Header cố định] + [Dữ liệu cảm biến động] + [Checksum]. 

Các phần này nằm rải rác trong RAM.

Cách gather: Tạo 3 descriptors trỏ tới 3 vùng nhớ đó, nhưng đích đến đều là thanh ghi `UART_DR` (hoặc MAC TX FIFO). DMA sẽ tự động ghép chúng thành một luồng dữ liệu liên tục đẩy ra ngoại vi.
