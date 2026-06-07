# Dm-verity

## 1. dm-verity là gì?

Trước hết cần hiểu dm-verity không phải filesystem mà nó là một device-mapper target. Device-mapper là lớp trung gian nằm giữa block device thật (eMMC partition) và filesystem driver.

Khi một proces đọc file, request đi qua nhiều lớp trong kernel:

```
Userspace process (cat /etc/passwd)
  -> VFS (Virtual File System)
    -> filesystem driver (ext4 / squashfs)
      -> device-mapper (dm-verity target)
        -> block device driver (mmcblk0p2, mmcblk0p3,...)
          -> eMMC hardware
```

Mỗi khi có I/O request đọc một block, dm-verity chặn lại, tính hash, verify trước khi trả data lên filesystem layer. Nếu hash không khớp, trả về I/O error thay vì data bị tamper.

dm-verity cần hai partition trên block device: data partition chứa rootfs thực tế và hash partition chứa toàn bộ Merkle hash tree đã được tính trước tại build time. Hai partition này có thể nằm cạnh nhau trên cùng block device.

## 2. Cơ chế hoạt động

Cơ chế dm-verity hoạt động theo nguyên lý Merkle tree: toàn bộ rootfs partition được chia thành các block 4KB, mỗi block được hash bằng SHA-256. Các hash này ghép cặp rồi hash tiếp lên tầng trên, cứ thế cho đến khi còn lại một root hash duy nhất. Khi đọc bất kỳ block nào, kernel tính hash của block đó rồi xác minh ngược lên root hash qua cây Merkle.

Giả sử rootfs partition có 1GB = 262144 block (mỗi block 4KB):

```
Level 0:    Mỗi data block được hash độc lập bằng SHA-256
            -> Tạo ra 262144 hash × 32 byte

Level 1:    Các hash được nối theo cặp thành một hash block rồi hash tiếp
            Mỗi hash block 4KB chứa được 4096/32 = 128 hash con
            -> 262144 hash / 128 hash per block = 2048 hash block
            -> 2048 hash block được hash -> 2048 hash

Level 2:    2048 / 128 = 16 hash block -> 16 hash

Level 3:    16 / 128 = 1 hash block -> nhưng chưa đủ tạo 1 block
            -> pad thành 1 block -> hash -> 1 hash

Root hash:  SHA-256 của level 3 block = 32 bytes
```

Tổng kích thước cho hash tree: (262144 + 2048 + 16 + 1) × 32 byte ≈ 8MB cho 1GB data. Tỷ lệ overhead khoảng 0.8% là rất nhỏ. Đây là lý do dm-verity phù hợp cho embedded device có storage hạn chế.

Hash tree được lưu trên một partition riêng hoặc append vào cuối data partition.

Layout trên eMMC/SD:

```
mmcblk0p1:  /boot (FAT32, chứa fitImage)
mmcblk0p2:  /rootfs (ext4, read-only)
mmcblk0p3:  verity hash tree (raw, không có filesystem)
mmcblk0p4:  /data (ext4, writable, cho config/logs/user data)
```

## 3. Quá trình verify (runtime)

Giả sử rootfs có kích thước 16 MB, block size = 4 KB, hash algorithm = SHA-256.

Data partition chứa rootfs gồm: `16 MB ÷ 4 KB = 4096 data block`, đánh số từ block #0 đến #4095.

Trên disk, hash partition trông như sau:

```
+-------------------------------------------------------------------+
| Level 0: 32 hash blocks                                           |
|  Block H0: hash(data#0), hash(data#1), ...,hash(data#127)         |
|  Block H1: hash(data#128), hash(data#129), ..., hash(data#255)    |
|  ...                                                              |
|  Block H31: hash(data#3968), ..., hash(data#4095)                 |
+-------------------------------------------------------------------+
| Level 1: 1 hash block                                             |
|  Block H32: hash(H0), hash(H1), ..., hash(H31)                    |
+-------------------------------------------------------------------+
| Level 2: Root hash (32 bytes)                                     |
|  = hash(H32)                                                      |
+-------------------------------------------------------------------+
```

Giả sử ứng dụng chạy `cat /etc/hostname`. File này nằm ở data block 300.


**Bước 1 — Ứng dụng gọi system call**

```
Userspace:  cat gọi read() -> system call vào kernel
                │
                ▼
VFS layer:  Kernel tra inode của /etc/hostname
            Xác định dữ liệu nằm ở block #300 trên block device
                │
                ▼
Device-mapper: Request đi đến dm-verity target
```

**Bước 2 — dm-verity kiểm tra cache**

Trước khi làm bất cứ điều gì, dm-verity kiểm tra buffer cache của kernel:

```
dm-verity nhận request đọc block #300
    │
    └── Block #300 đã có trong cache và đã verified trước đó?
          │
          ├── có -> trả dữ liệu ngay, không tính hash lại
          │        (đây là lý do overhead thực tế rất thấp)
          │
          └── không -> tiếp tục bước 3
```

**Bước 3 — Đọc data block từ disk**

```
dm-verity yêu cầu đọc block #300 từ data partition
    -> Dữ liệu raw của block #300 được load vào bộ nhớ
       (Chưa trả cho ứng dụng, chờ xác minh)
```

**Bước 4 — Tính hash của data block**

dm-verity tính hash:

```
computed_hash = SHA-256(salt ∥ block_data)
            = SHA-256(salt ∥ <4096 bytes của block #300>)
            = ví dụ: a3f2...9b71 (32 bytes)
```

**Bước 5 — Xác minh qua Merkle tree**

Đây là phần cốt lõi. dm-verity cần xác minh `computed_hash` khớp với hash đã lưu, rồi xác minh hash đó ngược lên đến root hash.

**5a. Tra Level 0 — tìm leaf hash:**

Block #300 nằm trong hash block nào ở Level 0?

```
Mỗi hash block chứa 128 hash entry
    -> hash block index = 300 :128 = 2 (hash block H2)
    -> Vị trí trong block = 300 mod 128 = 44  (entry thứ 44)
    -> dm-verity đọc hash block H2 từ hash partition
    -> Lấy entry #44 -> stored_hash_L0 = ví dụ: a3f2...9b71

So sánh: computed_hash   == stored_hash_L0 ?
```

**5b. Tra Level 1 — xác minh hash block H2:**

Bây giờ cần chắc chắn hash block H2 chưa bị sửa đổi.


```
dm-verity tính hash của toàn bộ hash block H2:

computed_hash_H2 = SHA-256(salt ∥ <4096 bytes của hash block H2>)
                 = ví dụ: 7c8e...3d42

Hash block H2 nằm ở đâu trong Level 1?
- Level 1 chỉ có 1 hash block (H32)
- Entry index = 2 vì H2 là hash block thứ 2 ở level 0

dm-verity đọc hash block H32 từ hash partition

Lấy entry #2 -> stored_hash_L1 = ví dụ: 7c8e...3d42

So sánh: computed_hash_H2 == stored_hash_L1 ?
```

**5c. Tra Level 2 — xác minh root hash:**

Cuối cùng, xác minh hash block H32 (level 1) với root hash.

```
dm-verity tính hash của hash block H32:
  computed_root = SHA-256(salt ∥ <4096 bytes của hash block H32>)
                = ví dụ: e5b1...0a88

So sánh với root hash được truyền qua kernel cmdline lúc boot:
```

Đến đây, chuỗi xác minh hoàn tất

## 4. Root hash & hash tree & salt

### 4.1. Root hash

Root hash là giá trị hash 32 byte nằm ở đỉnh cây Merkle, đại diện cho toàn bộ nội dung của rootfs. Nó được tạo ra trong quá trình build image, không phải lúc runtime.

Tính chất quan trọng: nếu bất kỳ byte nào trong rootfs thay đổi -> leaf hash thay đổi -> hash các level trung gian thay đổi -> root hash thay đổi. Vì vậy chỉ cần bảo vệ 32 byte root hash là đủ để bảo vệ toàn bộ filesystem (có thể hàng trăm MB hoặc vài GB).

