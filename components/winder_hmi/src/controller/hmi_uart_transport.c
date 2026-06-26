#include "hmi_uart_transport.h"

#include <stdint.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "winder_link_protocol.h"

#define HMI_UART_RX_CHUNK_SIZE 64U
#define HMI_UART_TASK_STOP_WAIT_MS 10U
#define HMI_UART_TASK_STOP_WAIT_LOOPS 20U
#define HMI_UART_FRAME_BUFFER_SIZE \
    (WINDER_LINK_FRAME_OVERHEAD + WINDER_LINK_MAX_PAYLOAD_SIZE)

static const char *TAG = "hmi_uart_transport";

typedef struct {
    bool initialized;
    volatile bool stop_requested;
    uart_port_t uart_num;
    TaskHandle_t rx_task;
    uint32_t rx_task_stack_size;
    uint32_t rx_task_priority;
    hmi_uart_transport_rx_callback_t rx_callback;
    hmi_uart_transport_error_callback_t error_callback;
    void *user_ctx;
    winder_link_decoder_t decoder;
} hmi_uart_transport_state_t;

static hmi_uart_transport_state_t s_transport;

static void report_error(int error_code)
{
    if (s_transport.error_callback != NULL) {
        s_transport.error_callback(error_code, s_transport.user_ctx);
    }
}

static bool validate_config(const hmi_uart_transport_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    if (config->rx_callback == NULL) {
        return false;
    }
    if (config->baud_rate <= 0) {
        return false;
    }
    if (config->rx_buffer_size == 0U || config->tx_buffer_size == 0U) {
        return false;
    }
    if (config->rx_buffer_size > (size_t)INT32_MAX ||
        config->tx_buffer_size > (size_t)INT32_MAX) {
        return false;
    }
    if (config->rx_task_stack_size == 0U) {
        return false;
    }
    if (config->uart_num < 0 || config->uart_num >= UART_NUM_MAX) {
        return false;
    }

    return true;
}

static void handle_received_frame(const winder_link_frame_t *frame)
{
    hmi_controller_link_decoded_t decoded = {0};
    if (!hmi_controller_link_decode_message(
            (winder_link_msg_type_t)frame->type,
            frame->payload,
            frame->payload_len,
            &decoded)) {
        report_error(HMI_UART_TRANSPORT_ERROR_MESSAGE_DECODE);
        return;
    }

    s_transport.rx_callback(&decoded, s_transport.user_ctx);
}

static void hmi_uart_rx_task(void *arg)
{
    (void)arg;

    uint8_t rx_bytes[HMI_UART_RX_CHUNK_SIZE] = {0};

    while (!s_transport.stop_requested) {
        int read_len = uart_read_bytes(
            s_transport.uart_num,
            rx_bytes,
            sizeof(rx_bytes),
            pdMS_TO_TICKS(50));

        if (read_len < 0) {
            report_error(HMI_UART_TRANSPORT_ERROR_UART_READ);
            continue;
        }

        for (int i = 0; i < read_len; i++) {
            winder_link_frame_t frame = {0};
            winder_link_error_t error = WINDER_LINK_ERROR_NONE;
            winder_link_decode_result_t result =
                winder_link_decoder_push_byte(&s_transport.decoder, rx_bytes[i], &frame, &error);

            if (result == WINDER_LINK_DECODE_RESULT_ERROR) {
                (void)error;
                report_error(HMI_UART_TRANSPORT_ERROR_FRAME_DECODE);
                continue;
            }

            if (result == WINDER_LINK_DECODE_RESULT_FRAME) {
                handle_received_frame(&frame);
            }
        }
    }

    s_transport.rx_task = NULL;
    vTaskDelete(NULL);
}

bool hmi_uart_transport_init(const hmi_uart_transport_config_t *config)
{
    if (!validate_config(config)) {
        return false;
    }
    if (s_transport.initialized) {
        return false;
    }

    const uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (uart_param_config(config->uart_num, &uart_config) != ESP_OK) {
        ESP_LOGE(TAG, "UART parameter configuration failed");
        return false;
    }

    if (uart_set_pin(
            config->uart_num,
            config->tx_gpio,
            config->rx_gpio,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "UART pin configuration failed");
        return false;
    }

    if (uart_driver_install(
            config->uart_num,
            (int)config->rx_buffer_size,
            (int)config->tx_buffer_size,
            0,
            NULL,
            0) != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed");
        return false;
    }

    s_transport = (hmi_uart_transport_state_t){0};
    s_transport.initialized = true;
    s_transport.uart_num = config->uart_num;
    s_transport.rx_task_stack_size = config->rx_task_stack_size;
    s_transport.rx_task_priority = config->rx_task_priority;
    s_transport.rx_callback = config->rx_callback;
    s_transport.error_callback = config->error_callback;
    s_transport.user_ctx = config->user_ctx;
    winder_link_decoder_init(&s_transport.decoder);

    ESP_LOGI(TAG, "UART transport initialized");
    return true;
}

bool hmi_uart_transport_start(void)
{
    if (!s_transport.initialized) {
        return false;
    }
    if (s_transport.rx_task != NULL) {
        return false;
    }

    s_transport.stop_requested = false;
    BaseType_t result = xTaskCreate(
        hmi_uart_rx_task,
        "hmi_uart_rx",
        s_transport.rx_task_stack_size,
        NULL,
        (UBaseType_t)s_transport.rx_task_priority,
        &s_transport.rx_task);

    if (result != pdPASS) {
        s_transport.rx_task = NULL;
        ESP_LOGE(TAG, "UART RX task creation failed");
        return false;
    }

    ESP_LOGI(TAG, "UART transport started");
    return true;
}

void hmi_uart_transport_stop(void)
{
    if (!s_transport.initialized || s_transport.rx_task == NULL) {
        return;
    }

    s_transport.stop_requested = true;

    for (uint32_t i = 0; i < HMI_UART_TASK_STOP_WAIT_LOOPS; i++) {
        if (s_transport.rx_task == NULL) {
            ESP_LOGI(TAG, "UART transport stopped");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(HMI_UART_TASK_STOP_WAIT_MS));
    }

    if (s_transport.rx_task != NULL) {
        vTaskDelete(s_transport.rx_task);
        s_transport.rx_task = NULL;
    }

    ESP_LOGI(TAG, "UART transport stopped");
}

bool hmi_uart_transport_send_message(
    const hmi_controller_message_t *message,
    uint16_t seq)
{
    if (!s_transport.initialized || message == NULL) {
        return false;
    }

    hmi_controller_link_encoded_t encoded = {0};
    if (!hmi_controller_link_encode_message(message, &encoded)) {
        return false;
    }

    uint8_t frame_buffer[HMI_UART_FRAME_BUFFER_SIZE] = {0};
    size_t frame_len = 0U;
    if (!winder_link_encode_frame(
            (uint8_t)encoded.type,
            seq,
            encoded.payload,
            (uint16_t)encoded.payload_len,
            frame_buffer,
            sizeof(frame_buffer),
            &frame_len)) {
        return false;
    }

    int written = uart_write_bytes(
        s_transport.uart_num,
        (const char *)frame_buffer,
        frame_len);

    return written == (int)frame_len;
}
