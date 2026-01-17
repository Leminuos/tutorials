#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <asm/io.h>

#define LED_GPIO                            2
#define BUTTON_GPIO                         3

/* Device name */
#define DEVICE_NAME                         "chardev"
#define MY_IOCTL_CMD                        _IOW('M', 1, int)

/* Level pin gpio */
#define HIGH                                0x01
#define LOW                                 0x00

/* GPIO mode */
#define GPIO_INPUT                          0x00
#define GPIO_OUTPUT                         0x01

/* Physical register address of gpio */
#define GPIO_PHYS_ADDR      0x3f200000
#define GPFSEL0_OFFSET      0x00000000
#define GPFSEL1_OFFSET      0x00000004
#define GPFSEL2_OFFSET      0x00000008
#define GPFSEL3_OFFSET      0x0000000C
#define GPFSEL4_OFFSET      0x00000010
#define GPFSEL5_OFFSET      0x00000014
#define GPSET0_OFFSET       0x0000001C
#define GPSET1_OFFSET       0x00000020
#define GPCLR0_OFFSET       0x00000028
#define GPCLR1_OFFSET       0x0000002C
#define GPLEV0_OFFSET       0x00000034
#define GPLEV1_OFFSET       0x00000038
#define GPEDS0_OFFSET       0x00000040
#define GPEDS1_OFFSET       0x00000044
#define GPREN0_OFFSET       0x0000004C
#define GPREN1_OFFSET       0x00000050
#define GPFEN0_OFFSET       0x00000058
#define GPFEN1_OFFSET       0x0000005C
#define GPHEN0_OFFSET       0x00000064
#define GPHEN1_OFFSET       0x00000068
#define GPLEN0_OFFSET       0x00000070
#define GPLEN1_OFFSET       0x00000074
#define GPAREN0_OFFSET      0x0000007C
#define GPAREN1_OFFSET      0x00000080
#define GPAFEN0_OFFSET      0x00000088
#define GPAFEN1_OFFSET      0x0000008C
#define GPPUD_OFFSET        0x00000094
#define GPPUDCLK0_OFFSET    0x00000098
#define GPPUDCLK1_OFFSET    0x0000009C

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class = NULL;
static struct device *device_name = NULL;

static int dev_open(struct inode *inode, struct file *file);
static int dev_release(struct inode *inode, struct file *file);
static ssize_t dev_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);
static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

void gpio_init(uint8_t gpio, uint8_t mode);
void set_level_gpio(uint8_t gpio, uint8_t level);
void set_pull_input_gpio(uint8_t gpio, uint8_t pull);
uint8_t read_level_gpio(uint8_t gpio);

static void __iomem *gpio_base;

void gpio_init(uint8_t gpio, uint8_t mode)
{
    void __iomem * mem = 0;
    uint32_t sel_offset = 0;
    uint32_t reg_val = 0;

    gpio_base = ioremap(GPIO_PHYS_ADDR, PAGE_SIZE);

    if (!gpio_base) {
        pr_err("Không thể ánh xạ vùng bộ nhớ thiết bị\n");
        return;
    }

    switch(gpio / 10)
    {
        case 0:
        sel_offset = GPFSEL0_OFFSET;
        break;

        case 1:
        sel_offset = GPFSEL1_OFFSET;
        break;

        case 2:
        sel_offset = GPFSEL2_OFFSET;
        break;

        case 3:
        sel_offset = GPFSEL3_OFFSET;
        break;

        case 4:
        sel_offset = GPFSEL4_OFFSET;
        break;

        case 5:
        sel_offset = GPFSEL5_OFFSET;
        break;

        default:
        break;
    }

    mem         = gpio_base + sel_offset;
    reg_val     = ioread32((const void __iomem *) mem);
    reg_val     = reg_val | (mode << ((gpio % 10) * 3));
    iowrite32(reg_val, mem);
}

void set_level_gpio(uint8_t gpio, uint8_t level)
{
    void __iomem * mem = 0;
    uint32_t reg_val = 0;
    uint32_t sel_offset = 0;
    uint32_t clr_offset = 0;

    sel_offset  = (gpio / 32) ? GPSET1_OFFSET : GPSET0_OFFSET;
    clr_offset  = (gpio / 32) ? GPCLR1_OFFSET : GPCLR0_OFFSET;
    mem         = level ? (gpio_base + sel_offset) : (gpio_base + clr_offset);
    reg_val     = ioread32((const void __iomem *) mem);
    reg_val     = reg_val | (1 << (gpio % 32));
    iowrite32(reg_val, mem);
}

