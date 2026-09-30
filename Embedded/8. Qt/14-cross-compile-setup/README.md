## Vì sao cần cross-compile

BBB có CPU một nhân 1 GHz và 512 MB RAM. Build một ví dụ nhỏ trên board còn chấp nhận được, nhưng build cả dự án mỗi lần sửa một dòng sẽ rất chậm, và build Qt trên board thì gần như không khả thi. Cách làm chuẩn là **cross-compile**: biên dịch trên máy tính (x86-64) ra file chạy cho board (ARM).

## Các khái niệm

| Khái niệm | Ý nghĩa |
|---|---|
| Host | Máy chạy trình biên dịch: máy tính Ubuntu |
| Target | Máy chạy chương trình đã biên dịch: BBB |
| Toolchain | Bộ trình biên dịch chạy trên host, sinh mã cho target, ví dụ `arm-linux-gnueabihf-g++` |
| Sysroot | Bản sao thư mục `/usr/include`, `/usr/lib`, `/lib` của target, đặt trên host. Trình biên dịch lấy header và thư viện của board từ đây |
| Công cụ host của Qt | `moc`, `uic`, `rcc` phải là bản chạy trên **host**, dù thư viện Qt là bản cho target |

```
 Máy tính (host, x86-64)                               BBB (target, ARMv7)
+----------------------------------------------+    +----------------------+
| main.cpp -> moc/uic (bản x86) -+             |    |                      |
|                                v             |    | /usr/lib/libQt5*.so  |
|        arm-linux-gnueabihf-g++ --sysroot ----+--> | ./ung-dung (ARM)     |
|                                ^             |scp |                      |
| sysroot: header + thư viện ARM +             |    |                      |
|          (chép từ board hoặc do Yocto tạo)   |    |                      |
+----------------------------------------------+    +----------------------+
```

Tên toolchain có dạng `<kiến trúc>-<hệ>-<ABI>`. `arm-linux-gnueabihf` nghĩa là ARM, Linux, ABI EABI với **hard-float** (số thực truyền qua thanh ghi của FPU). Các image Linux hiện nay cho BBB đều dùng hard-float.

## Những gì phải khớp giữa host và board

| Thành phần | Yêu cầu |
|---|---|
| Kiến trúc và ABI | ARMv7, hard-float |
| glibc | Phiên bản glibc lúc liên kết không được mới hơn glibc trên board |
| libstdc++ | Trình biên dịch không được mới hơn nhiều so với libstdc++ trên board, hoặc phải liên kết tĩnh libstdc++ |
| Qt | Qt dùng lúc build và Qt trên board cùng dòng 5.15 |
| Công cụ `moc`, `uic`, `rcc` | Cùng phiên bản với thư viện Qt đích, bản chạy trên host |

Hai dòng đầu là nguyên nhân phổ biến nhất của lỗi "chạy trên board báo thiếu phiên bản thư viện" (xem phần Lỗi thường gặp).

## Ba cách chuẩn bị môi trường

| Cách | Cách làm | Ưu điểm | Nhược điểm |
|---|---|---|---|
| 1. Thủ công | Image Debian cho BBB + toolchain của Ubuntu + sysroot chép từ board + tự build Qt | Hiểu rõ từng bước; dùng lại image có sẵn | Nhiều bước tay, dễ lệch phiên bản glibc/libstdc++ |
| 2. Buildroot | Một công cụ build toolchain, kernel, rootfs và Qt từ mã nguồn theo một file cấu hình | Nhanh để làm quen; image nhỏ; tự sinh SDK | Ít gói hơn Debian; thay đổi lớn thường phải build lại toàn bộ |
| 3. Yocto | Build toàn bộ bản phân phối theo các layer; Qt 5 qua layer `meta-qt5` | Dùng rộng rãi trong sản phẩm; tái lập được; sinh SDK trọn gói | Học lâu; lần build đầu mất nhiều giờ và nhiều dung lượng đĩa |

Với sản phẩm, nên dùng Buildroot hoặc Yocto: toolchain, sysroot và Qt được sinh ra từ cùng một cấu hình nên luôn khớp nhau. Cách thủ công hợp cho việc học và thử nghiệm.

:::warning Toolchain và sysroot phải cùng nguồn gốc
Lỗi khó chịu nhất khi cross-compile là chương trình build thành công nhưng lên board báo thiếu `GLIBC_...` hoặc `GLIBCXX_...`. Nguyên nhân là toolchain hoặc sysroot mới hơn hệ thống trên board. Khi board được cập nhật gói (`apt upgrade`) hoặc nạp image mới, cần đồng bộ lại sysroot hoặc sinh lại SDK.
:::

## Cách 1: làm thủ công với image Debian

Các bước dưới đây mô tả quy trình; tên gói và đường dẫn phụ thuộc phiên bản Debian trên board và phiên bản Ubuntu trên máy tính, cần đối chiếu khi làm.

**Bước 1. Cài toolchain trên Ubuntu**

```bash
sudo apt install g++-arm-linux-gnueabihf
arm-linux-gnueabihf-g++ --version
```

So phiên bản GCC này với GCC trên board (`gcc --version` trên board, hoặc `ls /usr/lib/gcc/arm-linux-gnueabihf/`). Toolchain không nên mới hơn board.

**Bước 2. Tạo sysroot**

Trên board, cài các gói phát triển mà Qt cần khi build, ví dụ header của udev, evdev, font:

```bash
sudo apt install libudev-dev libevdev-dev libfontconfig1-dev libfreetype-dev
```

