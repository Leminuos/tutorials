# Yocto

## Yocto project

Trước khi có yocto project hoặc build root, ta cần phải build image một cách thủ công: tự tải kernel, rootfs, toolchain và build từng phần. Điều này rất dễ lỗi, khó bảo trì,...

Yocto Project là một framework để xây dựng hệ điều hành Linux tùy biến cho hệ thống nhúng hay Embedded Linux Distro Builder.

Nó không phải là một Linux distro, mà là bộ công cụ giúp tạo ra một bộ image hoàn chỉnh chứa uboot, kernel, rootfs, sdk cross compiler cho thiết bị target.

## Phân biệt machine, distro và image trong Yocto

**Machine**

Machine sẽ mô tả phần cứng để yocto biết:
- Kiến trúc CPU.
- Cần sử dụng kernel và bootloader gì?
- Sử dụng device tree nào, có hỗ trợ GPU không, RAM bao nhiêu,…

::: tip Lúc nào cần sửa machine?
Khi cần custom board: đổi kernel/device tree, cần thêm hoặc tắt feature liên quan đến phần cứng.
:::

**Image**

Image là kết quả cuối cùng, nó đóng gói 3 thành phần: rootfs, kernel và bootloader.

Trong các cấu hình trong file `local.conf` thì các cấu hình liên quan đến image sẽ thêm một số feature hoặc package vào rootfs.

Ví dụ, thêm package vào image:

```conf
IMAGE_INSTALL += " nano htop i2c-tools"
```

**Distro**

Distro là bộ chính sách vận hành OS, nó không gắn với image hoặc hardware cụ thể.

Cách mà OS quyết định xử lý các hành vi, lựa chọn thành phần, cơ chế vận hành, chuẩn bảo mật, init system, package manager…

Một số OS policy như:
- Policy về Init System: Khởi động hệ thống, quản lý service. Ví dụ: systemd, sysvinit,...
- Policy về libc: Thư viện nền tảng. Ví dụ: glibc, musl,...
- Policy về package manager: apt, apk,...
- Policy về security: strict, moderate,...
- Policy về distribution features: quyết định hệ thống có những feature nào.

## Tổng quan cấu trúc thư mục `build`

Thư mục `build` sẽ có cấu trúc như sau:

```
build/
├── cache/
├── conf/
│   ├── bblayers.conf
│   └── local.conf
├── downloads/
├── sstate-cache/
├── tmp/
│   ├── deploy/
│   │   ├── images/
|   │   ├── sdk/
│   │   └── licenses/
│   ├── work/
│   │   └── <arch>/<package>/<version>/
│   ├── sysroots/
│   ├── log/
│   └── pkgdata/
└── bitbake-cookerdaemon.log
```

### User configuration

`build/conf/` là nơi BitBake lấy thông tin build đầu tiên.

| File                | Vai trò                                                                                                              |
| ------------------- | -------------------------------------------------------------------------------------------------------------------- |
| **`local.conf`**    | Cấu hình build cụ thể: chọn `MACHINE`, `DISTRO`, đường dẫn `DL_DIR`, `SSTATE_DIR`, số luồng build, image feature,…   |
| **`bblayers.conf`** | Liệt kê các layer mà BitBake sẽ load (meta, meta-poky, meta-ti, meta-custom,…).                                      |

**Một số cấu hình quan trọng trong file `local.conf`**

::: tip IMAGE_INSTALL

`IMAGE_INSTALL` định nghĩa danh sách các package sẽ được cài đặt vào root filesystem của image khi build.

Ví dụ:

```bash
IMAGE_INSTALL:append = " systemd"
```

Ta cần thêm dấu cách trước khi thêm package vì `IMAGE_INSTALL` đã tồn tại trong một image recipe như core-image-minimal, cho nên `IMAGE_INSTALL` trong file `local.conf` chỉ là mở rộng của nó.
:::

::: tip TOOLCHAIN_TARGET_TASK

`TOOLCHAIN_TARGET_TASK` định nghĩa các package target sẽ được đưa vào SDK Toolchain khi build meta-toolchain hoặc populate-sdk image.

