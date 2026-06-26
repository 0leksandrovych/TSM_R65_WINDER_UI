#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hmi_controller_messages.h"

typedef bool (*hmi_controller_send_fn_t)(
    const hmi_controller_message_t *message,
    uint16_t seq,
    void *user_ctx
);

typedef struct {
    hmi_controller_send_fn_t send;
    void *user_ctx;
} hmi_controller_transport_t;

void hmi_controller_transport_set(const hmi_controller_transport_t *transport);
bool hmi_controller_transport_send(const hmi_controller_message_t *message, uint16_t seq);
bool hmi_controller_transport_is_available(void);
