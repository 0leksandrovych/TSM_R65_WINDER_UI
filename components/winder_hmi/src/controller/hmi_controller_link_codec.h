#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hmi_controller_messages.h"
#include "winder_link_protocol.h"

typedef struct {
    winder_link_msg_type_t type;
    uint8_t payload[WINDER_LINK_MAX_PAYLOAD_SIZE];
    size_t payload_len;
} hmi_controller_link_encoded_t;

bool hmi_controller_link_encode_message(
    const hmi_controller_message_t *message,
    hmi_controller_link_encoded_t *out_encoded
);
