## Triển khai USB Protocol trên STM32F103
 
### Tổng quan STM32F103 USB
 
STM32F103 tích hợp USB 2.0 Full Speed Device peripheral với các đặc điểm:
- **Tốc độ**: Full Speed (12 Mbps)
- **Endpoints**: 8 endpoint (EP0-EP7)
- **Buffer**: 512 byte SRAM dành riêng (Packet Buffer Memory)
- **DMA**: Không hỗ trợ DMA cho USB
- **Pin**: PA11 (USB_DM), PA12 (USB_DP)
 
## Bước 1: Cấu hình hardware
 
### 1.1. Sơ đồ kết nối chân
 
```
STM32F103C8T6 USB Pins:
┌──────────────────────────────────────────┐
│                                          │
│  PA12 (USB_DP) ──┬── 1.5kΩ ──── +3.3V   │ ← Pull-up resistor
│                  │                       │
│                  └────────────── USB D+  │
│                                          │
│  PA11 (USB_DM) ──────────────── USB D-  │
│                                          │
│  GND ────────────────────────── USB GND  │
│                                          │
│  +5V ────────────────────────── USB VCC  │
│      (hoặc dùng nguồn riêng)             │
└──────────────────────────────────────────┘
```
 
:::warning Lưu ý quan trọng
- Pull-up resistor 1.5kΩ trên D+ báo cho host biết đây là Full Speed device
- PA12 có thể dùng internal pull-up (nếu có) hoặc external resistor
- Một số board Blue Pill có pull-up 10kΩ (sai giá trị) → cần thay bằng 1.5kΩ
:::

### 1.2. Oscillator và Clock
 
USB yêu cầu clock chính xác 48MHz từ PLL:
 
```
HSE (8MHz) → PLL (×9) → 72MHz System Clock
                    ↓
              USB Prescaler (/1.5)
                    ↓
                 48MHz USB Clock
```
 
**Code cấu hình clock:**
 
```c
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
 
    // Enable HSE (8MHz external crystal)
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;  // 8MHz × 9 = 72MHz
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
 
    // Configure system clock
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;     // 72MHz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;      // 36MHz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;      // 72MHz
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
 
    // USB clock = 72MHz / 1.5 = 48MHz
    __HAL_RCC_USB_CLK_ENABLE();
}
```
 
## Bước 2: Hiểu về USB Registers
 
### 2.1. Các thanh ghi quan trọng
 
STM32F103 có các nhóm register chính:
 
```
USB Peripheral Registers:
├── USB_CNTR    : Control Register (Điều khiển chính)
├── USB_ISTR    : Interrupt Status Register (Trạng thái ngắt)
├── USB_DADDR   : Device Address Register
├── USB_BTABLE  : Buffer Table Address
└── USB_EPnR    : Endpoint n Register (n = 0-7)
 
Packet Buffer Memory:
├── 0x40006000  : Start address
├── Size: 512 byte
└── Layout: Buffer Descriptor Table + Data Buffers
```
 
### 2.2. USB_CNTR Register
 
```
┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
│15 │14 │13 │12 │11 │10 │ 9 │ 8 │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │
├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
│CTR│PMO│ERR│WKU│SUS│RES│SOF│ESO│RES│RES│RES│RES│SUS│WKU│ERR│RES│
│M  │VRM│M  │PM │PM │M  │M  │FM │   │   │   │   │PEN│PEN│PEN│ET │
└───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
 
Bits quan trọng:
- Bit 15 (CTRM)   : Correct Transfer Interrupt Mask
- Bit 11 (SUSPM)  : Suspend Mode Interrupt Mask
- Bit 10 (RESETM) : Reset Interrupt Mask
- Bit 9  (SOFM)   : Start Of Frame Interrupt Mask
- Bit 1  (PDWN)   : Power Down
- Bit 0  (FRES)   : Force Reset
```
 
### 2.3. USB_EPnR Register (Endpoint Register)
 
Mỗi endpoint có một register riêng:
 
