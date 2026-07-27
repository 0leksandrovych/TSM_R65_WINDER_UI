#pragma once

#include <stdbool.h>

#include "hmi_controller_link_codec.h"

#define HMI_CONTROLLER_RX_QUEUE_CAPACITY 8U

/* Bounded decoded-response queue.
 * Producer: UART RX task callback.
 * Consumer: controller client process function in normal HMI/client context.
 * On overflow, the new decoded response is dropped. */
bool hmi_controller_rx_queue_init(void);
bool hmi_controller_rx_queue_push(const hmi_controller_link_decoded_t *decoded);
bool hmi_controller_rx_queue_pop(hmi_controller_link_decoded_t *out_decoded);
void hmi_controller_rx_queue_clear(void);
