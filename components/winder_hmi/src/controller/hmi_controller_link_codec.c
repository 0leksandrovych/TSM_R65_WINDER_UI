#include "hmi_controller_link_codec.h"

#include <limits.h>
#include <stddef.h>

#include "winder_link_contract.h"
#include "winder_link_payload.h"

typedef enum {
    HMI_CODEC_DEMO_PARAM_MASTER_SPEED = 1,
    HMI_CODEC_DEMO_PARAM_WINDING_PITCH,
    HMI_CODEC_DEMO_PARAM_TARGET_LENGTH,
    HMI_CODEC_DEMO_PARAM_SHIFT_EVERY,
    HMI_CODEC_DEMO_PARAM_RIGHT_EDGE_SHIFT,
} hmi_codec_demo_param_id_t;

typedef struct {
    uint16_t local_param_id;
    link_param_id_t wire_param_id;
    double scale;
} job_param_binding_t;

typedef enum {
    SNAPSHOT_VALUE_MACHINE_STATE = 0,
    SNAPSHOT_VALUE_HOMING_STATE,
    SNAPSHOT_VALUE_DOUBLE,
} snapshot_value_type_t;

typedef struct {
    link_field_id_t field_id;
    double scale;
    size_t value_offset;
    size_t present_offset;
    snapshot_value_type_t value_type;
} snapshot_field_binding_t;

static const job_param_binding_t job_param_bindings[] = {
    { HMI_CODEC_DEMO_PARAM_MASTER_SPEED,     LINK_PARAM_JOB_MASTER_SPEED,     100.0 },
    { HMI_CODEC_DEMO_PARAM_WINDING_PITCH,    LINK_PARAM_JOB_WINDING_PITCH,    100.0 },
    { HMI_CODEC_DEMO_PARAM_TARGET_LENGTH,    LINK_PARAM_JOB_TARGET_LENGTH,   1000.0 },
    { HMI_CODEC_DEMO_PARAM_SHIFT_EVERY,      LINK_PARAM_JOB_SHIFT_EVERY,        1.0 },
    { HMI_CODEC_DEMO_PARAM_RIGHT_EDGE_SHIFT, LINK_PARAM_JOB_RIGHT_EDGE_SHIFT,  100.0 },
};