-> Nếu muốn thêm package vào trong sdk ta cần thêm nó vào trong cấu hình này và build lại.
:::

### Thông tin cache của BitBake

`build/cache/` để lưu metadata đã parse.
- Khi BitBake muốn đọc `.bb`, `.bbclass`, `.conf`, nó cache kết quả parse tại đây.
- Lần build sau → không cần parse lại → tăng tốc build.

Nếu thay đổi recipe hoặc class, BitBake sẽ tự invalidate cache và parse lại phần đó.

### Kho lưu source code tải về

`build/downloads/` chứa toàn bộ source code từ git clone, patch, hoặc source mà các recipe yêu cầu.

Mặc định đường dẫn được set trong local.conf:

```bash
DL_DIR ?= "${TOPDIR}/downloads"
```

Có thể chia sẻ thư mục này giữa nhiều build để tiết kiệm thời gian tải. Ví dụ: `busybox-1.31.1.tar.bz2`, `git2_github.com.kernel.git.tar.gz`.

### Kết quả build

Mọi thứ Yocto sinh ra đều nằm trong `build/tmp/`.

**`tmp/deploy/`**

| Thư mục con             | Vai trò                                                                                                                   |
| ----------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| **`images/<machine>/`** | Chứa các file image đã build: kernel, rootfs, u-boot, SD card image (.wic, .tar.bz2, .ext4, .zImage, .dtb, .u-boot.bin…). |
| **`licenses/`**         | Lưu license của từng package (theo quy định GPL).                                                                         |
| **`sdk/`**              | Nếu chạy `bitbake -c populate_sdk`, SDK sẽ xuất hiện tại đây.                                                             |

**`tmp/work/`**

Cấu trúc tổng quát:

```
tmp/work/
 └── <something>/               ← thư mục kiến trúc
      └── <package>/            ← thư mục package hoặc recipe
          └── <version>-r<revision>/
               ├── temp/              ← log từng task (do_compile, do_install…)
               │   └── log.do_<task>  ← log.do_fetch, log.do_unpack, log.do_patch,...
               ├── build/             ← nơi biên dịch mã nguồn
               └── image/             ← chứa file được cài vào rootfs
```

Từ cấu trúc trên, ta có một số thông tin như sau:
- Khi `do_compile`, BitBake thực hiện biên dịch tại folder `build/`.
- khi `do_install`, tất cả file mà recipe muốn cài vào root filesystem sẽ nằm ở folder `image/`.

## Bitbake

Nhóm lệnh BitBake là một công cụ build engine của Yocto. Nó tương tự như makefile, nhưng mạnh hơn rất nhiều. Nó chịu trách nhiệm parse các metadata như recipe, class và config, từ đó xác định dependency và điều phối toàn bộ quá trình build image.

Câu lệnh:

```
bitbake [options] <target>
bitbake <target> [options]
```

Cú pháp:
- <target> là recipe hoặc image mà bạn muốn build, ví dụ:
  + core-image-minimal
  + meta-toolchain
- [options] là các tham số điều khiển hành vi của BitBake.

## Metadata

| Loại file                   | Phần mở rộng                      | Vai trò                                                         |
| --------------------------- | --------------------------------- | ----------------------------------------------------------------|
| **Recipe**                  | `.bb`                             | Định nghĩa cách build một package                               |
| **Append**                  | `.bbappend`                       | Ghi đè hoặc mở rộng recipe đã có từ layer khác                  |
| **Class**                   | `.bbclass`                        | Gom logic build chung cho nhiều recipe                          |
| **Configuration**           | `.conf`                           | Thiết lập biến cấu hình, version, distro, machine, layer path   |
| **Layer configuration**     | `layer.conf`                      | Khai báo phạm vi và ưu tiên của layer                           |
| **Distro & Machine config** | `distro/*.conf`, `machine/*.conf` | Định nghĩa đặc trưng của bản phân phối và phần cứng mục tiêu    |

