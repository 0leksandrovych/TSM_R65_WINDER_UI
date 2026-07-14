#include "hmi_controller_link_codec_selftest.h"

#include <stddef.h>
#include <stdint.h>

#include "hmi_controller_link_codec.h"
#include "hmi_controller_messages.h"
#include "hmi_link_state_mapper.h"
#include "winder_link_contract.h"
#include "winder_link_payload.h"
#include "winder_link_protocol.h"

#define HMI_LINK_SELFTEST_MAX_FRAME_SIZE \
    (WINDER_LINK_FRAME_OVERHEAD + WINDER_LINK_MAX_PAYLOAD_SIZE)

static bool decode_frame_from_bytes(
    const uint8_t *bytes,
    size_t byte_count,
    winder_link_frame_t *out_frame)
{
    if (bytes == NULL || out_frame == NULL) {
        return false;
    }

    winder_link_decoder_t decoder;
    winder_link_decoder_init(&decoder);

    winder_link_frame_t frame = {0};
    bool frame_seen = false;

    for (size_t i = 0; i < byte_count; i++) {
        winder_link_error_t error = WINDER_LINK_ERROR_NONE;
        winder_link_decode_result_t result =
            winder_link_decoder_push_byte(&decoder, bytes[i], &frame, &error);

        if (result == WINDER_LINK_DECODE_RESULT_ERROR) {
            return false;
        }

        if (result == WINDER_LINK_DECODE_RESULT_FRAME) {
            if (frame_seen) {
                return false;
            }
            frame_seen = true;
            *out_frame = frame;
        }
    }

    return frame_seen;
}

static bool encode_frame_from_encoded(
    const hmi_controller_link_encoded_t *encoded,
    uint16_t seq,
    uint8_t *frame_bytes,
    size_t frame_capacity,
    size_t *out_frame_len)
{
    if (encoded == NULL || frame_bytes == NULL || out_frame_len == NULL) {
        return false;
    }

    return winder_link_encode_frame(
        (uint8_t)encoded->type,
        seq,
        encoded->payload,
        (uint16_t)encoded->payload_len,
        frame_bytes,
        frame_capacity,
        out_frame_len);
}

static bool test_encode_start_homing_frame_loopback(void)
{
    hmi_controller_message_t message = {0};
    message.type = HMI_CONTROLLER_MSG_START_HOMING;

    hmi_controller_link_encoded_t encoded = {0};
    if (!hmi_controller_link_encode_message(&message, &encoded)) {
        return false;
    }

    uint8_t frame_bytes[HMI_LINK_SELFTEST_MAX_FRAME_SIZE] = {0};
    size_t frame_len = 0U;
    if (!encode_frame_from_encoded(&encoded, 0x1234U, frame_bytes, sizeof(frame_bytes), &frame_len)) {
        return false;
    }

    winder_link_frame_t frame = {0};
    if (!decode_frame_from_bytes(frame_bytes, frame_len, &frame)) {
        return false;
    }

    return frame.type == WINDER_LINK_MSG_START_HOMING &&
           frame.seq == 0x1234U &&
           frame.payload_len == 0U;
}

static bool test_encode_get_telemetry_frame(void)
{
    hmi_controller_message_t message = {
        .type = HMI_CONTROLLER_MSG_GET_TELEMETRY,
    };
    hmi_controller_link_encoded_t encoded = {0};

    if (!hmi_controller_link_encode_message(&message, &encoded)) {
        return false;
    }

    return encoded.type == WINDER_LINK_MSG_GET_TELEMETRY &&
           encoded.payload_len == 0U;
}