```
┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
│15 │14 │13 │12 │11 │10 │ 9 │ 8 │ 7 │ 6 │ 5 │ 4 │ 3 │ 2 │ 1 │ 0 │
├───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┼───┤
│CTR│DTG│STT│STT│SEU│EPI│EPI│EPI│EPI│CTR│DTG│STT│STT│SEA│EPA│EPA│
│_RX│_RX│_RX│_RX│P  │ND│ND│ND│ND│_TX│_TX│_TX│_TX│  │   │   │
└───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘
 
Fields quan trọng:
- CTR_RX (bit 15)     : Correct Transfer RX
- STAT_RX (bits 13-12): RX Status (00=DIS, 01=STALL, 10=NAK, 11=VALID)
- EP_TYPE (bits 10-9) : Endpoint Type (00=BULK, 01=CONTROL, 10=ISO, 11=INTERRUPT)
- EA (bits 3-0)       : Endpoint Address
- STAT_TX (bits 5-4)  : TX Status (00=DIS, 01=STALL, 10=NAK, 11=VALID)
- CTR_TX (bit 7)      : Correct Transfer TX
```
 
### 2.4. Packet Buffer Memory Layout
 
```
Packet Buffer Memory (512 byte):
┌────────────────────────────────────────┐ 0x40006000
│  Buffer Descriptor Table (64 byte)    │
│  ┌──────────────────────────────────┐  │
│  │ EP0 TX: ADDR, COUNT              │  │ ← 4 byte
│  │ EP0 RX: ADDR, COUNT              │  │ ← 4 byte
│  ├──────────────────────────────────┤  │
│  │ EP1 TX: ADDR, COUNT              │  │
│  │ EP1 RX: ADDR, COUNT              │  │
│  ├──────────────────────────────────┤  │
│  │ ...                              │  │
│  │ EP7 TX/RX                        │  │
│  └──────────────────────────────────┘  │
├────────────────────────────────────────┤ 0x40006040
│  Data Buffers (448 byte)               │
│  ┌──────────────────────────────────┐  │
│  │ EP0 TX Buffer (64 byte)          │  │
│  ├──────────────────────────────────┤  │
│  │ EP0 RX Buffer (64 byte)          │  │
│  ├──────────────────────────────────┤  │
│  │ EP1 TX Buffer                    │  │
│  │ EP1 RX Buffer                    │  │
│  │ ...                              │  │
│  └──────────────────────────────────┘  │
└────────────────────────────────────────┘ 0x400061FF
```

## Bước 3: Khởi tạo USB Peripheral
 
### 3.1. Cấu hình GPIO và Clock
 
```c
void USB_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // Enable clock cho GPIO A và USB
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USB_CLK_ENABLE();
    
    // PA11: USB_DM, PA12: USB_DP
    // Không cần cấu hình GPIO vì USB peripheral tự quản lý
    // Nhưng cần enable pull-up trên D+ (PA12)
    
    // Nếu dùng external pull-up: không làm gì
    // Nếu dùng software disconnect: cấu hình PA12 as output
}
```
 
### 3.2. Reset và Enable USB
 
```c
void USB_Device_Init(void)
{
    // 1. Reset USB peripheral
    USB->CNTR = USB_CNTR_FRES;  // Force USB Reset
    HAL_Delay(1);  // Đợi ổn định
    USB->CNTR = 0;  // Clear reset
    
    // 2. Clear interrupt flags
    USB->ISTR = 0;
    
    // 3. Set buffer table address (thường là 0x00)
    USB->BTABLE = 0x00;
    
    // 4. Enable interrupts
    USB->CNTR = USB_CNTR_CTRM  |  // Correct Transfer
                USB_CNTR_RESETM |  // Reset
                USB_CNTR_SUSPM  |  // Suspend
                USB_CNTR_WKUPM;    // Wakeup
    
    // 5. Connect to bus (enable pull-up)
    USB->BCDR |= USB_BCDR_DPPU;  // Enable internal pull-up (nếu có)
    // Hoặc control external pull-up qua GPIO
    
    // 6. Enable NVIC interrupt
    NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 0);
    NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
}
```
 
### 3.3. Cấu hình Endpoint 0 (Control)
 
