#pragma once

/* Bridge between hmi_command_bus and hmi_controller_transport.
 *
 * Responsibilities:
 *  - subscribes to hmi_command_bus as an internal listener
 *  - reads current job draft and capability models to build a complete payload
 *  - constructs hmi_controller_message_t
 *  - forwards the message through hmi_controller_transport
 *
 * This is the only place that converts HMI draft state into controller messages.
 */

#include <stdbool.h>

#include "hmi_uart_transport.h"

void hmi_controller_client_init(void);
void hmi_controller_client_deinit(void);

bool hmi_controller_client_use_uart_transport(
    const hmi_uart_transport_config_t *config
);

/* Drain decoded controller responses queued by the UART RX callback.
 * Call from normal HMI/client context, never from the UART RX task. */
void hmi_controller_client_process(void);