static const snapshot_field_binding_t snapshot_field_bindings[] = {
    {
        LINK_FIELD_MACHINE_STATE,
        1.0,
        offsetof(hmi_controller_link_state_snapshot_t, machine_state),
        offsetof(hmi_controller_link_state_snapshot_t, machine_state_present),
        SNAPSHOT_VALUE_MACHINE_STATE,
    },
    {
        LINK_FIELD_HOMING_STATE,
        1.0,
        offsetof(hmi_controller_link_state_snapshot_t, homing_state),
        offsetof(hmi_controller_link_state_snapshot_t, homing_state_present),
        SNAPSHOT_VALUE_HOMING_STATE,
    },
    {
        LINK_FIELD_JOB_MASTER_SPEED,
        100.0,
        offsetof(hmi_controller_link_state_snapshot_t, job_master_speed),
        offsetof(hmi_controller_link_state_snapshot_t, job_master_speed_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
    {
        LINK_FIELD_JOB_WINDING_PITCH,
        100.0,
        offsetof(hmi_controller_link_state_snapshot_t, job_winding_pitch),
        offsetof(hmi_controller_link_state_snapshot_t, job_winding_pitch_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
    {
        LINK_FIELD_JOB_TARGET_LENGTH,
        1000.0,
        offsetof(hmi_controller_link_state_snapshot_t, job_target_length),
        offsetof(hmi_controller_link_state_snapshot_t, job_target_length_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
    {
        LINK_FIELD_JOB_SHIFT_EVERY,
        1.0,
        offsetof(hmi_controller_link_state_snapshot_t, job_shift_every),
        offsetof(hmi_controller_link_state_snapshot_t, job_shift_every_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
    {
        LINK_FIELD_JOB_RIGHT_EDGE_SHIFT,
        100.0,
        offsetof(hmi_controller_link_state_snapshot_t, job_right_edge_shift),
        offsetof(hmi_controller_link_state_snapshot_t, job_right_edge_shift_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
};

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

static const job_param_binding_t *find_job_param_binding(uint16_t local_param_id)
{
    for (size_t i = 0; i < sizeof(job_param_bindings) / sizeof(job_param_bindings[0]); i++) {
        if (job_param_bindings[i].local_param_id == local_param_id) {
            return &job_param_bindings[i];
        }
    }

    return NULL;
}

static bool param_value_as_double(hmi_param_type_t type, hmi_param_value_t value, double *out_value)
{
    if (out_value == NULL) {
        return false;
    }

    switch (type) {
    case HMI_PARAM_TYPE_FLOAT:
        *out_value = (double)value.f32;
        return true;
    case HMI_PARAM_TYPE_UINT32:
        *out_value = (double)value.u32;
        return true;
    case HMI_PARAM_TYPE_BOOL:
        *out_value = value.boolean ? 1.0 : 0.0;
        return true;
    case HMI_PARAM_TYPE_ENUM:
        *out_value = (double)value.enum_value;
        return true;
    default:
        return false;
    }
}

static bool scale_to_i32(double value, double scale, int32_t *out_value)
{
    if (out_value == NULL) {
        return false;
    }

    const double scaled = value * scale;
    if (!(scaled <= (double)INT32_MAX && scaled >= (double)INT32_MIN)) {
        return false;
    }

    *out_value = (int32_t)(scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
    return true;
}

static bool encode_start_job(
    const hmi_controller_message_t *message,
    hmi_controller_link_encoded_t *out_encoded)
{
    const hmi_controller_job_payload_t *job = &message->data.job;

    if (job->param_count > LINK_MAX_JOB_PARAMS ||
        job->param_count > HMI_CONTROLLER_MAX_JOB_PARAMS) {
        return false;
    }

    winder_link_payload_writer_t writer;
    if (!winder_link_payload_writer_init(&writer, out_encoded->payload, sizeof(out_encoded->payload))) {
        return false;
    }
    if (!winder_link_payload_write_u8(&writer, (uint8_t)job->param_count)) {
        return false;
    }

    for (size_t i = 0; i < job->param_count; i++) {
        const hmi_controller_param_value_t *param = &job->params[i];
        const job_param_binding_t *binding = find_job_param_binding(param->param_id);
        if (binding == NULL) {
            return false;
        }

        double value = 0.0;
        int32_t scaled_value = 0;
        if (!param_value_as_double(param->type, param->value, &value) ||
            !scale_to_i32(value, binding->scale, &scaled_value)) {
            return false;
        }

        if (!winder_link_payload_write_u16_le(&writer, (uint16_t)binding->wire_param_id) ||
            !winder_link_payload_write_i32_le(&writer, scaled_value)) {
            return false;
        }
    }

    out_encoded->type = WINDER_LINK_MSG_START_JOB;
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

static const snapshot_field_binding_t *find_snapshot_field_binding(link_field_id_t field_id)
{
    for (size_t i = 0; i < sizeof(snapshot_field_bindings) / sizeof(snapshot_field_bindings[0]); i++) {
        if (snapshot_field_bindings[i].field_id == field_id) {
            return &snapshot_field_bindings[i];
        }
    }

    return NULL;
}

static bool apply_snapshot_field(
    hmi_controller_link_state_snapshot_t *state,
    const snapshot_field_binding_t *binding,
    int32_t scaled_value)
{
    if (state == NULL || binding == NULL) {
        return false;
    }

    uint8_t *base = (uint8_t *)state;
    bool *present = (bool *)(base + binding->present_offset);

    switch (binding->value_type) {
    case SNAPSHOT_VALUE_MACHINE_STATE:
        *(link_machine_state_t *)(base + binding->value_offset) =
            (link_machine_state_t)scaled_value;
        *present = true;
        return true;
    case SNAPSHOT_VALUE_HOMING_STATE:
        *(link_homing_state_t *)(base + binding->value_offset) =
            (link_homing_state_t)scaled_value;
        *present = true;
        return true;
    case SNAPSHOT_VALUE_DOUBLE:
        if (!(binding->scale > 0.0)) {
            return false;
        }
        *(double *)(base + binding->value_offset) = (double)scaled_value / binding->scale;
        *present = true;
        return true;
    default:
        return false;
    }
}

static bool decode_state_snapshot(
    winder_link_payload_reader_t *reader,
    hmi_controller_link_decoded_t *out_decoded)
{
    hmi_controller_link_state_snapshot_t state = {0};
    uint8_t field_count = 0U;

    if (!winder_link_payload_read_u8(reader, &field_count)) {
        return false;
    }

    for (uint8_t i = 0U; i < field_count; i++) {
        uint16_t field_id = 0U;
        int32_t scaled_value = 0;

        if (!winder_link_payload_read_u16_le(reader, &field_id) ||
            !winder_link_payload_read_i32_le(reader, &scaled_value)) {
            return false;
        }

        const snapshot_field_binding_t *binding =
            find_snapshot_field_binding((link_field_id_t)field_id);
        if (binding == NULL) {
            continue;
        }
        if (!apply_snapshot_field(&state, binding, scaled_value)) {
            return false;
        }
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
    case HMI_CONTROLLER_MSG_START_JOB:
        return encode_start_job(message, out_encoded);
    case HMI_CONTROLLER_MSG_APPLY_EDGE_TRIM:
        /* APPLY_EDGE_TRIM carries empty payload because the current HMI command
         * only requests applying the already staged edge trim state. It does not
         * transfer trim values. */
        return encode_empty(out_encoded, WINDER_LINK_MSG_APPLY_EDGE_TRIM);
    case HMI_CONTROLLER_MSG_GET_TELEMETRY:
        return encode_empty(out_encoded, WINDER_LINK_MSG_GET_TELEMETRY);
    case HMI_CONTROLLER_MSG_NONE:
    case HMI_CONTROLLER_MSG_GET_CAPABILITIES:
    case HMI_CONTROLLER_MSG_VALIDATE_JOB:
    default:
        /* Capability and unsupported job payload mappings are intentionally out of scope here. */
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