## Khởi tạo môi trường build

Để có thể sử dụng các nhóm lệnh bitbake, trước tiên, ta cần khởi tạo môi trường build bằng script:

```bash
source oe-init-build-env [build-directory]
```

Trong đó:
- `source` là lệnh nội bộ của shell, nó khởi tạo các biến môi trường ngay trong shell hiện tại, tức là mọi biến môi trường sẽ chỉ có tác dụng trong chính shell đó và không có giá trị đối với các shell khác.
- `[build-directory]`: optional, nếu không truyền thì yocto sẽ mặc định dùng `build/` nằm cùng cấp với `poky`.

Khi chạy, script này sẽ:
- Tạo thư mục `build/` nếu chưa tồn tại
- Generate các file cấu hình mặc định:

  ```
    build/
    ├── conf/
    │    ├── bblayers.conf
    │    └── local.conf
    ├── cache/
    ├── sstate-cache/
    └── tmp/
  ```
- Thiết lập các biến môi trường cần thiết cho BitBake: `BBPATH`, `TOPDIR`, `PATH`, `PYTHONPATH`
- Cuối cùng, trỏ vào thư mục `build/`


## Nhóm lệnh

| Cú pháp                           | Mô tả                                                |
| --------------------------------- | ---------------------------------------------------- |
| `bitbake <target>`                | Build recipe hoặc image                              |
| `bitbake -c <task> <target>`      | Chạy một task cụ thể: compile, install, cleanall,... |
| `bitbake -c clean <target>`       | Xóa kết quả build recipe (trong `tmp/work`)          |
| `bitbake -c listtasks <target>`   | Liệt kê toàn bộ task mà recipe hỗ trợ.               |
| `bitbake -c cleansstate <target>` | Xóa shared-state (cache build).                      |
| `bitbake -e <target>`             | Liệt kê toàn bộ biến môi trường sau khi parse recipe.|
| `bitbake -p <target>`             | Chỉ parse recipe, không build.                       |
| `bitbake -f <target>`             | Force rebuild recipe, bỏ qua check sstate.           |

## Shared State Cache

Shared State Cache là cơ chế:
- Lưu kết quả của từng task vào `sstate-cache/` trong quá trình build
- Nếu rebuild, nó reuse kết quả thay vì biên dịch lại.
- Giúp build nhanh hơn và có thể chia sẻ cache giữa các máy build khác nhau.

Cơ chế hoạt động: Khi một task trong file recipe được build thành công, BitBake tạo ra một gói nén `.tgz` chứa toàn bộ kết quả của task đó và lưu trong thư mục `sstate-cache/`.

File `.tgz` này chứa:
- Binary đã build
- Metadata xác định môi trường build
- Hash signature để BitBake xác nhận tính hợp lệ

Bitbake sẽ dựa vào hash được lưu trong `.tgz` để bỏ qua việc build và restore output từ sstate cache. Ngược lại, BitBake sẽ build lại task, rồi tạo sstate mới thay thế.

## Thao tác source kernel với bitbake

Để thao tác với source kernel, ta cần hiểu `virtual/kernel`. Đây là một virtual recipe, nó là một alias dùng để trỏ tới recipe cụ thể.

Vì yocto nó cho phép các layer khác nhau cung cấp kernel khác nhau, do đó nó yocto sẽ chỉ cần dựa vào `virtual/kernel` để tìm tới đúng recipe phù hợp cho `MACHINE`.

-> Đây cũng là cơ chế Virtual Provider.

Ví dụ:

| Recipe                | Layer               | Vai trò                         |
| --------------------- | ------------------- | ------------------------------- |
| `linux-yocto.bb`      | meta                | Kernel mặc định của Poky        |
| `linux-ti-staging.bb` | meta-ti             | Kernel cho SoC TI (BBB, AM335x) |
| `linux-intel.bb`      | meta-intel          | Kernel cho Intel SoC            |
| `linux-custom.bb`     | layer riêng của bạn | Kernel tự sửa                   |

Để xác định linux provider ta sử dụng lệnh:

```bash
bitbake -e virtual/kernel | grep ^PREFERRED_PROVIDER_virtual/kernel
```

Kết quả:

```bash
PREFERRED_PROVIDER_virtual/kernel="linux-ti"
```

hoặc:

```bash
PREFERRED_PROVIDER_virtual/kernel="linux-yocto"
```

hoặc một provider khác (vd: linux-stable, linux-bb.org, linux-custom… tuỳ distro/BSP).

**Mở kernel menuconfig**

```bash
bitbake -c menuconfig -f virtual/kernel
```

hoặc

```bash
bitbake -c devshell virtual/kernel
make menuconfig
exit #Thoát devshell
```

**Rebuild kernel source**

```bash
bitbake -f -c compile linux-yocto
bitbake -c deploy linux-yocto
bitbake core-image-full-cmdline
```

**Đọc đường dẫn kernel source**

```bash
bitbake -e virtual/kernel | grep ^S=
```

**Đọc Kernel recipe**

```bash
bitbake -e virtual/kernel | grep ^PN=
```

## Recipe

Trong Yocto Project, mỗi recipe là một công thức build, nó mô tả toàn bộ cách build một gói phần mềm cụ thể.

Recipe nói cho BitBake biết:
- Lấy source ở đâu.
- Làm thế nào để compile.
- Cách install vào root filesystem,
- Phụ thuộc vào những gì.
- Và license, version, metadata đi kèm.

Nói cách khác:
- Recipe = công thức build cho 1 package
- Image = tập hợp của nhiều recipe ghép lại thành 1 hệ thống Linux hoàn chỉnh.

### Cấu trúc của một recipe

Cấu trúc cơ bản của một recipe sẽ gồm các thành phần sau:

| Thành phần                             | Vai trò                                                        |
| -------------------------------------- | -------------------------------------------------------------- |
| **Biến (`VARIABLES`)**                 | Thông tin cấu hình cho BitBake (SRC_URI, DEPENDS, LICENSE, …). |
| **Task (`do_*`)**                      | Hành động (fetch, compile, install, package...).               |
| **Inheritance (`inherit ...`)**        | Kế thừa logic từ `.bbclass`                                    |
| **Dependency (`DEPENDS`, `RDEPENDS`)** | Xác định packet cần build trước hoặc cài cùng.                 |
| **Output Packages (`PACKAGES`)**       | Xác định các gói nhỏ sinh ra từ recipe.                        |

Ví dụ đơn giản như sau:

```bash
DESCRIPTION = "Simple Hello World Application"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=1e9e7..."

SRC_URI = "file://main.c"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} main.c -o hello
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 hello ${D}${bindir}/hello
}
```

### Vòng đời của một recipe

Một recipe đi qua chuỗi task chuẩn — BitBake gọi là Task Graph. Dưới đây là vòng đời tiêu biểu cho hầu hết recipe phần mềm:

```
fetch → unpack → patch → configure → compile → install → package → deploy
```

**`do_fetch`**

`do_fetch` là task đầu tiên trong vòng đời recipe, chịu trách nhiệm tải source code hoặc patch cần thiết từ các vị trí được định nghĩa trong biến `SRC_URI`.

Cú pháp của biến `SRC_URI`:

```bash
SRC_URI = "<protocol>://<location>;<params>"
```

Với mỗi loại URI sẽ được xử lý theo cách thức khác nhau.
- File cục bộ: `file://`
- Git repository: `git://`
- https: `https:..//`

Khi kết thúc, task `do_fetch` sẽ tạo một file `.done_fetch` trong `${WORKDIR}` để đánh dấu task đã hoàn thành.

**`do_patch`**

Trong yocto, kernel source không nằm cố định ở một nơi, mà nó được tải động thông qua recipe có thể là `linux-yocto.bb`, `linux-ti-staging.bb` hoặc các recipe tương tự khác. Do đó, sau mỗi lần build image thì yocto sẽ unpack lại source mới -> Bởi vì nguyên nhân này mà khi thay đổi source code thì ta cần force compile để không bị mất code.

