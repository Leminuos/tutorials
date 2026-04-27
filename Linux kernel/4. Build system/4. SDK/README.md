# SDK/Toolchain

## Toolchain là gì?

Khi build firmware/image cho một board nhúng (ví dụ BBB), máy tính của ta thường là x86_64. Ta không thể dùng gcc thông thường để compile code chạy trên ARM — ta cần một cross-compiler, ví dụ:

```
arm-poky-linux-gnueabi-gcc
```

Đây chính là toolchain — bộ công cụ compile code trên máy host (x86) nhưng sinh ra binary chạy trên target (ARM).

Yocto build ra 2 loại SDK:

**Standard SDK (`populate_sdk`)**

```bash
bitbake core-image-minimal -c populate_sdk
```

Bao gồm:
- Cross-compiler (gcc, g++, ld, ...)
- Cross-compiled libraries (libc, libstdc++, ...)
- Headers của target
- Một script cài đặt `.sh` tự động setup môi trường

Dùng để: developer viết application cho target, compile bên ngoài Yocto.

**Extensible SDK — eSDK (`populate_sdk_ext`)**

```bash
bitbake core-image-minimal -c populate_sdk_ext
```

Ngoài những thứ của Standard SDK, còn có thêm:
- `devtool` — cho phép modify recipe, build, deploy trực tiếp lên board
- Một bản rút gọn của BitBake bên trong SDK

Dùng để: developer muốn làm việc gần với workflow Yocto hơn, iterate nhanh.

## Tại sao cần SDK?

**Góc nhìn 1 — Tách biệt workflow**

Build một image Yocto đầy đủ mất hàng giờ. Không thể yêu cầu mỗi developer phải setup toàn bộ Yocto chỉ để compile một ứng dụng. SDK cho phép:

```
BSP team  ->  build image + SDK     ->  phân phối SDK
App team  ->  nhận SDK, compile app ->  không cần biết Yocto
```

**Góc nhìn 2 — Đảm bảo tính nhất quán**

SDK được build từ chính image đó, nên:
- Phiên bản thư viện trên SDK khớp với image trên board
- Không bị lỗi "compile OK nhưng chạy trên board bị lỗi" do mismatch library

## Cách dùng SDK

```bash
# Cài SDK (chạy script installer)
./tmp/deploy/sdk/poky-glibc-x86_64-core-image-minimal-cortexa8hf-neon-beaglebone-yocto-toolchain-4.0.sh

# Source environment
source /opt/poky/4.0/environment-setup-cortexa8hf-neon-poky-linux-gnueabi

# Lúc này biến môi trường đã được set
echo $CC
# -> arm-poky-linux-gnueabi-gcc --sysroot=...

# Compile ứng dụng bình thường
$CC hello.c -o hello
```

## Sysroot là gì?

**Vấn đề cần giải quyết**

Khi compiler build một chương trình, nó cần tìm:
- Headers (`.h`) — để biết API, struct, kiểu dữ liệu
- Libraries (`.so`, `.a`) — để link

Trên máy x86 bình thường, mọi thứ nằm ở `/usr/include`, `/usr/lib` — không có vấn đề gì.

Nhưng khi cross-compile cho ARM, nếu compiler đi tìm ở `/usr/include`, `/usr/lib` trên máy host thì sẽ lấy headers/libs của x86, điều này là sai hoàn toàn!

**Vậy thì sysroot là gì?**

Sysroot là một thư mục đóng vai trò như root filesystem giả (`/`) cho target, đặt trên máy host.

```
/opt/poky/4.0/sysroots/cortexa8hf-neon-poky-linux-gnueabi/
├── usr/
│   ├── include/        ← headers của target (ARM)
│   └── lib/            ← libraries của target (ARM)
├── lib/                ← libc, ld-linux của target
└── ...
```

Compiler được chỉ định tìm headers/libs tại đây thay vì `/usr` của host.

Một bộ SDK sẽ có 2 sysroot:

```
sysroots/
├── x86_64-pokysdk-linux/                   ← host sysroot
│   └── usr/bin/                            (chứa cross-compiler tool, cmake, pkg-config... chạy trên x86 host)
└── cortexa8hf-neon-poky-linux-gnueabi/     ← target sysroot
    └── usr/include, usr/lib/               (headers + libs của ARM target)
```

**Cách hoạt động**

Compiler được gọi với `--sysroot`

```bash
arm-poky-linux-gnueabi-gcc --sysroot=/opt/poky/4.0/sysroots/cortexa8hf-neon-poky-linux-gnueabi myapp.c -o myapp
```

Lúc này compiler sẽ:
- Tìm headers tại `<sysroot>/usr/include` thay vì `/usr/include`
- Tìm libs tại `<sysroot>/usr/lib` thay vì `/usr/lib`
- Output binary chạy được trên ARM

Khi ta thiết lập môi trường bằng lệnh `source environment-setup-...`, biến `$CC` đã được set sẵn `--sysroot`:

```bash
echo $CC
# arm-poky-linux-gnueabi-gcc -march=armv8-a --sysroot=/opt/poky/4.0/sysroots/cortexa8hf-neon-poky-linux-gnueabi
```

Nên ta chỉ cần gọi `$CC myapp.c -o myapp` là đủ.