static bool test_encode_set_speed_override_frame_loopback(void)
{
    hmi_controller_message_t message = {0};
    message.type = HMI_CONTROLLER_MSG_SET_SPEED_OVERRIDE;
    message.data.speed_override_percent = 85.0f;

    hmi_controller_link_encoded_t encoded = {0};
    if (!hmi_controller_link_encode_message(&message, &encoded)) {
        return false;
    }

    uint8_t frame_bytes[HMI_LINK_SELFTEST_MAX_FRAME_SIZE] = {0};
    size_t frame_len = 0U;
    if (!encode_frame_from_encoded(&encoded, 0x1235U, frame_bytes, sizeof(frame_bytes), &frame_len)) {
        return false;
    }

    winder_link_frame_t frame = {0};
    if (!decode_frame_from_bytes(frame_bytes, frame_len, &frame)) {
        return false;
    }

    return frame.type == WINDER_LINK_MSG_SET_SPEED_OVERRIDE &&
           frame.seq == 0x1235U &&
           frame.payload_len == 2U &&
           frame.payload[0] == 0x52U &&
           frame.payload[1] == 0x03U;
}

static bool test_encode_start_job_keyed_payload(void)
{
    hmi_controller_message_t message = {0};
    message.type = HMI_CONTROLLER_MSG_START_JOB;
    message.data.job.mode_id = HMI_JOB_MODE_CONICAL;
    message.data.job.param_count = 5U;
    message.data.job.params[0] = (hmi_controller_param_value_t){
        .param_id = 1U,
        .type = HMI_PARAM_TYPE_FLOAT,
        .value.f32 = 2.50f,
    };
    message.data.job.params[1] = (hmi_controller_param_value_t){
        .param_id = 2U,
        .type = HMI_PARAM_TYPE_FLOAT,
        .value.f32 = 0.80f,
    };
    message.data.job.params[2] = (hmi_controller_param_value_t){
        .param_id = 3U,
        .type = HMI_PARAM_TYPE_FLOAT,
        .value.f32 = 120.0f,
    };
    message.data.job.params[3] = (hmi_controller_param_value_t){
        .param_id = 4U,
        .type = HMI_PARAM_TYPE_UINT32,
        .value.u32 = 3U,
    };
    message.data.job.params[4] = (hmi_controller_param_value_t){
        .param_id = 5U,
        .type = HMI_PARAM_TYPE_FLOAT,
        .value.f32 = 1.00f,
    };

    hmi_controller_link_encoded_t encoded = {0};
    if (!hmi_controller_link_encode_message(&message, &encoded)) {
        return false;
    }

    winder_link_payload_reader_t reader;
    if (!winder_link_payload_reader_init(&reader, encoded.payload, encoded.payload_len)) {
        return false;
    }

    uint8_t param_count = 0U;
    if (!winder_link_payload_read_u8(&reader, &param_count) || param_count != 5U) {
        return false;
    }

    const uint16_t expected_ids[] = {1U, 2U, 3U, 4U, 5U};
    const int32_t expected_values[] = {250, 80, 120000, 3, 100};
    for (size_t i = 0; i < param_count; i++) {
        uint16_t param_id = 0U;
        int32_t scaled_value = 0;
        if (!winder_link_payload_read_u16_le(&reader, &param_id) ||
            !winder_link_payload_read_i32_le(&reader, &scaled_value)) {
            return false;
        }
        if (param_id != expected_ids[i] || scaled_value != expected_values[i]) {
            return false;
        }
    }

    return encoded.type == WINDER_LINK_MSG_START_JOB &&
           encoded.payload_len == 31U &&
           winder_link_payload_reader_done(&reader);
}

static bool test_encode_start_job_unknown_param_rejected(void)
{
    hmi_controller_message_t message = {0};
    message.type = HMI_CONTROLLER_MSG_START_JOB;
    message.data.job.mode_id = HMI_JOB_MODE_CONICAL;
    message.data.job.param_count = 1U;
    message.data.job.params[0] = (hmi_controller_param_value_t){
        .param_id = 99U,
        .type = HMI_PARAM_TYPE_FLOAT,
        .value.f32 = 1.0f,
    };

    hmi_controller_link_encoded_t encoded = {0};
    return !hmi_controller_link_encode_message(&message, &encoded);
}

static bool encode_payload_frame(
    winder_link_msg_type_t type,
    uint16_t seq,
    const uint8_t *payload,
    size_t payload_len,
    winder_link_frame_t *out_frame)
{
    uint8_t frame_bytes[HMI_LINK_SELFTEST_MAX_FRAME_SIZE] = {0};
    size_t frame_len = 0U;

    if (!winder_link_encode_frame(
            (uint8_t)type,
            seq,
            payload,
            (uint16_t)payload_len,
            frame_bytes,
            sizeof(frame_bytes),
            &frame_len)) {
        return false;
    }

    return decode_frame_from_bytes(frame_bytes, frame_len, out_frame);
}