Trên máy tính, chép các thư mục cần thiết về (giả sử board có địa chỉ `192.168.7.2`, user `debian`):

```bash
mkdir -p ~/bbb/sysroot/usr
rsync -avz debian@192.168.7.2:/lib ~/bbb/sysroot/
rsync -avz debian@192.168.7.2:/usr/include ~/bbb/sysroot/usr/
rsync -avz debian@192.168.7.2:/usr/lib ~/bbb/sysroot/usr/
```

Nhiều symlink trong `/usr/lib` trỏ theo đường dẫn tuyệt đối (ví dụ `/lib/arm-linux-gnueabihf/libz.so.1`). Trên máy tính, đường dẫn đó trỏ vào thư mục của Ubuntu chứ không phải sysroot. Đổi chúng thành đường dẫn tương đối bằng công cụ `symlinks` (gói `symlinks` của Ubuntu):

```bash
symlinks -rc ~/bbb/sysroot
```

**Bước 3. Build Qt 5.15 tối thiểu**

Tải mã nguồn `qtbase-everywhere-src-5.15.x` từ download.qt.io. Chỉ build những gì HMI cần: Widgets, linuxfb, không OpenGL, không X11:

```bash
./configure -release -opensource -confirm-license \
    -device linux-beagleboard-g++ \
    -device-option CROSS_COMPILE=arm-linux-gnueabihf- \
    -sysroot ~/bbb/sysroot \
    -prefix /usr/local/qt5 \
    -extprefix ~/bbb/qt5 \
    -hostprefix ~/bbb/qt5-host \
    -no-opengl -no-xcb -linuxfb -qpa linuxfb \
    -nomake examples -nomake tests
make -j$(nproc)
make install
```

| Tham số | Ý nghĩa |
|---|---|
| `-device linux-beagleboard-g++` | Cấu hình có sẵn trong Qt cho BeagleBoard: cờ CPU `-march=armv7-a -mtune=cortex-a8 -mfpu=neon`, hard-float |
| `-device-option CROSS_COMPILE=...` | Tiền tố của toolchain |
| `-sysroot` | Sysroot đã chép từ board |
| `-prefix /usr/local/qt5` | Nơi Qt sẽ nằm **trên board** |
| `-extprefix ~/bbb/qt5` | Nơi `make install` đặt thư viện cho board **trên máy tính**; thư mục này sẽ được chép lên board |
| `-hostprefix ~/bbb/qt5-host` | Nơi đặt `qmake`, `moc`, `uic`, `rcc` bản chạy trên máy tính |
| `-no-opengl -no-xcb -linuxfb` | Bỏ OpenGL và X11, bật plugin linuxfb |
| `-qpa linuxfb` | Plugin mặc định khi không truyền `-platform`. Cấu hình `linux-beagleboard-g++` mặc định dùng `eglfs` |

Danh sách tùy chọn đầy đủ xem bằng `./configure -help`. Các module khác (Serial Port, Serial Bus) build sau qtbase bằng `qmake` vừa tạo:

```bash
cd qtserialport-everywhere-src-5.15.x
~/bbb/qt5-host/bin/qmake && make -j$(nproc) && make install
```

Cuối cùng chép Qt lên board, đúng đường dẫn `-prefix`:

```bash
rsync -avz ~/bbb/qt5/ debian@192.168.7.2:/usr/local/qt5/
```

Nếu board đã có Qt 5.15 từ gói Debian (`libqt5widgets5`...), có thể không cần build Qt; khi đó cần `moc`, `uic`, `rcc` bản host cùng phiên bản 5.15. Bài này đi theo hướng tự build để kiểm soát module và plugin.

## Cách 2: Buildroot

Buildroot cấu hình bằng `make menuconfig`. Những tùy chọn liên quan (tên theo các bản Buildroot có hỗ trợ Qt 5; bản mới có thể chỉ còn Qt 6, kiểm tra trong menuconfig):

| Tùy chọn | Tác dụng |
|---|---|
| `BR2_PACKAGE_QT5` | Bật Qt 5 |
| `BR2_PACKAGE_QT5BASE_WIDGETS` | Qt Widgets |
| `BR2_PACKAGE_QT5BASE_LINUXFB` | Plugin linuxfb |
| `BR2_PACKAGE_QT5SERIALPORT`, `BR2_PACKAGE_QT5SERIALBUS` | Serial Port, Serial Bus |

`make` sinh ra image; `make sdk` đóng gói toolchain, sysroot và công cụ Qt thành một SDK dùng được trên máy khác.

## Cách 3: Yocto

Yocto dùng layer `meta-qt5` cho Qt 5. Thêm các gói cần vào image (ví dụ `qtbase`, `qtserialport`, `qtserialbus`), bật linuxfb qua `PACKAGECONFIG` của `qtbase`, rồi sinh SDK:

```bash
bitbake <tên-image> -c populate_sdk
```

Kết quả là một file cài đặt `.sh`. Sau khi cài, mỗi phiên làm việc chỉ cần nạp script môi trường rồi dùng CMake như bình thường:

```bash
source <thư-mục-sdk>/environment-setup-cortexa8hf-neon-poky-linux-gnueabi
cmake -S . -B build-bbb
cmake --build build-bbb
```

Lệnh `cmake` của SDK tự áp file toolchain của Yocto. Toàn bộ ví dụ của chuỗi bài đã được cross-compile theo cách này với một SDK Yocto dùng Qt 5.15.7, không có lỗi hay cảnh báo nào.