Tuy nhiên, điều này chỉ có thể làm trên máy local, ta không thể đẩy code lên server hay repo để dev khác sử dụng được -> Ta cần sử dụng task `do_patch`.

Task này có tác dụng apply các file patch vào source code vừa được giải nén từ task `do_unpack`, nó giúp dev có thể custom lại source code hoặc fix bug trước khi compile.

## Layer

Layer là nơi gom nhóm các metadata: recipe, class, config, patch và device tree, nhằm mở rộng hoặc tuỳ biến linux distro.

Nói cách khác: **Layer = một module chứa metadata để build Linux theo cách mà ta mong muốn.**

Nếu không có layer:
- Ta sửa trực tiếp vào core recipes → không maintain được
- Update version mới là hỏng hết → không tái sử dụng được
- Không thể chia nhỏ dự án → khó quản lý

Nhờ layer, Yocto mới có thể:
- Tách biệt phần BSP, middleware, ứng dụng, config distro.
- Cho phép các nhóm phát triển làm việc song song mà không va chạm.
- Cho phép override, append, priority, điều chỉnh mọi thành phần build.

Layer giải quyết bằng cách tách metadata thành từng module logic:

| Loại Layer                         | Vai trò                                                       |
| ---------------------------------- | ------------------------------------------------------------- |
| **BSP Layer**                      | Chứa bootloader, kernel, device tree, drivers, machine config |
| **Distro Layer**                   | Định nghĩa cách build hệ điều hành                            |
| **Middleware Layer**               | Qt, Python, GStreamer, ROS, OpenCV                            |
| **Application Layer**              | Ứng dụng người dùng, startup script                           |
| **Override / Customization Layer** | Dùng để modify recipe qua `.bbappend`                         |

Trong layer, thì nơi quan trọng nhất là `conf/layer.conf`:
- Định nghĩa name của layer
- Thiết lập priority
- Chỉ ra nơi bitbake phải tìm recipes

## Layer priority

Nếu 2 layer có cùng recipe hoặc cùng file `.bbappend`: **Layer có priority cao hơn sẽ override layer thấp hơn.**

Ví dụ:

| Layer          | Priority | Ghi đè           |
| -------------- | -------- | ---------------- |
| meta           | 5        | ❌               |
| meta-yocto     | 6        | ghi đè được meta |
| meta-yourboard | 9        | ghi đè hết       |

## Layer mở rộng recipe

Một layer có thể mở rộng recipe của layer khác mà không cần phải sửa file gốc.

Điều này được thực hiện chủ yếu nhờ:
- `.bbappend` file
- Layer priority

Một `.bbappend` chỉ có tác dụng nếu tên nó trùng pattern của recipe `.bb`.

Ví dụ:

```bash
busybox_1.32.0.bbappend     → match    busybox_1.32.0.bb  
busybox_%.bbappend          → match    mọi version busybox
myapp_git.bbappend          → match    myapp_git.bb
```

**Ví dụ: Thêm patch vào Busybox mà không sửa file gốc**

Trong meta-custom:

```bash
recipes-core/busybox/busybox_%.bbappend
```

Nội dung:

```shell
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "file://fix_bug.patch"
```

Giải thích:
- FILESEXTRAPATHS: thêm đường dẫn chứa file patch của bạn
- SRC_URI +=: thêm patch vào recipe gốc

## Các bước tạo layer mới

1. **Thiết lập môi trường build**

    ```bash
    cd <path_to_build_directory>
    source oe-init-build-env
    ```

2. **Tạo layer mới**

    ```bash
    bitbake-layers create-layer ../meta-customlayer
    ```

    Kết quả: thư mục `meta-customlayer/` được tạo với cấu trúc cơ bản:

    ```
    meta-mycustomlayer/
      ├── conf/
      │    └── layer.conf
      ├── recipes-example/
      │    └── example/
      │         └── example_0.1.bb
      ├── COPYING.MIT
      └── README
    ```

