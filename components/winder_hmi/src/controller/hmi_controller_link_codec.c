#include "hmi_controller_link_codec.h"

#include "winder_link_payload.h"

static bool encode_empty(
    hmi_controller_link_encoded_t *out_encoded,
    winder_link_msg_type_t type)
{
    out_encoded->type = type;
    out_encoded->payload_len = 0U;
    return true;
}

static bool encode_speed_override(
    const hmi_controller_message_t *message,
    hmi_controller_link_encoded_t *out_encoded)
{
    const float percent = message->data.speed_override_percent;
    if (!(percent >= 0.0f) || !(percent <= 6553.5f)) {
        return false;
    }

    const uint16_t speed_override_permille = (uint16_t)((percent * 10.0f) + 0.5f);

    winder_link_payload_writer_t writer;
    if (!winder_link_payload_writer_init(&writer, out_encoded->payload, sizeof(out_encoded->payload))) {
        return false;
    }
    if (!winder_link_payload_write_u16_le(&writer, speed_override_permille)) {
        return false;
    }

    out_encoded->type = WINDER_LINK_MSG_SET_SPEED_OVERRIDE;
    out_encoded->payload_len = winder_link_payload_writer_len(&writer);
    return true;
}

static bool read_original_command(
    winder_link_payload_reader_t *reader,
    uint16_t *out_original_seq,
    winder_link_msg_type_t *out_original_type)
{
    uint8_t original_type = 0U;

    if (!winder_link_payload_read_u16_le(reader, out_original_seq)) {
        return false;
    }
    if (!winder_link_payload_read_u8(reader, &original_type)) {
        return false;
    }

    *out_original_type = (winder_link_msg_type_t)original_type;
    return true;
}

static bool decode_command_accepted(
    winder_link_payload_reader_t *reader,
    hmi_controller_link_decoded_t *out_decoded)
{
    hmi_controller_link_command_accepted_t accepted = {0};

    if (!read_original_command(reader, &accepted.original_seq, &accepted.original_type)) {
        return false;
    }
    if (!winder_link_payload_reader_done(reader)) {
        return false;
    }

    out_decoded->type = HMI_CONTROLLER_LINK_DECODED_COMMAND_ACCEPTED;
    out_decoded->data.command_accepted = accepted;
    return true;
}

static bool decode_command_rejected(
    winder_link_payload_reader_t *reader,
    hmi_controller_link_decoded_t *out_decoded)
{
    hmi_controller_link_command_rejected_t rejected = {0};

    if (!read_original_command(reader, &rejected.original_seq, &rejected.original_type)) {
        return false;
    }
    if (!winder_link_payload_read_u16_le(reader, &rejected.reason_code)) {
        return false;
    }
    if (!winder_link_payload_reader_done(reader)) {
        return false;
    }

    out_decoded->type = HMI_CONTROLLER_LINK_DECODED_COMMAND_REJECTED;
    out_decoded->data.command_rejected = rejected;
    return true;
}

static bool decode_state_snapshot(
    winder_link_payload_reader_t *reader,
    hmi_controller_link_decoded_t *out_decoded)
{
    hmi_controller_link_state_snapshot_t state = {0};

    if (!winder_link_payload_read_u8(reader, &state.machine_state)) {
        return false;
    }
    if (!winder_link_payload_read_u8(reader, &state.homing_state)) {
        return false;
    }
    if (!winder_link_payload_read_u8(reader, &state.job_state)) {
        return false;
    }
    if (!winder_link_payload_read_u16_le(reader, &state.progress_permille)) {
        return false;
    }
    if (!winder_link_payload_read_u32_le(reader, &state.wound_length_mm)) {
        return false;
    }
    if (!winder_link_payload_read_u32_le(reader, &state.target_length_mm)) {
        return false;
    }
    if (!winder_link_payload_read_u16_le(reader, &state.master_speed_centirps)) {
        return false;
    }
    if (!winder_link_payload_read_u16_le(reader, &state.speed_override_permille)) {
        return false;
    }
    if (!winder_link_payload_read_u16_le(reader, &state.error_code)) {
        return false;
    }
    if (!winder_link_payload_read_u16_le(reader, &state.event_code)) {
        return false;
    }
    if (!winder_link_payload_reader_done(reader)) {
        return false;
    }

    out_decoded->type = HMI_CONTROLLER_LINK_DECODED_STATE_SNAPSHOT;
    out_decoded->data.state_snapshot = state;
    return true;
}

