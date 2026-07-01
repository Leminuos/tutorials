Khi viết BSP cho một board, trước tiên, ta cần lấy được các thông tin của SoC như sau:
- Schematic của board
- Datasheet/reference manual của SoC
- Loại và dung lượng RAM
- Xác định nguồn, clock, reset
- Xác định boot media: SD, SPI hay eMMC
- UART debug: UART nào, chân TX/RX nào, baudrate

Với Lichee Pi Nano, bảng thông tin ban đầu sẽ gần như sau:

```
SoC          Allwinner F1C100s
CPU          ARM926EJ-S / ARMv5TE
Clock        533MHz
Cache        16KB Data, 32KB Instruction
RAM          32DDR tích hợp trong SoC
Boot media   microSD hoặc SPI flash
```

Serial port UART0:

| Licheepi Nano | Serial |
| --- | --- |
| E0 | RX |
| E1 | TX |

Với các document:
- Datasheet: [F1C100s Datasheet V1.0](https://linux-sunxi.org/images/b/ba/F1C100s_Datasheet_V1.0.pdf)
- Reference manual: giống với [F1C600 User Manual V1.0](https://linux-sunxi.org/images/8/85/Allwinner_F1C600_User_Manual_V1.0.pdf)

Mục tiêu bringup đầu tiên là:

```
Khi cấp nguồn
    -> Boot ROM chạy
    -> SPL khởi tạo DRAM
    -> Uboot in được log qua UART
```

Check xem SoC đã được U-Boot/Linux hỗ trợ ở mainline hay chưa? Nếu chưa có thì ta mới phải tìm xem repositoru của hãng và các bản fork từ cộng đồng

Theo bảng sau từ sunxi communication cho board [Lichee Pi Nano](https://linux-sunxi.org/U-Boot#Status_Matrix):

![Licheepi uboot support](img/licheepi-uboot-support.png)

Thì board được uboot mainline bắt đầu support từ version `v2022.04`. Tuy nhiên, uboot bản này chỉ SPL hỗ trợ MMC, device tree của uboot chưa khai báo controller MMC nên nó chưa chuyển sang kernel được. Do đó, để thực hiện load được kernel thì ta cần sử dụng version `v2022.07`.

Ta có thể thử build uboot để xác nhận điều này:

```bash
git clone --depth 1 --branch v2022.07 --single-branch https://github.com/u-boot/u-boot.git
cd u-boot/

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- licheepi_nano_defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j$(nproc)
```

Khi build thành công, ta sẽ được file binary chứa cả SPL và uboot sau:

```
u-boot-sunxi-with-spl.bin
├── SPL
└── U-Boot
```

Giờ ta sẽ thực hiện nạp file này vào sdcard:
1. Cắm sdcard và xác định sdcard device.

   ```bash
   lsblk
   # => giả sử sdcard là /dev/sdX
   export card=/dev/sdX
   ```

2. Xoá dữ liệu trên sdcard

   ```bash
   dd if=/dev/zero of=${card} bs=1M count=1
   ```

3. Ghi file binary vào offset 8KB:

   ```bash
   sudo dd if=u-boot-sunxi-with-spl.bin \
        of=${card} \
        bs=1024 seek=8 \
        conv=fsync,notrunc \
        status=progress
   ```

4. Đồng bộ và tháo thẻ

   ```bash
   sync
   sudo eject ${card}
   ```

Lưu ý: Cần cẩn thận với `/dev/sdX`, nếu chọn nhầm có thể ghi đè vào ổ cứng hệ thống.

Khi thực hiện cắm thẻ sdcard vào board và boot nó sẽ hiển thị log như sau:

```log
U-Boot SPL 2022.07 (Jun 28 2026 - 20:07:14 +0700)
DRAM: 32 MiB
Trying to boot from MMC1


U-Boot 2022.07 (Jun 28 2026 - 20:07:14 +0700) Allwinner Technology

CPU:   Allwinner F Series (SUNIV)
Model: Lichee Pi Nano
DRAM:  32 MiB
Core:  26 devices, 14 uclasses, devicetree: separate
WDT:   Not starting watchdog@1c20ca0
MMC:   mmc@1c0f000: 0
Loading Environment from FAT... Unable to use mmc 0:0...
In:    serial@1c25000
Out:   serial@1c25000
Err:   serial@1c25000
Net:   No ethernet found.
Hit any key to stop autoboot:  0
switch to partitions #0, OK
mmc0 is current device
** No partition table - mmc 0 **
Couldn't find partition mmc 0:1
No ethernet found.
missing environment variable: pxeuuid
Retrieving file: pxelinux.cfg/00000000
No ethernet found.
Retrieving file: pxelinux.cfg/0000000
No ethernet found.
Retrieving file: pxelinux.cfg/000000
No ethernet found.
Retrieving file: pxelinux.cfg/00000
No ethernet found.
Retrieving file: pxelinux.cfg/0000
No ethernet found.
Retrieving file: pxelinux.cfg/000
No ethernet found.
Retrieving file: pxelinux.cfg/00
No ethernet found.
Retrieving file: pxelinux.cfg/0
No ethernet found.
Retrieving file: pxelinux.cfg/default-arm-sunxi-sunxi
No ethernet found.
Retrieving file: pxelinux.cfg/default-arm-sunxi
No ethernet found.
Retrieving file: pxelinux.cfg/default-arm
No ethernet found.
Retrieving file: pxelinux.cfg/default
No ethernet found.
Config file not found
No ethernet found.
=>
```

Log đến đây thì có nghĩa là uboot đã boot thành công. Lỗi hiện tại là SD card chưa có bảng phân vùng. Ta mới ghi uboot raw vào offset 8 KB, chưa tạo MBR và các phân vùng boot và rootfs.

Tiếp theo thì ta sẽ thực hiện nạp kernel image vào sdcard.

Theo cộng đồng sunxi thì board cũng được linux mainline support, tuỳ vào linux version mà có mức support khác nhau:

| Mức hỗ trợ | Linux | Github commit |
| --- | --- | --- |
| Kernel chạy và in log qua UART | v5.0 | [324f4071a080](https://github.com/torvalds/linux/commit/324f4071a08046676a637521f211a34848f0cc0d) |
| Boot hoàn chỉnh từ microSD, mount rootfs | v5.19 | [30b6259f8bb8](https://github.com/torvalds/linux/commit/30b6259f8bb8f17377d13c61e47b66d71ec3abfe) |
| SPI NOR được mô tả trong board DTS | v5.19 | [37384b81bc25](https://github.com/torvalds/linux/commit/37384b81bc255bca3412536c50598fa50d05c751) |
| USB OTG/gadget mainline | v6.4 | [bedc7c5490fc](https://github.com/torvalds/linux/commit/bedc7c5490fce4e57b55e025b4adfbd31f25623d) |

Một số driver chưa được mainline hỗ trợ như Display Engine, TCON/LCD RGB controller,Camera/DVP...Nếu muốn sử dụng thì ta cần sử dụng các bản fork khác:
- Lichee-Pi U-Boot fork: [Lichee-Pi/u-boot](https://github.com/Lichee-Pi/u-boot)
- Lichee-Pi Linux fork: [Lichee-Pi/linux](https://github.com/Lichee-Pi/linux)
- Buildroot dùng mainline: [goediy/licheepi-nano-mainline](https://github.com/goediy/licheepi-nano-mainline/tree/main)

Do định hướng của ta là build image cho board bằng yocto version scarthgap, mà version này đã có kernel version 6.6 nên ta sẽ sử dụng version này luôn.

**Clone linux stable 6.6**

```bash
git clone --depth 1 single-branch --branch linux-6.6.y https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git linux
cd linux
```

Kiểm tra version

```bash
git describe --tags --always
```

Branch `linux-6.6.y` nhận các bản batch mới nhất của version 6.6.

**Tạo thư mục build riêng**

Không build trực tiếp vào source

```bash
mkdir -p build
```

Dọn cấu hình cũ nếu có:

```bash
make O=build ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- mrproper
```

**Tạo defconfig cho board**

```bash
make O=build ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- multi_v5_defconfig
```

Bật ext4 để mount rootfs và tắt USB:

```bash
./scripts/config --file build/.config \
    --enable EXT4_FS \
    --enable DEVTMPFS \
    --enable DEVTMPFS_MOUNT \
    --enable TMPFS \
    --disable USB \
    --disable USB_GADGET
```

Cập nhật dependency:

```bash
make O=build ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
```

**Build kernel, DTB và module**

```bash
make O=build ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage dtbs modules
```

Kết quả cần thiết:

```
build/arch/arm/boot/zImage
build/arch/arm/boot/dts/allwinner/suniv-f1c100s-licheepi-nano.dtb
```

**Tạo partition cho sdcard**

```bash
sudo parted -s /dev/sdX mklabel msdos
sudo parted -s /dev/sdX unit MiB mkpart primary fat32 1 65
sudo parted -s /dev/sdX set 1 boot on
sudo parted -s /dev/sdX unit MiB mkpart primary ext4 65 100%
sudo partprobe /dev/sdX
```

Kiểm tra:

```bash
lsblk -p /dev/sdX
```

Phải thấy:

```
/dev/sdX
├─/dev/sdX1
└─/dev/sdX2
```

**Tiến hành format**

```bash
sudo mkfs.vfat -F 32 -n BOOT /dev/sdX1
sudo mkfs.ext4 -F -L rootfs /dev/sdX2
```

**Ghi lại uboot**

Thực hiện sau khi partition xong:

```bash
sudo dd if=u-boot-sunxi-with-spl.bin \
    of=/dev/sdX \
    bs=1024 seek=8 \
    conv=fsync,notrunc \
    status=progress

sync
```

**Copy kernel và DTB vào boot partition**

```bash
sudo mkdir -p /media/lichee
sudo mount /dev/sdX1 /media/lichee

sudo cp linux/build/arch/arm/boot/zImage /media/lichee/
sudo cp linux/build/arch/arm/boot/dts/allwinner/suniv-f1c100s-licheepi-nano.dtb /media/lichee/

sync
sudo umount /media/lichee
```

**Boot kernel thủ công trong uboot**

Thực hiện kiểm tra:

```
=> mmc dev 0
=> mmc part
=> fatls mmc 0:1
```

Ta phải thấy có:

```
zImage
suniv-f1c100s-licheepi-nano.dtb
```

Nạp vào RAM:

```
=> fatload mmc 0:1 0x80008000 zImage
=> fatload mmc 0:1 0x80c00000 suniv-f1c100s-licheepi-nano.dtb
```

Đặt kernel command line:

```
=> setenv bootargs console=ttyS0,115200 earlycon root=/dev/mmcblk0p2 rootwait rw
```

Boot:

```
=> bootz 0x80008000 - 0x80c00000
```

Nếu partition 2 chưa có rootfs, kernel vẫn có thể khởi động và in log nhưng cuối cùng sẽ panic:

```
VFS: Unable to mount root fs
Kernel panic
```

**Tạo boot script boot.scr**

Khi đã kiểm tra được rằng kernel đã có thể khởi động thì ta viết file `boot.cmd` như sau:

```
setenv bootargs console=ttyS0,115200 panic=5 earlycon root=/dev/mmcblk0p2 rootwait rw
fatload mmc 0:1 0x80008000 zImage
fatload mmc 0:1 0x80c00000 suniv-f1c100s-licheepi-nano.dtb
bootz 0x80008000 - 0x80c00000
```

Sau đó, ta sẽ thực hiện generate một file boot script:

```bash
mkimage -C none -A arm -T script -d boot.cmd boot.scr
```

**Copy boot script vào boot partition**

```bash
sudo mkdir -p /media/lichee
sudo mount /dev/sdX1 /media/lichee

sudo cp boot.scr /media/lichee/

sync
sudo umount /media/lichee
```

Bring-up bootloader



Bring-up kernel



Sau cùng mới đóng gói thành Yocto BSP:
- Machine configuration.
- U-Boot recipe.
- Kernel recipe.
- Image layout.
- Package và distro policy.