static bool test_decode_command_accepted_frame_loopback(void)
{
    uint8_t payload[WINDER_LINK_MAX_PAYLOAD_SIZE] = {0};
    winder_link_payload_writer_t writer;
    if (!winder_link_payload_writer_init(&writer, payload, sizeof(payload))) {
        return false;
    }
    if (!winder_link_payload_write_u16_le(&writer, 0x1234U)) {
        return false;
    }
    if (!winder_link_payload_write_u8(&writer, WINDER_LINK_MSG_START_HOMING)) {
        return false;
    }

    winder_link_frame_t frame = {0};
    if (!encode_payload_frame(
            WINDER_LINK_MSG_COMMAND_ACCEPTED,
            0x2001U,
            payload,
            winder_link_payload_writer_len(&writer),
            &frame)) {
        return false;
    }

    hmi_controller_link_decoded_t decoded = {0};
    if (!hmi_controller_link_decode_message(
            (winder_link_msg_type_t)frame.type,
            frame.payload,
            frame.payload_len,
            &decoded)) {
        return false;
    }

    return decoded.type == HMI_CONTROLLER_LINK_DECODED_COMMAND_ACCEPTED &&
           decoded.data.command_accepted.original_seq == 0x1234U &&
           decoded.data.command_accepted.original_type == WINDER_LINK_MSG_START_HOMING;
}

static bool test_decode_command_rejected_frame_loopback(void)
{
    uint8_t payload[WINDER_LINK_MAX_PAYLOAD_SIZE] = {0};
    winder_link_payload_writer_t writer;
    if (!winder_link_payload_writer_init(&writer, payload, sizeof(payload))) {
        return false;
    }
    if (!winder_link_payload_write_u16_le(&writer, 0x1236U)) {
        return false;
    }
    if (!winder_link_payload_write_u8(&writer, WINDER_LINK_MSG_START_JOB)) {
        return false;
    }
    if (!winder_link_payload_write_u16_le(&writer, 2U)) {
        return false;
    }

    winder_link_frame_t frame = {0};
    if (!encode_payload_frame(
            WINDER_LINK_MSG_COMMAND_REJECTED,
            0x2002U,
            payload,
            winder_link_payload_writer_len(&writer),
            &frame)) {
        return false;
    }

    hmi_controller_link_decoded_t decoded = {0};
    if (!hmi_controller_link_decode_message(
            (winder_link_msg_type_t)frame.type,
            frame.payload,
            frame.payload_len,
            &decoded)) {
        return false;
    }

    return decoded.type == HMI_CONTROLLER_LINK_DECODED_COMMAND_REJECTED &&
           decoded.data.command_rejected.original_seq == 0x1236U &&
           decoded.data.command_rejected.original_type == WINDER_LINK_MSG_START_JOB &&
           decoded.data.command_rejected.reason_code == 2U;
}

typedef struct {
    uint16_t field_id;
    int32_t scaled_value;
} snapshot_test_field_t;

static bool decode_snapshot_fields(
    const snapshot_test_field_t *fields,
    size_t field_count,
    hmi_controller_link_decoded_t *out_decoded)
{
    if ((fields == NULL && field_count > 0U) ||
        field_count > UINT8_MAX ||
        out_decoded == NULL) {
        return false;
    }

    uint8_t payload[WINDER_LINK_MAX_PAYLOAD_SIZE] = {0};
    winder_link_payload_writer_t writer;
    if (!winder_link_payload_writer_init(&writer, payload, sizeof(payload)) ||
        !winder_link_payload_write_u8(&writer, (uint8_t)field_count)) {
        return false;
    }

    for (size_t i = 0; i < field_count; i++) {
        if (!winder_link_payload_write_u16_le(&writer, fields[i].field_id) ||
            !winder_link_payload_write_i32_le(&writer, fields[i].scaled_value)) {
            return false;
        }
    }

    winder_link_frame_t frame = {0};
    if (!encode_payload_frame(
            WINDER_LINK_MSG_STATE_SNAPSHOT,
            0x2003U,
            payload,
            winder_link_payload_writer_len(&writer),
            &frame)) {
        return false;
    }

    return hmi_controller_link_decode_message(
        (winder_link_msg_type_t)frame.type,
        frame.payload,
        frame.payload_len,
        out_decoded);
}