```c
#define EP0_TX_ADDR  0x40  // Offset trong packet buffer
#define EP0_RX_ADDR  0x80
#define EP0_SIZE     64    // Max packet size
 
void USB_EP0_Init(void)
{
    uint16_t *pBuf;
    
    // 1. Cấu hình Buffer Descriptor Table cho EP0
    pBuf = (uint16_t *)(USB_PMAADDR + 0x00);  // EP0 TX descriptor
    pBuf[0] = EP0_TX_ADDR;  // TX buffer address
    pBuf[1] = 0;            // TX count (sẽ set khi gửi)
    
    pBuf = (uint16_t *)(USB_PMAADDR + 0x04);  // EP0 RX descriptor
    pBuf[0] = EP0_RX_ADDR;  // RX buffer address
    pBuf[1] = 0x8000 | ((EP0_SIZE / 32 - 1) << 10);  // RX count (64 byte)
    
    // 2. Cấu hình EPnR register cho EP0
    USB->EP0R = USB_EP_CONTROL |     // Endpoint Type = Control
                0x0000 |              // Endpoint Address = 0
                USB_EP_RX_VALID |     // RX Status = VALID (ready to receive)
                USB_EP_TX_NAK;        // TX Status = NAK (not ready to send)
}
```
 
**Giải thích RX COUNT register:**
```
RX COUNT format (cho buffer > 32 byte):
┌───┬───────────┬──────────────────┐
│15 │  14-10    │    9-0           │
├───┼───────────┼──────────────────┤
│ 1 │ NUM_BLOCK │   Reserved       │
└───┴───────────┴──────────────────┘
 
NUM_BLOCK = (Buffer_size / 32) - 1
 
Ví dụ: Buffer 64 byte
  NUM_BLOCK = (64 / 32) - 1 = 1
  RX_COUNT = 0x8000 | (1 << 10) = 0x8400
```
 
## Bước 4: Xử lý USB Reset Event
 
Khi host reset USB bus, STM32 sẽ nhận interrupt:
 
```c
void USB_Reset_Handler(void)
{
    // 1. Set device address = 0 (default)
    USB->DADDR = USB_DADDR_EF;  // Enable function, address = 0
    
    // 2. Initialize endpoint 0
    USB_EP0_Init();
    
    // 3. Reset application state
    usb_device_state = USB_STATE_DEFAULT;
    usb_configuration = 0;
    
    // 4. Clear reset flag
    USB->ISTR = ~USB_ISTR_RESET;
}
```
 
**State machine của USB device:**
```
┌─────────────────────────────────────────────────┐
│                                                 │
│  ATTACHED → POWERED → DEFAULT → ADDRESS → CONFIGURED
│     ↑                    ↑          ↑         ↑
│     │                    │          │         │
│  Connect            USB Reset   Set Addr  Set Config
│                                                 │
└─────────────────────────────────────────────────┘
```
 
## Bước 5: Xử lý Control Transfer - Setup Stage
 
### 5.1. Nhận Setup Packet
 
```c
void USB_EP0_Setup_Handler(void)
{
    uint16_t *pBuf;
    USB_SetupPacket_t setup;
    
    // 1. Đọc setup packet từ RX buffer
    pBuf = (uint16_t *)(USB_PMAADDR + EP0_RX_ADDR);
    uint8_t *pSetup = (uint8_t *)&setup;
    
    for (int i = 0; i < 8; i += 2) {
        uint16_t temp = *pBuf++;
        pSetup[i] = temp & 0xFF;
        pSetup[i + 1] = (temp >> 8) & 0xFF;
    }
    
    // 2. Parse setup packet
    uint8_t bmRequestType = setup.bmRequestType;
    uint8_t bRequest = setup.bRequest;
    uint16_t wValue = setup.wValue;
    uint16_t wIndex = setup.wIndex;
    uint16_t wLength = setup.wLength;
    
    // 3. Clear RX flag
    USB->EP0R &= ~USB_EP_CTR_RX;
    
    // 4. Process request
    if ((bmRequestType & 0x60) == 0x00) {
        // Standard request
        USB_Handle_StandardRequest(bRequest, wValue, wIndex, wLength);
    } else if ((bmRequestType & 0x60) == 0x20) {
        // Class request
        USB_Handle_ClassRequest(bRequest, wValue, wIndex, wLength);
    } else if ((bmRequestType & 0x60) == 0x40) {
        // Vendor request
        USB_Handle_VendorRequest(bRequest, wValue, wIndex, wLength);
    }
}
```
 
