Giả sử ta đã cross-compile được file chạy cho ARM bằng một SDK Yocto hoặc toolchain và sysroot tự dựng. Mỗi lần sửa code, vòng lặp làm việc gồm: build, deploy lên board, đăng nhập SSH, đặt biến môi trường, chạy, xem log, và khi crash thì phải debug. Làm tay từng bước mất vài phút mỗi lần. Qt Creator gộp tất cả vào một nút **Run** (Ctrl+R) và một nút **Debug** (F5).

```
        Máy tính: Qt Creator                              BBB
+---------------------------------+   SSH / SFTP   +--------------------------+
| 1. Build bằng kit "BBB"         |                |                          |
| 2. Deploy file theo install() --+--------------->| /opt/deploy-demo/bin/... |
| 3. Run: chạy qua SSH -----------+--------------->| QT_QPA_PLATFORM=linuxfb  |
|    Application Output <---------+---- stdout ----|                          |
| 4. Debug: gdb (ARM) <-----------+-- cổng TCP ----| gdbserver                |
+---------------------------------+                +--------------------------+
```

Mọi giao tiếp giữa máy tính và board đi qua SSH. Board không cần cài Qt Creator, không cần compiler, chỉ cần thư viện Qt, SSH server, và `gdbserver` khi muốn debug.

## Chuẩn bị trên board

| Việc | Lệnh ví dụ (Debian trên board) |
|---|---|
| Máy chủ SSH đang chạy | `systemctl status ssh` |
| Cài gdbserver | `sudo apt install gdbserver` |
| Thư mục cài ghi được | `sudo mkdir -p /opt/deploy-demo && sudo chown debian: /opt/deploy-demo` |
| User có quyền với framebuffer, cảm ứng | `sudo usermod -aG video,input debian` |

Với image Yocto, thêm gdbserver vào image, ví dụ qua `EXTRA_IMAGE_FEATURES += "tools-debug"`.

## Đăng nhập SSH bằng khóa

Qt Creator đăng nhập board rất nhiều lần; dùng khóa SSH để không phải nhập mật khẩu:

```bash
ssh-keygen -t ed25519            # nếu chưa có khóa
ssh-copy-id debian@192.168.7.2
ssh debian@192.168.7.2 true      # không hỏi mật khẩu là được
```

## Cấu hình Qt Creator

Mọi mục nằm trong **Edit -> Preferences** (hoặc **Tools -> Options**):

1. **Devices -> Devices -> Add -> Generic Linux Device**: nhập địa chỉ IP, user, chọn file khóa SSH. Bấm **Test** để kiểm tra kết nối.
2. **Kits -> Compilers -> Add -> GCC**: thêm trình biên dịch C và C++ cho ARM (ví dụ `arm-linux-gnueabihf-g++`, hoặc `arm-poky-linux-gnueabi-g++` của SDK Yocto).
3. **Kits -> Debuggers -> Add**: thêm gdb đọc được mã ARM: `gdb-multiarch` (gói `gdb-multiarch` của Ubuntu) hoặc gdb đi kèm SDK.
4. **Kits -> Qt Versions -> Add**: chọn `qmake` bản host của Qt cho board (với Qt tự build: `qmake` trong thư mục host, ví dụ `~/bbb/qt5-host/bin/qmake`; với SDK Yocto: `qmake` trong thư mục `sysroots/x86_64-pokysdk-linux/usr/bin` của SDK).
5. **Kits -> Kits -> Add**: tạo kit "BBB" gồm **Device type** là Remote Linux Device, **Device** vừa tạo, **Sysroot**, trình biên dịch, debugger, Qt version. Ở **CMake Configuration**, thêm `CMAKE_TOOLCHAIN_FILE` trỏ tới file toolchain nếu dùng toolchain tự dựng.

:::tip Dùng SDK Yocto với Qt Creator
Cách đơn giản nhất là mở Qt Creator từ chính terminal đã nạp script môi trường của SDK: `source environment-setup-... && qtcreator &`. Qt Creator thừa hưởng toàn bộ biến môi trường, nên CMake tìm đúng trình biên dịch, sysroot và Qt của SDK. Trong kit, đặt `CMAKE_TOOLCHAIN_FILE` bằng `$OECORE_NATIVE_SYSROOT/usr/share/cmake/OEToolchainConfig.cmake`.
:::

## Deploy

Với project CMake, Qt Creator lấy danh sách file cần chép từ các lệnh `install()` trong `CMakeLists.txt`. Mục **Projects -> Run -> Deployment** hiển thị danh sách đó: file nguồn trên máy tính và đường dẫn đích trên board.

```cmake
install(TARGETS deploy-demo RUNTIME DESTINATION bin)
```

Đường dẫn đích bằng `CMAKE_INSTALL_PREFIX` cộng với `DESTINATION`. Ví dụ đặt prefix mặc định là `/opt/deploy-demo`, nên file chạy tới `/opt/deploy-demo/bin/deploy-demo`.

:::note Không có install() thì không có gì để deploy
Project thiếu lệnh `install()` vẫn build được, nhưng bước deploy không chép file nào lên board, và bước Run báo không tìm thấy file chạy trên thiết bị. Mọi project dự định chạy trên board nên có `install()` ngay từ đầu.
:::

## Chạy trên board

Trong **Projects -> Run** của kit BBB:

- **Environment**: thêm `QT_QPA_PLATFORM=linuxfb:fb=/dev/fb1`. Board không có X11, thiếu biến này ứng dụng sẽ không khởi động được.
- **Command line arguments**: tham số riêng của ứng dụng, ví dụ `--hw` để chọn phần cứng thật thay cho bản giả lập.

Nhấn Ctrl+R: Qt Creator build, deploy, chạy qua SSH. Mọi dòng `qDebug()` hiện trong khung **Application Output**.

## Debug từ xa

Khi debug trên máy tính, gdb vừa điều khiển chương trình vừa đọc thông tin debug (tên hàm, tên biến, số dòng) trong cùng một máy. Khi debug trên board, hai việc này được tách ra:

```
       Máy tính                                         BBB
+-----------------------------+                +----------------------+
| Qt Creator                  |                |                      |
|   giao diện debug           |                |                      |
|        |                    |                |                      |
| gdb-multiarch               |  lệnh debug    | gdbserver            |
|   đọc thông tin debug từ:   | <----- TCP --> |   điều khiển trực    |
|   - file build trên máy     |                |   tiếp chương trình  |
|   - thư viện trong sysroot  |                |   HmiApp             |
+-----------------------------+                +----------------------+
```

- `gdbserver` chạy trên board, rất nhẹ: nó chỉ dừng, chạy tiếp, đọc bộ nhớ của chương trình theo lệnh.
- `gdb-multiarch` chạy trên máy tính, làm phần việc nặng: đọc thông tin debug, hiểu mã nguồn, hiển thị biến. Nó tìm thông tin debug của các thư viện hệ thống trong sysroot, đây là lý do sysroot phải match với board.

Với Qt Creator, toàn bộ việc này diễn ra tự động khi ta nhấn Debug (F5) với kit BBB. Ta đặt breakpoint, chạy từng dòng, xem giá trị biến giống hệt như debug trên máy tính.
