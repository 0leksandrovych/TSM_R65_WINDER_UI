#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hmi_controller_messages.h"
#include "winder_link_contract.h"
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
    bool machine_state_present;
    link_machine_state_t machine_state;

    bool homing_alarm_code_present;
    uint32_t homing_alarm_code;

    bool carriage_reference_position_present;
    link_carriage_reference_position_t carriage_reference_position;

    bool left_edge_sample_count_present;
    uint32_t left_edge_sample_count;

    bool right_edge_sample_count_present;
    uint32_t right_edge_sample_count;

    bool homing_sample_target_count_present;
    uint32_t homing_sample_target_count;

    bool travel_range_mm_present;
    double travel_range_mm;

    bool job_master_speed_present;
    double job_master_speed;

    bool master_speed_rps_present;
    double master_speed_rps;

    bool wound_length_m_present;
    double wound_length_m;

    bool completed_layers_present;
    double completed_layers;

    bool applied_right_edge_offset_mm_present;
    double applied_right_edge_offset_mm;

    bool active_left_edge_trim_mm_present;
    double active_left_edge_trim_mm;

    bool active_right_edge_trim_mm_present;
    double active_right_edge_trim_mm;

    bool job_winding_pitch_present;
    double job_winding_pitch;

    bool job_target_length_present;
    double job_target_length;

    bool job_shift_every_present;
    double job_shift_every;

    bool job_right_edge_shift_present;
    double job_right_edge_shift;
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