static bool test_decode_valid_machine_state(void)
{
    const snapshot_test_field_t fields[] = {
        { LINK_FIELD_MACHINE_STATE, LINK_MACHINE_STATE_READY },
    };
    hmi_controller_link_decoded_t decoded = {0};

    if (!decode_snapshot_fields(fields, 1U, &decoded)) {
        return false;
    }

    const hmi_controller_link_state_snapshot_t *state = &decoded.data.state_snapshot;
    return decoded.type == HMI_CONTROLLER_LINK_DECODED_STATE_SNAPSHOT &&
           state->machine_state_present &&
           state->machine_state == LINK_MACHINE_STATE_READY;
}

static bool test_decode_valid_homing_state(void)
{
    const snapshot_test_field_t fields[] = {
        { LINK_FIELD_HOMING_STATE, LINK_HOMING_STATE_COMPLETE },
    };
    hmi_controller_link_decoded_t decoded = {0};

    if (!decode_snapshot_fields(fields, 1U, &decoded)) {
        return false;
    }

    const hmi_controller_link_state_snapshot_t *state = &decoded.data.state_snapshot;
    return decoded.type == HMI_CONTROLLER_LINK_DECODED_STATE_SNAPSHOT &&
           state->homing_state_present &&
           state->homing_state == LINK_HOMING_STATE_COMPLETE;
}

static bool test_decode_snapshot_with_machine_and_homing_states(void)
{
    const snapshot_test_field_t fields[] = {
        { LINK_FIELD_MACHINE_STATE, LINK_MACHINE_STATE_HOMING },
        { LINK_FIELD_HOMING_STATE, LINK_HOMING_STATE_IN_PROGRESS },
    };
    hmi_controller_link_decoded_t decoded = {0};

    if (!decode_snapshot_fields(fields, 2U, &decoded)) {
        return false;
    }

    const hmi_controller_link_state_snapshot_t *state = &decoded.data.state_snapshot;
    return state->machine_state_present &&
           state->machine_state == LINK_MACHINE_STATE_HOMING &&
           state->homing_state_present &&
           state->homing_state == LINK_HOMING_STATE_IN_PROGRESS;
}

static bool test_decode_unknown_snapshot_field_is_ignored(void)
{
    const snapshot_test_field_t fields[] = {
        { 0x9999U, 123 },
        { LINK_FIELD_MACHINE_STATE, LINK_MACHINE_STATE_RUNNING },
    };
    hmi_controller_link_decoded_t decoded = {0};

    if (!decode_snapshot_fields(fields, 2U, &decoded)) {
        return false;
    }

    const hmi_controller_link_state_snapshot_t *state = &decoded.data.state_snapshot;
    return state->machine_state_present &&
           state->machine_state == LINK_MACHINE_STATE_RUNNING &&
           !state->homing_state_present;
}

static bool test_unsupported_enum_values_do_not_map(void)
{
    const snapshot_test_field_t fields[] = {
        { LINK_FIELD_MACHINE_STATE, 99 },
        { LINK_FIELD_HOMING_STATE, 99 },
    };
    hmi_controller_link_decoded_t decoded = {0};

    if (!decode_snapshot_fields(fields, 2U, &decoded)) {
        return false;
    }

    const hmi_controller_link_state_snapshot_t *snapshot = &decoded.data.state_snapshot;
    hmi_machine_state_t machine_state = HMI_MACHINE_READY;
    hmi_homing_state_t homing_state = HMI_HOMING_OK;

    return snapshot->machine_state_present &&
           snapshot->homing_state_present &&
           !hmi_link_state_mapper_machine_state(snapshot->machine_state, &machine_state) &&
           machine_state == HMI_MACHINE_READY &&
           !hmi_link_state_mapper_homing_state(snapshot->homing_state, &homing_state) &&
           homing_state == HMI_HOMING_OK;
}

