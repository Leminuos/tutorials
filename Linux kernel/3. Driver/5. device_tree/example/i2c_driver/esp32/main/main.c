#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_slave.h"
#include "esp_log.h"
#include <string.h>

#define I2C_SLAVE_SDA   22
#define I2C_SLAVE_SCL   21
#define I2C_SLAVE_NUM   I2C_NUM_0
#define I2C_SLAVE_ADDR  0x69
#define ALERT_GPIO      23

typedef enum {
    EVT_RX = 1,
    EVT_TX
} evt_t;

typedef struct {
    size_t len;
    uint8_t *buf;
} rx_info_t;

static QueueHandle_t s_evtq;
static rx_info_t s_last_rx = {0};
static const char *TAG = "I2C_SLAVE";
static i2c_slave_dev_handle_t i2c_slave;

static void alert_task(void *arg)
{
    while (1)
    {
        gpio_set_level(ALERT_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(2));
        gpio_set_level(ALERT_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static IRAM_ATTR bool on_request_cb(i2c_slave_dev_handle_t h, const i2c_slave_request_event_data_t *e, void *arg)
{
    evt_t evt = EVT_TX;
    BaseType_t xTaskWoken  = pdFALSE;
    xQueueSendFromISR(s_evtq, &evt, &xTaskWoken);
    return xTaskWoken;
}

static IRAM_ATTR bool on_receive_cb(i2c_slave_dev_handle_t h, const i2c_slave_rx_done_event_data_t *e, void *arg)
{
    evt_t evt = EVT_RX;
    BaseType_t xTaskWoken = pdFALSE;
    s_last_rx.buf   = e->buffer;
    s_last_rx.len  = e->length;
    xQueueSendFromISR(s_evtq, &evt, &xTaskWoken);
    return xTaskWoken;
}

static void i2c_slave_task(void *arg)
{
    evt_t evt;
    static bool alert_flag = false;
    const char *reply = "Hello BBB";

    while (1)
    {
        if (xQueueReceive(s_evtq, &evt, portMAX_DELAY) == pdTRUE)
        {
            if (evt == EVT_RX)
            {
                switch (s_last_rx.buf[0])
                {
                    case 0x20:
                        alert_flag = true;
                        break;
                    
                    case 0x30:
                        ESP_LOGI(TAG, "BBB send string (%d): %s", s_last_rx.len, &s_last_rx.buf[1]);
                        break;
                }
            }
            else if (evt == EVT_TX)
            {
                if (alert_flag)
                {
                    uint32_t written = 0;
                    (void)i2c_slave_write(i2c_slave, (const uint8_t*)reply, strlen(reply), &written, 50);
                    ESP_LOGI(TAG, "Prepared reply for BBB(%" PRIu32 "): %s", written, reply);
                }
            }
        }
    }
}

void gpio_alert_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << ALERT_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 1
    };

    gpio_config(&io_conf);
    gpio_set_level(ALERT_GPIO, 1);

    xTaskCreate(alert_task, "alert_task", 2048, NULL, 5, NULL);
}

void i2c_slave_init(void)
{
    i2c_slave_config_t cfg = {
        .i2c_port       = I2C_NUM_0,
        .sda_io_num     = I2C_SLAVE_SDA,
        .scl_io_num     = I2C_SLAVE_SCL,
        .clk_source     = I2C_CLK_SRC_DEFAULT,
        .slave_addr     = I2C_SLAVE_ADDR,
        .addr_bit_len   = I2C_ADDR_BIT_LEN_7,
        .intr_priority  = 0,
        .send_buf_depth = 128,                  // ringbuffer TX
        .receive_buf_depth = 128,
        .flags.enable_internal_pullup = 1,
    };

    ESP_ERROR_CHECK(i2c_new_slave_device(&cfg, &i2c_slave));

    // Đăng ký callback nhận dữ liệu từ master
    i2c_slave_event_callbacks_t cbs = {
        .on_request = on_request_cb,
        .on_receive = on_receive_cb,
    };

    ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(i2c_slave, &cbs, NULL));

    // Tạo queue và task xử lý
    s_evtq = xQueueCreate(4, sizeof(evt_t));
    xTaskCreate(i2c_slave_task, "i2c_slave_task", 2048, NULL, 5, NULL);
}

void app_main(void)
{
    gpio_alert_init();
    i2c_slave_init();
}
