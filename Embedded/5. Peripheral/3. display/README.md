## 1. Tổng quan về giao diện hiển thị

### 1.1. Bài toán

Một tấm panel TFT không tự hiểu được dữ liệu số. Nó chỉ nhận tín hiệu analog để điều khiển từng transistor của mỗi subpixel. Vì vậy giữa MCU và panel luôn tồn tại một con chip trung gian được gọi là **display controller** hay display driver IC - ILI9341 chính là một con chip như vậy.

Câu hỏi đặt ra là: MCU nói chuyện với display controller đó bằng ngôn ngữ nào? Đây chính là phần mà MIPI đứng ra chuẩn hóa.

### 1.2. MIPI là gì?

**MIPI (Mobile Industry Processor Interface) Alliance** là liên minh các hãng bán dẫn (ARM, Intel, Nokia, Samsung, TI...) thành lập năm 2003, chuyên xây dựng chuẩn giao tiếp giữa processor và các thành phần trong thiết bị di động: camera (CSI), màn hình (DSI/DBI/DPI), audio (SLIMbus), RF...

Trước khi có MIPI, mỗi hãng làm màn hình theo một kiểu riêng. MIPI gom các cách giao tiếp phổ biến lại, đặt tên và mô tả chính xác tín hiệu, timing, tập lệnh. Nhờ vậy code viết cho ILI9341 có thể port sang ST7789 với rất ít thay đổi vì cả hai đều cùng tuân theo một họ chuẩn.

### 1.3. Các chuẩn giao thức hiển thị

| Chuẩn | Tên đầy đủ | Kiểu | Đặc điểm cốt lõi |
|-------|-----------|------|------------------|
| **DSI** | Display Serial Interface | serial, differential | Tốc độ rất cao, truyền trên lane D-PHY |
| **DPI** | Display Pixel Interface | parallel, đồng bộ | Flush pixel liên tục theo timing video |
| **DBI** | Display Bus Interface | parallel/serial, bất đồng bộ | Ghi vào bộ nhớ của display |
| **DCS** | Display Command Set | tập lệnh | Không phải giao thức truyền mà là bộ lệnh chạy trên DSI/DBI |

**DSI** dùng cặp dây differential (1-4 lane data + 1 lane clock), băng thông tới hàng Gbps mỗi lane. Đây là chuẩn của điện thoại, tablet, các SoC lớn (Snapdragon, Raspberry Pi CM). MCU phổ thông không có DSI vì cần khối D-PHY analog chuyên dụng.

**DPI** còn gọi là RGB interface. MCU xuất thẳng bus song song RGB (16/18/24 bit) kèm các tín hiệu đồng bộ HSYNC, VSYNC, DE, PCLK. Panel loại này **không có bộ nhớ bên trong** nên MCU phải liên tục quét lại toàn bộ khung hình 60 lần mỗi giây, đồng nghĩa với việc phải có framebuffer trong RAM và một khối phần cứng như LTDC. Dùng cho panel lớn 5-10 inch.

**DBI** còn gọi là **MCU interface** hoặc **MPU interface**. MCU ghi dữ liệu vào **GRAM** (Graphics RAM) nằm ngay trong display controller và controller tự lo việc quét lại panel. MCU chỉ ghi khi có sự thay đổi. Đây là chuẩn của hầu hết module TFT nhỏ 1.8-3.5 inch trên thị trường.

**DCS** không phải là giao tiếp vật lý mà là danh sách các lệnh chuẩn mà display controller phải hiểu. DCS được truyền bên trong DSI hoặc DBI giống như HTTP chạy trên TCP.

:::warning Đừng nhầm DPI với DBI
Cả hai đều có thể là bus parallel 16-bit nhưng bản chất khác hẳn nhau. DPI cần MCU gửi frame liên tục theo timing cố định. DBI chỉ cần gửi khi nội dung thay đổi. Nhìn tên chân là biết ngay: có HSYNC/VSYNC/DE/PCLK là DPI, có CS/WR/RD/DC là DBI.
:::

Toàn bộ phần còn lại của tài liệu tập trung vào chuẩn DBI.

## 2. Chuẩn MIPI DBI

DBI còn gọi là MCU interface hoặc CPU interface, là chuẩn giao tiếp giữa host và display controller theo mô hình bus bộ nhớ. Host ghi dữ liệu vào panel giống như ghi vào một chip SRAM ngoài - có địa chỉ, có tín hiệu ghi/đọc, có bus dữ liệu.

Điểm mấu chốt khiến DBI khác hẳn DPI và DSI là nó có RAM riêng (GRAM). Driver IC trên panel tự lo việc quét màn hình liên tục, host chỉ cần ghi dữ liệu khi nội dung thay đổi.

### 2.1. Mô hình giao tiếp

Trong DBI, MCU đóng vai trò là master và gửi hai loại thông tin:

| Loại | Ý nghĩa | Ví dụ |
|------|-------|-------|
| **Command** | Một byte mã lệnh, yêu cầu controller thực hiện một hành động | Bật màn hình, đặt vùng vẽ, đổi hướng xoay... |
| **Data** | Các byte đi sau command: tham số của lệnh hoặc dữ liệu pixel | 4 byte toạ độ vùng vẽ hoặc 153,600 byte pixel |

```
        STM32F103                        ILI9341
   +----------------+             +-----------------------+
   |                |  D[15:0]    |   +---------------+   |
   |                |------------>|   | thanh ghi lệnh|   |
   |  FSMC hoặc SPI |  D/CX       |   +---------------+   |     +---------+
   |                |------------>|                       |     |         |
   |                |  WRX / RDX  |   +---------------+   |     |  panel  |
   |                |------------>|   |     GRAM      |---+---->|         |
   +----------------+             |   +---------------+   |     +---------+
                                  +-----------------------+
                                        ^
                                 controller tự quét GRAM ra panel,
                                 không cần MCU can thiệp
```

Toàn bộ chuẩn DBI xoay quanh việc trả lời ba câu hỏi:
1. Làm sao display biết khi nào giá trị trên dây là hợp lệ để đọc? $\rightarrow$ tín hiệu strobe
2. Làm sao display biết byte vừa nhận là command hay data? $\rightarrow$ chân D/CX
3. Dữ liệu ghi vào chỗ nào trên màn hình? $\rightarrow$ GRAM và con trỏ tự tăng

Ba mục tiếp theo lần lượt trả lời ba câu hỏi này.

### 2.2. Tín hiệu strobe

**Vấn đề:** MCU đặt một giá trị lên 16 sợi dây `D[15:0]`, display phải đọc giá trị đó. Nhưng đọc vào lúc nào?

Display không thể đọc tuỳ ý, vì 16 sợi dây **không đổi trạng thái cùng một lúc**. Khi MCU chuyển từ giá trị này sang giá trị khác, sợi này nhanh hơn sợi kia vài nano giây do độ dài mạch in và tải khác nhau. Nếu display đọc bus đúng vào khoảng khắc đó, nó sẽ nhận một giá trị lai giữa cũ và mới:

```
             giá trị cũ        giao thời         giá trị mới
              (ổn định)          (rác)            (ổn định)
D[15:0]  ====== 0x0000 ======><XX XX XX><====== 0xF800 ======
                              ^^^^^^^^^^
                        đọc vào đây thì nhận rác
```

