#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#define GPIO_SETDATAOUT_OFFSET      0x194
#define GPIO_CLEARDATAOUT_OFFSET    0x190
#define GPIO_OE_OFFSET              0x134

#define GPIO30                      ~(1 << 30)
#define GPIO30_DATA_OUT             (1 << 30)

#define GPIO03                      ~(1 << 3)
#define GPIO03_DATA_OUT             (1 << 3)

struct led_config_t {
    uint32_t led;
    uint32_t data_out;
    uint32_t time;
};

struct led_driver_data {
    void __iomem *base_addr;
    unsigned int count;
    const struct led_config_t *led_config;
    struct timer_list my_timer;
};

struct led_config_t led_gpio30_config = {
    .led = GPIO30,
    .data_out = GPIO30_DATA_OUT,
    .time = 1,
};

struct led_config_t led_gpio03_config = {
    .led = GPIO03,
    .data_out = GPIO03_DATA_OUT,
    .time = 3,
};

static const struct of_device_id blink_led_of_match[] = {
    { .compatible = "led-example1", .data = &led_gpio30_config },
    { .compatible = "led-example2", .data = &led_gpio03_config },
    {},
};

static void timer_function(struct timer_list *timer)
{
    struct led_driver_data *led_data = container_of(timer, struct led_driver_data, my_timer);

    if ((led_data->count % 2) == 0) 
        writel_relaxed(led_data->led_config->data_out,  led_data->base_addr + GPIO_SETDATAOUT_OFFSET);
    else
        writel_relaxed(led_data->led_config->data_out,  led_data->base_addr + GPIO_CLEARDATAOUT_OFFSET); 

    (led_data->count)++;
    mod_timer(&(led_data->my_timer), jiffies + led_data->led_config->time * HZ);
}

static int blink_led_probe(struct platform_device *pdev)
{
    uint32_t reg_data = 0;
    struct resource *res = NULL;
    struct led_driver_data  *led_data = NULL;
    const struct of_device_id *of_id = NULL;
    
    pr_info("+++ %s\n", pdev->name);
    
    led_data = devm_kzalloc(&pdev->dev, sizeof(struct led_driver_data), GFP_KERNEL);
    if (!led_data) return -ENOMEM;

    led_data->count = 0;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) return -ENODEV;
    pr_info("start address: 0x%08X, end address: 0x%08X\n", res->start, res->end);

    led_data->base_addr = ioremap(res->start, res->end - res->start);
    if (IS_ERR(led_data->base_addr)) return PTR_ERR(led_data->base_addr);

    of_id = of_match_device(blink_led_of_match, &pdev->dev);
    if (!of_id || !of_id->data) return -EINVAL;
    led_data->led_config = of_id->data;
    pr_info("led: 0x%08X\n", led_data->led_config->led);

    reg_data = readl_relaxed(led_data->base_addr + GPIO_OE_OFFSET);
    reg_data &= led_data->led_config->led;
    writel_relaxed(reg_data, led_data->base_addr + GPIO_OE_OFFSET);
    
    platform_set_drvdata(pdev, led_data);
    timer_setup(&(led_data->my_timer), timer_function, 0);
	mod_timer(&(led_data->my_timer), jiffies + HZ);

    return 0;
}

static int blink_led_remove(struct platform_device *pdev)
{
    struct led_driver_data *led = platform_get_drvdata(pdev);
    del_timer_sync(&led->my_timer);
    return 0;
}

static struct platform_driver blink_led_driver = {
    .probe      = blink_led_probe,
    .remove     = blink_led_remove,
    .driver     = {
        .name   = "blink_led",
        .of_match_table = blink_led_of_match,
    },
};

module_platform_driver(blink_led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pei");
MODULE_DESCRIPTION("Hello world kernel module");