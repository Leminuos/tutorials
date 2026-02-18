## Memory mapped

Thanh ghi Non – Memory Mapped, là những thanh ghi nằm trong core và không có bất kỳ địa chỉ nào để truy cập chúng từ chương trình C => Muốn truy cập phải dùng tập lệnh Assembly.

Thanh ghi Memory Mapped – Thanh ghi ánh xạ bộ nhớ: là một phần của memory và là những thanh ghi có địa chỉ. Bằng cách sử dụng địa chỉ này ta có thể đọc và ghi dữ liệu của thanh ghi tương ứng bằng chương trình C.
- Thanh ghi liên quan đến ngoại vi bộ xử lý: NVIC, MPU, SCB, DEBUG …
- Thanh ghi liên quan đến ngoại vi của vi điều khiển: GPIO, RTC, I2C, TIMER, CAN, …

Để truy cập vào Memory Mapped Registers từ chương trình C, chúng ta sử dụng con trỏ để dereferencing địa chỉ của thanh ghi. Có 3 cách phổ biến:
- Truy cập trực tiếp qua con trỏ
  ```c
  // Đọc giá trị từ thanh ghi GPIOA_IDR
  #define GPIOA_IDR_ADDR  0x40020010

  uint32_t *pGPIOA_IDR = (uint32_t*)GPIOA_IDR_ADDR;
  uint32_t value = *pGPIOA_IDR;

  // Ghi giá trị vào thanh ghi GPIOA_ODR
  #define GPIOA_ODR_ADDR  0x40020014
  uint32_t *pGPIOA_ODR = (uint32_t*)GPIOA_ODR_ADDR;
  *pGPIOA_ODR = 0x0001;
  ```
  
- Sử dụng Structure để nhóm các thanh ghi
  ```c
  typedef struct {
      volatile uint32_t MODER;    // Mode register
      volatile uint32_t OTYPER;   // Output type register
      volatile uint32_t OSPEEDR;  // Output speed register
      volatile uint32_t PUPDR;    // Pull-up/pull-down register
      volatile uint32_t IDR;      // Input data register
      volatile uint32_t ODR;      // Output data register
      volatile uint32_t BSRR;     // Bit set/reset register
      volatile uint32_t LCKR;     // Configuration lock register
      volatile uint32_t AFR[2];   // Alternate function registers
  } GPIO_TypeDef;
  
  #define GPIOA_BASE  0x40020000
  #define GPIOA  ((GPIO_TypeDef*)GPIOA_BASE)

  GPIOA->ODR = 0x0001;           // Ghi vào Output Data Register
  uint32_t input = GPIOA->IDR;   // Đọc từ Input Data Register
  ```

- Sử dụng Bit-banding: bit-banding cho phép truy cập từng bit riêng lẻ như thể nó là một byte trong bộ nhớ:

  ```c
  // Công thức tính địa chỉ bit-band
  // bit_band_addr = bit_band_base + (byte_offset × 32) + (bit_number × 4)
  
  #define BITBAND_PERI_REF   0x40000000
  #define BITBAND_PERI_BASE  0x42000000
  
  #define BITBAND_PERI(addr, bit) \
      ((BITBAND_PERI_BASE + ((addr) - BITBAND_PERI_REF) * 32 + (bit) * 4))
  
  // Ví dụ: Set bit 5 của GPIOA_ODR
  #define GPIOA_ODR  0x40020014
  volatile uint32_t *bit5 = (uint32_t*)BITBAND_PERI(GPIOA_ODR, 5);
  *bit5 = 1;  // Set bit 5
  *bit5 = 0;  // Clear bit 5
  ```

:::warning Lưu ý
Luôn sử dụng từ khóa `volatile` khi khai báo con trỏ trỏ đến thanh ghi phần cứng. Điều này đảm bảo compiler không tối ưu hóa bỏ qua các lệnh đọc/ghi, vì giá trị thanh ghi có thể thay đổi bởi phần cứng.
:::

## Memory map

Memory map giải thích ánh xạ của các thanh ghi ngoại vi và bộ nhớ khác nhau trong phạm vi vùng nhớ có thể định địa chỉ của bộ xử lý. Trong bộ xử lý, vùng nhớ có thể định địa chỉ phụ thuộc vào kích thước của bus địa chỉ.

2 loại bộ nhớ:
- Bộ nhớ chương trình (Flash): là nơi lưu trữ tạm thời các lệnh của chương trình
- Bộ nhớ dữ liệu (RAM): là nơi lưu trữ dữ liệu tạm thời của chương trình
Địa chỉ bắt đầu của memory thường là bộ nhớ Flash.

## Bộ nhớ flash

Bộ nhớ flash được chia thành các sector để thao tác với bộ nhớ hiệu quả. Bộ nhớ flash, ngoài lưu trữ MSP, Vector Table, bộ nhớ flash sẽ lưu trữ vùng nhớ chương trình ứng dụng, cùng với đó là vùng data (bao gồm read-only data, và vùng nhớ `.data`).

Bộ nhớ flash có thể được thao tác ghi trên từng Word (4 bytes), nhưng lại chỉ có thể xóa theo từng Sector. Vì vậy để tiết kiệm số lần ghi/xóa (ảnh hưởng tới tuổi thọ của flash) cũng như để dễ quản lý, bộ nhớ flash được chia nhỏ thành các sector, với những sector đầu có kích thước nhỏ, và những sector sau có kích thước lớn dần.