**Cấu trúc Setup Packet:**
```c
typedef struct {
    uint8_t  bmRequestType;  // D7: Direction, D6-5: Type, D4-0: Recipient
    uint8_t  bRequest;       // Request code
    uint16_t wValue;         // Request-specific parameter
    uint16_t wIndex;         // Request-specific parameter (usually endpoint/interface)
    uint16_t wLength;        // Length of data stage
} __attribute__((packed)) USB_SetupPacket_t;
```
 
### 5.2. Xử lý Standard Requests
 
```c
void USB_Handle_StandardRequest(uint8_t bRequest, uint16_t wValue, 
                                uint16_t wIndex, uint16_t wLength)
{
    switch (bRequest) {
        case USB_REQ_GET_DESCRIPTOR:
            USB_GetDescriptor(wValue, wIndex, wLength);
            break;
            
        case USB_REQ_SET_ADDRESS:
            USB_SetAddress(wValue);
            break;
            
        case USB_REQ_SET_CONFIGURATION:
            USB_SetConfiguration(wValue);
            break;
            
        case USB_REQ_GET_STATUS:
            USB_GetStatus(wIndex, wLength);
            break;
            
        case USB_REQ_SET_FEATURE:
        case USB_REQ_CLEAR_FEATURE:
            // Handle feature requests
            break;
            
        default:
            // Unsupported request → STALL
            USB_EP0_Stall();
            break;
    }
}
```
 
## Bước 6: Xử lý Control Transfer - Data Stage
 
### 6.1. Gửi dữ liệu (IN)
 
```c
void USB_EP0_SendData(uint8_t *data, uint16_t len)
{
    uint16_t *pBuf;
    uint16_t count;
    
    // 1. Tính số byte cần gửi (tối đa 64 byte)
    count = (len > EP0_SIZE) ? EP0_SIZE : len;
    
    // 2. Copy data vào TX buffer
    pBuf = (uint16_t *)(USB_PMAADDR + EP0_TX_ADDR);
    for (uint16_t i = 0; i < count; i += 2) {
        uint16_t temp = data[i] | (data[i + 1] << 8);
        *pBuf++ = temp;
    }
    
    // 3. Set TX count
    pBuf = (uint16_t *)(USB_PMAADDR + 0x02);  // EP0 TX count
    *pBuf = count;
    
    // 4. Set TX status = VALID (sẵn sàng gửi)
    USB->EP0R = (USB->EP0R & USB_EPREG_MASK) | USB_EP_TX_VALID;
    
    // 5. Lưu lại data còn lại (nếu len > 64)
    ep0_tx_ptr = data + count;
    ep0_tx_remaining = len - count;
}
```
 
**Quy trình gửi descriptor lớn (ví dụ: Config Descriptor 67 byte):**
 
```
Transaction 1:
  Host → IN Token
  Device → DATA1 (64 byte: byte 0-63)
  Host → ACK
  
Transaction 2:
  Host → IN Token
  Device → DATA0 (3 byte: byte 64-66) ← Short packet
  Host → ACK
  
Transaction 3 (Status Stage):
  Host → OUT Token
  Host → DATA1 (0 byte)
  Device → ACK
```
 
### 6.2. Nhận dữ liệu (OUT)
 
```c
void USB_EP0_ReceiveData(void)
{
    uint16_t *pBuf;
    uint16_t count;
    
    // 1. Đọc RX count
    pBuf = (uint16_t *)(USB_PMAADDR + 0x06);  // EP0 RX count
    count = *pBuf & 0x3FF;  // 10 bit lower
    
    // 2. Copy data từ RX buffer
    pBuf = (uint16_t *)(USB_PMAADDR + EP0_RX_ADDR);
    for (uint16_t i = 0; i < count; i += 2) {
        uint16_t temp = *pBuf++;
        ep0_rx_buffer[i] = temp & 0xFF;
        ep0_rx_buffer[i + 1] = (temp >> 8) & 0xFF;
    }
    
    // 3. Process received data
    USB_ProcessOutData(ep0_rx_buffer, count);
    
    // 4. Prepare for next OUT or switch to Status stage
    if (ep0_rx_remaining > 0) {
        // More data expected
        USB->EP0R = (USB->EP0R & USB_EPREG_MASK) | USB_EP_RX_VALID;
    } else {
        // Data stage complete, prepare Status stage
        USB_EP0_StatusIn();  // Send ZLP
    }
}
```
 