**Cách truyền root hash vào kernel command line**

- Sử dụng tham số `dm-mod.create`: Điều này sẽ được nói rõ ở phần [Tham số `dm-modcreate`](#tham-số-dm-modcreate).
- Setup trong initramfs: Thay vì truyền dm table qua cmdline, initramfs chứa script tự setup:

**Root hash được bảo vệ như thế nào?**

Nếu attacker sửa được root hash, họ có thể tạo hash tree giả cho rootfs giả. Vậy thì root hash được bảo vệ thế nào?

```
Root hash được truyền vào kernel cmdline
  -> Kernel cmdline nằm trong FIT image (trong configuration node)
  -> FIT image được sign bằng RSA private key
  -> U-Boot verify FIT signature bằng public key nằm trong uboot DTB
  -> U-Boot được verify bởi SPL (HS device)
  -> SPL được verify bởi ROM bootloader (HS device)
  -> ROM kiểm tra signature dựa trên key hash trong eFuse
```

Nếu attacker muốn thay đổi root hash phải thì cần phải sửa cmdline -> phải sửa FIT image -> chữ ký FIT không match -> U-Boot từ chối boot. Muốn sign lại FIT -> cần private key. Muốn thay public key trong U-Boot -> phải sửa U-Boot → SPL từ chối. Chuỗi này kéo dài đến tận eFuse trong silicon — không thể sửa đổi bằng phần mềm.

Ngoài ra, mỗi lần rootfs thay đổi như thêm/xóa file, cập nhật package...thì root hash sẽ khác hoàn toàn. Vì vậy quy trình build phải tự động tính root hash mới và nhúng vào FIT image mỗi lần build.

### 4.2. Cách tạo root hash và hash tree

Root hash và hash tree được tạo ra trong quá trình build, không phải trên device. Quá trình này sử dụng công cụ `veritysetup` từ package `cryptsetup`. Đây là flow chính xác:

```
veritysetup format <data_device> <hash_device> \
    --data-block-size=4096 \
    --hash-block-size=4096 \
    --hash=sha256 \
    --salt=<random_hex_string>
```

Giả sử rootfs.ext4 là image đã build xong và hash partition sẽ được ghi vào `rootfs-hash.img`:

```bash
veritysetup format rootfs.ext4 rootfs-hash.img \
    --data-block-size=4096 \
    --hash-block-size=4096 \
    --hash=sha256 \
    --salt=a1b2c3d4e5f6...
```

Output:

```
VERITY header information for rootfs-hash.img
UUID:               a1b2c3d-...
Hash type:          1
Data blocks:        262144
Data block size:    4096
Hash block size:    4096
Hash algorithm:     sha256
Salt:               a1b2c3d4e5f6...  (random, 256-bit)
Root hash:          7e0ad1...3f82   <- Giá trị này cần truyền cho kernel
```

`veritysetup` format thực hiện:
1. Đọc toàn bộ rootfs image `rootfs.ext4`, chia thành 4KB mỗi block.
2. Với mỗi block: `hash = SHA-256(salt || block_data)`
   Salt được thêm vào trước data để ngăn pre-computation attack
3. Gộp 128 hash thành 1 hash block (4KB)
4. Hash tiếp lên level trên, lặp lại cho đến root
5. Ghi toàn bộ hash tree vào `rootfs.hashtree`
6. In ra root hash — 32 byte hex

:::tip Salt là gì?
Salt là giá trị random 32 byte được chọn lúc build image. Nó được nối vào trước mỗi data block trước khi hash: `hash = SHA-256(salt || block_data)`. Mục đích của nó là đảm bảo rằng hai block có cùng content nhưng nằm trong hai image khác nhau sẽ có hash khác nhau. Điều này ngăn attacker dùng pre-computed hash table.
:::

:::warning Lưu ý
Tool `verifysetup` sẽ tự động sinh một salt ngẫu nhiên nếu ta không truyền tham số `--salt` cho nó. Ngoài ra, nếu ta truyền `--salt=-` thì salt sẽ là chuỗi rỗng, tức là không dùng salt.
:::

Root hash và salt từ output này cần được inject vào kernel command line trong FIT image. Thứ tự build vì vậy phải là:
1. Build rootfs image (ext4/squashfs)
2. Chạy veritysetup format -> sinh hash tree + root hash + salt
3. Inject root hash vào kernel cmdline / FIT config
4. Build FIT image (kernel + DTB + cmdline với root hash)
5. Ký FIT image bằng RSA private key
6. Flash tất cả lên device: boot partition, rootfs, hash tree

Thứ tự này tạo ra circular dependency nếu không cẩn thận: rootfs phải build xong trước khi tính hash và root hash phải có trước khi ký FIT image. Trong Yocto, ta có thể định nghĩa class `dm-verity-img` để nó tự động xử lý các bước này (Điều này sẽ được nói rõ [ở đây](#3-tạo-file-bbclass-generate-hash-tree)).

## 5. Tham số `dm-mod.create`

### 5.1. Tổng quan

`dm-mod.create` là một tham số kernel command line cho phép tạo device-mapper device ngay trong quá trình khởi động kernel, trước khi bất kỳ userspace process nào chạy (kể cả init). Điều này rất quan trọng vì rootfs cần phải được mount trước khi init khởi động, nên dm-verity device phải tồn tại trước thời điểm đó.

Tham số này được xử lý bởi module `dm-mod` (device-mapper) trong kernel, cụ thể là hàm `dm_setup_cleanup()` được gọi rất sớm trong quá trình boot.

Để `dm-mod.create` hoạt động, kernel phải được build với các option sau:

```
CONFIG_BLK_DEV_DM=y         # Device mapper support
CONFIG_DM_INIT=y            # Cho phép tạo dm device từ cmdline
CONFIG_DM_VERITY=y          # dm-verity target
```

`CONFIG_BLK_DEV_DM` giúp module `dm-mod` được built-in trong kernel image -> mới có khả năng đọc `dm-mod.create`. 

Nếu thiếu `CONFIG_DM_INIT` kernel sẽ bỏ qua `dm-mod.create` mà không báo lỗi -> rootfs mount fail và kernel panic với "unable to mount root fs on /dev/dm-0".

**Cú pháp tổng quát:**

```
dm-mod.create="<name>,<uuid>,<minor>,<flags>,<table>;..."
```

Có thể tạo nhiều device-mapper device cùng lúc, phân cách bằng dấu `;`. Mỗi device gồm hai phần: header và table.

### 5.2. Phần header - mô tả device

**name:** Tên của device-mapper device. Kernel sẽ tạo `/dev/dm-N` và device-mapper tạo symlink `/dev/mapper/<name>`. Ví dụ:

```
name = rootfs-verity
  -> /dev/dm-0                  (device node, nếu đây là dm device đầu tiên)
  -> /dev/mapper/rootfs-verity  (symlink do udev tạo, nhưng lúc early boot
                                 chưa có udev, nên dùng /dev/dm-0)
```

**uuid:** UUID của device. Để trống nếu không cần:

```
rootfs-verity,,,...             <- uuid trống (phổ biến nhất)
rootfs-verity,abcd-1234,,...    <- có uuid
```

**minor:** Minor number cho device node. Để trống để kernel tự gán:

```
rootfs-verity,,,         ← uuid trống, minor tự gán
rootfs-verity,,5,        ← minor = 5 → /dev/dm-5
```

**flags:** Cờ điều khiển thuộc tính device. Giá trị phổ biến:

```
ro    -> Read-only (bắt buộc cho dm-verity)
rw    -> Read-write (dùng cho dm-crypt, dm-linear...)
```

Ví dụ header hoàn chỉnh:

```
rootfs-verity,,,ro
```

### 5.3. Phần table - định nghĩa mapping

Phần table nằm ngay sau flags, mô tả cách device-mapper xử lý I/O. Cú pháp tổng quát:

```
<start_sector> <num_sectors> <target_type> <target_args>
```

Ý nghĩa của từng trường:

| Trường | Ý nghĩa |
| --- | --- |
| `start_sector` | Sector bắt đầu trên virtual device (thường là 0). |
| `num_sectors` | Kích thước vùng mapping tính theo sector (1 sector = 512 bytes) |
| `target_type` | Loại device-mapper target. Một số target phổ biến:<br>- verity -> dm-verity<br>- crypt -> dm-crypt<br>- linear -> dm-linear<br>- striped -> dm-striped |
| `target-args` | Tham số riêng cho từng target type. Đây là phần phức tạp nhất và khác nhau tùy target. |

`target_args` cho dm-verity:

```
verity <version> <data_dev> <hash_dev> <data_block_size> <hash_block_size> <num_data_blocks> <hash_start_block> <algorithm> <root_hash> <salt> [<optional_args>]
```

Ý nghĩa của từng trường:

| Trường | Ý nghĩa |
| --- | --- |
| `version` | Phiên bản dm-verity format. |
| `data_dev` | Block device chứa rootfs data. |
| `hash_dev` | Block device chứa Merkle hash tree. |
| `data_block_size` | Kích thước mỗi data block tính bằng byte. |
| `hash_block_size` | Kích thước mỗi block trong hash tree. |
| `num_data_blocks` | Tổng số data blocks trong data partition. |
| `hash_start_block` | Block bắt đầu của hash tree trên `hash_dev`, tính bằng hash block (không phải byte hay sector) |
| `algorithm` | Thuật toán hash. |
| `root_hash` | Giá trị hash ở đỉnh cây Merkle, biểu diễn dưới dạng hex string. |
| `salt` | Chuỗi hex ngẫu nhiên, được thêm vào mỗi phép tính hash. |

Sau salt, có thể thêm các tham số bổ sung:

```
<num_optional_args> <arg1> <arg2> ...
```

Các optional arg phổ biến:

| Option | Ý nghĩa |
| --- | --- |
| `ignore_corruption` | Không trả lỗi khi verify fail, chỉ log |
| `restart_on_corruption` | Reboot hệ thống khi verify fail |
| `panic_on_corruption` | Kernel panic khi verify fail |
| `ignore_zero_blocks` | Bỏ qua verify cho block toàn zero (tối ưu performance) |
| `check_at_most_once` | Chỉ verify mỗi block 1 lần, sau đó tin tưởng cache -> Giảm overhead nhưng kém an toàn hơn nếu storage bị sửa đổi sau lần đọc đầu tiên |
| `root_hash_sig_key /path/to/cert` | Yêu cầu root hash phải có chữ ký hợp lệ |

Ví dụ với optional args:

```
... sha256 <root_hash> <salt> 2 restart_on_corruption ignore_zero_blocks
                              │
                              └─ num_optional_args = 2
```

### 5.4. Ví dụ hoàn chỉnh

Ví dụ đối với BBB có rootfs 256MB trên eMMC partition 2, hash trên partition 3, restart sau khi verify fail:

```
dm-mod.create="rootfs-verity,,,ro,0 524288 verity 1 /dev/mmcblk1p2 /dev/mmcblk1p3 4096 4096 65536 0 sha256 e5b1c4f28a3d7e9f0b6c2d8a1e4f7b3c9d0a5e8f2c6b1d4a7e3f9c0b5d8a2e1f aaf2d04e9c5bef0a1acda78bce7a4c8751a06e3b 1 restart_on_corruption"
```

Phân tích từng tham số:

```
dm-mod.create="
  rootfs-verity          device name -> /dev/dm-0, /dev/mapper/rootfs-verity
  ,                      uuid (trống)
  ,                      minor (tự gán)
  ,                      (dấu phân cách)
  ro                     read-only
  ,                      (phân cách header và table)
  0                      start sector
  524288                 num sectors (256MB ÷ 512)
  verity                 target type
  1                      verity version
  /dev/mmcblk1p2         data device
  /dev/mmcblk1p3         hash device
  4096                   data block size
  4096                   hash block size
  65536                  num data blocks (256MB ÷ 4096)
  0                      hash start block
  sha256                 hash algorithm
  e5b1c4f2...            root hash (64 hex chars)
  aaf2d04e...            salt
  1                      num optional args
  restart_on_corruption  reboot khi verify fail
"
```

Và kernel cmdline đầy đủ trên BBB:

```
console=ttyO0,115200n8
root=/dev/dm-0
ro
rootwait
dm-mod.create="rootfs-verity,,,ro,0 524288 verity 1 /dev/mmcblk1p2 /dev/mmcblk1p3 4096 4096 65536 0 sha256 e5b1c4f28a3d7e9f0b6c2d8a1e4f7b3c9d0a5e8f2c6b1d4a7e3f9c0b5d8a2e1f aaf2d04e9c5bef0a1acda78bce7a4c8751a06e3b 1 restart_on_corruption"
```

Những điểm cần chú ý:
- `root=/dev/dm-0` chứ không phải `/dev/mmcblk1p2` vì kernel phải mount qua device-mapper layer.
- `rootwait` bắt buộc trên eMMC/SD vì kernel cần đợi block device sẵn sàng trước khi dm-mod tạo verity device.
- Toàn bộ giá trị `dm-mod.create` nằm trong dấu ngoặc kép vì chứa khoảng trắng.

### 5.5. Luồng hoạt động

Khi kernel boot, nó xử lý cmdline theo nhiều giai đoạn. dm-mod.create được xử lý rất sớm, trong giai đoạn init trước khi chạy /sbin/init:

```
Kernel boot sequence (đơn giản hóa):

tart_kernel()
├── setup_arch()          -> khởi tạo phần cứng (AM335x, DRAM...)
├── parse_early_param()   -> xử lý tham số sớm (console=, earlycon=...)
├── ...
├── do_basic_setup()
│   └── do_initcalls()    -> gọi các init function theo thứ tự
│       ├── ...
│       ├── dm_init()     -> device-mapper core khởi tạo
│       └── dm_setup()    -> đọc dm-mod.create, tạo device
│
├── prepare_namespace()
│   └── mount_root()      -> mount rootfs từ root=/dev/dm-0
│
└── run_init_process()    -> chạy /sbin/init
```

Chi tiết hơn về bước `dm_setup()`:

```
dm_setup() được gọi (trong drivers/md/dm-init.c)
    -> Đọc chuỗi dm-mod.create từ saved_command_line (cmdline đã parse)
    -> Parse header: name="rootfs-verity", uuid="", minor=auto, flags=ro
    -> Parse table: "0 524288 verity 1 /dev/mmcblk1p2 /dev/mmcblk1p3 ..."
    -> Gọi dm_create() -> tạo mapped device nội bộ
    -> Gọi dm_table_create() -> tạo mapping table
    -> Gọi dm_table_add_target("verity", ...) -> khởi tạo dm-verity target
       ├── Parse root_hash, salt, algorithm
       ├── Mở data_dev (/dev/mmcblk1p2) và hash_dev (/dev/mmcblk1p3)
       └── Đọc verity superblock từ hash_dev
    -> Gọi dm_setup_md_queue() -> tạo request queue cho block device
    -> Đăng ký với kernel block layer → major:minor = 253:0
    -> devtmpfs tạo /dev/dm-0
    -> Device sẵn sàng nhận I/O
```

Sau bước này, `/dev/dm-0` tồn tại và hoạt động đầy đủ. Kernel tiếp tục đến `mount_root()`, thấy `root=/dev/dm-0`, mount nó làm rootfs.

### 5.6. Tại sao phải tạo device-mapper trước khi mount rootfs?

Đây là câu hỏi cốt lõi. Lý do là vì rootfs không nằm trực tiếp trên partition vật lý nữa, mà nằm sau một lớp device-mapper xử lý I/O.

**Không có dm-verity**

```
root=/dev/mmcblk1p2

Ứng dụng đọc file -> VFS -> ext4 -> block layer -> eMMC driver -> phần cứng
                                                        │
                                                  /dev/mmcblk1p2
```

Kernel mount trực tiếp partition. Không có lớp nào xác minh tính toàn vẹn. Kẻ tấn công sửa bất kỳ byte nào trên eMMC, ứng dụng đọc được dữ liệu bị sửa mà không hề biết.

**Có dm-verity**

```
root=/dev/dm-0

Ứng dụng đọc file -> VFS -> ext4 -> block layer -> device-mapper (dm-verity)
                                                      │
                                               Verify hash <- hash từ /dev/mmcblk1p3
                                                      │
                                               Đọc data từ /dev/mmcblk1p2
```

Kernel mount `/dev/dm-0`, không phải `/dev/mmcblk1p2`. Mọi I/O đều đi qua dm-verity layer. dm-verity đóng vai trò trung gian — nó đọc data từ partition thật, verify hash, rồi mới trả dữ liệu lên cho ext4/VFS.

Nếu `/dev/dm-0` chưa tồn tại lúc kernel muốn mount rootfs, kernel không có gì để mount -> panic. Vì vậy dm-mod phải tạo device trước.

## 6. Initramfs

### 6.1. Tổng quan

Initramfs (Initial RAM Filesystem) là một hệ thống file tạm thời được load vào RAM ngay khi kernel khởi động, trước khi rootfs thật được mount. Nó chứa đủ công cụ tối thiểu để kernel chuẩn bị môi trường trước khi chuyển giao sang rootfs thật.

**Initramfs chứa gì?**

Initramfs là một cpio archive (thường nén gzip), bên trong chứa cấu trúc filesystem tối thiểu:

```
initramfs/
├── init                    <- Script hoặc binary chạy đầu tiên (PID 1)
├── bin/
│   ├── busybox             <- Cung cấp sh, mount, mkdir, sleep...
│   └── veritysetup         <- Tool tạo dm-verity device
├── sbin/
│   └── switch_root         <- Chuyển sang rootfs thật
├── lib/
│   ├── ld-linux.so         <- Dynamic linker
│   ├── libc.so             <- C library
│   └── libcryptsetup.so
├── dev/                    <- Device nodes (hoặc mount devtmpfs)
├── proc/                   <- Mount point cho procfs
├── sys/                    <- Mount point cho sysfs
└── mnt/
    └── root/               <- Mount point cho rootfs thật
```

Kích thước của nó thường rất nhỏ chỉ 2-10 MB, vừa đủ để hoàn thành nhiệm vụ rồi biến mất.

### 6.2. Vòng đời của initramfs

```
ROM -> SPL -> U-Boot -> Load FIT Image vào RAM
                          │
                          ├── zImage (kernel)
                          ├── DTB (device tree)
                          └── initramfs.cpio.gz  <- nằm trong FIT
                                │
                                ▼
                    Kernel khởi động, giải nén initramfs
                    vào ramfs (tmpfs trong RAM)
                                │
                                ▼
                    Kernel chạy /init từ initramfs
                    (đây là process đầu tiên, PID 1)
                                │
                                ▼
                    /init thực hiện các bước chuẩn bị:
                      - Load driver nếu cần
                      - Setup dm-verity
                      - Mount rootfs thật
                      - Setup overlayfs
                                │
                                ▼
                    switch_root sang rootfs thật
                    initramfs được giải phóng khỏi RAM
                                │
                                ▼
                    /sbin/init của rootfs thật chạy (systemd, sysvinit...)
                    Hệ thống hoạt động bình thường
```

### 6.3. Tại sao dm-verity cần initramfs?

dm-verity không bắt buộc cần initramfs. Ngoài initramfs, thì còn một cách khác là sử dụng `dm-mod.create` như đã nói ở phần trên. Tuy nhiên cách này có một số điểm hạn chế:

**Mọi thứ phải built-in**

```
CONFIG_BLK_DEV_DM=y
CONFIG_DM_INIT=y
CONFIG_DM_VERITY=y
CONFIG_CRYPTO_SHA256=y
```

Tất cả phải là `=y`, nếu bất kỳ cái nào `=m` thì kernel không load được module vì rootfs chưa được mount.

**Cmdline rất dài và cứng nhắc**

```
dm-mod.create="rootfs-verity,,,ro,0 524288 verity 1 /dev/mmcblk1p2
/dev/mmcblk1p3 4096 4096 65536 0 sha256 e5b1c4f28a3d7e9f0b6c2d8a
1e4f7b3c9d0a5e8f2c6b1d4a7e3f9c0b5d8a2e1f aaf2d04e9c5bef0a1acda
78bce7a4c8751a06e3b 1 restart_on_corruption"
```

- Chuỗi rất dài, khó debug khi sai
- Một số bootloader có giới hạn cmdline (1024 hoặc 2048 bytes)

**Overlayfs cho writable layer**

```
Rootfs là read-only (yêu cầu của dm-verity)
    │
    ├── Nhưng hệ thống cần ghi /var/log, /tmp, /etc/hostname...
    │
    ▼
Cần mount overlayfs:
    mount -t overlay overlay \
        -o lowerdir=/mnt/root,upperdir=/mnt/rw/upper,workdir=/mnt/rw/work \
        /mnt/merged

Overlay mount phải xảy ra sau khi dm-verity rootfs mount
nhưng trước khi /sbin/init chạy
    │
    ▼
Chỉ initramfs có thể làm điều này
(dm-mod.create không có khả năng chạy thêm lệnh mount)
```

**Không thể làm gì phức tạp:**

- Kiểm tra partition table trước khi setup
- Thử fallback sang partition backup nếu verify fail
- Log lỗi ra serial trước khi panic
- Chạy firmware update recovery
- Giải mã partition (dm-crypt) TRƯỚC KHI verify (dm-verity)
- Kết hợp nhiều device-mapper layer

-> Initramfs giúp giải quyết tất cả hạn chế trên.

Đối với các sản phẩm production thì initramfs thường là lựa chọn tốt hơn vì tính linh hoạt và khả năng xử lý lõi.

Khi sử dụng initramfs thì một số cấu hình kernel có thể dùng `=m` hoặc không cần:

```
CONFIG_BLK_DEV_DM=m
CONFIG_DM_VERITY=m
# CONFIG_DM_INIT is not set   <- loại bỏ vì không dùng dm-mod.create
```

**Vậy thì initramfs tạo dm-device và setup dm-verity như thế nào?**

```
Kernel khởi động
    -> Giải nén initramfs vào RAM
    -> Chạy /init từ initramfs
        ├── veritysetup có sẵn trong initramfs (Không phụ thuộc rootfs)
        ├── Libraries có sẵn trong initramfs
        └── Kernel modules có sẵn (hoặc đã built-in)
    -> /init load kernel module dm-mod.ko hoặc dm-verity.ko (nếu cần)
    -> /init chạy veritysetup
    -> dm-verity device được tạo thành công
    -> switch_root
    -> Rootfs thật chạy bình thường, được bảo vệ bởi dm-verity
```

Điểm mấu chốt ở đây là tool `veritysetup`, đây là tool userspace thuộc package `cryptsetup`. Khi gọi `veritysetup open`, nó sẽ tự động thực hiện để tạo dm-verity device.

## 7. Apply dm-verity vào yocto

### 7.1. Tổng quan luồng build

```
Yocto build rootfs.ext4
        │
        ▼
Post-processing: Tạo hash tree + root hash + salt
        │
        ▼
Root hash được inject vào kernel cmdline trong FIT image source (.its file)
        │
        ▼
Sign FIT image, nhúng public key vào U-Boot device tree
        │
        ▼
Tạo image layout cho eMMC/SD:
  Partition 1: boot (MLO, u-boot, fitImage)
  Partition 2: rootfs (data - ext4 read-only)
  Partition 3: hash (Merkle tree)
  Partition 4: data (writable, cho overlayfs)
```

### 7.2. Thêm kernel config cho dm-verity

Tìm xem ta đang dùng kernel recipe nào:

```bash
# Trong build directory
bitbake -e virtual/kernel | grep "^PN="
# Output ví dụ: PN="linux-ti-staging" hoặc PN="linux-yocto"
```

Tạo bbappend tương ứng. Giả sử kernel là linux-yocto:

```bash
mkdir -p meta-secure/recipes-kernel/linux/cfg
```

File `meta-secure/recipes-kernel/linux/cfg/dm-verity.cfg`:

```conf
# Device mapper core
CONFIG_BLK_DEV_DM=y
CONFIG_DM_INIT=y

# dm-verity
CONFIG_DM_VERITY=y

# Crypto support
CONFIG_CRYPTO_SHA256=y
CONFIG_CRYPTO_HASH=y
CONFIG_CRYPTO_HASH_INFO=y

# Bật nếu muốn yêu cầu root hash phải có chữ ký riêng
CONFIG_DM_VERITY_VERIFY_ROOTHASH_SIG is not set
```

`CONFIG_DM_INIT=y` cho phép kernel parse dm target từ kernel command line parameter `dm-mod.create=` mà không cần initramfs. Nếu không bật, ta phải dùng initramfs để setup dm-verity trước khi mount rootfs.

`CONFIG_DM_VERITY` bật dm-verity target, đây chính là code thực hiện việc verify từng block qua Merkle tree. Phải là `=y` nếu ta dùng `dm-mod.create` không có initramfs, vì kernel cần verity target sẵn sàng ngay lúc parse cmdline. Nếu dùng initramfs, có thể để `=m` rồi `modprobe dm-verity` trong init script, nhưng `=y` vẫn đơn giản hơn và được khuyến nghị.

File `meta-secure/recipes-kernel/linux/linux-yocto_%.bbappend`:

```bash
FILESEXTRAPATHS:prepend := "${THISDIR}/cfg:"
SRC_URI += "file://dm-verity.cfg"
```

Nếu kernel là `linux-ti-staging` thì ta cần đổi tên file thành `linux-ti-staging_%.bbappend`.

Build lại kernel:

```bash
bitbake virtual/kernel -c cleansstate
bitbake virtual/kernel
```

**Verify kernel config đã apply:**

```bash
# Kiểm tra trong build output
grep DM_VERITY tmp/work/*/linux-yocto/*/build/.config
# Phải thấy:
#   CONFIG_DM_VERITY=y
#   CONFIG_DM_INIT=y

grep BLK_DEV_DM tmp/work/*/linux-yocto/*/build/.config
# Phải thấy:
#   CONFIG_BLK_DEV_DM=y
```

Nếu thấy `=m` hoặc `is not set` thì kernel config fragment chưa apply đúng. Nguyên nhân phổ biến là `FILESEXTRAPATHS` sai path, hoặc bbappend filename không match với kernel recipe.

Sau đó, build full image và boot lại.

**Verify trên device:**

```bash
# SSH hoặc serial console vào BBB
zcat /proc/config.gz | grep DM_VERITY
# Phải thấy: CONFIG_DM_VERITY=y

ls /sys/module/dm_mod
# Phải tồn tại (dm-mod loaded)

dmsetup targets
# Phải liệt kê "verity" trong danh sách targets
```

### 7.3. Tạo file bbclass generate hash tree

Yocto build image theo pipeline:

```
bitbake core-image-minimal
  │
  ├── do_rootfs()
  │   └── Cài package vào rootfs directory
  │
  ├── do_image()
  │   └── Tạo rootfs.ext4 từ rootfs directory
  │
  ├── do_image_complete()
  │   └── IMAGE_POSTPROCESS_COMMAND chạy ở đây ◄── hook vào chỗ này
  │       │
  │       └── Rootfs image đã tồn tại, chưa deploy
  │           -> Chạy veritysetup format ở đây
  │
  └── Deploy các artifact vào deploy dir
```

`IMAGE_POSTPROCESS_COMMAND` là danh sách các shell function được chạy sau khi image đã build xong. Ta sẽ thêm 1 function vào đây để gọi `veritysetup format`.

File `classes/dm-verity-img.bbclass`:

```bash
# -----------------------------------------------------------------------------
# Khai báo dependencies
# -----------------------------------------------------------------------------

# cryptsetup-native: cung cấp veritysetup chạy trên host machine (build machine)
# Phân biệt:
#   - cryptsetup-native: chạy trên host lúc build
#   - cryptsetup: chạy trên target lúc runtime
DEPENDS += "cryptsetup-native"

# coreutils-native: cần stat, dd, wc... (thường đã có sẵn)
# util-linux-native: cần cho blockdev nếu dùng block device thay vì file
DEPENDS += "coreutils-native util-linux-native"

# -----------------------------------------------------------------------------
# Biến cấu hình — user có thể override trong local.conf hoặc trong image recipe
# -----------------------------------------------------------------------------

# Thuật toán hash
DM_VERITY_HASH_ALGORITHM ?= "sha256"

# Block size cho data và hash
DM_VERITY_DATA_BLOCK_SIZE ?= "4096"
DM_VERITY_HASH_BLOCK_SIZE ?= "4096"

# Giá trị salt để trống -> veritysetup tự generate random salt mỗi lần build
DM_VERITY_SALT ?= ""

# Loại image rootfs
DM_VERITY_IMAGE_TYPE ?= "ext4"

# Device paths trên TARGET
DM_VERITY_DATA_DEV ?= "/dev/mmcblk1p2"
DM_VERITY_HASH_DEV ?= "/dev/mmcblk1p3"

# Tên device-mapper device trên target
DM_VERITY_DEV_NAME ?= "rootfs-verity"

# Error behavior khi verify fail
DM_VERITY_ERROR_BEHAVIOR ?= "restart"

# Tự động inject root hash
# "cmdline" = inject vào kernel cmdline qua dm-mod.create
# "initramfs" = inject vào init script trong initramfs
DM_VERITY_MODE ?= "cmdline"

# Đảm bảo IMAGE_FSTYPES bao gồm ext4
IMAGE_FSTYPES += "${DM_VERITY_IMAGE_TYPE}"

# Thư mục output cho verity artifacts
DM_VERITY_DEPLOYDIR ?= "${IMGDEPLOYDIR}"

# -----------------------------------------------------------------------------
# Hàm helper nội bộ
# -----------------------------------------------------------------------------

# Hàm parse output của veritysetup format
# Input: raw output text từ veritysetup
# Output: set các biến shell (ROOT_HASH, SALT, DATA_BLOCKS, etc.)

dm_verity_parse_output() {
    local VERITY_OUTPUT="$1"

    # veritysetup format output trông như thế này:
    # ─────────────────────────────────────────────
    # VERITY header information for /path/to/hash
    # UUID:            a1b2c3d4-5678-9abc-def0-123456789abc
    # Hash type:       1
    # Data blocks:     4096
    # Data block size: 4096
    # Hash block size: 4096
    # Hash algorithm:  sha256
    # Salt:            aaf2d04e9c5bef0a1acda78bce7a4c8751a06e3b
    # Root hash:       e5b1c4f28a3d7e9f...0a88
    # ─────────────────────────────────────────────

    # Parse từng trường bằng grep + awk
    # awk '{print $NF}' lấy field cuối cùng trên mỗi dòng
    ROOT_HASH=$(echo "${VERITY_OUTPUT}" | grep "^Root hash:" | awk '{print $NF}')
    SALT=$(echo "${VERITY_OUTPUT}" | grep "^Salt:" | awk '{print $NF}')
    DATA_BLOCKS=$(echo "${VERITY_OUTPUT}" | grep "^Data blocks:" | awk '{print $NF}')
    HASH_TYPE=$(echo "${VERITY_OUTPUT}" | grep "^Hash type:" | awk '{print $NF}')
    UUID=$(echo "${VERITY_OUTPUT}" | grep "^UUID:" | awk '{print $NF}')
    HASH_ALGORITHM=$(echo "${VERITY_OUTPUT}" | grep "^Hash algorithm:" | awk '{print $NF}')

    # Validate root hash format: phải là hex string
    # SHA-256 -> 64 hex chars, SHA-512 -> 128 hex chars
    local HASH_LEN=${#ROOT_HASH}
    case "${DM_VERITY_HASH_ALGORITHM}" in
        sha256)
            if [ "${HASH_LEN}" -ne 64 ]; then
                bbfatal "dm-verity: Invalid root hash length ${HASH_LEN} (expected 64 for SHA-256)"
            fi
            ;;
        sha512)
            if [ "${HASH_LEN}" -ne 128 ]; then
                bbfatal "dm-verity: Invalid root hash length ${HASH_LEN} (expected 128 for SHA-512)"
            fi
            ;;
    esac

    bbnote "dm-verity: Parsed successfully:"
    bbnote "  Root hash:   ${ROOT_HASH}"
    bbnote "  Salt:        ${SALT}"
    bbnote "  Data blocks: ${DATA_BLOCKS}"
    bbnote "  Hash type:   ${HASH_TYPE}"
    bbnote "  UUID:        ${UUID}"
}

# Hàm tạo kernel cmdline dm-mod.create string
dm_verity_build_cmdline() {
    local DATA_SECTORS="$1"

    DM_TABLE="${DM_VERITY_DEV_NAME},,,ro,0 ${DATA_SECTORS} verity 1 \
${DM_VERITY_DATA_DEV} ${DM_VERITY_HASH_DEV} \
${DM_VERITY_DATA_BLOCK_SIZE} ${DM_VERITY_HASH_BLOCK_SIZE} \
${DATA_BLOCKS} 0 \
${DM_VERITY_HASH_ALGORITHM} ${ROOT_HASH} ${SALT}"

    # Optional params cho error behavior
    if [ "${DM_VERITY_ERROR_BEHAVIOR}" != "eio" ]; then
        case "${DM_VERITY_ERROR_BEHAVIOR}" in
            restart)
                DM_TABLE="${DM_TABLE} 1 restart_on_corruption"
                ;;
            panic)
                DM_TABLE="${DM_TABLE} 1 panic_on_corruption"
                ;;
            logging)
                DM_TABLE="${DM_TABLE} 1 ignore_corruption"
                ;;
        esac
    fi

    CMDLINE="root=/dev/dm-0 ro rootwait dm-mod.create=\"${DM_TABLE}\""

    bbnote "dm-verity: Built cmdline: ${CMDLINE}"
}

# -----------------------------------------------------------------------------
# Hàm chính — verity_setup
# -----------------------------------------------------------------------------

verity_setup() {
    # ================================================================
    # Bước 1: Xác định đường dẫn file
    # ================================================================

    local IMAGE_FILE="${IMGDEPLOYDIR}/${IMAGE_NAME}.rootfs.${DM_VERITY_IMAGE_TYPE}"
    local HASH_FILE="${IMGDEPLOYDIR}/${IMAGE_NAME}.rootfs.hashtree"
    local ENV_FILE="${IMGDEPLOYDIR}/${IMAGE_NAME}.verity.env"
    local CMDLINE_FILE="${IMGDEPLOYDIR}/${IMAGE_NAME}.verity.cmdline"

    bbnote "dm-verity: Generating hash tree for ${IMAGE_FILE}"

    # ================================================================
    # Bước 2: Chạy veritysetup format, tạo hash tree
    # ================================================================

    local VERITY_OUTPUT=$(veritysetup format \
        "${IMAGE_FILE}" \
        "${HASH_FILE}" \
        --data-block-size=${DM_VERITY_DATA_BLOCK_SIZE} \
        --hash-block-size=${DM_VERITY_HASH_BLOCK_SIZE} \
        --hash=${DM_VERITY_HASH_ALGORITHM} \
        ${@'--salt=${DM_VERITY_SALT}' if d.getVar('DM_VERITY_SALT') else ''}
        2>&1)

    local VERITY_EXIT_CODE=$?

    if [ ${VERITY_EXIT_CODE} -ne 0 ]; then
        bbfatal "dm-verity: veritysetup format failed (exit code: ${VERITY_EXIT_CODE})\n\
        Output: ${VERITY_OUTPUT}"
    fi

    bbnote "dm-verity: veritysetup format completed successfully"

    # ================================================================
    # Bước 3: Parse output để lấy root hash và metadata
    # ================================================================

    dm_verity_parse_output "${VERITY_OUTPUT}"

    # ================================================================
    # Bước 4: Tính các giá trị
    # ================================================================

    # Data sectors
    local IMAGE_SIZE=$(stat -c %s "${IMAGE_FILE}")
    local DATA_SECTORS=$((IMAGE_SIZE / 512))

    # Hash image size
    local HASH_SIZE=$(stat -c %s "${HASH_FILE}")

    # ================================================================
    # Bước 5: Ghi environment file:
    # Lưu tất cả vào env file để các recipe khác sử dụng
    # ================================================================
    cat > "${ENV_FILE}" << EOF
DM_VERITY_ROOT_HASH=${ROOT_HASH}
DM_VERITY_SALT=${SALT}
DM_VERITY_UUID=${UUID}
DM_VERITY_HASH_ALGORITHM=${DM_VERITY_HASH_ALGORITHM}
DM_VERITY_DATA_BLOCKS=${DATA_BLOCKS}
DM_VERITY_DATA_SECTORS=${DATA_SECTORS}
DM_VERITY_DATA_SIZE=${IMAGE_SIZE}
DM_VERITY_DATA_BLOCK_SIZE=${DM_VERITY_DATA_BLOCK_SIZE}
DM_VERITY_HASH_SIZE=${HASH_SIZE}
DM_VERITY_HASH_BLOCK_SIZE=${DM_VERITY_HASH_BLOCK_SIZE}
DM_VERITY_DATA_DEV=${DM_VERITY_DATA_DEV}
DM_VERITY_HASH_DEV=${DM_VERITY_HASH_DEV}
DM_VERITY_DEV_NAME=${DM_VERITY_DEV_NAME}
DM_VERITY_ERROR_BEHAVIOR=${DM_VERITY_ERROR_BEHAVIOR}
EOF

    # ================================================================
    # Bước 6: Tạo kernel cmdline
    # ================================================================
    dm_verity_build_cmdline "${DATA_SECTORS}"
    bbnote "dm-verity: Cmdline written to ${CMDLINE_FILE}"

    # ================================================================
    # Bước 7: Verify hash tree bằng cách chạy veritysetup verify
    #
    # Bước này không bắt buộc nhưng rất khuyến nghi.
    # Chạy veritysetup verify để đảm bảo hash tree vừa tạo
    # thực sự khớp với data image. Phát hiện lỗi ngay lúc build
    # thay vì lúc boot trên target (khi đó debug rất khó).
    # ================================================================

    bbnote "dm-verity: Verifying hash tree integrity..."

    local VERIFY_OUTPUT
    VERIFY_OUTPUT=$(veritysetup verify \
        "${IMAGE_FILE}" \
        "${HASH_FILE}" \
        "${ROOT_HASH}" \
        --hash=${DM_VERITY_HASH_ALGORITHM} \
        --data-block-size=${DM_VERITY_DATA_BLOCK_SIZE} \
        --hash-block-size=${DM_VERITY_HASH_BLOCK_SIZE} \
        --salt="${SALT}" \
        2>&1)
    
    local VERIFY_EXIT_CODE=$?

    if [ ${VERIFY_EXIT_CODE} -ne 0 ]; then
        bbfatal "dm-verity: Hash tree verification FAILED!\n\
        Output: ${VERIFY_OUTPUT}"
    fi

    bbnote "dm-verity: Hash tree verification PASSED"
}

# -----------------------------------------------------------------------------
# Hook vào Yocto build system
# -----------------------------------------------------------------------------

# Hook vào sau khi rootfs image được tạo
IMAGE_POSTPROCESS_COMMAND += "verity_setup;"

# Đảm bảo rootfs là read-only
IMAGE_FEATURES += "read-only-rootfs"
```

:::warning Tại sao cần file .verity.env?
File này là nguyên liệu để các task khác trong build pipeline lấy từng giá trị riêng lẻ và sử dụng theo cách riêng của chúng. Ví dụ:
- Task build FIT image chỉ cần root hash và salt để inject vào `.its` template.
- Task build initramfs cũng cần root hash và salt để inject vào init script.
- Task tạo WKS image có thể cần DATA_BLOCKS để tính kích thước partition.
- Script CI/CD có thể cần UUID để tracking, hoặc root hash để ghi vào manifest.
:::

:::warning Tại sao cần file .verity.cmdline?
File này chứa một chuỗi cmdline đã ghép sẵn, đúng format mà kernel yêu cầu. Nó được dùng cho trường hợp chỉ cần gán thẳng vào bootargs mà không cần tự ghép chuỗi. Ví dụ, uboot environment đọc trực tiếp:

```bash
BOOTARGS=$(cat ${IMGDEPLOYDIR}/${IMAGE_NAME}.verity.cmdline)
```
:::

Để sử dụng file này, thì ta cần thêm trong `local.conf` hoặc image recipe như sau:

```bash
inherit dm-verity-img

# Override biến nếu cần
DM_VERITY_HASH_ALGORITHM = "sha256"
DM_VERITY_ERROR_BEHAVIOR = "restart"
DM_VERITY_DATA_DEV = "/dev/mmcblk1p2"
DM_VERITY_HASH_DEV = "/dev/mmcblk1p3"
```

Sau khi build xong, có thể verify trên host machine trước khi flash lên BBB:

```bash
# Vào build directory
cd tmp/deploy/images/<machine>/

# 1. Source env file
source core-image-verity-<machine>-*.verity.env
echo "Root hash: ${DM_VERITY_ROOT_HASH}"

# 2. Xem cmdline sẽ truyền cho kernel
cat core-image-verity-<machine>-*.verity.cmdline

# 3. Verify lại bằng tay
veritysetup verify \
    core-image-verity-<machine>-*.rootfs.ext4 \
    core-image-verity-<machine>-*.rootfs.verity \
    ${DM_VERITY_ROOT_HASH}
# Output: "Verification passed" nếu OK

# 4. Kiểm tra hash tree size
ls -lh core-image-verity-<machine>-*.rootfs.verity
# Ví dụ: 132K (cho rootfs 16MB)

# 5. Test tamper detection: sửa 1 byte trong rootfs rồi verify lại
cp core-image-verity-<machine>-*.rootfs.ext4 /tmp/tampered.ext4
printf '\x00' | dd of=/tmp/tampered.ext4 bs=1 seek=1000 count=1 conv=notrunc
veritysetup verify \
    /tmp/tampered.ext4 \
    core-image-verity-<machine>-*.rootfs.verity \
    ${DM_VERITY_ROOT_HASH}
# Output: "Verification failed" — hash tree phát hiện thay đổi
```

### 7.4. Truyền root hash qua kernel cmdline

**Cách 1: Trực tiếp trong cmdline**

Đơn giản nhất, uboot set kernel command line lúc boot.

File `recipes-bsp/u-boot/u-boot_%.bbappend`:

```bash
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI += "file://verity-bootcmd.cfg"
```

File `verity-bootcmd.cfg`:

```conf
# Cách 1: Hardcode bootargs (root hash sẽ được inject lúc build)
CONFIG_BOOTARGS="console=ttyO0,115200n8 root=/dev/dm-0 ro rootwait dm-mod.create=\"rootfs-verity,,,ro,0 ${dm_verity_sectors} verity 1 /dev/mmcblk1p2 /dev/mmcblk1p3 4096 4096 ${dm_verity_blocks} 0 sha256 ${dm_verity_root_hash} ${dm_verity_salt}\""

# Cách 2: Đọc verity args từ file trên boot partition
CONFIG_USE_BOOTARGS=y
```

Hạn chế: root hash phải được nhúng cứng vào U-Boot environment hoặc đọc từ file.

**Cách 2: Nhúng vào FIT image**

Root hash nằm trong bootargs bên trong FIT image .its file. Khi FIT image được ký, root hash cũng được bảo vệ bởi chữ ký.

File `fitImage-verity.its.in`:

```
/dts-v1/;

/ {
    description = "BBB Verified Boot Image";
    #address-cells = <1>;

    images {
        kernel {
            description = "Linux Kernel";
            data = /incbin/("zImage");
            type = "kernel";
            arch = "arm";
            os = "linux";
            compression = "none";
            load = <0x82000000>;
            entry = <0x82000000>;
            hash-1 { algo = "sha256"; };
        };

        fdt {
            description = "Device Tree";
            data = /incbin/("am335x-boneblack.dtb");
            type = "flat_dt";
            arch = "arm";
            compression = "none";
            load = <0x88000000>;
            hash-1 { algo = "sha256"; };
        };

        ramdisk {
            description = "Initramfs";
            data = /incbin/("initramfs.cpio.gz");
            type = "ramdisk";
            arch = "arm";
            os = "linux";
            compression = "gzip";
            hash-1 { algo = "sha256"; };
        };
    };

    configurations {
        default = "verity-boot";

        verity-boot {
            description = "Verified Boot Configuration";
            kernel = "kernel";
            fdt = "fdt";
            ramdisk = "ramdisk";

            /* Root hash được nhúng vào đây */
            bootargs = "console=ttyO0,115200n8 root=/dev/dm-0 ro rootwait dm-mod.create=\"rootfs-verity,,,ro,0 @@DATA_SECTORS@@ verity 1 /dev/mmcblk1p2 /dev/mmcblk1p3 4096 4096 @@DATA_BLOCKS@@ 0 sha256 @@ROOT_HASH@@ @@SALT@@\"";

            signature {
                algo = "sha256,rsa2048";
                key-name-hint = "dev-key";
                sign-images = "kernel", "fdt", "ramdisk";
            };
        };
    };
};
```

Script để thay thế placeholder và ký FIT image. File `scripts/build-fit-verity.sh`:

```bash
#!/bin/bash

source ${IMGDEPLOYDIR}/${IMAGE_NAME}.verity.env

# Thay thế placeholder trong .its template
sed -e "s|@@ROOT_HASH@@|${DM_VERITY_ROOT_HASH}|g" \
    -e "s|@@SALT@@|${DM_VERITY_SALT}|g" \
    -e "s|@@DATA_SECTORS@@|${DM_VERITY_DATA_SECTORS}|g" \
    -e "s|@@DATA_BLOCKS@@|${DM_VERITY_DATA_BLOCKS}|g" \
    fitImage-verity.its.in > fitImage-verity.its

# Tạo và ký FIT image
mkimage -f fitImage-verity.its \
        -k ${SIGNING_KEY_DIR} \
        -K u-boot.dtb \
        -r \
        fitImage
```

Vấn đề: root hash thay đổi mỗi khi rootfs image thay đổi. Cmdline nằm trong FIT image (đã signed), nên mỗi build phải: build rootfs -> tính hash -> inject vào cmdline -> build FIT -> ký FIT. Quy trình này cần automation.

**Cách 3: Qua initramfs**

Nếu cmdline quá dài hoặc ta cần logic phức tạp hơn, dùng initramfs:

File init script `recipes-core/initramfs-verity/files/init.sh`:

```bash
#!/bin/sh

# Mount pseudo filesystems
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev

# Đợi eMMC device sẵn sàng
while [ ! -b /dev/mmcblk0p2 ]; do
    sleep 0.1
done

# Root hash và salt được inject lúc build time
DATA_DEV="/dev/mmcblk1p2"
HASH_DEV="/dev/mmcblk1p3"
ROOT_HASH="@@ROOT_HASH@@"
SALT="@@SALT@@"

# Setup dm-verity
veritysetup open \
    "${DATA_DEV}" \         # rootfs partition
    "${HASH_DEV}" \         # hash tree partition
    "${ROOT_HASH}"
    --salt="${SALT}" \
    --hash=sha256 \
    vroot                   # dm device name → /dev/mapper/vroot

# Kiểm tra verify setup thành công
if [ $? -ne 0 ]; then
    echo "ERROR: dm-verity setup failed!"
    exec /bin/sh  # drop to shell hoặc reboot
fi

echo "dm-verity activated successfully"

# Mount verified rootfs
mount -o ro /dev/mapper/vroot /mnt/vroot

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to mount verified rootfs"
    exec /bin/sh
fi

# Cleanup
umount /proc /sys /dev 2>/dev/null

echo "Switching to verified rootfs..."
exec switch_root /mnt/vroot /sbin/init
```

File `recipes-core/initramfs-verity/initramfs-verity_1.0.bb`:

```bash
SUMMARY = "Initramfs with dm-verity setup"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=..."

SRC_URI = "file://init.sh"

RDEPENDS:${PN} = "cryptsetup busybox"

do_install() {
    install -d ${D}
    install -m 0755 ${WORKDIR}/init.sh ${D}/init

    # Install required binaries
    install -d ${D}/sbin ${D}/usr/sbin ${D}/dev ${D}/proc ${D}/sys ${D}/mnt
}

FILES:${PN} = "/*"
```

Trong bbclass, thêm bước inject root hash vào init script:

```python
# Trong dm-verity-img.bbclass, thêm vào verity_create_image():

    # Inject root hash vào initramfs init script
    INITRAMFS_INIT="${IMGDEPLOYDIR}/initramfs-verity/init"
    sed -i "s|@@ROOT_HASH@@|${ROOT_HASH}|g" "${INITRAMFS_INIT}"
    sed -i "s|@@SALT@@|${SALT}|g" "${INITRAMFS_INIT}"
    sed -i "s|@@DATA_BLOCKS@@|${DATA_BLOCKS}|g" "${INITRAMFS_INIT}"
```

Initramfs sau đó được đóng gói vào FIT image (node ramdisk), cùng được ký. Attacker không thể sửa root hash trong init script vì initramfs nằm trong signed FIT.

### 7.5. Image recipe tổng hợp

File `recipes-images/core-image-verity.bb:`

```
SUMMARY = "BBB image with dm-verity"
LICENSE = "MIT"

inherit core-image

IMAGE_FEATURES:append = "read-only-rootfs"

IMAGE_INSTALL:append = " \
    kernel-modules \
    cryptsetup \
"

# Bật dm-verity post-processing
inherit dm-verity-img

# Initramfs cho dm-verity setup
INITRAMFS_IMAGE = "initramfs-verity"
INITRAMFS_IMAGE_BUNDLE = "1"

# FIT image configuration
KERNEL_IMAGETYPE = "fitImage"
KERNEL_CLASSES += "kernel-fitimage"

# Signing
UBOOT_SIGN_ENABLE = "1"
UBOOT_SIGN_KEYDIR = "${TOPDIR}/keys"
UBOOT_SIGN_KEYNAME = "dev"
UBOOT_MKIMAGE_DTCOPTS = "-I dts -O dtb -p 2000"

FIT_SIGN_ALG = "rsa2048"
FIT_HASH_ALG = "sha256"
```

### 7.6. Partition layout

File `wic/bbb-dm-verity.wks`:

```
part /boot --source bootimg-partition --fstype=vfat --label boot --active --align 4096 --size 64M
part / --source rootfs --fstype=ext4 --label rootfs --align 4096 --size 512M --uuid=root-part
part --source rawcopy --sourceparams="file=rootfs.hashtree" --label hashtree --align 4096 --size 16M
part /data --fstype=ext4 --label data --align 4096 --size 256M
```

### 7.7. Cấu trúc thư mục

```
meta-secure/
├── conf/
│   └── layer.conf                      <- Metadata, depends meta-yocto
├── classes/
│   └── dm-verity-img.bbclass           <- Sinh hash tree sau khi rootfs build xong
├── recipes-core/
│   └── initrdscripts/
│       ├── initramfs-dm-verity_1.0.bb
│       └── files/
│           └── init.sh                 <- Initramfs init: dmsetup + switch_root
├── recipes-kernel/
│   └── linux/
│       ├── linux-yocto_%.bbappend      <- Bật dm-verity trong kernel config
│       └── files/
│           └── dm-verity.cfg           <- Kernel config fragment
├── wic/
│   └── bbb-dm-verity.wks               <- Partition layout
└── recipes-images/
    └── core-image-verity.bb            <- Image recipe
```

## 7.8. Build flow tổng hợp

Thứ tự build trong Yocto:

```
bitbake core-image-verity
         │
         ▼
    do_rootfs
    Tạo rootfs với tất cả packages
         │
         ▼
    do_image_ext4
    Đóng gói thành rootfs.ext4
         │
         ▼
    IMAGE_POSTPROCESS_COMMAND: verity_setup() (từ dm-verity-img.bbclass)
      ├── veritysetup format rootfs.ext4 -> rootfs.verity
      ├── Extract root hash, salt, data blocks
      ├── Ghi ra .verity.env
      └── Ghi ra .verity.cmdline
         │
         ▼
    do_verity_fit
      ├── Đọc .verity.env
      ├── Inject root hash vào init script / .its file
      ├── mkimage -f ... -k keys/ → fitImage (đã ký)
      └── Public key nhúng vào u-boot.dtb
         │
         ▼
    do_image_wic
      ├── p1: boot/ (MLO, u-boot.img, fitImage)
      ├── p2: rootfs.ext4 (data, read-only)
      ├── p3: rootfs.verity (hash tree)
      └── p4: data partition (writable, ext4)
         │
         ▼
    Output: bbb-verity.wic.gz
    Flash lên eMMC/SD card
```

## 8. Một số điểm quan trọng khác

### 8.1. Error handling policy

Giả sử kẻ tấn công sửa đổi block #300 trên eMMC:

```
dm-verity đọc block #300 (đã bị sửa)
    -> computed_hash = SHA-256(salt ∥ <dữ liệu bị sửa>)
    -> So sánh với stored_hash_L0 trong hash partition
    -> Không khớp
    -> dm-verity dừng ngay, không cần kiểm tra lên level cao hơn
```

Khi dm-verity phát hiện hash mismatch, tùy vào cấu hình `error_behavior` thì nó sẽ xử lý khác nhau:

```
error_behavior=EIO (default):
  -> Block read trả -EIO
  -> Process nhận error
  -> System vẫn chạy, nhưng file bị corrupt không đọc được

error_behavior=panic:
  -> kernel_panic("dm-verity failure")
  -> Hệ thống treo hoàn toàn

error_behavior=restart:
  -> Kernel gọi emergency_restart()
  -> Kernel reboot ngay lập tức

error_behavior=logging:
  -> Ghi log cảnh báo vào dmesg
  -> Vẫn trả dữ liệu bị sửa cho ứng dụng
```

### 8.2. Hạn chế và trace off cần biết

- Rootfs bắt buộc read-only: Bất kỳ write nào vào rootfs partition đều phá vỡ hash tree. Mọi dữ liệu cần ghi ví dụ như config runtime, logs, sensor data, OTA staging,...phải nằm trên partition riêng.
- Performance: Mỗi block read cần đọc thêm hash tree để thực hiện verify.
- Không bảo vệ writable partition. dm-verity chỉ bảo vệ rootfs. Partition data cần cơ chế riêng, thường dùng dm-crypt kết hợp dm-integrity hoặc đơn giản hơn là HMAC-based file integrity checks ở application layer.
- Không detect tampering realtime. dm-verity chỉ verify khi block được đọc. Nếu attacker sửa block chưa từng được đọc kể từ lần boot hiện tại, hệ thống không biết cho đến khi có process đọc block đó.