static bool test_decode_state_snapshot_frame_loopback(void)
{
    const snapshot_test_field_t fields[] = {
        { LINK_FIELD_MACHINE_STATE, LINK_MACHINE_STATE_READY },
        { LINK_FIELD_HOMING_STATE, LINK_HOMING_STATE_COMPLETE },
        { LINK_FIELD_JOB_MASTER_SPEED, 250 },
        { LINK_FIELD_JOB_WINDING_PITCH, 80 },
        { LINK_FIELD_JOB_TARGET_LENGTH, 120000 },
        { LINK_FIELD_JOB_SHIFT_EVERY, 3 },
        { LINK_FIELD_JOB_RIGHT_EDGE_SHIFT, 100 },
    };
    hmi_controller_link_decoded_t decoded = {0};

    if (!decode_snapshot_fields(
            fields,
            sizeof(fields) / sizeof(fields[0]),
            &decoded)) {
        return false;
    }

    const hmi_controller_link_state_snapshot_t *state = &decoded.data.state_snapshot;
    return decoded.type == HMI_CONTROLLER_LINK_DECODED_STATE_SNAPSHOT &&
           state->machine_state_present &&
           state->machine_state == LINK_MACHINE_STATE_READY &&
           state->homing_state_present &&
           state->homing_state == LINK_HOMING_STATE_COMPLETE &&
           state->job_master_speed_present &&
           state->job_master_speed == 2.5 &&
           state->job_winding_pitch_present &&
           state->job_winding_pitch == 0.8 &&
           state->job_target_length_present &&
           state->job_target_length == 120.0 &&
           state->job_shift_every_present &&
           state->job_shift_every == 3.0 &&
           state->job_right_edge_shift_present &&
           state->job_right_edge_shift == 1.0;
}

static bool test_decode_command_accepted_truncated_payload_rejected(void)
{
    const uint8_t payload[] = {0x34U, 0x12U};
    hmi_controller_link_decoded_t decoded = {0};

    return !hmi_controller_link_decode_message(
        WINDER_LINK_MSG_COMMAND_ACCEPTED,
        payload,
        sizeof(payload),
        &decoded);
}

static bool test_decode_command_accepted_extra_payload_rejected(void)
{
    uint8_t payload[4] = {0};
    winder_link_payload_writer_t writer;
    if (!winder_link_payload_writer_init(&writer, payload, sizeof(payload))) {
        return false;
    }
    if (!winder_link_payload_write_u16_le(&writer, 0x1234U)) {
        return false;
    }
    if (!winder_link_payload_write_u8(&writer, WINDER_LINK_MSG_START_HOMING)) {
        return false;
    }
    if (!winder_link_payload_write_u8(&writer, 0xFFU)) {
        return false;
    }

    hmi_controller_link_decoded_t decoded = {0};
    return !hmi_controller_link_decode_message(
        WINDER_LINK_MSG_COMMAND_ACCEPTED,
        payload,
        winder_link_payload_writer_len(&writer),
        &decoded);
}

bool hmi_controller_link_codec_selftest(void)
{
    return test_encode_start_homing_frame_loopback() &&
           test_encode_get_telemetry_frame() &&
           test_encode_set_speed_override_frame_loopback() &&
           test_encode_start_job_keyed_payload() &&
           test_encode_start_job_unknown_param_rejected() &&
           test_decode_command_accepted_frame_loopback() &&
           test_decode_command_rejected_frame_loopback() &&
           test_decode_valid_machine_state() &&
           test_decode_valid_homing_state() &&
           test_decode_snapshot_with_machine_and_homing_states() &&
           test_decode_unknown_snapshot_field_is_ignored() &&
           test_unsupported_enum_values_do_not_map() &&
           test_decode_state_snapshot_frame_loopback() &&
           test_decode_command_accepted_truncated_payload_rejected() &&
           test_decode_command_accepted_extra_payload_rejected();
}
