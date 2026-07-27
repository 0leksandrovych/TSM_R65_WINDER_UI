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
#include <stdint.h>

#include "hmi_events.h"
#include "hmi_uart_transport.h"
#include "winder_link_protocol.h"

void hmi_controller_client_init(void);
void hmi_controller_client_deinit(void);

bool hmi_controller_client_use_uart_transport(
    const hmi_uart_transport_config_t *config
);

/* Request the latest controller snapshot without creating a tracked command. */
bool hmi_controller_client_request_telemetry(void);

/* Resolve a controller response to its originating HMI command.
 * The tracked sequence entry is consumed when a match is found. */
bool hmi_controller_client_resolve_response(
    uint16_t original_seq,
    winder_link_msg_type_t original_type,
    hmi_command_t *out_command
);
