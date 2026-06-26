#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/uart.h"
#include "hmi_controller_link_codec.h"
#include "hmi_controller_messages.h"

typedef enum {
    HMI_UART_TRANSPORT_ERROR_UART_READ = 1,
    HMI_UART_TRANSPORT_ERROR_FRAME_DECODE,
    HMI_UART_TRANSPORT_ERROR_MESSAGE_DECODE,
} hmi_uart_transport_error_t;

/* The RX callback is called from the UART RX task context.
 * It must not call LVGL directly.
 * It must not block for a long time.
 * A higher layer must forward decoded responses to the HMI event queue or controller client. */
typedef void (*hmi_uart_transport_rx_callback_t)(
    const hmi_controller_link_decoded_t *decoded,
    void *user_ctx
);

typedef void (*hmi_uart_transport_error_callback_t)(
    int error_code,
    void *user_ctx
);

typedef struct {
    uart_port_t uart_num;

    int tx_gpio;
    int rx_gpio;

    int baud_rate;

    size_t rx_buffer_size;
    size_t tx_buffer_size;

    uint32_t rx_task_stack_size;
    uint32_t rx_task_priority;

    hmi_uart_transport_rx_callback_t rx_callback;
    hmi_uart_transport_error_callback_t error_callback;
    void *user_ctx;
} hmi_uart_transport_config_t;

bool hmi_uart_transport_init(const hmi_uart_transport_config_t *config);

bool hmi_uart_transport_start(void);

void hmi_uart_transport_stop(void);

bool hmi_uart_transport_send_message(
    const hmi_controller_message_t *message,
    uint16_t seq
);
