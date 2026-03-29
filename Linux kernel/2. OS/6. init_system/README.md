# Init system

## Khái niệm

Init system là process đầu tiên được kernel chạy ngay sau khi kernel hoàn tất quá trình boot. Trong linux, process này luôn có PID là 1.

Sau khi kernel khởi động xong, init system thực hiện các công việc sau:
- Mount các pseudo file system quan trọng: `/proc`, `/sys`, `/dev`,...
- Khởi động các background process cần thiết như network, sshd, udevd,...
- Quản lý process.

Một số init system phổ biến: SysInit, systemd, busybox,...

:::tip Ngắn ngọn
Init system là cha của mọi process trong linux.
:::

:::tip
Mọi process chết mà không có parent thì kernel sẽ chuyển về cho init system xử lý.
:::

:::tip
Kernel sẽ tìm đường dẫn đến init system theo thứ tự: `/sbin/init`, `/etc/init`, `/bin/init`, `/bin/sh`.
:::

## Daemon

Daemon là một background process được tách khỏi terminal, thường hoạt động liên tục trong hệ thống để cung cấp một dịch vụ nào đó. Init system sẽ chịu trách nhiệm quản lý vòng đời của daemon.

:::tip
Foreground process là các process được kernel gắn với terminal để nhận input và signal. 
:::

Một daemon được tạo ra thông qua các bước được gọi là daemonize như sau:
- `fork()` → parent exit → process không còn là leader của terminal
- `setsid()` → tách session, không còn controlling terminal
- `fork()` lần 2 (option) → để đảm bảo không thể có controlling terminal
- `chdir("/")` → tránh khóa filesystem
- `umask()` → set umask phù hợp
- đóng tất cả file descriptor (0, 1, 2 → tty)
- ghi PID file (option)

## Systemd

### Service

Service là một thành phần chạy nền mà PID 1 chịu trách nhiệm khởi động, giám sát và quản lý.

Service có thể là daemon, chương trình chạy một lần, hoặc bất kỳ chương trình nào mà init system quản lý vòng đời của nó.

Vòng đời cùa một service như sau:
- start
- stop
- restart
- reload
- enable/disable
- monitor
- reload configuration

### Cấu trúc một file .service

Một file `.service` là file text kiểu INI, thường nằm ở:
- `/etc/systemd/system/` (cho custom service của bạn)
- `/usr/lib/systemd/system/` hoặc `/lib/systemd/system/` (của distro)

Cấu trúc của một file `.service` gốm các section, mỗi section phục vụ một vai trò rõ ràng:

```
[Unit]
...

[Service]
...

[Install]
...
```

**Section `[Unit]`**

Section này cho biết unit này tồn tại trong hệ thống với vai trò gì, và phụ thuộc vào những unit nào.

Các key thường dùng nhất:

```
[Unit]
Description=My demo service
Documentation=man:my_app(1)
After=network.target
Requires=network.target
```

- `Description=`: mô tả ngắn, hiện trong systemctl status.
- `Documentation=`: trỏ tới man page hoặc URL docs.
- `After=`/`Before=`: quan hệ thứ tự, không bắt buộc service đó phải tồn tại.
  - After=B → A phải start sau B
  - Before=C → A phải start trước C
- `Requires=`: quan hệ phụ thuộc, nếu B fail thì A cũng fail.
- `Want=`: A muốn B chạy, nếu B fail thì A vẫn chạy được.

**Section `[service]`**

Section `[service]` cho systemd biết phải fork/exec process này như thế nào và theo dõi nó ra sao.

Key `Type` quyết định systemd mong muốn process chạy như thế nào.

Các giá trị hay dùng:
- simple: Service được coi là active ngay khi systemd fork/exec xong process.
- forking: Service được coi là active khi process cha exit, còn process con tiếp tục chạy. Đây chính là daemon kiểu SysVinit.
- oneshot: Service chạy một lần rồi thoát.

`ExecStart`/`ExecStop`/`ExecReload` là các lệnh mà systemd chủ động thực thi, để điều khiển vòng đời process của service.

- `ExecStart` là cách mà service thực hiện gọi chương trình khi start.
- `ExecStop` là cách mà service thực hiện gọi chương trình khi stop.
- `ExecReload` là cách mà service thực hiện gọi chương trình khi reload.

### Sử dụng file .service

- Copy vào folder `/etc/systemd/system/`

- Reload systemd để nó đọc file mới:

  ```bash
  sudo systemctl daemon-reload
  ```

- Enable để tự khởi động cùng hệ thống:
  
  ```bash
  sudo systemctl enable sensor-daemon.service
  ```

- Start & kiểm tra trạng thái:
  
  ```bash
  sudo systemctl start sensor-daemon.service
  sudo systemctl status sensor-daemon.service
  ```

- Xem log:
  
  ```bash
  journalctl -u sensor-daemon.service -f
  ```