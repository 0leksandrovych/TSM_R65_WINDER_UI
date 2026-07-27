#include "driver/uart.h"
#include "esp_log.h"
#include "waveshare_rgb_lcd_port.h"
#include "winder_hmi.h"

#define HMI_CONTROLLER_UART_NUM                UART_NUM_2
#define HMI_CONTROLLER_UART_TX_GPIO            43
#define HMI_CONTROLLER_UART_RX_GPIO            44
#define HMI_CONTROLLER_UART_BAUD_RATE          115200
#define HMI_CONTROLLER_UART_RX_BUFFER_SIZE     512U
#define HMI_CONTROLLER_UART_TX_BUFFER_SIZE     512U
#define HMI_CONTROLLER_UART_RX_TASK_STACK_SIZE 4096U
#define HMI_CONTROLLER_UART_RX_TASK_PRIORITY   5U

static const char *TAG_HMI = "hmi_test";

static const winder_hmi_uart_controller_config_t s_uart_config = {
    .uart_num = HMI_CONTROLLER_UART_NUM,
    .tx_gpio = HMI_CONTROLLER_UART_TX_GPIO,
    .rx_gpio = HMI_CONTROLLER_UART_RX_GPIO,
    .baud_rate = HMI_CONTROLLER_UART_BAUD_RATE,
    .rx_buffer_size = HMI_CONTROLLER_UART_RX_BUFFER_SIZE,
    .tx_buffer_size = HMI_CONTROLLER_UART_TX_BUFFER_SIZE,
    .rx_task_stack_size = HMI_CONTROLLER_UART_RX_TASK_STACK_SIZE,
    .rx_task_priority = HMI_CONTROLLER_UART_RX_TASK_PRIORITY,
};

static void hmi_command_cb(hmi_command_t command,
                           const hmi_command_payload_t *payload,
                           void *user_ctx)
{
    (void)payload;
    (void)user_ctx;
    ESP_LOGI(TAG_HMI, "HMI command: %d", (int)command);
}

void app_main(void)
{
    ESP_ERROR_CHECK(waveshare_esp32_s3_rgb_lcd_init());

    if (lvgl_port_lock(-1)) {
        winder_hmi_init(lv_scr_act());
        winder_hmi_set_command_callback(hmi_command_cb, NULL);

        if (!winder_hmi_use_uart_controller(&s_uart_config)) {
            ESP_LOGE(TAG_HMI, "Failed to start UART controller link");
        } else {
            ESP_LOGI(
                TAG_HMI,
                "PRODUCTION: UART controller link started uart=%d tx=%d rx=%d baud=%d",
                (int)HMI_CONTROLLER_UART_NUM,
                HMI_CONTROLLER_UART_TX_GPIO,
                HMI_CONTROLLER_UART_RX_GPIO,
                HMI_CONTROLLER_UART_BAUD_RATE);
        }

        lvgl_port_unlock();
    }
}