**Giải pháp:** thêm một sợi dây riêng làm nhiệm vụ báo hiệu gọi là strobe. MCU chờ cho bus ổn định rồi mới tạo một xung trên sợi dây này, mang ý nghĩa: *"ngay bây giờ, giá trị trên bus đã ổn định, hãy chốt nó lại"*.

Ở ILI9341, chân strobe dùng khi ghi là chân **WRX** (Write strobe) và display chốt dữ liệu tại cạnh lên của nó. Một chu kỳ ghi diễn ra như sau:

```
CSX     ‾‾‾‾‾\_________________________________________/‾‾‾‾

D/CX    -------< mức 1 = đây là data                 >-------

D[15:0] -------------< 0xF800 (đã ổn định)           >-------

WRX     ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾\____________________/‾‾‾‾‾‾‾‾‾‾‾‾‾‾
                                              ^
                                     cạnh lên: display đọc bus
                                     và chốt giá trị 0xF800
```

Datasheet quy định hai khoảng thời gian bắt buộc quanh cạnh lên:

| Thông số | Ý nghĩa | ILI9341 |
|----------|---------|---------|
| Setup time (tDST) | Dữ liệu phải ổn định trước cạnh lên ít nhất bao lâu | 10 ns |
| Hold time (tDHT) | Dữ liệu phải giữ nguyên sau cạnh lên ít nhất bao lâu | 10 ns |