void set_pull_input_gpio(uint8_t gpio, uint8_t pull)
{
    uint8_t i = 0;
    uint32_t reg_val = 0;
    void __iomem * mem = 0;

    mem = gpio_base + GPPUD_OFFSET;
    iowrite32(pull, mem);
    for (i = 0; i < 150; ++i);

    mem     = (gpio / 32) ? (gpio_base + GPPUDCLK1_OFFSET) : (gpio_base + GPPUDCLK0_OFFSET);
    reg_val = ioread32((const void __iomem *) mem);
    reg_val = reg_val | 1 << (gpio % 32);
    iowrite32(reg_val, mem);
    for (i = 0; i < 150; ++i);

    mem = gpio_base + GPPUD_OFFSET;
    iowrite32(0, mem);

    mem = (gpio / 32) ? (gpio_base + GPPUDCLK1_OFFSET) : (gpio_base + GPPUDCLK0_OFFSET);
    iowrite32(0, mem);
}

uint8_t read_level_gpio(uint8_t gpio)
{
    void __iomem * mem = 0;
    uint32_t lev_offset = 0;
    uint32_t ret_val = 0;
    uint32_t bit_check = 0;

    lev_offset  = (gpio / 32) ? GPLEV1_OFFSET : GPLEV0_OFFSET;
    mem         = gpio_base + lev_offset;
    ret_val     = ioread32((const void __iomem *) mem);
    bit_check   = 1 << (gpio % 32);

    return ((ret_val & bit_check) == bit_check) ? HIGH : LOW;
}

static const struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = dev_open,
    .release        = dev_release,
    .read           = dev_read,
    .write          = dev_write,
    .unlocked_ioctl = dev_ioctl
};

static int my_driver_init(void)
{
    int ret = 0;
   
    gpio_init(LED_GPIO, GPIO_OUTPUT);
    set_level_gpio(LED_GPIO, LOW);

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);

    if (ret)
    {
        printk("Allocate character device fail\n");
        return ret;
    }

    printk("Register successfully major now is: %d, minor now is: %d\n", MAJOR(dev_num), MINOR(dev_num));

    my_class = class_create(DEVICE_NAME);

    if (IS_ERR(my_class))
    {
        printk("Create class failed\n");
        return -2;
    }

    cdev_init(&my_cdev, &fops);

    my_cdev.owner = THIS_MODULE;
    my_cdev.dev = dev_num;

    ret = cdev_add(&my_cdev, dev_num, 1);

    if (ret < 0)
    {
        printk("cdev_add fail\n");
        return -3;
    }

    device_name = device_create(my_class, NULL, dev_num, NULL, "ex_chardev");

    if (IS_ERR(device_name))
    {
        printk("Create device failed\n");
        return -4;
    }

    printk("Create device successs\n");

    return 0;
}

static void my_driver_exit(void)
{
    device_destroy(my_class, dev_num);
    class_unregister(my_class);
    class_destroy(my_class);
    unregister_chrdev_region(dev_num, 1);
    printk("Character device unregistered\n");
}

static char msg[100];

static int dev_open(struct inode *inode, struct file *file)
{
    printk("Device open\n");
    return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
    printk("Device close\n");
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    msg[0] = read_level_gpio(2) + '0';
    msg[1] = '\0';
    size_t byte_read = 1;

    printk("Test count read\n");

    if (*offset >= byte_read) {
        return 0;
    }

    if (count > byte_read - *offset)
    {
        count = byte_read - *offset;
    }

    // Sao chép dữ liệu từ kernel sang user space
    if (copy_to_user(buf, msg + *offset, count))
    {
        pr_err("Failed to send data to user\n");
        return -EFAULT;  // Trả về mã lỗi khi sao chép thất bại
    }

    // Cập nhật offset sau khi đọc
    *offset += count;

    pr_info("Sent data to user %s\n", msg);
    return count;
}

static ssize_t dev_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    // Sao chép dữ liệu từ user sang kernel space
    if (copy_from_user(msg, buf, count))
    {
        pr_err("Failed to send data to user\n");
    }

    msg[count - 1] = '\0';

    printk("Device write: %s\n", msg);

    if (strcmp(msg, "0") == 0)
    {
        printk("Led off\n");
        set_level_gpio(2, LOW);
    }
   
    if (strcmp(msg, "1") == 0)
    {
        printk("Led on\n");
        set_level_gpio(2, HIGH);
    }

    return count;
}

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int value;

    switch (cmd) {
        case MY_IOCTL_CMD:
            if (copy_from_user(&value, (int __user *)arg, sizeof(value)))
                return -EFAULT;
            printk(KERN_INFO "Driver nhận lệnh MY_IOCTL_CMD với giá trị: %d\n", value);
            break;
        default:
            return -ENOTTY;
    }


    return 0;
}

MODULE_LICENSE("GPL");

module_init(my_driver_init);
module_exit(my_driver_exit);