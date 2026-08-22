# Misc driver

## Misc device là gì?

Thay vì đi qua toàn bộ quy trình đăng ký character driver thủ công - `alloc_chrdev_region`, `cdev_init`, `cdev_add`, `class_create`, `device_create` - kernel cung cấp một shortcut gọi là **misc device**.

Misc device là một wrapper của character driver với một số đặc điểm cố định:
- Major number luôn là **10** (xem trong `/proc/devices` với tên `misc`)
- Minor number được kernel cấp động
- Device file được tạo tự động trong `/dev`

Toàn bộ quá trình đăng ký rút gọn xuống còn hai bước: khai báo `struct miscdevice` và gọi `misc_register()`.

```c
#include <linux/miscdevice.h>

static struct miscdevice my_misc = {
    .minor = MISC_DYNAMIC_MINOR,  // kernel tự cấp minor number
    .name  = "mydevice",          // tạo ra /dev/mydevice
    .fops  = &my_fops,            // file_operations đã định nghĩa
};
```

## Sequence khởi tạo và cleanup

**init:**

```c
static int __init my_init(void)
{
    int ret;

    ret = misc_register(&my_misc);
    if (ret) {
        pr_err("misc_register failed: %d\n", ret);
        return ret;
    }

    pr_info("mydevice registered at /dev/%s\n", my_misc.name);
    return 0;
}
```

**exit:**

```c
static void __exit my_exit(void)
{
    misc_deregister(&my_misc);
    pr_info("mydevice unregistered\n");
}
```

So sánh với character driver truyền thống - driver dùng misc tiết kiệm khoảng 20 dòng. Phần còn lại - implement `file_operations` - hoàn toàn giống nhau.

## Tại sao Linux kernel phải theo template driver?

Khi ta muốn viết driver cho device nào đó thì ta cần phải biết được cách giao tiếp với nó. Tuy nhiên, có rất nhiều loại device, mỗi device lại có cách thức giao tiếp và vendor khác nhau. Ta không thể viết được hết cho các device này. Giả sử ta viết được hết thì trong tương lai thì vendor sẽ cập nhập lại giao thức kiểu khác -> Ta phải build lại kernel, rất phức tạp.

Vì vậy, tất cả device thuộc cùng một loại thì phải tuân theo một template driver.

Để làm được điều này, thì kernel sẽ cho ta một struct gồm các hàm callback. Ta phải đăng ký các hàm callback này khi viết driver -> Kernel chỉ quan tâm driver có đúng format API và trả kết quả đúng hay không, còn cách driver nói chuyện với hardware như thế nào là việc của developer.

Nếu viết driver mà không theo đúng template thì kernel sẽ không hiểu driver mà ta muốn viết thuộc loại nào.

## Quy tắc tư duy viết driver trên Linux

Thói quen từ MCU development là nhận datasheet, mở IDE, bắt đầu gõ. Trên MCU điều này hoạt động vì môi trường đơn giản - một chương trình, một CPU, không có lớp trung gian.

Trên Linux, nhảy thẳng vào code driver mà không hiểu đủ context dẫn đến một vòng lặp quen thuộc: viết → build lỗi → sửa → kernel panic → reboot → lặp lại. Mỗi vòng lặp mất nhiều thời gian hơn MCU vì debug kernel khó hơn debug bare-metal.

Quy tắc dưới đây giúp phá vỡ vòng lặp đó.

**Bước 1 - Đọc hiểu tài liệu phần cứng**

Output cần đạt: hiểu hardware đến mức có thể code được trên MCU nếu cần.

Cụ thể: biết hardware giao tiếp qua giao thức gì (I2C, SPI, UART,...), biết các thanh ghi quan trọng và ý nghĩa từng bit, biết timing nếu có, biết interrupt hay polling.

Nếu chưa đạt output này, mọi code driver viết ra chỉ là đoán mò.

**Bước 2 - Tìm hiểu quy tắc giao tiếp giữa app và hardware**

Output cần đạt: biết cách test driver sau khi code xong - không cần viết application phức tạp.

Cụ thể: hardware này thường được user space truy cập qua interface nào? `/dev/i2c-X` và `ioctl` với `I2C_RDWR`? `/dev/spidevX.Y` và `SPI_IOC_MESSAGE`? Hay một device file custom với read/write đơn giản?

Biết interface trước giúp developer viết driver với đúng API ngay từ đầu, đồng thời biết ngay câu lệnh test sau khi `insmod`.

**Bước 3 - Tìm template driver cho loại device đó**

Output cần đạt: có một driver hoạt động được làm điểm xuất phát - không bắt đầu từ trang trắng.

Cụ thể: tìm trong kernel source một driver của device khác cùng loại - cùng bus, cùng loại sensor, cùng class. Copy skeleton đó, giữ nguyên phần đăng ký template, chỉ thay phần logic hardware. Kernel source trên GitHub là nguồn tham khảo tốt nhất: `drivers/i2c/`, `drivers/spi/`, `drivers/char/`.

Đây là cách kernel developer thực tế làm - không ai viết driver từ đầu nếu đã có driver tương tự.

**Bước 4 - Code driver**

Chỉ đến bước này mới bắt đầu viết code thực sự. Lúc này developer đã có:
- Hiểu biết đầy đủ về hardware
- Biết interface cần implement
- Có skeleton làm điểm xuất phát
- Biết cách verify kết quả

Code viết ra ở bước này thường đúng ngay lần đầu - hoặc ít nhất fail theo cách có thể debug được.