3. **Kiểm tra và chỉnh sửa file** `conf/layer.conf`

    Mở `meta-customlayer/conf/layer.conf`, xác nhận hoặc điều chỉnh các thông số như:

    - `BBFILE_PRIORITY_meta-customlayer` → độ ưu tiên layer
    - `LAYERSERIES_COMPAT_meta-customlayer` → phiên bản tương thích của Yocto

4. **Thêm layer vào cấu hình build**

    Thêm layer vào `conf/bblayers.conf` hoặc dùng `bitbake-layers` để thêm tự động.

    ```bash
    bitbake-layers add-layer ../meta-custom
    ```

    Sau đó, ta có thể kiểm tra với:

    ```bash
    bitbake-layers show-layers
    ```
5. **Tạo recipe hoặc thêm metadata trong layer**
    Ví dụ thêm ứng dụng “HelloWorld”:

    ```bash
    mkdir -p meta-customlayer/recipes-myapp/helloworld/helloworld
    ```

    Tạo file `helloworld_1.0.bb` với nội dung:

    ```bash
    SUMMARY = "Hello World"
    LICENSE = "MIT"
    LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/MIT;md5=<checksum>"

    SRC_URI = "file://helloworld.c"
    S = "${WORKDIR}"

    do_compile() {
        ${CC} ${LDFLAGS} helloworld.c -o helloworld
    }

    do_install() {
        install -d ${D}${bindir}                 # Tạo directory nếu thư mục chưa tồn tại
        install -m 0755 helloworld ${D}${bindir} # Copy file
    }
    ```

    Sau đó build thử:

    ```bash
    bitbake helloworld
    ```

6. **(Tùy chọn) Tạo hoặc sửa image để bao gồm recipe mới**
    Ví dụ trong conf/local.conf hoặc file image recipe:

    ```bash
    IMAGE_INSTALL:append = " helloworld"
    ```

    Rồi build image:

    ```bash
    bitbake <image-name>
    ```

## Chuyển cấu hình từ local.conf sang layer để tự động hóa build

Thông thường `local.conf` có các nhóm:

**Thông tin môi trường build**

- DL_DIR, SSTATE_DIR, TMPDIR
- BB_NUMBER_THREADS, PARALLEL_MAKE
- Các path đặc thù của máy build, proxy, mirror nội bộ…

→ Mấy cấu hình này vẫn để nguyên trong `local.conf`, không nên đưa vào layer.

**Cấu hình distro/policy**

Ví dụ:

```conf
DISTRO_FEATURES:append = " systemd"
VIRTUAL-RUNTIME_init_manager = "systemd"
DISTRO_FEATURES_BACKFILL_CONSIDERED += "sysvinit"
VIRTUAL_RUNTIME_initscripts = "symtemd-compat-units"
```

→ Những cấu hình này nên được gom vào một file distro riêng: `conf/distro/*.conf` trong layer.

**Cấu hình image**

Ví dụ:

```conf
IMAGE_INSTALL:append = " nano htop i2c-tools"
```

-> Những cấu hình này nên chuyển sang file `.bb` image riêng trong layer, ví dụ như: `recipes-core/images/my-image.bb`.

**Cấu hình machine**

Ví dụ:

```conf
require conf/machine/beaglebone-yocto.conf

APPEND:append = " fbcon=map:off"
```

Những cấu hình này nên gom vào một file machine riêng: `conf/machine/*.conf` trong layer.

Tiếp theo, ta cần phải nói cho yocto biết là kernel mới dùng BSP nào, nếu không sẽ gặp lỗi:

```
Nothing PROVIDES 'virtual/kernel'
linux-yocto ... skipped: incompatible with machine beaglebone-yocto-smartfarm (not in COMPATIBLE_MACHINE)
```

Để fix lỗi này, ta cần một số config sau vào file `recipes-kernel/linux/linux-yocto_%.bbappend`:

```conf
# Cho phép linux-yocto build cho machine
COMPATIBLE_MACHINE:append = "|name-machine"

# Map KMACHINE của machine mới về BSP beaglebone
KMACHINE:beaglebone-yocto-smartfarm = "beaglebone"

# Đảm bảo chọn đúng provider
PREFERRED_PROVIDER_virtual/kernel ?= "linux-yocto"
```

