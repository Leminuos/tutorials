#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/timer.h>
#include <linux/of_device.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>

struct led_config_t {
    uint32_t time;
};

struct led_driver_data {
    unsigned int      count;
    const char        *name;
	struct gpio_desc  *gpiod;
    struct timer_list my_timer;
	const struct led_config_t *led_config;
};

struct led_config_t led_gpio30_config = {
    .time = 1,
};

struct led_config_t led_gpio03_config = {
    .time = 3,
};

static const struct of_device_id blink_led_of_match[] = {
	{ .compatible = "led-example1", .data = &led_gpio30_config },
	{ .compatible = "led-example2", .data = &led_gpio03_config},
	{},
};

static bool of_get_default_state(struct device *dev, struct device_node *np)
{
	int ret = 0;
	const char *str = NULL;

	if (np == NULL) return false;

    ret = of_property_read_string(np, "default-state", &str);
	if (ret) return false;

	if (!strcmp(str, "on"))  return true;

	return false;
}

static void timer_function(struct timer_list *timer)
{
    struct led_driver_data *led_data = container_of(timer, struct led_driver_data, my_timer);

    if ((led_data->count % 2) == 0)  gpiod_set_value(led_data->gpiod, 1);
    else gpiod_set_value(led_data->gpiod, 0);

    (led_data->count)++;
    mod_timer(&(led_data->my_timer), jiffies + HZ);
}

static int blink_led_probe(struct platform_device *pdev)
{
	bool init_on = false;
    struct led_driver_data    *led_data = NULL;
	const struct of_device_id *of_id = NULL;

	pr_info("+++ %s\n", pdev->name);

	led_data = devm_kzalloc(&pdev->dev, sizeof(struct led_driver_data), GFP_KERNEL);
    if (!led_data) return -ENOMEM;

    led_data->count = 0;
    led_data->name = pdev->name;
	led_data->gpiod = devm_gpiod_get(&pdev->dev, NULL, GPIOD_OUT_LOW);
	if (IS_ERR(led_data->gpiod)) return PTR_ERR(led_data->gpiod);

	of_id = of_match_device(blink_led_of_match, &pdev->dev);
    if (!of_id || !of_id->data) return -EINVAL;
    led_data->led_config = of_id->data;

	init_on = of_get_default_state(&pdev->dev, pdev->dev.of_node);
	gpiod_set_value(led_data->gpiod, init_on);

	platform_set_drvdata(pdev, led_data);
	timer_setup(&(led_data->my_timer), timer_function, 0);
	mod_timer(&(led_data->my_timer), jiffies + HZ);

	pr_info("--- %s\n", pdev->name);

	return 0;
}

static int blink_led_remove(struct platform_device *pdev)
{
	struct led_driver_data *led_data = platform_get_drvdata(pdev);
    del_timer_sync(&led_data->my_timer);
	return 0;
}

static struct platform_driver blink_led_driver = {
	.probe		= blink_led_probe,
	.remove		= blink_led_remove,
	.driver		= {
		.name	= "blink_led",
		.of_match_table = blink_led_of_match,
	},
};

module_platform_driver(blink_led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pei");
MODULE_DESCRIPTION("Hello world kernel module");