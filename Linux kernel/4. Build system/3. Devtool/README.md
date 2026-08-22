# Công cụ Devtool

## 1. Devtool là gì?

`devtool` là công cụ dòng lệnh đi kèm với Yocto, được thiết kế để giúp developer làm việc với recipe nhanh hơn mà không cần phải chỉnh sửa trực tiếp vào layer gốc hay chạy lại toàn bộ quá trình build.

Nó nằm trong extensible SDK (eSDK) và cả trong môi trường Yocto build bình thường.

## 2. Workspace layer

Khi dùng `devtool`, mọi thứ được quản lý trong workspace layer:

```
build/
└── workspace/
    ├── conf/
    │   └── layer.conf
    ├── sources/
    │   └── myapp/          <- source code được extract ra đây
    │       ├── src/
    │       └── CMakeLists.txt
    └── recipes/
        └── myapp/
            └── myapp.bb    <- recipe tạm thời
```

Workspace này tự động được thêm vào `bblayers.conf` với priority cao nhất, nên nó override layer gốc.

## 3. Các lệnh chính

### 3.1. `devtool modify` - chỉnh sửa recipe có sẵn

```bash
devtool modify myapp
```

Lệnh này:
- Extract source code của `myapp` ra `workspace/sources/myapp/`
- Apply tất cả patch có sẵn
- Tạo recipe tạm trong workspace
- Init git repo trong thư mục source

```bash
# Sau đó vào sửa code thoải mái
cd build/workspace/sources/myapp/
vim src/main.c
```

### 3.2. `devtool build` - build sau khi sửa

```bash
devtool build myapp
```

Chỉ rebuild những gì thay đổi, nhanh hơn bitbake nhiều.

### 3.3. `devtool deploy-target` - đẩy lên board

```bash
devtool deploy-target myapp root@192.168.1.100
```

Copy binary/lib mới nhất lên board qua SSH mà không cần flash lại image. Đây là bước tiết kiệm thời gian nhất.

```bash
# Sau khi deploy, SSH vào board test luôn
ssh root@192.168.1.100
/usr/bin/myapp
```

:::warning Điều kiện tiên quyết
Board phải có SSH server và được kết nối mạng với máy host. Phải thêm package `openssh` vào image.
:::

### 3.4. `devtool undeploy-target` - gỡ khỏi board

```bash
devtool undeploy-target myapp root@192.168.1.100
```

Xóa những file đã deploy, trả board về trạng thái ban đầu.

### 3.5. `devtool add` - tạo recipe mới từ source

```bash
# Từ thư mục source local
devtool add myapp /path/to/myapp-source

# Từ git repo
devtool add myapp https://github.com/example/myapp.git

# Từ tarball
devtool add myapp https://example.com/myapp-1.0.tar.gz
```

`devtool` sẽ tự động detect build system (CMake, autotools, meson...) và sinh ra recipe tạm trong workspace. Rất hữu ích khi thêm package mới.

### 3.6. `devtool finish` - đưa thay đổi về layer gốc

```bash
devtool finish myapp meta-mylayer
```

Lệnh này:
- Tạo patch từ những thay đổi mà ta đã commit trong git
- Cập nhật `SRC_URI` trong recipe gốc để thêm patch đó
- Xóa recipe tạm trong workspace

```
workspace/sources/myapp/  -> meta-mylayer/recipes-myapp/myapp/
                               ├── myapp_1.0.bb        (cập nhật SRC_URI)
                               └── files/
                                   └── fix-bug.patch   (patch mới)
```

### 3.7. `devtool reset` - hủy bỏ, quay về trạng thái ban đầu

```bash
devtool reset myapp
```

Xóa recipe tạm trong workspace, trả lại quyền kiểm soát cho layer gốc. Source code trong `workspace/sources/myapp/` vẫn còn nếu muốn giữ lại.

### 3.8. `devtool status` - xem đang modify recipe nào

```bash
devtool status

# Output:
# myapp: /home/user/build/workspace/sources/myapp
```

## 4. Vấn đề devtool giải quyết

### 4.1. Tình huống 1

Giả sử ta đang làm dự án thực tế:
- Board: Raspberry Pi 4 (ARM)
- Có một ứng dụng `myapp` đang chạy trên board bị lỗi
- Ta cần fix bug và test

**Workflow thông thường không có `devtool`:**

```bash
# 1. Tìm source code của myapp trong layer
# 2. Sửa code
vim meta-mylayer/recipes-myapp/myapp/files/main.c

# 3. Build lại toàn bộ
bitbake myapp

# 4. Build lại image
bitbake core-image-minimal

# 5. Flash image lên board
dd if=core-image-minimal.wic of=/dev/sdX

# 6. Boot board lên test
# 7. Phát hiện vẫn còn bug -> quay lại bước 2
```

Ta thấy rằng mỗi lần build xong ta lại phải flash lại image lên board, việc này rất mất thời gian.

**Workflow có `devtool`:**

```bash
# 1. Bắt đầu modify
devtool modify myapp

# 2. Vào sửa code
cd build/workspace/sources/myapp
vim src/main.c

# 3. Build thử
devtool build myapp

# 4. Deploy lên board test
devtool deploy-target myapp root@192.168.1.100

# 5. Test trên board
ssh root@192.168.1.100 "/usr/bin/myapp"

# 6. Commit thay đổi (bắt buộc nếu muốn devtool finish tạo patch)
git add .
git commit -m "fix issue: ..."

# 7. Nếu OK, đưa về layer chính thức
devtool finish myapp meta-mylayer

# 8. Nếu muốn hủy
devtool reset myapp
```

### 4.2. Tình huống 2

Board của ta đang chạy image `core-image-minimal`, image này không có ứng dụng `myapp`. Ta muốn thêm và test thử mà không cần build lại image.

```bash
# 1. Tạo recipe mới

# Từ source local
devtool add myapp /path/to/myapp-source

# Hoặc từ git
devtool add myapp https://github.com/example/myapp.git

# Hoặc từ tarball
devtool add myapp https://example.com/myapp-1.0.tar.gz

# 2. Build recipe
devtool build myapp

# 3. Deploy lên board test
devtool deploy-target myapp root@192.168.1.100

# 4. Test trên board
ssh root@192.168.1.100 "/usr/bin/myapp"

# 7. Nếu OK, đưa về layer chính thức
devtool finish myapp meta-mylayer

# 8. Nếu muốn hủy

# Gỡ khỏi board
devtool undeploy-target myapp root@192.168.1.100

# Xóa khỏi workspace
devtool reset myapp
```

:::warning Lưu ý quan trọng
Nếu `myapp` phụ thuộc vào thư viện mà image không có sẵn, ta sẽ gặp lỗi:

```bash
# Trên board
/usr/bin/myapp
# error while loading shared libraries: libfoo.so.1: cannot open shared object file
```

Lúc này cần deploy cả dependency:

```bash
# Trên máy host
devtool build libfoo
devtool deploy-target libfoo root@192.168.1.100

# Rồi mới deploy myapp
devtool deploy-target myapp root@192.168.1.100
```
:::