## Từ khoá `inherit`

`inherit` dùng để import một class (`.bbclass`) vào trong recipe `.bb`.

Ví dụ khi gọi:

```bitbake
inherit cmake_qt5
```

BitBake sẽ:
- Tìm file `cmake_qt5.bbclass` trong BBLAYERS
- Đọc toàn bộ nội dung file đó
- Gộp nội dung vào recipe
- Gọi override logic của chúng
- Tích hợp vào pipeline build -> các task `do_configure`, `do_compile`...

`cmake_qt5` cung cấp task `do_configure`, task này sẽ tự gọi trong pipeline build:

```
cmake \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_TOOLCHAIN_FILE=...
```

## Các biến quan trọng

**Nhóm WORKDIR — các thư mục build**

| Biến      | Ý nghĩa                             | Ví dụ                         |
| --------- | ----------------------------------- | ----------------------------- |
| `WORKDIR` | Thư mục làm việc chính của recipe   | `tmp/work/.../recipe/1.0-r0/` |
| `S`       | Source directory (sau unpack/patch) | `${WORKDIR}/git`              |
| `B`       | Build directory                     | `${WORKDIR}/build`            |
| `D`       | Staging install directory           | `${WORKDIR}/image`            |
| `T`       | Temp/log directory                  | `${WORKDIR}/temp`             |
| `PN`      | Package/recipe name                 | `qtsmartfarm`                 |
| `PV`      | Version                             | `1.0`                         |
| `PR`      | Revision                            | `r0`                          |
| `PF`      | PN + PV + PR                        | `qtsmartfarm-1.0-r0`          |

**Nhóm Filesystem**

Đây là nhóm dùng trong `do_install()`.

| Biến                     | Giá trị chuẩn           | Mục đích                      |
| ------------------------ | ----------------------- | ----------------------------- |
| `bindir`                 | `/usr/bin`              | Binary cho người dùng         |
| `sbindir`                | `/usr/sbin`             | System binary                 |
| `base_bindir`            | `/bin`                  | Binary cơ bản                 |
| `base_sbindir`           | `/sbin`                 | System binary cơ bản          |
| `libdir`                 | `/usr/lib`              | Thư viện (.so)                |
| `includedir`             | `/usr/include`          | Header files                  |
| `sysconfdir`             | `/etc`                  | File config                   |
| `datadir`                | `/usr/share`            | Data file (icons, fonts, qml) |
| `localstatedir`          | `/var`                  | Data runtime                  |

## Troubleshoot

### Vấn đề về tài nguyên

Điều kiện bộ nhớ tối thiểu mà một máy build có thể dùng yocto là:
- RAM 8GB
- DISK 120GB

Nếu không sẽ dẫn đến lỗi thiếu bộ nhớ, ví dụ như sau:

```bash
WARNING: The free space of /home/nguyenbui/tutorial/yocto/build/tmp (/dev/sda5) is running low (0.998GB left)
ERROR: No new tasks can be executed since the disk space monitor action is "STOPTASKS"!
```

Lúc gặp lỗi này thì ta sẽ cần tăng kích thước disk trong máy ảo và dùng tool `GParted` để tăng kích thước ổ đĩa.

```bash
sudo apt install gparted
sudo gparted
```

`GParted` là một ứng dụng GUI nên ta không thể sử dụng ssh để cài đặt mà cần màn hình đồ họa.

### Vấn đề về luồng thực thi

Khi build, yocto có xu hướng dùng hết các luồng mà SoC hỗ trợ → Điều này sẽ dẫn đến làm máy build bị lag → Do đó ta cần giảm số luồng mà yocto có thể sử dụng. Để làm được điều này, ta sẽ cần thêm hai cấu hình vào file `build/conf/local.conf`. Ví dụ như sau:

```conf
BB_NUMBER_THREADS = "8"
PARALLEL_MAKE = "-j8"
```