bool hmi_controller_link_encode_message(
    const hmi_controller_message_t *message,
    hmi_controller_link_encoded_t *out_encoded)
{
    if (out_encoded == NULL) {
        return false;
    }

    *out_encoded = (hmi_controller_link_encoded_t){0};

    if (message == NULL) {
        return false;
    }

    /* PING is not mapped because no internal HMI controller message exists yet. */
    switch (message->type) {
    case HMI_CONTROLLER_MSG_START_HOMING:
        return encode_empty(out_encoded, WINDER_LINK_MSG_START_HOMING);
    case HMI_CONTROLLER_MSG_ABORT_HOMING:
        return encode_empty(out_encoded, WINDER_LINK_MSG_ABORT_HOMING);
    case HMI_CONTROLLER_MSG_PAUSE_JOB:
        return encode_empty(out_encoded, WINDER_LINK_MSG_PAUSE_JOB);
    case HMI_CONTROLLER_MSG_RESUME_JOB:
        return encode_empty(out_encoded, WINDER_LINK_MSG_RESUME_JOB);
    case HMI_CONTROLLER_MSG_STOP_JOB:
        return encode_empty(out_encoded, WINDER_LINK_MSG_STOP_JOB);
    case HMI_CONTROLLER_MSG_RESET_ALARM:
        return encode_empty(out_encoded, WINDER_LINK_MSG_RESET_ALARM);
    case HMI_CONTROLLER_MSG_RESET_UNWOUND_COUNTER:
        return encode_empty(out_encoded, WINDER_LINK_MSG_RESET_UNWOUND_COUNTER);
    case HMI_CONTROLLER_MSG_SET_SPEED_OVERRIDE:
        return encode_speed_override(message, out_encoded);
    case HMI_CONTROLLER_MSG_APPLY_EDGE_TRIM:
        /* APPLY_EDGE_TRIM carries empty payload because the current HMI command
         * only requests applying the already staged edge trim state. It does not
         * transfer trim values. */
        return encode_empty(out_encoded, WINDER_LINK_MSG_APPLY_EDGE_TRIM);
    case HMI_CONTROLLER_MSG_NONE:
    case HMI_CONTROLLER_MSG_GET_CAPABILITIES:
    case HMI_CONTROLLER_MSG_VALIDATE_JOB:
    case HMI_CONTROLLER_MSG_START_JOB:
    default:
        /* Capability and job payload mappings are intentionally out of scope here. */
        return false;
    }
}

bool hmi_controller_link_decode_message(
    winder_link_msg_type_t type,
    const uint8_t *payload,
    size_t payload_len,
    hmi_controller_link_decoded_t *out_decoded)
{
    if (out_decoded == NULL) {
        return false;
    }

    *out_decoded = (hmi_controller_link_decoded_t){0};

    winder_link_payload_reader_t reader;
    if (!winder_link_payload_reader_init(&reader, payload, payload_len)) {
        return false;
    }

    switch (type) {
    case WINDER_LINK_MSG_COMMAND_ACCEPTED:
        return decode_command_accepted(&reader, out_decoded);
    case WINDER_LINK_MSG_COMMAND_REJECTED:
        return decode_command_rejected(&reader, out_decoded);
    case WINDER_LINK_MSG_STATE_SNAPSHOT:
        return decode_state_snapshot(&reader, out_decoded);
    default:
        return false;
    }
}
