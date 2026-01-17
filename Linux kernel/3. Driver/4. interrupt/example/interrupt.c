#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/irq.h>

#define BUTTON_GPIO         44

uint32_t irq_number;

static irqreturn_t button_irq_handler(int irq, void *dev_id)
{
    pr_info("GPIO IRQ: Falling edge detected on GPIO %d\n", BUTTON_GPIO);
    return IRQ_HANDLED;
}

static int my_driver_init(void)
{
    int ret = 0;

    pr_info("+++ Driver init\n");

    ret = gpio_is_valid(BUTTON_GPIO);
    if (!ret)
    {
        pr_err("Invalid GPIO pin\n");
        return -ENODEV;
    }

    // 1. Request GPIO
    ret = gpio_request(BUTTON_GPIO, "irq_button_gpio");
    if (ret)
    {
        pr_err("Can't not allocate GPIO %d\n", BUTTON_GPIO);
        return -1;
    }

    // 2. Set direction as input
    ret = gpio_direction_input(BUTTON_GPIO);
    if (ret)
    {
        pr_err("Can't set GPIO %d to init\n", BUTTON_GPIO);
        gpio_free(BUTTON_GPIO);
        return -2;
    }

    // 3. Convert GPIO → Linux IRQ number
    irq_number = gpio_to_irq(BUTTON_GPIO);
    if (irq_number < 0)
    {
        pr_err("Failed to map GPIO %d to IRQ\n", BUTTON_GPIO);
        gpio_free(BUTTON_GPIO);
        return irq_number;
    }
    
    pr_info("GPIO %d mapped to IRQ %d\n", BUTTON_GPIO, irq_number);

    // 4. Request IRQ line
    ret = request_irq( irq_number,  // IRQ number
            button_irq_handler,     // ISR
            IRQF_TRIGGER_FALLING,   // Trigger type
            "button_irq_handler",   // Name
            NULL );                 // dev_id
    
    if (ret)
    {
        pr_err("Không thể đăng ký IRQ %d\n", irq_number);
        gpio_free(BUTTON_GPIO);
        return ret;
    }

    return 0;
}

static void my_driver_exit(void)
{
    pr_info("Driver exit\n");
    free_irq(irq_number, NULL);
    gpio_free(BUTTON_GPIO);
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Interrupt Example");

module_init(my_driver_init);
module_exit(my_driver_exit);