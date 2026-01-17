#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/gpio/consumer.h>

#define ESP_CMD_READ  0x20

struct esp_driver_data {
    struct i2c_client  *esp32_client;
	int                alert_irq;
	struct gpio_desc   *alert_gpiod;
	struct work_struct alert_work;
};

static void esp32_alert_work(struct work_struct *work)
{
    uint8_t cmd = 0;
	uint8_t buf[32] = {0};
	struct esp_driver_data *esp_data = NULL;

	cmd      = ESP_CMD_READ;
	esp_data = container_of(work, struct esp_driver_data, alert_work);

    i2c_master_send(esp_data->esp32_client, &cmd, 1);
    msleep(10);
    i2c_master_recv(esp_data->esp32_client, buf, sizeof(buf));
    pr_info("ESP32 -> BBB: %s\n", buf);
}

static irqreturn_t esp_alert_isr(int irq, void *dev_id)
{
	struct esp_driver_data *esp_data = NULL;
	esp_data = (struct esp_driver_data *) dev_id;
	if (esp_data == NULL) return IRQ_NONE;
    schedule_work(&esp_data->alert_work);
    return IRQ_HANDLED;
}

static int esp_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
    struct esp_driver_data *esp_data = NULL;

	esp_data = devm_kzalloc(&client->dev, sizeof(struct esp_driver_data), GFP_KERNEL);
    if (!esp_data) return -ENOMEM;

	esp_data->esp32_client = client;
	esp_data->alert_gpiod = devm_gpiod_get(&client->dev, "alert", GPIOD_IN);
	if (IS_ERR(esp_data->alert_gpiod)) return PTR_ERR(esp_data->alert_gpiod);

	INIT_WORK(&esp_data->alert_work, esp32_alert_work);
	esp_data->alert_irq = gpiod_to_irq(esp_data->alert_gpiod);
	if (esp_data->alert_irq < 0) return esp_data->alert_irq;
	ret = devm_request_irq(	&client->dev,
							esp_data->alert_irq,
							esp_alert_isr,
							IRQF_TRIGGER_FALLING,
							"esp_alert",
							esp_data
						);
	if (ret) return ret;

	i2c_set_clientdata(client, esp_data);

	pr_info("esp32 i2c driver probed (addr=0x%02x)\n", client->addr);

	return 0;
}

static int esp_remove(struct i2c_client *client)
{
	struct esp_driver_data *esp_data = NULL;
	esp_data = i2c_get_clientdata(client);
	cancel_work_sync(&esp_data->alert_work);
    pr_info("esp32 i2c driver removed\n");
	return 0;
}

static const struct of_device_id esp_of_match[] = {
	{ .compatible = "esp32,i2c-client" },
	{},
};

MODULE_DEVICE_TABLE(of, esp_of_match);

static struct i2c_driver esp32_i2c_driver = {
	.probe		= esp_probe,
	.remove		= esp_remove,
	.driver		= {
		.name	= "esp32_i2c",
		.of_match_table = esp_of_match,
	},
};

module_i2c_driver(esp32_i2c_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pei");
MODULE_DESCRIPTION("BBB I2C driver");