→ Yocto chỉ sử dụng tối đa 8 luồng để build.

### Vấn đề vể recipe

Khi build yocto mà gặp lỗi kiểu như sau:

```bash
Parsing of 883 .bb files complete (0 cached, 883 parsed). 1644 targets, 45 skipped, 0 masked, 0 errors.
ERROR: Nothing PROVIDES 'binutils-cross'. Close matches:
binutils
binutils-cross-testsuite
binutils-cross-arm
```

Thì là do `binutils-cross` không phải là recipe để gọi trực tiếp. Trong Yocto, recipe cross binutils sẽ đổi tên theo kiến trúc target: `binutils-cross-${TARGET_ARCH}`. Ví dụ: `binutils-cross-arm`, `binutils-cross-aarch6`4,…

Lúc này ta sẽ build riêng recipe `binutils-cross` tùy thuộc vào kiến trúc target. Ví dụ như kiến trúc arm thì ta làm như sau:

```bash
bitbake binutils-cross-arm
```

## Debug

### Cách lấy log task của một recipe bất kỳ

Giả sử, ta muốn xem log task `do_configure` của recipe `qtbase` thì trước tiên, ta cần xác định `WORKDIR` của recipe: 

```bash
bitbake -e qtbase | grep ^WORKDIR=
```

Output sẽ là đường dẫn tới thư mục `WORKDIR`.

```bash
WORKDIR="/home/nguyenbui/tutorial/yocto/build/tmp/work/x86_64-linux/qtbase-native/5.15.7+gitAUTOINC+abcd1234-r0"
```

Sau đó, ta sẽ vào thư mục `WORKDIR\temp\log.do_<task>` và xem task log mong muốn.

### `oe-pkgdata-util`

Đây là tool của Yocto dùng để đọc và tra cứu metadata của các package sau khi đã được build.

Khi ta build một recipe, Yocto tạo ra nhiều package con. Mỗi package đó sẽ được ghi metadata vào thư mục `tmp/work/<...>/package/`

-> `oe-pkgdata-util` cho phép truy vấn lại dữ liệu đó, thay vì phải mò vào trong thư mục.

| Lệnh                                                 | Ý nghĩa                                                    |
| ---------------------------------------------------- | ---------------------------------------------------------- |
| `oe-pkgdata-util list-pkgs`                          | Liệt kê toàn bộ package đã build ra                        |
| `oe-pkgdata-util find-path /usr/include/mosquitto.h` | Tìm xem file này thuộc gói nào                             |
| `oe-pkgdata-util list-pkg-files mosquitto-dev`       | Liệt kê toàn bộ file có trong gói `mosquitto-dev`          |
| `oe-pkgdata-util lookup-recipe mosquitto-dev`        | Tìm recipe gốc tạo ra gói này                              |
| `oe-pkgdata-util list-pkg-provides mosquitto`        | Xem gói này `PROVIDES` gì                                  |
| `oe-pkgdata-util list-pkg-rdepends mosquitto`        | Xem gói này `RDEPENDS` vào đâu (tức là dependency runtime) |
| `oe-pkgdata-util list-pkg-builddeps mosquitto`       | Xem dependency build-time của recipe này                   |

Ví dụ:

- Xem toàn bộ package có trong hệ thống:

```bash
oe-pkgdata-util list-pkgs | grep mosquitto
```

Kết quả:

```
libmosquitto1
libmosquittopp1
mosquitto
mosquitto-dev
mosquitto-clients
mosquitto-staticdev
```

- Tìm xem header thuộc package nào

```bash
oe-pkgdata-util find-path /usr/include/mosquitto.h
```

→ Trả về: mosquitto-dev
=> nghĩa là header nằm trong gói này.

- Xem package `mosquitto-dev` chứa gì

```bash
oe-pkgdata-util list-pkg-files mosquitto-dev
```

Output:

```
/usr/include/mosquitto.h
/usr/lib/libmosquitto.so
/usr/lib/pkgconfig/libmosquitto.pc
```