## Bước 7: Xử lý Control Transfer - Status Stage
 
### 7.1. Status IN (gửi ZLP)
 
```c
void USB_EP0_StatusIn(void)
{
    uint16_t *pBuf;
    
    // 1. Set TX count = 0 (Zero Length Packet)
    pBuf = (uint16_t *)(USB_PMAADDR + 0x02);  // EP0 TX count
    *pBuf = 0;
    
    // 2. Set TX status = VALID
    USB->EP0R = (USB->EP0R & USB_EPREG_MASK) | USB_EP_TX_VALID;
}
```
 
### 7.2. Status OUT (nhận ZLP)
 
```c
void USB_EP0_StatusOut(void)
{
    // 1. Set RX status = VALID (ready to receive ZLP)
    USB->EP0R = (USB->EP0R & USB_EPREG_MASK) | USB_EP_RX_VALID;
}
```
 
## Bước 8: Xử lý các Request quan trọng
 
### 8.1. GET_DESCRIPTOR Request
 
```c
void USB_GetDescriptor(uint16_t wValue, uint16_t wIndex, uint16_t wLength)
{
    uint8_t descriptorType = wValue >> 8;
    uint8_t descriptorIndex = wValue & 0xFF;
    
    uint8_t *descriptor = NULL;
    uint16_t descriptorLength = 0;
    
    switch (descriptorType) {
        case USB_DESC_TYPE_DEVICE:
            descriptor = DeviceDescriptor;
            descriptorLength = sizeof(DeviceDescriptor);
            break;
            
        case USB_DESC_TYPE_CONFIGURATION:
            descriptor = ConfigDescriptor;
            descriptorLength = sizeof(ConfigDescriptor);
            break;
            
        case USB_DESC_TYPE_STRING:
            descriptor = USB_GetStringDescriptor(descriptorIndex);
            descriptorLength = descriptor[0];  // First byte = length
            break;
            
        default:
            USB_EP0_Stall();
            return;
    }
    
    // Truncate to requested length
    if (descriptorLength > wLength) {
        descriptorLength = wLength;
    }
    
    // Send descriptor
    USB_EP0_SendData(descriptor, descriptorLength);
}
```
 
**Ví dụ Device Descriptor:**
```c
const uint8_t DeviceDescriptor[] = {
    0x12,        // bLength = 18 byte
    0x01,        // bDescriptorType = Device
    0x00, 0x02,  // bcdUSB = 2.00 (USB 2.0)
    0x00,        // bDeviceClass = 0 (defined in interface)
    0x00,        // bDeviceSubClass
    0x00,        // bDeviceProtocol
    0x40,        // bMaxPacketSize0 = 64 byte
    0x83, 0x04,  // idVendor = 0x0483 (STMicroelectronics)
    0x40, 0x57,  // idProduct = 0x5740
    0x00, 0x02,  // bcdDevice = 2.00
    0x01,        // iManufacturer = String index 1
    0x02,        // iProduct = String index 2
    0x03,        // iSerialNumber = String index 3
    0x01         // bNumConfigurations = 1
};
```
 
### 8.2. SET_ADDRESS Request
 
```c
void USB_SetAddress(uint16_t address)
{
    // 1. Send Status IN (ZLP) first
    USB_EP0_StatusIn();
    
    // 2. Wait for Status stage to complete
    // (sẽ được xử lý trong interrupt handler)
    
    // 3. Set address (chỉ set sau khi Status stage xong)
    pending_address = address & 0x7F;
}
 
// Trong interrupt handler, sau khi TX complete:
void USB_EP0_TxComplete(void)
{
    if (pending_address != 0xFF) {
        USB->DADDR = USB_DADDR_EF | pending_address;
        pending_address = 0xFF;
        usb_device_state = USB_STATE_ADDRESS;
    }
}
```
 