Chạy bus nhanh hơn con số này đồng nghĩa với việc vi phạm setup/hold time và hậu quả cụ thể được phân tích ở [mục 5.1](#51-tốc-độ-spi-và-chu-kỳ-ghi-tối-thiểu).

:::warning Strobe khác clock
Clock (như SCK của SPI) chạy đều đặn theo chu kỳ do phần cứng sinh ra. Strobe thì chỉ xuất hiện khi thực sự có một byte cần chốt. Đây chính là ý nghĩa của chữ **bất đồng bộ** trong DBI: nhịp truyền hoàn toàn do MCU quyết định, muốn nhanh chậm tuỳ ý, dừng hẳn giữa chừng rồi vài giây sau ghi tiếp cũng không sao. Ngược lại, DPI bắt buộc phải có PCLK chạy liên tục đúng tần số, chỉ cần lệch nhịp là mất hình.
:::

Riêng với biến thể serial (Type C) thì không có bus song song nên cũng không có vấn đề lệch dây. Ở đó chân SCL đóng cả hai vai trò: vừa là clock định nhịp, vừa là strobe chốt từng bit tại mỗi cạnh lên.

### 2.3. Chân D/CX

**Vấn đề:** MCU gửi byte `0x2C` sang display. Đó là lệnh `RAMWR` hay là một nửa của pixel màu nào đó? Bản thân 8 bit `0010 1100` không mang theo thông tin nào cho biết nó thuộc loại gì.

I2C hay UART giải quyết bằng vị trí trong khung truyền: byte đầu tiên là địa chỉ, các byte sau là dữ liệu. DBI chọn cách đơn giản hơn nhiều - **thêm hẳn một sợi dây** để nói loại. Sợi dây đó tên là **D/CX** (Data/Command select):

| D/CX | Display hiểu byte đang nhận là | Xử lý |
|------|--------------------------------|-------|
| 0 | Command | Tra bảng lệnh và thực thi ngay |
| 1 | Data | Tham số của lệnh vừa nhận hoặc pixel để ghi vào GRAM |

Display đọc mức của D/CX cùng thời điểm chốt dữ liệu, tức tại cạnh lên của WRX ở mục trên. Nói cách khác, mỗi byte đi qua bus luôn kèm một bit thứ 9 vô hình cho biết nó là loại gì.

:::warning Chữ X trong tên chân
Trong tài liệu MIPI và datasheet display, hậu tố X nghĩa là active low - tín hiệu tích cực ở mức thấp. Vì vậy CSX, WRX, RDX, RESX đều là chân tích cực mức 0. Với D/CX thì X gắn với vế Command, nghĩa là mức 0 tương ứng command còn mức 1 tương ứng data.
:::

Hai điểm dễ hiểu nhầm:

- **Display không ghi nhớ trạng thái của D/CX.** Nó chỉ lấy mẫu mức logic tại đúng thời điểm strobe rồi quyết định. Bên trong không có thanh ghi địa chỉ nào cả.
- **D/CX không quyết định pixel nằm ở đâu trên màn hình.** Nó chỉ phân loại byte. Vị trí ghi hoàn toàn do lệnh `CASET`/`PASET` đặt trước đó quyết định, sẽ nói ở mục ngay sau.

Việc chỉ cần 1 bit cũng mở ra một cách ghép rất gọn với FSMC: nối một đường address bất kỳ của FSMC vào chân D/CX là xong, không cần chân GPIO nào phải đảo bằng phần mềm. Chi tiết ở [mục 4](#4-giao-tiếp-qua-fsmc).

### 2.4. Bộ nhớ GRAM

Display controller chứa sẵn một vùng SRAM nội bộ đúng bằng kích thước màn hình được gọi là **GRAM** (Graphics RAM). Với ILI9341: 240 x 320 x 18 bit = 172800 byte.

Cơ chế ghi gồm hai bước: khai báo một cửa sổ rồi đổ dữ liệu vào cửa sổ đó.

1. `CASET` khai báo dải cột, `PASET` khai báo dải hàng $\rightarrow$ xác định một cửa sổ hình chữ nhật trong GRAM
2. `RAMWR` đặt con trỏ ghi về góc trên bên trái của cửa sổ
3. Mỗi pixel gửi vào sau đó được ghi vào vị trí con trỏ, rồi con trỏ tự động nhảy sang ô kế tiếp: hết một hàng thì xuống đầu hàng dưới, hết cửa sổ thì quay về góc trên bên trái

```
Cửa sổ do CASET (100..103) và PASET (50..51) tạo ra:

           x=100  101   102   103
   y=50  |  [1] | [2] | [3] | [4] |
   y=51  |  [5] | [6] | [7] | [8] |

Sau RAMWR, các pixel gửi tới lần lượt lấp đầy theo thứ tự 1 -> 8.
Gửi tiếp pixel thứ 9 thì con trỏ quay lại ô [1] và ghi đè.
```

Nhờ cơ chế này, MCU không cần gửi kèm toạ độ cho từng pixel. Vẽ một vùng 100 x 100 chỉ tốn 11 byte lệnh khai báo cửa sổ, sau đó là 20000 byte pixel thuần tuý.

Trong khi đó, controller liên tục quét GRAM ra panel ở tần số riêng của nó (ILI9341 mặc định khoảng 70 Hz). Điều này hoàn toàn song song và độc lập với MCU.

### 2.5. Các biến thể của DBI

Chuẩn MIPI DBI định nghĩa ba Type, phân biệt theo cách tạo tín hiệu strobe.

#### 2.5.1. Type A - Motorola 6800

Type A dùng một chân **E (Enable)** làm strobe chung cho cả đọc lẫn ghi, và một chân **R/W** để chọn chiều đọc/ghi. Muốn ghi thì đặt R/W = 0 rồi tạo xung trên E; muốn đọc thì đặt R/W = 1 rồi cũng tạo xung trên E. Dữ liệu được chốt ở **cạnh xuống** của E.

| Chân | Vai trò |
|------|---------|
| CSX | Chip select, active low |
| D/CX | Chọn command hay data |
| R/W | 0 = write, 1 = read |
| E | Enable strobe |
| D[7:0] hoặc D[15:0] | Bus dữ liệu |

Ngày nay ít dùng loại này vì không map trực tiếp được sang bus SRAM của MCU.

#### 2.5.2. Type B - Intel 8080

Type B đi theo hướng ngược lại, nó dùng **hai strobe riêng biệt**, WRX cho ghi và RDX cho đọc. Chiều truyền được suy ra từ việc chân nào đang có xung. Dữ liệu ghi được chốt ở cạnh lên của WRX.

| Chân | Vai trò |
|------|---------|
| CSX | Chip select, active low |
| D/CX | 0 = command, 1 = data |
| WRX | Write strobe, active low |
| RDX | Read strobe, active low |
| D[7:0] hoặc D[15:0] | Bus dữ liệu hai chiều |

Type B chia tiếp thành **8080-I** (bus 8/16 bit) và **8080-II** (bus 9/18 bit). Datasheet ILI9341 ghi rõ nó hỗ trợ 8080-I system 16-bit parallel bus interface.

Đây là biến thể phổ biến nhất cho parallel do tập tín hiệu CSX + WRX + RDX + bus dữ liệu trùng khớp với giao diện của một con SRAM bất đồng bộ. Nhờ vậy MCU chỉ cần có external memory controller là điều khiển được màn hình mà không tốn thêm phần cứng nào, chi tiết ở [mục 4](#4-giao-tiếp-qua-fsmc).

#### 2.5.3. Type C - Serial

Type C hy sinh tốc độ để đổi lấy số chân. Nó sử dụng giao tiếp serial SPI mode 0 (CPOL = 0, CPHA = 0, MSB first), với SCL vừa làm clock vừa làm strobe cho từng bit.

Spec định nghĩa 3 option nhưng trên thực tế nhà sản xuất thường gọi theo số dây.

**3-line serial (khung 9 bit)** - chỉ cần CSX, SCL, SDA. Không có chân D/CX vật lý, thay vào đó bit D/CX được nhúng làm bit đầu tiên của mỗi khung 9 bit:

```
 bit 8  |            bit 7 ... bit 0
 D/CX   |  D7  D6  D5  D4  D3  D2  D1  D0
```

Tiết kiệm được 1 chân nhưng rất khó dùng: ngoại vi SPI của STM32F103 chỉ hỗ trợ khung 8 hoặc 16 bit, không có 9 bit. Muốn dùng phải bit-bang bằng GPIO hoặc gom 8 khung 9-bit thành 9 byte rồi mới đẩy qua SPI.

**4-line serial (khung 8 bit)** - CSX, SCL, SDA và D/CX tách ra thành một chân GPIO riêng. Mỗi khung đúng 8 bit nên dùng thẳng được ngoại vi SPI kèm DMA, code cũng đơn giản: hạ D/CX xuống gửi lệnh, kéo lên gửi dữ liệu. Đây là kiểu mà hầu như mọi module SPI TFT trên thị trường sử dụng.

Cả hai kiểu đều có thể thêm **chân SDO (MISO)** nếu cần đọc ngược dữ liệu từ display.

### 2.6. Các tín hiệu phụ trợ

Ngoài các chân thuộc bus DBI, module TFT thường đưa ra thêm vài chân không nằm trong chuẩn nhưng cần cho vận hành:

| Chân | Bắt buộc | Chức năng |
|------|----------|-----------|
| RESX | Nên có | Hardware reset, active low, giữ mức thấp tối thiểu 10 µs |
| TE | Không | Tearing Effect output, báo thời điểm panel quét xong một khung |
| LED / BLK | Có | Backlight, thường điều khiển qua transistor và PWM |
| IM[3:0] | Không | Chọn kiểu interface, đa số module đã hàn cứng sẵn trên PCB |

### 2.7. Một pixel đi qua bus DBI như thế nào

Đến đây ta đã biết bus DBI truyền byte ra sao. Nhưng thứ cần vẽ lên màn hình là pixel nên còn một câu hỏi: một pixel được đóng gói thành bao nhiêu byte và bus rộng bao nhiêu bit thì tải được gì?

Cần tách bạch hai khái niệm hay bị gộp làm một:

| | Ý nghĩa | Quyết định bởi |
|---|---|---|
| **Bus width** | Số sợi dây dữ liệu nối giữa MCU và display | Phần cứng, cố định từ lúc thiết kế mạch |
| **Pixel format** | Số bit mã hoá một pixel | Phần mềm, có thể đổi bằng lệnh `COLMOD` (0x3A) bất cứ lúc nào |

Hai định dạng pixel thông dụng:

| Giá trị COLMOD | Format | Bit/pixel | Ghi chú |
|----------------|--------|-----------|---------|
| 0x55 | RGB565 | 16 | Phổ biến nhất, 1 pixel = 2 byte |
| 0x66 | RGB666 | 18 | 1 pixel = 3 byte, mỗi byte chỉ dùng 6 bit cao |

Bố cục bit của một pixel RGB565 - 5 bit đỏ, 6 bit xanh lá, 5 bit xanh dương:

```
        Byte 0                    Byte 1
R4 R3 R2 R1 R0 G5 G4 G3   G2 G1 G0 B4 B3 B2 B1 B0
```

Cùng một pixel RGB565 đó sẽ đi qua bus theo những cách khác nhau:

| Kiểu bus | Số chu kỳ cho 1 pixel | Cách đi |
|----------|----------------------|---------|
| Parallel 16 bit | 1 | Cả 16 bit lên bus cùng lúc, một xung WRX là xong |
| Parallel 8 bit | 2 | Byte 0 trước, byte 1 sau, hai xung WRX |
| Serial | 16 xung SCL | Từng bit một, bắt đầu từ R4 |

Đây là lý do bus parallel 16 bit nhanh hơn hẳn: mỗi pixel chỉ tốn đúng một chu kỳ bus.

Với RGB666, mỗi pixel là 3 byte nên trên bus 16 bit phải chia thành 2 chu kỳ. Đó chính là lý do MIPI sinh ra biến thể 8080-II với bus 18 bit để một pixel RGB666 vẫn gọn trong một chu kỳ.

:::warning Thứ tự byte là bẫy lớn nhất
Bảng trên cho thấy với bus 8 bit và với serial, một pixel bị tách thành chuỗi byte và byte chứa Red bắt buộc phải đi trước. Trong khi đó STM32 lưu số 16 bit trong bộ nhớ theo kiểu little-endian, tức byte thấp nằm trước. Chỉ cần đẩy thẳng mảng `uint16_t` ra SPI 8 bit là màu sẽ sai hoàn toàn. Cơ chế và cách xử lý được phân tích ở [mục 5.2](#52-thứ-tự-byte-little-endian-hay-big-endian).
:::

## 3. Bộ lệnh DCS

### 3.1. Cách phân biệt command và parameter

Mọi giao dịch DCS đều theo khuôn mẫu:

```
D/CX = 0  ->  gửi 1 byte mã lệnh
D/CX = 1  ->  gửi N byte tham số (N phụ thuộc từng lệnh, có thể bằng 0)
```

Với `RAMWR`, phần tham số chính là toàn bộ dữ liệu pixel và có thể dài tùy ý cho đến khi có lệnh khác.

:::warning Tham số DCS luôn là big-endian
Mọi tham số nhiều byte của DCS đều gửi byte cao trước. Ví dụ `CASET` với cột 0 đến 239 phải gửi `0x00 0x00 0x00 0xEF`, không phải theo thứ tự byte trong bộ nhớ của STM32.
:::

### 3.2. Các lệnh DCS quan trọng

| Mã | Tên | Chức năng |
|----|-----|-----------|
| 0x00 | NOP | Không làm gì, dùng để hủy lệnh đang dở |
| 0x01 | SWRESET | Software reset, chờ 5 ms (120 ms nếu đang ở sleep mode) |
| 0x04 | RDDID | Đọc ID nhà sản xuất, dùng để kiểm tra kết nối |
| 0x09 | RDDST | Đọc thanh ghi trạng thái display |
| 0x0A | RDDPM | Đọc power mode hiện tại |
| 0x10 | SLPIN | Vào sleep mode, tắt bộ tạo điện áp bên trong |
| 0x11 | SLPOUT | Thoát sleep mode, **bắt buộc chờ 120 ms** trước lệnh kế tiếp |
| 0x12 | PTLON | Bật partial mode, chỉ quét vùng đã khai báo bằng PTLAR |
| 0x13 | NORON | Trở về normal display mode, quét toàn màn hình |
| 0x20 | INVOFF | Tắt đảo màu |
| 0x21 | INVON | Bật đảo màu, nhiều panel IPS cần bật lệnh này |
| 0x26 | GAMSET | Chọn một trong các gamma curve dựng sẵn |
| 0x28 | DISPOFF | Tắt hiển thị, GRAM vẫn giữ nguyên nội dung |
| 0x29 | DISPON | Bật hiển thị |
| 0x2A | CASET | Đặt vùng cột (Column Address Set) |
| 0x2B | PASET | Đặt vùng hàng (Page Address Set) |
| 0x2C | RAMWR | Bắt đầu ghi pixel vào GRAM, con trỏ về góc trên trái của cửa sổ |
| 0x2E | RAMRD | Đọc pixel ngược từ GRAM |
| 0x30 | PTLAR | Khai báo vùng cho partial mode |
| 0x33 | VSCRDEF | Định nghĩa vùng cuộn dọc bằng phần cứng |
| 0x34 | TEOFF | Tắt Tearing Effect |
| 0x35 | TEON | Bật Tearing Effect |
| 0x36 | MADCTL | Điều khiển hướng quét, xoay, lật, thứ tự RGB/BGR |
| 0x37 | VSCRSADD | Đặt dòng bắt đầu của vùng cuộn dọc |
| 0x38 | IDMOFF | Tắt idle mode |
| 0x39 | IDMON | Bật idle mode, giảm còn 8 màu để tiết kiệm điện |
| 0x3A | COLMOD | Chọn pixel format (0x55 = RGB565, 0x66 = RGB666) |
| 0x3C | RAMWRC | Ghi tiếp vào GRAM mà không reset con trỏ về đầu cửa sổ |
| 0x44 | STE | Đặt dòng quét sẽ phát tín hiệu TE |
| 0x51 | WRDISBV | Đặt độ sáng backlight (chỉ với panel có khối điều khiển đèn) |
| 0x53 | WRCTRLD | Bật/tắt khối điều khiển hiển thị và backlight |

### 3.3. Lệnh riêng của nhà sản xuất

Bên cạnh DCS, mỗi controller có thêm tập lệnh riêng, thường nằm ở dải 0xB0-0xFF. Đây là các lệnh chỉnh phần analog và bắt buộc phải đọc từ datasheet, không thể suy đoán:

| Mã | Tên (ILI9341) | Chức năng |
|----|---------------|-----------|
| 0xB1 | FRMCTR1 | Frame rate control ở normal mode, quyết định tần số quét panel |
| 0xB4 | INVTR | Display inversion control |
| 0xB6 | DISCTRL | Display function control, số dòng quét, chiều quét |
| 0xC0 | PWCTR1 | Power control 1, mức GVDD |
| 0xC1 | PWCTR2 | Power control 2, hệ số bơm điện áp |
| 0xC5 | VMCTR1 | VCOM control, ảnh hưởng trực tiếp tới độ tương phản |
| 0xE0 | PGAMCTRL | 15 hệ số gamma cho nửa chu kỳ dương |
| 0xE1 | NGAMCTRL | 15 hệ số gamma cho nửa chu kỳ âm |
| 0xF2 | E3GAMMA | Bật/tắt khối gamma 3 điểm |

### 3.4. Thanh ghi MADCTL (0x36)

Đây là thanh ghi hay phải chỉnh nhất, chỉ 1 byte nhưng quyết định cả hướng xoay lẫn thứ tự màu:

| Bit | Tên | Ý nghĩa khi bằng 1 |
|-----|-----|--------------------|
| D7 | MY | Đảo thứ tự hàng (lật dọc) |
| D6 | MX | Đảo thứ tự cột (lật ngang) |
| D5 | MV | Hoán đổi hàng và cột (xoay 90 độ) |
| D4 | ML | Đảo chiều quét dọc của panel |
| D3 | BGR | Panel dùng thứ tự BGR thay vì RGB |
| D2 | MH | Đảo chiều quét ngang của panel |

Bốn hướng xoay thông dụng của ILI9341 (module thường cần bật sẵn bit BGR):

| Hướng | Độ phân giải | MADCTL |
|-------|--------------|--------|
| 0 độ (portrait) | 240 x 320 | 0x48 |
| 90 độ (landscape) | 320 x 240 | 0x28 |
| 180 độ | 240 x 320 | 0x88 |
| 270 độ | 320 x 240 | 0xE8 |

### 3.5. Luồng khởi tạo và vẽ điển hình

```c
/* Khởi tạo tối thiểu */
lcd_reset_hw();                      /* RESX thấp >= 10us, rồi chờ 120ms */
lcd_cmd(0x01);        HAL_Delay(120);          /* SWRESET            */
lcd_cmd(0x11);        HAL_Delay(120);          /* SLPOUT             */
lcd_cmd_data(0x3A, (uint8_t[]){0x55}, 1);      /* COLMOD  = RGB565   */
lcd_cmd_data(0x36, (uint8_t[]){0x48}, 1);      /* MADCTL  = portrait */
/* ... nạp tiếp các lệnh power/gamma theo datasheet ... */
lcd_cmd(0x29);                                 /* DISPON             */

/* Vẽ một vùng chữ nhật */
void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];

    buf[0] = x0 >> 8;  buf[1] = x0 & 0xFF;
    buf[2] = x1 >> 8;  buf[3] = x1 & 0xFF;
    lcd_cmd_data(0x2A, buf, 4);                /* CASET              */

    buf[0] = y0 >> 8;  buf[1] = y0 & 0xFF;
    buf[2] = y1 >> 8;  buf[3] = y1 & 0xFF;
    lcd_cmd_data(0x2B, buf, 4);                /* PASET              */

    lcd_cmd(0x2C);                             /* RAMWR, sau đó đẩy pixel */
}
```

:::warning CASET và PASET thay đổi ý nghĩa khi xoay
Toạ độ trong CASET/PASET là toạ độ **của GRAM**, không phải của khung nhìn. Khi bật bit MV để xoay 90 độ, chiều mà CASET điều khiển sẽ trở thành chiều ngang trên màn hình. Nếu quên hoán đổi giới hạn 239/319 tương ứng, hình sẽ bị cắt mất một phần hoặc quấn vòng sang hàng kế tiếp.
:::

## 4. Giao tiếp qua FSMC

### 4.1. FSMC là gì

**FSMC (Flexible Static Memory Controller)** là ngoại vi cho phép STM32 mở rộng bộ nhớ ra bên ngoài. Nó tạo ra một bus song song gồm address bus, data bus và các tín hiệu điều khiển, hỗ trợ SRAM, PSRAM, NOR Flash và NAND Flash.

Điểm đặc biệt của FSMC là nó ánh xạ thiết bị ngoài vào không gian địa chỉ của CPU. Sau khi cấu hình, việc ghi vào SRAM ngoài chỉ đơn giản là gán giá trị cho một con trỏ và FSMC sẽ tự sinh toàn bộ chu kỳ bus mà CPU không cần biết.

Không gian địa chỉ Bank 1 (dành cho NOR/PSRAM/SRAM):

| Chip select | Vùng địa chỉ |
|-------------|--------------|
| NE1 | 0x6000_0000 - 0x63FF_FFFF |
| NE2 | 0x6400_0000 - 0x67FF_FFFF |
| NE3 | 0x6800_0000 - 0x6BFF_FFFF |
| NE4 | 0x6C00_0000 - 0x6FFF_FFFF |

:::warning Không phải STM32F103 nào cũng có FSMC
FSMC chỉ có trên các bản high-density và XL-density với 100 hoặc 144 chân. Các bản LQFP48 như F103C8T6 (Blue Pill) không có FSMC, chỉ có thể dùng SPI hoặc bit-bang GPIO.
:::

### 4.2. Vì sao màn hình dùng được ngoại vi vốn dành cho SRAM

Hãy đặt cạnh nhau chu kỳ ghi của SRAM bất đồng bộ và của DBI Type B:

| SRAM bất đồng bộ | ILI9341 (8080) |
|------------------|----------------|
| CE - chip enable | CSX |
| WE - write enable | WRX |
| OE - output enable | RDX |
| D[15:0] - bus dữ liệu | D[15:0] |
| A[n:0] - bus địa chỉ | D/CX (chỉ 1 bit) |

Hai bên **giống nhau về mặt điện**: kéo chip select xuống thấp, đặt dữ liệu lên bus, tạo một xung strobe ghi, dữ liệu được chốt ở cạnh lên của xung đó. Khác biệt duy nhất là số lượng "ô nhớ": SRAM có hàng triệu ô nên cần cả bus địa chỉ, còn màn hình chỉ có đúng **hai ô** - một ô command và một ô data.

Từ đó ra cách làm kinh điển: **nối một đường address bất kỳ của FSMC vào chân D/CX**. Khi CPU ghi vào địa chỉ có bit đó bằng 0, FSMC kéo đường address xuống thấp, màn hình hiểu là command. Ghi vào địa chỉ có bit đó bằng 1 thì màn hình hiểu là data.

Bản thân màn hình **không hề giải mã địa chỉ**. Nó chỉ lấy mẫu mức logic của chân D/CX tại thời điểm WRX tích cực. Đường address chỉ đóng vai trò một chân GPIO được phần cứng bật/tắt tự động theo địa chỉ mà CPU truy cập - nhờ vậy tiết kiệm được thao tác đảo chân D/CX bằng phần mềm giữa command và data.

### 4.3. Ánh xạ chân và địa chỉ

Cách nối phổ biến nhất là dùng NE1 và đường A16:

| FSMC | ILI9341 | Ghi chú |
|------|---------|---------|
| FSMC_NE1 | CSX | Chip select |
| FSMC_NWE | WRX | Write strobe |
| FSMC_NOE | RDX | Read strobe |
| FSMC_A16 | D/CX | Chọn command hay data |
| FSMC_D[15:0] | D[15:0] | Bus dữ liệu 16-bit |
| GPIO bất kỳ | RESX | Reset, điều khiển bằng phần mềm |

Cần lưu ý cách FSMC dịch địa chỉ: với **bus dữ liệu 16-bit**, chân FSMC_A[x] được nối vào HADDR[x+1] (vì mỗi địa chỉ ứng với 2 byte). Do đó để bật FSMC_A16 phải bật bit 17 của địa chỉ CPU:

```c
/* Bus 16-bit: A16 <-> HADDR17 -> offset 1 << 17 = 0x20000 */
#define LCD_BASE        0x60000000UL
#define LCD_REG         (*(volatile uint16_t *)(LCD_BASE + 0x00000))
#define LCD_RAM         (*(volatile uint16_t *)(LCD_BASE + 0x20000))
```

:::warning Offset khác nhau giữa bus 8-bit và 16-bit
Với bus 8-bit thì A16 tương ứng HADDR16, offset là 0x10000. Với bus 16-bit thì offset là 0x20000. Dùng nhầm offset khiến D/CX không bao giờ đổi trạng thái, hậu quả là màn hình nhận toàn command hoặc toàn data - biểu hiện là màn hình trắng hoàn toàn dù code chạy đúng.
:::

### 4.4. Cấu hình timing

FSMC sinh chu kỳ bus theo số chu kỳ HCLK khai báo trong thanh ghi `FSMC_BTR`:

| Trường | Ý nghĩa |
|--------|---------|
| ADDSET | Số chu kỳ giữ address ổn định trước khi strobe tích cực |
| DATAST | Số chu kỳ giữ strobe ở mức tích cực |
| BUSTURN | Số chu kỳ nghỉ giữa hai giao dịch |

Với mode A / mode 1, độ dài một chu kỳ ghi xấp xỉ:

```
T_write = (ADDSET + 1 + DATAST + 1) x T_HCLK
```

Áp dụng cho STM32F103 chạy 72 MHz (T_HCLK = 13.9 ns) và ILI9341 yêu cầu chu kỳ ghi tối thiểu tWC = 66 ns:

```
ADDSET = 1, DATAST = 2
T_write = (1 + 1 + 2 + 1) x 13.9 = 69.5 ns  >= 66 ns   -> hợp lệ
Độ rộng xung WRX = (DATAST + 1) x 13.9 = 41.7 ns >= 15 ns -> hợp lệ
```

:::warning Chu kỳ đọc chậm hơn chu kỳ ghi rất nhiều
ILI9341 yêu cầu tWC (write cycle) tối thiểu 66 ns nhưng tRC (read cycle) tối thiểu tới 450 ns - chậm gần 7 lần. Nếu dùng chung một bộ timing cho cả đọc và ghi, hoặc là ghi sai (khi timing quá nhanh) hoặc là chậm mất 7 lần (khi timing quá chậm). Giải pháp là bật EXTMOD để tách timing đọc trong `FSMC_BTR` và timing ghi trong `FSMC_BWTR`.
:::

### 4.5. Truy cập màn hình như truy cập biến

Sau khi cấu hình xong, toàn bộ driver rút gọn còn vài dòng:

```c
static inline void lcd_cmd(uint16_t cmd)    { LCD_REG = cmd; }
static inline void lcd_data(uint16_t data)  { LCD_RAM = data; }

void lcd_fill(uint16_t color, uint32_t count)
{
    lcd_cmd(0x2C);                       /* RAMWR */
    while (count--) {
        LCD_RAM = color;                 /* mỗi lần gán = 1 chu kỳ bus ~70ns */
    }
}
```

Không cần hàm HAL, không cần chờ cờ trạng thái, không cần đảo chân D/CX. Một lệnh `STR` của CPU là xong một pixel. Đây chính là lý do FSMC nhanh hơn SPI rất nhiều.

Muốn nhanh hơn nữa thì dùng DMA memory-to-memory: nguồn là buffer trong RAM (bật tăng địa chỉ), đích là `LCD_RAM` (tắt tăng địa chỉ), data width 16-bit.

### 4.6. So sánh FSMC và SPI

| | SPI 4-line @ 9 MHz | FSMC 16-bit, chu kỳ 70 ns |
|---|---|---|
| Thời gian mỗi pixel | ~1.78 µs (16 bit) | ~0.07 µs |
| Băng thông pixel | ~0.56 Mpx/s | ~14.3 Mpx/s |
| Thời gian vẽ full frame 240x320 | ~137 ms (~7 fps) | ~5.4 ms (~185 fps lý thuyết) |
| Số chân tiêu tốn | 5 | 21+ |

Con số FSMC ở trên là giới hạn của bus. Trên thực tế tốc độ bị chặn bởi việc CPU sinh dữ liệu, nên hãy dùng DMA hoặc vòng lặp đã unroll để tiến gần con số này.

## 5. Các thông số cần quan tâm khi làm việc với display

### 5.1. Tốc độ SPI và chu kỳ ghi tối thiểu

Datasheet ILI9341 quy định ở phần Serial Interface Characteristics:

| Thông số | Ký hiệu | Giá trị tối thiểu | Tần số tương ứng |
|----------|---------|-------------------|------------------|
| Chu kỳ clock khi ghi | tSCYCW | 100 ns | 10 MHz |
| Chu kỳ clock khi đọc | tSCYCR | 150 ns | 6.6 MHz |

Trên STM32F103, với SPI1 nằm trên APB2 chạy 72 MHz:

| Prescaler | Tần số SCK | Đánh giá |
|-----------|-----------|----------|
| /8 | 9 MHz | Đúng spec, an toàn |
| /4 | 18 MHz | Vượt spec 1.8 lần |
| /2 | 36 MHz | Vượt spec 3.6 lần |

**Điều gì xảy ra khi vượt chu kỳ ghi tối thiểu?**

Display không kịp lấy mẫu mức logic tại cạnh clock, dẫn tới:
- Bit bị đọc sai ngẫu nhiên: xuất hiện các pixel lỗi màu rải rác
- Mất hoặc thừa một xung clock: toàn bộ ảnh lệch đi một bit, biểu hiện là hình bị xé chéo và đổi màu hoàn toàn
- Byte lệnh bị hiểu sai: display nhảy vào chế độ lạ, mất hình hoặc treo cứng
- Lỗi phụ thuộc nhiệt độ và độ dài dây: chạy tốt ở bàn nhưng lỗi khi vào vỏ hoặc khi thiết bị nóng lên

:::warning Chú ý
Rất nhiều thư viện chạy được ở 36 MHz vì bản thân IC còn dư margin và dây ngắn. Nhưng đó là chạy ngoài spec: mọi lỗi phát sinh sẽ không tái hiện đều đặn và cực kỳ khó debug. Nguyên tắc an toàn: dùng 9 MHz cho sản phẩm thật và mỗi khi gặp lỗi hiển thị lạ hãy hạ tốc độ xuống trước tiên để loại trừ nguyên nhân.
:::

### 5.2. Thứ tự byte: little-endian hay big-endian

Đây là nguyên nhân số một gây lỗi màu và cũng là chỗ dễ hiểu nhầm nhất.

**Quy tắc: mọi thứ đi trên dây đều là big-endian.** DCS quy định byte có trọng số cao được truyền trước, áp dụng cho cả tham số lệnh lẫn dữ liệu pixel.

Nhưng STM32 là kiến trúc little-endian. Một biến `uint16_t color = 0xF800` nằm trong bộ nhớ theo thứ tự:

```
địa chỉ +0 : 0x00   <- byte thấp
địa chỉ +1 : 0xF8   <- byte cao
```

Ví dụ lỗi kinh điển:

```c
uint16_t buf[240];
for (int i = 0; i < 240; i++) buf[i] = 0xF800;      /* muốn màu đỏ */

/* SPI 8 bit gửi 0x00 rồi mới tới 0xF8
   -> display nhận pixel 0x00F8 -> ra màu xanh dương đậm */
HAL_SPI_Transmit_DMA(&hspi1, (uint8_t *)buf, 480);
```

### 5.3. Thứ tự màu RGB hay BGR

Bit **BGR (D3 của MADCTL)** cho biết panel nối subpixel theo thứ tự nào. Nếu sai bit này, màu đỏ và xanh dương sẽ đổi chỗ cho nhau trong khi màu xanh lá vẫn đúng - đây là dấu hiệu nhận biết rất rõ ràng.

Cách phân biệt nhanh khi debug màu sai:

| Hiện tượng | Nguyên nhân |
|------------|-------------|
| Đỏ và xanh dương đổi chỗ, xanh lá đúng | Bit BGR trong MADCTL sai |
| Màu hoàn toàn không liên quan, ảnh còn bị lệch | Sai thứ tự byte (mục 5.2) |
| Màu như âm bản của ảnh gốc | Thiếu `INVON` hoặc `INVOFF` |
| Ảnh chỉ có 8 màu | Đang ở idle mode, gọi `IDMOFF` |

### 5.4. Reset và timing khởi tạo

Chuỗi khởi tạo có vài mốc thời gian bắt buộc, bỏ qua là màn hình không lên:

| Bước | Thời gian chờ | Lý do |
|------|---------------|-------|
| RESX giữ mức thấp | tối thiểu 10 µs | Đảm bảo logic bên trong nhận được reset |
| Sau khi nhả RESX | 120 ms | Chờ khối power-on nội bộ ổn định |
| Sau `SWRESET` | 5 ms (120 ms nếu từ sleep) | Nạp lại giá trị mặc định |
| Sau `SLPOUT` | **120 ms** | Chờ bơm điện áp và bộ dao động khởi động |
| Sau `DISPON` | không bắt buộc | - |

:::warning Bỏ 120 ms sau SLPOUT là lỗi hay gặp nhất
Nếu gửi lệnh tiếp theo quá sớm, display sẽ nhận lệnh nhưng bộ tạo điện áp bên trong chưa sẵn sàng. Kết quả là màn hình trắng hoặc hiển thị nhạt màu - dù logic code hoàn toàn đúng. Đừng thay bằng delay ngắn hơn, đây là con số trong datasheet.
:::

### 5.5. Nguồn và backlight

- **Mức logic**: đa số module TFT chạy logic 3.3 V. Nối thẳng vào MCU 5 V có thể làm hỏng IC nếu module không có level shifter
- **Dòng backlight**: đèn nền tiêu thụ 40-80 mA, vượt xa khả năng của một chân GPIO (20 mA). Phải qua transistor hoặc MOSFET
- **Tụ decoupling**: thiếu tụ 100 nF sát chân VCC gây sụt áp mỗi khi backlight bật, biểu hiện là nhiễu ngẫu nhiên trên màn hình
- **Điều chỉnh độ sáng**: dùng PWM tần số trên 200 Hz để mắt không thấy nhấp nháy, đồng thời tránh hiện tượng vân sọc khi chụp ảnh màn hình

## 6. Hiệu năng và chất lượng hiển thị

### 6.1. Băng thông và FPS

Công thức nền tảng:

```
Dữ liệu 1 frame = width x height x (bit/pixel) / 8
FPS tối đa      = Băng thông interface / Dữ liệu 1 frame
```

Với ILI9341 240x320 ở RGB565: 240 x 320 x 2 = **153,600 byte mỗi frame**.

| Interface | Băng thông lý thuyết | FPS full-screen |
|-----------|----------------------|-----------------|
| SPI @ 9 MHz | 1.125 MB/s | ~7 fps |
| SPI @ 18 MHz | 2.25 MB/s | ~14 fps |
| SPI @ 36 MHz | 4.5 MB/s | ~29 fps |
| FSMC 16-bit, chu kỳ 70 ns | 28.6 MB/s | ~185 fps |

Đây là con số **lý thuyết**, chưa trừ overhead của lệnh `CASET`/`PASET`/`RAMWR`, thời gian CPU tính toán nội dung, và độ trễ khi bật/tắt chân CS. Thực tế nên trừ đi khoảng 20-30%.

Kết luận thực dụng: với SPI, **đừng bao giờ vẽ lại toàn màn hình mỗi frame**. Chỉ cập nhật vùng thực sự thay đổi (dirty rectangle) - vẽ lại một ô chữ 16x16 chỉ tốn 512 byte, tức nhanh gấp 300 lần so với vẽ cả màn hình.

### 6.2. Tearing và tín hiệu TE

**Tearing** là hiện tượng một khung hình trên màn hình chứa một phần nội dung cũ và một phần nội dung mới, tạo ra đường gãy ngang khi có chuyển động.

Nguyên nhân: MCU ghi vào GRAM trong khi controller đang quét chính vùng GRAM đó ra panel. Hai quá trình này chạy độc lập và không đồng bộ với nhau.

```
Con trỏ quét của panel   ────────────►  đang ở dòng 150
Con trỏ ghi của MCU      ────────────►  vừa ghi tới dòng 100

-> dòng 0-100 hiển thị nội dung mới
-> dòng 101-239 vẫn là nội dung cũ
-> mắt nhìn thấy một vết cắt ngang tại dòng 100
```

**Cách khắc phục bằng chân TE:**

1. Bật `TEON` (0x35) với tham số 0x00 - display sẽ phát xung trên chân TE mỗi khi quét xong một khung
2. Nối TE vào một chân EXTI của STM32
3. Trong ISR, bắt đầu việc ghi frame mới ngay khi có cạnh tích cực

Lúc đó MCU ghi đúng vào khoảng thời gian **V-blank** (panel đang nghỉ giữa hai khung), đảm bảo con trỏ ghi luôn đi trước con trỏ quét.

Lệnh `STE` (0x44) cho phép chọn dòng cụ thể sẽ phát tín hiệu TE, hữu ích khi chỉ cập nhật một phần màn hình và muốn canh chính xác thời điểm.

:::warning TE chỉ có ý nghĩa khi ghi đủ nhanh
Đồng bộ với TE chỉ hết tearing nếu MCU ghi xong frame trước khi con trỏ quét đuổi kịp. Với SPI 9 MHz (137 ms/frame) và panel quét 70 Hz (14 ms/frame), con trỏ quét sẽ vượt qua con trỏ ghi vài lần - TE không cứu được. Trong trường hợp này giải pháp thật sự là giảm diện tích vùng cập nhật hoặc chuyển sang parallel.
:::

Hai kỹ thuật bổ trợ:
- **Partial update**: chỉ ghi lại vùng thay đổi, giảm cả thời gian ghi lẫn xác suất bị tearing
- **Hardware vertical scroll** (`VSCRDEF` + `VSCRSADD`): controller tự dịch vùng hiển thị mà không cần ghi lại pixel nào, dùng cho danh sách cuộn hoặc terminal

### 6.3. Frame rate của panel

Tần số quét panel do lệnh `FRMCTR1` (0xB1) quy định, gồm hai tham số: hệ số chia clock (DIVA) và số dòng của một chu kỳ khung (RTNA).

```
Frame rate = f_OSC / (DIVA x (RTNA_clocks x (lines + VFP + VBP)))
```

Giá trị mặc định của ILI9341 cho khoảng 70 Hz. Hạ frame rate xuống 50-60 Hz giúp giảm tiêu thụ điện, nhưng xuống thấp quá sẽ thấy nhấp nháy rõ.

:::warning Frame rate của panel không phải FPS của ứng dụng
Đây là hai khái niệm hoàn toàn tách biệt. Frame rate là tốc độ controller quét GRAM ra panel - luôn chạy kể cả khi MCU không làm gì. FPS ứng dụng là số lần MCU ghi nội dung mới vào GRAM mỗi giây. Tăng `FRMCTR1` không làm ứng dụng mượt hơn.
:::

### 6.4. Gamma curve

Quan hệ giữa giá trị số của pixel và độ sáng thực tế của tinh thể lỏng là **phi tuyến**. Gamma curve là bảng hiệu chỉnh giúp giá trị 128 thực sự trông như "sáng một nửa" đối với mắt người.

ILI9341 cung cấp hai mức điều chỉnh:
- `GAMSET` (0x26): chọn một curve dựng sẵn, nhanh và đơn giản
- `PGAMCTRL` (0xE0) và `NGAMCTRL` (0xE1): 15 hệ số riêng cho nửa chu kỳ dương và nửa chu kỳ âm, cho phép tinh chỉnh chi tiết

Dấu hiệu gamma sai:

| Hiện tượng | Hướng xử lý |
|------------|-------------|
| Ảnh bạc màu, thiếu tương phản | Chỉnh lại E0/E1, kiểm tra VCOM (0xC5) |
| Vùng tối bệt thành một mảng đen | Sửa các hệ số gamma ở đoạn đầu |
| Vùng sáng cháy trắng, mất chi tiết | Sửa các hệ số gamma ở đoạn cuối |
| Ám màu ở vùng xám | Lệch giữa gamma dương và gamma âm |

Trên thực tế, hãy **copy nguyên bộ giá trị gamma từ code mẫu của nhà sản xuất module** rồi chỉ tinh chỉnh khi thật sự cần. Đây là các con số phụ thuộc vào từng loại tấm panel cụ thể, không thể tính ra bằng lý thuyết.

### 6.5. Rotate

Có hai cách xoay ảnh, khác nhau hoàn toàn về chi phí:

| Cách | Cơ chế | Chi phí |
|------|--------|---------|
| Xoay bằng MADCTL | Controller đổi chiều con trỏ ghi GRAM | Bằng 0, chỉ 1 lệnh |
| Xoay bằng phần mềm | MCU tính lại toạ độ từng pixel | Rất tốn CPU |

Luôn ưu tiên MADCTL. Khi xoay bằng MADCTL cần đồng bộ ba thứ trong driver:

```c
void lcd_set_rotation(uint8_t rot)
{
    static const uint8_t madctl[4] = { 0x48, 0x28, 0x88, 0xE8 };

    lcd_cmd_data(0x36, &madctl[rot], 1);

    /* Cập nhật kích thước logic, nếu quên thì clipping sẽ sai */
    if (rot & 1) { lcd_w = 320; lcd_h = 240; }
    else         { lcd_w = 240; lcd_h = 320; }
}
```

:::warning Một số panel cần cộng offset khi xoay
Với các panel có độ phân giải nhỏ hơn GRAM của controller (điển hình là ST7789 240x240 dùng GRAM 240x320), sau khi xoay 180 độ thì vùng hiển thị bị lệch đi đúng bằng phần GRAM dư. Khi đó phải cộng thêm offset vào tham số `CASET`/`PASET` tuỳ theo hướng xoay.
:::

### 6.6. Các kỹ thuật tối ưu

| Kỹ thuật | Nội dung | Hiệu quả |
|----------|----------|----------|
| Dirty rectangle | Chỉ vẽ lại vùng thay đổi | Cao nhất, giảm tới hàng trăm lần dữ liệu |
| DMA | Giải phóng CPU trong lúc truyền | Cho phép tính frame kế tiếp song song |
| Buffer theo dòng | Dựng sẵn 1-2 dòng rồi đẩy một lượt | Tránh overhead mỗi pixel một lời gọi hàm |
| Giữ CS ở mức thấp | Không nhả CS giữa các pixel | Giảm đáng kể overhead trên SPI |
| Bỏ qua CASET/PASET | Ghi tuần tự thì con trỏ tự chạy | Tiết kiệm 11 byte cho mỗi lần vẽ |
| `RAMWRC` (0x3C) | Ghi tiếp mà không reset con trỏ | Dùng khi vẽ nối tiếp nhiều mảnh |

## 7. Bảng tra lỗi thường gặp

| Hiện tượng | Nguyên nhân thường gặp | Cách kiểm tra |
|------------|------------------------|---------------|
| Màn hình trắng hoàn toàn | Chưa chạy init, sai chân D/CX, sai offset FSMC | Đọc `RDDID` (0x04), nếu trả về rác thì lỗi ở lớp giao tiếp |
| Màn hình đen, backlight sáng | Thiếu `SLPOUT` hoặc `DISPON`, sai chuỗi khởi tạo | Kiểm tra lại thứ tự và các mốc delay ở mục 5.4 |
| Không sáng gì cả | Backlight chưa cấp nguồn, thiếu transistor | Đo điện áp trực tiếp trên chân LED |
| Đỏ và xanh dương đổi chỗ | Bit BGR (D3 của MADCTL) sai | Đảo bit D3 |
| Màu như âm bản | Thiếu `INVON` (panel IPS) hoặc thừa `INVON` | Thử đảo giữa 0x20 và 0x21 |
| Màu sai hoàn toàn kèm ảnh lệch | Sai thứ tự byte của RGB565 | Đảo byte hoặc chuyển SPI sang frame 16-bit |
| Pixel lỗi rải rác ngẫu nhiên | SPI quá nhanh, dây dài, thiếu tụ decoupling | Hạ prescaler xuống 1-2 nấc, nếu hết lỗi thì đúng nguyên nhân |
| Ảnh bị xé chéo, dịch dần | Mất hoặc thừa xung clock | Hạ tốc độ SPI, rút ngắn dây |
| Ảnh xoay hoặc lật sai | Sai bit MX/MY/MV của MADCTL | Thử lần lượt 4 giá trị 0x48/0x28/0x88/0xE8 |
| Một phần màn hình không cập nhật | CASET/PASET chưa đổi theo hướng xoay | In ra giá trị lcd_w, lcd_h sau khi xoay |
| Ảnh lệch một vài pixel | Panel cần offset (ST7789 240x240) | Cộng offset vào CASET/PASET theo hướng xoay |
| Đường xé ngang khi có chuyển động | Tearing | Bật TE, hoặc giảm diện tích vùng cập nhật |
| Hiển thị nhạt, thiếu tương phản | Sai gamma hoặc VCOM | Nạp lại bộ giá trị E0/E1/C5 từ code mẫu |
| Chỉ hiển thị 8 màu | Đang bật idle mode | Gửi `IDMOFF` (0x38) |
| Chạy tốt lúc đầu, lỗi khi nóng | Timing ngoài spec | Hạ tốc độ SPI hoặc nới DATAST của FSMC |
| Đọc thanh ghi luôn trả về 0 hoặc 0xFF | Tốc độ đọc quá cao, hoặc chưa nối MISO | Hạ tốc độ riêng cho thao tác đọc |

## 8. Tóm tắt

**Chọn interface:**

- Thiếu chân, UI tĩnh, chi phí thấp → **DBI Type C (4-line serial)**
- Cần refresh nhanh, có animation, MCU có FSMC → **DBI Type B (8080 parallel)**
- Panel lớn không có GRAM → **DPI**, cần MCU có LTDC
- Thiết bị di động, độ phân giải cao → **DSI**, ngoài tầm của MCU phổ thông

**Ba quy tắc dễ nhớ nhất:**

1. Mọi thứ đi trên dây DBI đều **big-endian**, còn STM32 lưu trong bộ nhớ theo **little-endian**. Chỉ cần một trong hai bên bị bỏ sót là màu sẽ sai.
2. Đừng vượt chu kỳ ghi tối thiểu trong datasheet. Gặp lỗi hiển thị lạ, việc đầu tiên luôn là **hạ tốc độ**.
3. Chỉ vẽ lại **vùng thực sự thay đổi**. Đây là tối ưu hiệu quả nhất, hơn mọi kỹ thuật khác cộng lại.

## Tài liệu tham khảo

- [ILI9341 Datasheet](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)
- [MIPI Alliance - Display Interface Specifications](https://www.mipi.org/specifications/display-interface)
- [RM0008 - STM32F103 Reference Manual, chương FSMC](https://www.st.com/resource/en/reference_manual/cd00171190.pdf)
- [AN2784 - Using the STM32 FSMC to drive an LCD](https://www.st.com/resource/en/application_note/an2784-using-the-highdensity-stm32f10xxx-fsmc-peripheral-to-drive-external-memories-stmicroelectronics.pdf)
