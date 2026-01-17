# Command Arm Toolchain

## Mở đầu

**Khác nhau giữa project C và C nhúng**

**Compiler:** Project C thông thường dùng compiler là GCC, trong khi với C cho vi điều khiển, thì cần cross compiler. Chẳng hạn, với vi điều khiển STM32 thì ta cần dùng GNU Toolchain, chính là một GCC riêng cho dòng chip ARM.

**Linker:** Đối với một chương trình C trên máy tính, ta không cần quan tâm đến linker, vì việc allocate máy tính đã làm, nhưng với vi điều khiển, vấn đề này rất quan trọng. Cần phải viết một file linker (`.ld`) cho quá trình linking.

**Compiler và linker trong Makefile**

Ta cần phải trỏ đến file linker và folder của cross compiler.

```bash
LDSCRIPT     := LinkerScript.ld
COMPILER_DIR := C:/GNU_Toolchain/bin
```

Tiếp theo là compiler và linker flags:

```bash
CC  = $(COMPILER_DIR)/arm-none-eabi-gcc
```

Để build cho vi điều khiển sẽ cần thêm compiler options và linker options.

## Compiler options

- `-mcpu=cortex-m4`: Chọn kiến trúc CPU đích là ARM Cortex-M4.
  - Lệnh này yêu cầu compiler sinh mã máy cho Cortex-M4.
  - Có ảnh hưởng đến tập lệnh và tối ưu hóa.
- `-mthumb`: Bật chế độ Thumb instruction set (tập lệnh 16-bit của ARM).
- `-fdata-sections` và `-ffunction-sections`: Mỗi biến toàn cục và hàm được đặt vào một section riêng biệt trong output object.
  - Bình thường: tất cả hàm/biến đi vào `.text`, `.data` chung.
  - Với các flag này: hàm `foo()` ➜ `.text.foo`, hàm `bar` ➜ `.data.bar`.
  -> Kết hợp với linker flag `--gc-sections` ➜ loại bỏ mã không dùng ➜ giúp giảm kích thước firmware!
- `-MMD, -MP, -MF"$(@:%.o=%.d)`: Dùng để tạo file dependency `.d` tự động.
  | Flag                | Ý nghĩa |
  | ------------------- | ------- |
  | `-MMD`              | Tạo file `.d` chứa dependency, bỏ qua các system headers |
  | `-MP`               | Thêm dummy rules để tránh lỗi nếu file `.h` bị xóa |
  | `-MF"$(@:%.o=%.d)"` | Đặt tên file `.d` dựa trên tên file `.o `|

- `-Ox`: Tối ưu hóa mức độ x khi biên dịch.
- `-I$(INCLUDE_DIR)`: Thêm đường dẫn tìm kiếm file header vào trình biên dịch.
- `-std=gnu11`: Chỉ định chuẩn ngôn ngữ C được sử dụng là GNU C 2011.

Ví dụ:

```bash
ASFLAGS     = -mcpu=cortex-m3 $(AS_DEFS) $(AS_INCLUDES) -O0 -Wall -fdata-sections -ffunction-sections
CCFLAGS		+= -mcpu=cortex-m3 $(C_DEFS) $(C_INCLUDES) -O0 -Wall -fdata-sections -ffunction-sections

# Generate dependency information
CCFLAGS 	+= -MMD -MP -MF"$(@:%.o=%.d)"
```

Sử dụng như sau:

```bash
$(BUILD_DIR)/%.o: %.c
	$(CC) -c $(CCFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) -o $@ $<
```

## Linker options

- `-lc`: Link thư viện C chuẩn (`libc`).
- `-lm`: Link thư viện math, cần khi dùng `sin()`, `sqrt()`, `pow()`,...
- `-lnosys`:  Stub cho các syscall chưa định nghĩa để tránh lỗi link khi không có `syscalls.c.`. `-lnosys` thường dùng khi không có OS, nó sẽ link tới `libnosys.a`, trong đó các syscall như `_write`, `_read`,...là hàm rỗng.
- `-L$(LIBDIR)`: Chỉ định thư mục chứa các thư viện .a, nếu ta cần link các thư viện tự tạo.
- `-T$(LDSCRIPT)`: Chỉ định linker script (`.ld` file) — là file định nghĩa cách sắp xếp vùng nhớ (FLASH, RAM, v.v.)
- `-Map=[path_to_file.map]`: Tạo file .map chứa thông tin symbol.

Ví dụ:

```bash
LIBS 		= -lc -lm -lnosys 
LIBDIR 		= 
LDFLAGS		:= -mcpu=cortex-m3 -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections
```

Sử dụng như sau:

```bash
$(BUILD_DIR)/$(TARGET).elf: $(OBJ_PATHS)
	$(CC) $(LDFLAGS) -o $@ $^
```