#include "esp_log.h"
#include "waveshare_rgb_lcd_port.h"
#include "winder_hmi.h"
#include "hmi_demo_state.h"

static const char *TAG_HMI = "hmi_test";

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
        winder_hmi_enable_demo_mode(true);
        winder_hmi_set_command_callback(hmi_command_cb, NULL);
        winder_hmi_set_state(hmi_demo_state_ready());
        lvgl_port_unlock();
    }
}