**Lưu ý:** Set Address **PHẢI** được thực hiện **SAU** Status Stage, không phải ngay lập tức!
 
### 8.3. SET_CONFIGURATION Request
 
```c
void USB_SetConfiguration(uint16_t config)
{
    if (config == 0) {
        // Deconfigure device
        usb_configuration = 0;
        usb_device_state = USB_STATE_ADDRESS;
        USB_EP0_StatusIn();
    } else if (config == 1) {
        // Configure device
        usb_configuration = 1;
        usb_device_state = USB_STATE_CONFIGURED;
        
        // Initialize other endpoints (EP1, EP2, ...)
        USB_ConfigureEndpoints();
        
        // Send Status IN
        USB_EP0_StatusIn();
    } else {
        // Invalid configuration
        USB_EP0_Stall();
    }
}
```
 
## Bước 9: Cấu hình các Endpoint khác (Non-Control)
 
### 9.1. Interrupt IN Endpoint (ví dụ: HID)
 
```c
#define EP1_IN_ADDR  0xC0  // Offset trong packet buffer
#define EP1_IN_SIZE  8     // 8 byte cho HID report
 
void USB_EP1_Init(void)
{
    uint16_t *pBuf;
    
    // 1. Cấu hình Buffer Descriptor Table cho EP1
    pBuf = (uint16_t *)(USB_PMAADDR + 0x08);  // EP1 TX descriptor
    pBuf[0] = EP1_IN_ADDR;  // TX buffer address
    pBuf[1] = 0;            // TX count
    
    // 2. Cấu hình EPnR register cho EP1
    USB->EP1R = USB_EP_INTERRUPT |  // Endpoint Type = Interrupt
                0x0001 |            // Endpoint Address = 1
                USB_EP_TX_NAK;      // TX Status = NAK (no data yet)
}
```
 
### 9.2. Gửi dữ liệu qua Interrupt IN
 
```c
void USB_EP1_SendReport(uint8_t *data, uint16_t len)
{
    uint16_t *pBuf;
    
    // 1. Copy data vào TX buffer
    pBuf = (uint16_t *)(USB_PMAADDR + EP1_IN_ADDR);
    for (uint16_t i = 0; i < len; i += 2) {
        uint16_t temp = data[i] | (data[i + 1] << 8);
        *pBuf++ = temp;
    }
    
    // 2. Set TX count
    pBuf = (uint16_t *)(USB_PMAADDR + 0x0A);  // EP1 TX count
    *pBuf = len;
    
    // 3. Set TX status = VALID
    USB->EP1R = (USB->EP1R & USB_EPREG_MASK) | USB_EP_TX_VALID;
}
 
// Ví dụ: Gửi mouse report
void USB_SendMouseReport(int8_t x, int8_t y, uint8_t buttons)
{
    uint8_t report[3] = {buttons, x, y};
    USB_EP1_SendReport(report, 3);
}
```
 
### 9.3. Bulk OUT Endpoint (ví dụ: CDC Data)
 
```c
#define EP2_OUT_ADDR 0x100
#define EP2_OUT_SIZE 64
 
void USB_EP2_Init(void)
{
    uint16_t *pBuf;
    
    // 1. Cấu hình Buffer Descriptor
    pBuf = (uint16_t *)(USB_PMAADDR + 0x14);  // EP2 RX descriptor
    pBuf[0] = EP2_OUT_ADDR;  // RX buffer address
    pBuf[1] = 0x8000 | ((EP2_OUT_SIZE / 32 - 1) << 10);  // RX count
    
    // 2. Cấu hình EPnR register
    USB->EP2R = USB_EP_BULK |       // Endpoint Type = Bulk
                0x0002 |            // Endpoint Address = 2
                USB_EP_RX_VALID;    // RX Status = VALID
}
 
void USB_EP2_ReceiveHandler(void)
{
    uint16_t *pBuf;
    uint16_t count;
    
    // 1. Đọc count
    pBuf = (uint16_t *)(USB_PMAADDR + 0x16);
    count = *pBuf & 0x3FF;
    
    // 2. Copy data
    pBuf = (uint16_t *)(USB_PMAADDR + EP2_OUT_ADDR);
    for (uint16_t i = 0; i < count; i += 2) {
        uint16_t temp = *pBuf++;
        rx_buffer[i] = temp & 0xFF;
        rx_buffer[i + 1] = (temp >> 8) & 0xFF;
    }
    
    // 3. Process data
    ProcessReceivedData(rx_buffer, count);
    
    // 4. Re-enable RX
    USB->EP2R = (USB->EP2R & USB_EPREG_MASK) | USB_EP_RX_VALID;
}
```
 
