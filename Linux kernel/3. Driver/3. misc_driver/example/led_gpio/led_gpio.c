#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>

#define LED_GPIO 60   // GPIO1_28 = 60

static int led_open(struct inode *node, struct file *f)
{
    int ret;

    pr_info("Led open\n");

    ret = gpio_request(LED_GPIO, "led_gpio");
    if (ret) return ret;

    gpio_direction_output(LED_GPIO, 0);

    return 0;
}

static int led_release(struct inode *node, struct file *f)
{
    pr_info("Led release\n");
    // gpio_set_value(LED_GPIO, 0);
    // gpio_free(LED_GPIO);
    return 0;
}

static ssize_t led_write(struct file *f, const char __user *buf, size_t count, loff_t *off)
{
    char c;

    if (copy_from_user(&c, buf, 1)) return -EFAULT;
    pr_info("Set led: %c\n", c);
    if (c == '1') gpio_set_value(LED_GPIO, 1);
    else if (c == '0') gpio_set_value(LED_GPIO, 0);
    return count;
}

static ssize_t led_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    char c = 0;

    c = gpio_get_value(LED_GPIO);

    // Sao chép dữ liệu từ kernel sang user space
    if (copy_to_user(buf, &c, 1)) return -EFAULT;

    pr_info("Status led: %d\n", c);

    return 0;
}

static const struct file_operations fops = {
    .owner      = THIS_MODULE,
    .open       = led_open,
    .release    = led_release,
    .write      = led_write,
    .read       = led_read
};

static struct miscdevice led_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "led_gpio",
    .fops  = &fops,
    .mode  = 0666,
};

static int __init led_init(void)
{
    pr_info("led init\n");
    return misc_register(&led_dev);
}

static void __exit led_exit(void)
{
    pr_info("led exit\n");
    misc_deregister(&led_dev);
    gpio_set_value(LED_GPIO, 0);
    gpio_free(LED_GPIO);
}

module_init(led_init);
module_exit(led_exit);

MODULE_AUTHOR("Pei");
MODULE_DESCRIPTION("Example gpio driver.");
MODULE_LICENSE("GPL");
