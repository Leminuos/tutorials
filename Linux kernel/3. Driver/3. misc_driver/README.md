# Misc driver

## Tại sao Linux kernel phải theo template driver?

Khi ta muốn viết driver cho device nào đó thì ta cần phải biết được cách giao tiếp với nó. Tuy nhiên, có rất nhiều loại device, mỗi device lại có cách thức giao tiếp và vendor khác nhau. Ta không thể viết được hết cho các device này. Giả sử ta viết được hết thì trong tương lai thì vendor sẽ cập nhập lại giao thức kiểu khác -> Ta phải build lại kernel, rất phức tạp.

Vì vậy, tất cả device thuộc cùng một loại thì phải tuân theo một template driver.

Để làm được điều này, thì kernel sẽ cho ta một struct gồm các hàm callback. Ta phải đăng ký các hàm callback này khi viết driver -> Kernel chỉ quan tâm driver có đúng format API và trả kết quả đúng hay không, còn cách driver nói chuyện với hardware như thế nào là việc của developer.

Nếu viết driver mà không theo đúng template thì kernel sẽ không hiểu driver mà ta muốn viết thuộc loại nào.

## Sysfs

Sysfs là một virtual filesystem trong linux kernel, được dùng để user space có thể tương tác với các đối tượng trong kernel như device, driver, subsystem và bus.

Sysfs giúp user:
- Xem trạng thái của kernel, thiết bị, hoặc driver.
- Điều khiển cấu hình phần cứng khi runtime
- Debug

| Loại đối tượng   | Cấu trúc đại diện trong kernel  | Mô tả                                             |
| ---------------- | ------------------------------- | ------------------------------------------------- |
| **Device**       | `struct device`                 | Thiết bị vật lý hoặc logic                        |
| **Driver**       | `struct device_driver`          | Trình điều khiển cho thiết bị                     |
| **Bus**          | `struct bus_type`               | Kiểu bus kết nối (I2C, SPI, platform, PCI, …)     |
| **Class**        | `struct class`                  | Nhóm logic của các thiết bị (net, leds, input, …) |
| **Kobject/Kset** | `struct kobject`, `struct kset` | Hạ tầng quản lý sysfs                             |

Tất cả mọi đối tượng trong `/sys` đều bắt đầu từ `struct kobject`:
- Khi kernel tạo ra một `kobject`, sysfs sẽ tự động sinh ra một thư mục tương ứng trong `/sys`.
- Các file bên trong thư mục đó được gọi là attributes,
- và được ánh xạ với các callback hàm `show`() và `store`() trong kernel.

👉 Kết luận:
- kobject → tạo thư mục.
- attribute → tạo file.
- show()/store() → quy định cách đọc/ghi file đó.

### Cơ chế show/store hoạt động như thế nào

Khi user đọc file bằng `cat`, sysfs gọi:

```
show() → ghi dữ liệu vào buffer (buf)
```

Khi user ghi file bằng `echo`, sysfs gọi:

```
store() → nhận buffer (buf) chứa dữ liệu
```

Ngoài ra:
- Kích thước buffer mặc định ~ PAGE_SIZE (4KB).
- Giá trị trả về là số byte đã đọc/ghi.

### Quản lý nhóm attribute

Khi có nhiều file, thay vì gọi `sysfs_create_file` nhiều lần, ta dùng nhóm:

```c
static struct attribute *my_attrs[] = {
    &my_attribute1.attr,
    &my_attribute2.attr,
    NULL,
};

static const struct attribute_group my_attr_group = {
    .attrs = my_attrs,
};
```

Rồi đăng ký:

```c
sysfs_create_group(my_kobj, &my_attr_group);
```

→ Tạo đồng loạt nhiều file.