## Bước 10: Interrupt Handler chính
 
```c
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    uint16_t istr = USB->ISTR;
    
    // 1. Correct Transfer interrupt
    if (istr & USB_ISTR_CTR) {
        uint8_t ep_id = istr & USB_ISTR_EP_ID;
        
        if (ep_id == 0) {
            // Endpoint 0 (Control)
            uint16_t ep0r = USB->EP0R;
            
            if (ep0r & USB_EP_CTR_RX) {
                if (ep0r & USB_EP_SETUP) {
                    // Setup packet received
                    USB_EP0_Setup_Handler();
                } else {
                    // OUT data received
                    USB_EP0_Out_Handler();
                }
            }
            
            if (ep0r & USB_EP_CTR_TX) {
                // IN data sent
                USB_EP0_In_Handler();
            }
        } else {
            // Other endpoints
            USB_EPn_Handler(ep_id);
        }
    }
    
    // 2. USB Reset
    if (istr & USB_ISTR_RESET) {
        USB_Reset_Handler();
        USB->ISTR = ~USB_ISTR_RESET;
    }
    
    // 3. Suspend
    if (istr & USB_ISTR_SUSP) {
        USB_Suspend_Handler();
        USB->ISTR = ~USB_ISTR_SUSP;
    }
    
    // 4. Wakeup
    if (istr & USB_ISTR_WKUP) {
        USB_Wakeup_Handler();
        USB->ISTR = ~USB_ISTR_WKUP;
    }
    
    // 5. Start of Frame
    if (istr & USB_ISTR_SOF) {
        USB_SOF_Handler();
        USB->ISTR = ~USB_ISTR_SOF;
    }
}
```
 
## Bước 11: Ví dụ hoàn chỉnh - USB HID Mouse
 
### 11.1. Descriptors
 
```c
// Device Descriptor
const uint8_t DeviceDescriptor[] = {
    0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
    0x83, 0x04, 0x20, 0x57, 0x00, 0x02, 0x01, 0x02,
    0x03, 0x01
};
 
// Configuration Descriptor + Interface + HID + Endpoint
const uint8_t ConfigDescriptor[] = {
    // Configuration Descriptor (9 byte)
    0x09, 0x02, 0x22, 0x00, 0x01, 0x01, 0x00, 0xA0, 0x32,
    
    // Interface Descriptor (9 byte)
    0x09, 0x04, 0x00, 0x00, 0x01, 0x03, 0x01, 0x02, 0x00,
    
    // HID Descriptor (9 byte)
    0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x34, 0x00,
    
    // Endpoint Descriptor (7 byte)
    0x07, 0x05, 0x81, 0x03, 0x04, 0x00, 0x0A
};
 
// HID Report Descriptor
const uint8_t HID_ReportDescriptor[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x02,  // Usage (Mouse)
    0xA1, 0x01,  // Collection (Application)
    0x09, 0x01,  //   Usage (Pointer)
    0xA1, 0x00,  //   Collection (Physical)
    
    // Buttons
    0x05, 0x09,  //     Usage Page (Buttons)
    0x19, 0x01,  //     Usage Minimum (1)
    0x29, 0x03,  //     Usage Maximum (3)
    0x15, 0x00,  //     Logical Minimum (0)
    0x25, 0x01,  //     Logical Maximum (1)
    0x95, 0x03,  //     Report Count (3)
    0x75, 0x01,  //     Report Size (1)
    0x81, 0x02,  //     Input (Data, Variable, Absolute)
    
    // Padding
    0x95, 0x01,  //     Report Count (1)
    0x75, 0x05,  //     Report Size (5)
    0x81, 0x01,  //     Input (Constant)
    
    // X, Y
    0x05, 0x01,  //     Usage Page (Generic Desktop)
    0x09, 0x30,  //     Usage (X)
    0x09, 0x31,  //     Usage (Y)
    0x15, 0x81,  //     Logical Minimum (-127)
    0x25, 0x7F,  //     Logical Maximum (127)
    0x75, 0x08,  //     Report Size (8)
    0x95, 0x02,  //     Report Count (2)
    0x81, 0x06,  //     Input (Data, Variable, Relative)
    
    0xC0,        //   End Collection
    0xC0         // End Collection
};
```
 
