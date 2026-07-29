#include "hmi_controller_link_codec.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>

#include "winder_link_contract.h"
#include "winder_link_payload.h"

_Static_assert(LINK_UPDATE_PAUSED_JOB_PARAM_COUNT == 6U,
               "paused job update must contain six parameters");
_Static_assert(LINK_UPDATE_PAUSED_JOB_PAYLOAD_ENCODED_SIZE == 37U,
               "paused job update payload must be exactly 37 bytes");

typedef enum {
    SNAPSHOT_VALUE_MACHINE_STATE = 0,
    SNAPSHOT_VALUE_CARRIAGE_POSITION,
    SNAPSHOT_VALUE_UINT32,
    SNAPSHOT_VALUE_DOUBLE,
} snapshot_value_type_t;

typedef struct {
    link_field_id_t field_id;
    double scale;
    size_t value_offset;
    size_t present_offset;
    snapshot_value_type_t value_type;
} snapshot_field_binding_t;

static const snapshot_field_binding_t snapshot_field_bindings[] = {
    {
        LINK_FIELD_MACHINE_STATE,
        1.0,
        offsetof(hmi_controller_link_state_snapshot_t, machine_state),
        offsetof(hmi_controller_link_state_snapshot_t, machine_state_present),
        SNAPSHOT_VALUE_MACHINE_STATE,
    },
    {
        LINK_FIELD_HOMING_ALARM_CODE,
        1.0,
        offsetof(hmi_controller_link_state_snapshot_t, homing_alarm_code),
        offsetof(hmi_controller_link_state_snapshot_t, homing_alarm_code_present),
        SNAPSHOT_VALUE_UINT32,
    },
    {
        LINK_FIELD_CARRIAGE_REFERENCE_POSITION,
        1.0,
        offsetof(hmi_controller_link_state_snapshot_t, carriage_reference_position),
        offsetof(hmi_controller_link_state_snapshot_t, carriage_reference_position_present),
        SNAPSHOT_VALUE_CARRIAGE_POSITION,
    },
    {
        LINK_FIELD_LEFT_EDGE_SAMPLE_COUNT,
        1.0,
        offsetof(hmi_controller_link_state_snapshot_t, left_edge_sample_count),
        offsetof(hmi_controller_link_state_snapshot_t, left_edge_sample_count_present),
        SNAPSHOT_VALUE_UINT32,
    },
    {
        LINK_FIELD_RIGHT_EDGE_SAMPLE_COUNT,
        1.0,
        offsetof(hmi_controller_link_state_snapshot_t, right_edge_sample_count),
        offsetof(hmi_controller_link_state_snapshot_t, right_edge_sample_count_present),
        SNAPSHOT_VALUE_UINT32,
    },
    {
        LINK_FIELD_HOMING_SAMPLE_TARGET_COUNT,
        1.0,
        offsetof(hmi_controller_link_state_snapshot_t, homing_sample_target_count),
        offsetof(hmi_controller_link_state_snapshot_t, homing_sample_target_count_present),
        SNAPSHOT_VALUE_UINT32,
    },
    {
        LINK_FIELD_JOB_MASTER_SPEED,
        100.0,
        offsetof(hmi_controller_link_state_snapshot_t, job_master_speed),
        offsetof(hmi_controller_link_state_snapshot_t, job_master_speed_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
    {
        LINK_FIELD_MASTER_SPEED_RPS,
        100.0,
        offsetof(hmi_controller_link_state_snapshot_t, master_speed_rps),
        offsetof(hmi_controller_link_state_snapshot_t, master_speed_rps_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
    {
        LINK_FIELD_WOUND_LENGTH_M,
        1000.0,
        offsetof(hmi_controller_link_state_snapshot_t, wound_length_m),
        offsetof(hmi_controller_link_state_snapshot_t, wound_length_m_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
    {
        LINK_FIELD_COMPLETED_LAYERS,
        1.0,
        offsetof(hmi_controller_link_state_snapshot_t, completed_layers),
        offsetof(hmi_controller_link_state_snapshot_t, completed_layers_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
    {
        LINK_FIELD_APPLIED_RIGHT_EDGE_OFFSET_MM,
        100.0,
        offsetof(hmi_controller_link_state_snapshot_t, applied_right_edge_offset_mm),
        offsetof(hmi_controller_link_state_snapshot_t, applied_right_edge_offset_mm_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
    {
        LINK_FIELD_ACTIVE_LEFT_EDGE_TRIM_MM,
        100.0,
        offsetof(hmi_controller_link_state_snapshot_t, active_left_edge_trim_mm),
        offsetof(hmi_controller_link_state_snapshot_t, active_left_edge_trim_mm_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
    {
        LINK_FIELD_ACTIVE_RIGHT_EDGE_TRIM_MM,
        100.0,
        offsetof(hmi_controller_link_state_snapshot_t, active_right_edge_trim_mm),
        offsetof(hmi_controller_link_state_snapshot_t, active_right_edge_trim_mm_present),
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
    {
        LINK_FIELD_TRAVEL_RANGE_MM,
        100.0,
        offsetof(hmi_controller_link_state_snapshot_t, travel_range_mm),
        offsetof(hmi_controller_link_state_snapshot_t, travel_range_mm_present),
        SNAPSHOT_VALUE_DOUBLE,
    },
    {
        LINK_FIELD_JOB_PAUSE_REASON,
        1.0,
        offsetof(hmi_controller_link_state_snapshot_t, job_pause_reason),
        offsetof(hmi_controller_link_state_snapshot_t, job_pause_reason_present),
        SNAPSHOT_VALUE_UINT32,
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
        if (param->wire_param_id == 0U ||
            !isfinite(param->wire_scale) ||
            !(param->wire_scale > 0.0)) {
            return false;
        }

        double value = 0.0;
        int32_t scaled_value = 0;
        if (!param_value_as_double(param->type, param->value, &value) ||
            !scale_to_i32(value, param->wire_scale, &scaled_value)) {
            return false;
        }

        if (!winder_link_payload_write_u16_le(&writer, param->wire_param_id) ||
            !winder_link_payload_write_i32_le(&writer, scaled_value)) {
            return false;
        }
    }

    out_encoded->type = WINDER_LINK_MSG_START_JOB;
    out_encoded->payload_len = winder_link_payload_writer_len(&writer);
    return true;
}

static bool write_keyed_i32(winder_link_payload_writer_t *writer,
                            link_param_id_t param_id,
                            int32_t scaled_value)
{
    return winder_link_payload_write_u16_le(writer, (uint16_t)param_id) &&
           winder_link_payload_write_i32_le(writer, scaled_value);
}

static bool encode_paused_job_update(
    const hmi_controller_message_t *message,
    hmi_controller_link_encoded_t *out_encoded)
{
    const hmi_controller_paused_job_update_t *update =
        &message->data.paused_job_update;
    int32_t master_speed = 0;
    int32_t winding_pitch = 0;
    int32_t right_edge_shift = 0;
    int32_t additional_length = 0;

    if (update->shift_every_layers > (uint32_t)INT32_MAX ||
        !scale_to_i32(update->master_speed_rps, 100.0, &master_speed) ||
        !scale_to_i32(update->winding_pitch_mm, 100.0, &winding_pitch) ||
        !scale_to_i32(update->right_edge_shift_mm, 100.0, &right_edge_shift) ||
        !scale_to_i32(update->additional_length_present
                          ? update->additional_length_m
                          : 0.0,
                      1000.0,
                      &additional_length)) {
        return false;
    }

    winder_link_payload_writer_t writer;
    if (!winder_link_payload_writer_init(
            &writer, out_encoded->payload, sizeof(out_encoded->payload)) ||
        !winder_link_payload_write_u8(
            &writer, LINK_UPDATE_PAUSED_JOB_PARAM_COUNT) ||
        !write_keyed_i32(&writer, LINK_PARAM_JOB_MASTER_SPEED, master_speed) ||
        !write_keyed_i32(&writer, LINK_PARAM_JOB_WINDING_PITCH, winding_pitch) ||
        !write_keyed_i32(&writer,
                         LINK_PARAM_JOB_SHIFT_EVERY,
                         (int32_t)update->shift_every_layers) ||
        !write_keyed_i32(&writer,
                         LINK_PARAM_JOB_RIGHT_EDGE_SHIFT,
                         right_edge_shift) ||
        !write_keyed_i32(&writer,
                         LINK_PARAM_ADDITIONAL_LENGTH_PRESENT,
                         update->additional_length_present ? 1 : 0) ||
        !write_keyed_i32(&writer,
                         LINK_PARAM_ADDITIONAL_LENGTH_M,
                         additional_length)) {
        return false;
    }

    out_encoded->type = WINDER_LINK_MSG_UPDATE_PAUSED_JOB;
    out_encoded->payload_len = winder_link_payload_writer_len(&writer);
    return out_encoded->payload_len ==
           LINK_UPDATE_PAUSED_JOB_PAYLOAD_ENCODED_SIZE;
}

static bool encode_edge_trim(
    const hmi_controller_message_t *message,
    hmi_controller_link_encoded_t *out_encoded)
{
    int32_t left_centi_mm = 0;
    int32_t right_centi_mm = 0;
    if (!scale_to_i32(message->data.edge_trim.left_trim_mm,
                      100.0,
                      &left_centi_mm) ||
        !scale_to_i32(message->data.edge_trim.right_trim_mm,
                      100.0,
                      &right_centi_mm)) {
        return false;
    }

    winder_link_payload_writer_t writer;
    if (!winder_link_payload_writer_init(
            &writer,
            out_encoded->payload,
            sizeof(out_encoded->payload)) ||
        !winder_link_payload_write_u8(&writer, LINK_EDGE_TRIM_PARAM_COUNT) ||
        !winder_link_payload_write_u16_le(
            &writer,
            LINK_PARAM_LEFT_EDGE_TRIM_MM) ||
        !winder_link_payload_write_i32_le(&writer, left_centi_mm) ||
        !winder_link_payload_write_u16_le(
            &writer,
            LINK_PARAM_RIGHT_EDGE_TRIM_MM) ||
        !winder_link_payload_write_i32_le(&writer, right_centi_mm)) {
        return false;
    }

    out_encoded->type = WINDER_LINK_MSG_APPLY_EDGE_TRIM;
    out_encoded->payload_len = winder_link_payload_writer_len(&writer);
    return out_encoded->payload_len == LINK_EDGE_TRIM_PAYLOAD_ENCODED_SIZE;
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

static bool decode_resume_rejected(
    winder_link_payload_reader_t *reader,
    hmi_controller_link_decoded_t *out_decoded)
{
    hmi_controller_link_resume_rejected_t rejected = {0};
    if (!winder_link_payload_read_u16_le(reader, &rejected.reason_code) ||
        !winder_link_payload_reader_done(reader)) {
        return false;
    }

    out_decoded->type = HMI_CONTROLLER_LINK_DECODED_RESUME_REJECTED;
    out_decoded->data.resume_rejected = rejected;
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
        if (scaled_value < (int32_t)LINK_MACHINE_STATE_HOMING_REQUIRED ||
            scaled_value > (int32_t)LINK_MACHINE_STATE_HOMING_MASTER_POSITIONING) {
            *present = false;
            return true;
        }
        *(link_machine_state_t *)(base + binding->value_offset) =
            (link_machine_state_t)scaled_value;
        *present = true;
        return true;
    case SNAPSHOT_VALUE_CARRIAGE_POSITION:
        if (scaled_value < (int32_t)LINK_CARRIAGE_REFERENCE_POSITION_UNKNOWN ||
            scaled_value > (int32_t)LINK_CARRIAGE_REFERENCE_POSITION_MOVING) {
            *present = false;
            return true;
        }
        *(link_carriage_reference_position_t *)(base + binding->value_offset) =
            (link_carriage_reference_position_t)scaled_value;
        *present = true;
        return true;
    case SNAPSHOT_VALUE_UINT32:
        if (scaled_value < 0) {
            *present = false;
            return true;
        }
        *(uint32_t *)(base + binding->value_offset) = (uint32_t)scaled_value;
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
    case HMI_CONTROLLER_MSG_HOMING_NEXT_MEASUREMENT:
        return encode_empty(out_encoded, WINDER_LINK_MSG_HOMING_NEXT_MEASUREMENT);
    case HMI_CONTROLLER_MSG_MOVE_CARRIAGE_TO_ZERO:
        return encode_empty(out_encoded, WINDER_LINK_MSG_MOVE_CARRIAGE_TO_ZERO);
    case HMI_CONTROLLER_MSG_MOVE_CARRIAGE_TO_LEFT_EDGE:
        return encode_empty(out_encoded, WINDER_LINK_MSG_MOVE_CARRIAGE_TO_LEFT_EDGE);
    case HMI_CONTROLLER_MSG_PAUSE_JOB:
        return encode_empty(out_encoded, WINDER_LINK_MSG_PAUSE_JOB);
    case HMI_CONTROLLER_MSG_RESUME_JOB:
        return encode_empty(out_encoded, WINDER_LINK_MSG_RESUME_JOB);
    case HMI_CONTROLLER_MSG_ABORT_JOB:
        return encode_empty(out_encoded, WINDER_LINK_MSG_ABORT_JOB);
    case HMI_CONTROLLER_MSG_FINISH_JOB:
        return encode_empty(out_encoded, WINDER_LINK_MSG_FINISH_JOB);
    case HMI_CONTROLLER_MSG_RESET_JOB:
        return encode_empty(out_encoded, WINDER_LINK_MSG_RESET_JOB);
    case HMI_CONTROLLER_MSG_RESET_ALARM:
        return encode_empty(out_encoded, WINDER_LINK_MSG_RESET_ALARM);
    case HMI_CONTROLLER_MSG_RESET_UNWOUND_COUNTER:
        return encode_empty(out_encoded, WINDER_LINK_MSG_RESET_UNWOUND_COUNTER);
    case HMI_CONTROLLER_MSG_SET_SPEED_OVERRIDE:
        return encode_speed_override(message, out_encoded);
    case HMI_CONTROLLER_MSG_START_JOB:
        return encode_start_job(message, out_encoded);
    case HMI_CONTROLLER_MSG_UPDATE_PAUSED_JOB:
        return encode_paused_job_update(message, out_encoded);
    case HMI_CONTROLLER_MSG_APPLY_EDGE_TRIM:
        return encode_edge_trim(message, out_encoded);
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
    case WINDER_LINK_MSG_RESUME_REJECTED:
        return decode_resume_rejected(&reader, out_decoded);
    case WINDER_LINK_MSG_STATE_SNAPSHOT:
        return decode_state_snapshot(&reader, out_decoded);
    default:
        return false;
    }
}
