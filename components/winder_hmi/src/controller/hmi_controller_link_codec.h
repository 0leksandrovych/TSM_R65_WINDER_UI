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

typedef enum {
    HMI_CONTROLLER_LINK_DECODED_NONE = 0,
    HMI_CONTROLLER_LINK_DECODED_COMMAND_ACCEPTED,
    HMI_CONTROLLER_LINK_DECODED_COMMAND_REJECTED,
    HMI_CONTROLLER_LINK_DECODED_STATE_SNAPSHOT,
} hmi_controller_link_decoded_type_t;

typedef struct {
    uint16_t original_seq;
    winder_link_msg_type_t original_type;
} hmi_controller_link_command_accepted_t;

typedef struct {
    uint16_t original_seq;
    winder_link_msg_type_t original_type;
    uint16_t reason_code;
} hmi_controller_link_command_rejected_t;

typedef struct {
    uint8_t machine_state;
    uint8_t homing_state;
    uint8_t job_state;
    uint16_t progress_permille;
    uint32_t wound_length_mm;
    uint32_t target_length_mm;
    uint16_t master_speed_centirps;
    uint16_t speed_override_permille;
    uint16_t error_code;
    uint16_t event_code;
} hmi_controller_link_state_snapshot_t;

typedef struct {
    hmi_controller_link_decoded_type_t type;
    union {
        hmi_controller_link_command_accepted_t command_accepted;
        hmi_controller_link_command_rejected_t command_rejected;
        hmi_controller_link_state_snapshot_t state_snapshot;
    } data;
} hmi_controller_link_decoded_t;

bool hmi_controller_link_encode_message(
    const hmi_controller_message_t *message,
    hmi_controller_link_encoded_t *out_encoded
);

bool hmi_controller_link_decode_message(
    winder_link_msg_type_t type,
    const uint8_t *payload,
    size_t payload_len,
    hmi_controller_link_decoded_t *out_decoded
);