### 11.2. Main Loop
 
```c
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    USB_Device_Init();
    
    int8_t x = 0, y = 0;
    uint8_t buttons = 0;
    
    while (1) {
        if (usb_device_state == USB_STATE_CONFIGURED) {
            // Read input (ví dụ: từ sensor, button)
            x = ReadMouseX();
            y = ReadMouseY();
            buttons = ReadButtons();
            
            // Send report every 10ms
            USB_SendMouseReport(x, y, buttons);
            HAL_Delay(10);
        }
    }
}
```

## Bước 12: Debug và Tools
 
### 12.1. USB Analyzer Software
 
- **Wireshark**: Bắt USB traffic (cần USBPcap trên Windows)
- **Bus Hound**: Phân tích USB transactions
- **USBlyzer**: Windows-only, rất chi tiết
 
### 12.2. Common Errors
 
| Lỗi | Nguyên nhân | Giải pháp |
|-----|-------------|-----------|
| Device not recognized | Pull-up sai, clock sai | Kiểm tra 1.5kΩ trên D+, verify 48MHz |
| Enumeration failed | Descriptor lỗi | Check descriptor format |
| Data corruption | Buffer overwrite | Kiểm tra buffer size, PMA layout |
| Endpoint STALL | NAK timeout | Handle NAK/STALL correctly |
| Reset loop | Firmware crash trong ISR | Add watchdog, fix ISR |
 
### 12.3. Printf Debug qua SWO
 
```c
// Cấu hình SWO trong SystemClock_Config
void SWO_Init(void)
{
    uint32_t SWOSpeed = 2000000;  // 2MHz
    uint32_t SWOPrescaler = (SystemCoreClock / SWOSpeed) - 1;
    
    DBGMCU->CR |= DBGMCU_CR_TRACE_IOEN;
    *((volatile unsigned *)(ITM_BASE + 0x400)) = 0x00000001;
    *((volatile unsigned *)(ITM_BASE + 0x000)) = 0x00000001;
    
    TPI->ACPR = SWOPrescaler;
    TPI->SPPR = 0x00000002;
    TPI->FFCR = 0x00000100;
}
 
// Printf via SWO
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++) {
        ITM_SendChar(*ptr++);
    }
    return len;
}
```
 
## Tóm tắt quy trình triển khai
 
```
1. Hardware Setup
   ├── Kết nối PA11 (D-), PA12 (D+)
   ├── Pull-up 1.5kΩ trên D+
   └── Clock 48MHz chính xác
 
2. Software Init
   ├── RCC: Enable USB clock
   ├── USB: Reset peripheral
   ├── NVIC: Enable interrupt
   └── EP0: Configure control endpoint
 
3. Handle Reset Event
   ├── Set address = 0
   ├── Init EP0
   └── Enter DEFAULT state
 
4. Handle Setup Packets
   ├── Parse bmRequestType, bRequest
   ├── GET_DESCRIPTOR → Send descriptor
   ├── SET_ADDRESS → Set pending address
   └── SET_CONFIGURATION → Configure endpoints
 
5. Data Stage
   ├── IN: Copy to TX buffer, set VALID
   ├── OUT: Read from RX buffer
   └── Handle multi-packet transfers
 
6. Status Stage
   ├── Send/Receive ZLP
   └── Complete request
 
7. Other Endpoints
   ├── Configure endpoint type
   ├── Allocate buffers
   └── Handle TX/RX in ISR
 
8. Test & Debug
   ├── USB analyzer
   ├── Printf via SWO
   └── Verify with host tools
```