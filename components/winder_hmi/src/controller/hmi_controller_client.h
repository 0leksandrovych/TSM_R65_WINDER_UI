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

void hmi_controller_client_init(void);
void hmi_controller_client_deinit(void);
