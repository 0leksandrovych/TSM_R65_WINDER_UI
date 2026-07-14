#pragma once

#include <stdbool.h>

#include "hmi_uart_transport.h"

/* Install the RX callbacks used by the controller link while preserving the
 * caller's error callback and context. The supplied config is updated in place. */
bool hmi_controller_rx_handler_prepare_uart_config(
    hmi_uart_transport_config_t *config
);

/* Drain decoded controller responses in normal HMI context.
 * Never call this function from the UART RX task. */
void hmi_controller_rx_handler_process(void);
