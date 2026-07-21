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
        .wire_param_id = LINK_PARAM_JOB_MASTER_SPEED,
        .wire_scale = 100.0,
        .type = HMI_PARAM_TYPE_FLOAT,
        .value.f32 = 2.50f,
    };
    message.data.job.params[1] = (hmi_controller_param_value_t){
        .wire_param_id = LINK_PARAM_JOB_WINDING_PITCH,
        .wire_scale = 100.0,
        .type = HMI_PARAM_TYPE_FLOAT,
        .value.f32 = 1.25f,
    };
    message.data.job.params[2] = (hmi_controller_param_value_t){
        .wire_param_id = LINK_PARAM_JOB_TARGET_LENGTH,
        .wire_scale = 1.0,
        .type = HMI_PARAM_TYPE_UINT32,
        .value.u32 = 500U,
    };
    message.data.job.params[3] = (hmi_controller_param_value_t){
        .wire_param_id = LINK_PARAM_JOB_SHIFT_EVERY,
        .wire_scale = 1.0,
        .type = HMI_PARAM_TYPE_UINT32,
        .value.u32 = 10U,
    };
    message.data.job.params[4] = (hmi_controller_param_value_t){
        .wire_param_id = LINK_PARAM_JOB_RIGHT_EDGE_SHIFT,
        .wire_scale = 100.0,
        .type = HMI_PARAM_TYPE_FLOAT,
        .value.f32 = 0.75f,
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
    const int32_t expected_values[] = {250, 125, 500, 10, 75};
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

static bool test_encode_start_job_invalid_wire_mapping_rejected(void)
{
    hmi_controller_message_t message = {0};
    message.type = HMI_CONTROLLER_MSG_START_JOB;
    message.data.job.mode_id = HMI_JOB_MODE_CONICAL;
    message.data.job.param_count = 1U;
    message.data.job.params[0] = (hmi_controller_param_value_t){
        .wire_param_id = 0U,
        .wire_scale = 1.0,
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

static bool test_machine_state_numeric_contract(void)
{
    return LINK_MACHINE_STATE_HOMING_REQUIRED == 0 &&
           LINK_MACHINE_STATE_HOMING_SEARCHING_RIGHT_REFERENCE == 1 &&
           LINK_MACHINE_STATE_HOMING_BACKING_OFF_RIGHT_REFERENCE == 2 &&
           LINK_MACHINE_STATE_HOMING_SEARCHING_LEFT_REFERENCE == 3 &&
           LINK_MACHINE_STATE_HOMING_BACKING_OFF_LEFT_REFERENCE == 4 &&
           LINK_MACHINE_STATE_HOMING_MEASURING_TRAVEL == 5 &&
           LINK_MACHINE_STATE_HOMING_APPLYING_OFFSET == 6 &&
           LINK_MACHINE_STATE_HOMING_COMPLETING == 7 &&
           LINK_MACHINE_STATE_READY == 8 &&
           LINK_MACHINE_STATE_ACCELERATING == 9 &&
           LINK_MACHINE_STATE_RUNNING == 10 &&
           LINK_MACHINE_STATE_PAUSED == 11 &&
           LINK_MACHINE_STATE_STOPPING == 12 &&
           LINK_MACHINE_STATE_FINISHED == 13 &&
           LINK_MACHINE_STATE_ALARM == 14 &&
           LINK_FIELD_TRAVEL_RANGE_MM == 8 &&
           LINK_FIELD_MASTER_SPEED_RPS == 9 &&
           WINDER_LINK_MSG_ABORT_HOMING == 0x06;
}

static bool test_machine_state_mapper(void)
{
    static const struct {
        link_machine_state_t link_state;
        hmi_machine_state_t hmi_state;
    } cases[] = {
        { LINK_MACHINE_STATE_HOMING_REQUIRED, HMI_MACHINE_HOMING_REQUIRED },
        { LINK_MACHINE_STATE_HOMING_SEARCHING_RIGHT_REFERENCE, HMI_MACHINE_HOMING_SEARCHING_RIGHT_REFERENCE },
        { LINK_MACHINE_STATE_HOMING_BACKING_OFF_RIGHT_REFERENCE, HMI_MACHINE_HOMING_BACKING_OFF_RIGHT_REFERENCE },
        { LINK_MACHINE_STATE_HOMING_SEARCHING_LEFT_REFERENCE, HMI_MACHINE_HOMING_SEARCHING_LEFT_REFERENCE },
        { LINK_MACHINE_STATE_HOMING_BACKING_OFF_LEFT_REFERENCE, HMI_MACHINE_HOMING_BACKING_OFF_LEFT_REFERENCE },
        { LINK_MACHINE_STATE_HOMING_MEASURING_TRAVEL, HMI_MACHINE_HOMING_MEASURING_TRAVEL },
        { LINK_MACHINE_STATE_HOMING_APPLYING_OFFSET, HMI_MACHINE_HOMING_APPLYING_OFFSET },
        { LINK_MACHINE_STATE_HOMING_COMPLETING, HMI_MACHINE_HOMING_COMPLETING },
        { LINK_MACHINE_STATE_READY, HMI_MACHINE_READY },
        { LINK_MACHINE_STATE_ACCELERATING, HMI_MACHINE_ACCELERATING },
        { LINK_MACHINE_STATE_RUNNING, HMI_MACHINE_RUNNING },
        { LINK_MACHINE_STATE_PAUSED, HMI_MACHINE_PAUSED },
        { LINK_MACHINE_STATE_STOPPING, HMI_MACHINE_STOPPING },
        { LINK_MACHINE_STATE_FINISHED, HMI_MACHINE_FINISHED },
        { LINK_MACHINE_STATE_ALARM, HMI_MACHINE_ALARM },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        hmi_machine_state_t mapped = HMI_MACHINE_ALARM;
        if (!hmi_link_state_mapper_machine_state(cases[i].link_state, &mapped) ||
            mapped != cases[i].hmi_state) {
            return false;
        }
    }

    hmi_machine_state_t unchanged = HMI_MACHINE_READY;
    return !hmi_link_state_mapper_machine_state((link_machine_state_t)99, &unchanged) &&
           unchanged == HMI_MACHINE_READY &&
           !hmi_link_state_mapper_machine_state(LINK_MACHINE_STATE_READY, NULL);
}

static bool test_encode_abort_homing_frame_loopback(void)
{
    hmi_controller_message_t message = {
        .type = HMI_CONTROLLER_MSG_ABORT_HOMING,
    };
    hmi_controller_link_encoded_t encoded = {0};
    if (!hmi_controller_link_encode_message(&message, &encoded)) {
        return false;
    }

    uint8_t frame_bytes[HMI_LINK_SELFTEST_MAX_FRAME_SIZE] = {0};
    size_t frame_len = 0U;
    winder_link_frame_t frame = {0};
    return encode_frame_from_encoded(&encoded, 0x1250U, frame_bytes, sizeof(frame_bytes), &frame_len) &&
           decode_frame_from_bytes(frame_bytes, frame_len, &frame) &&
           frame.type == WINDER_LINK_MSG_ABORT_HOMING &&
           frame.seq == 0x1250U &&
           frame.payload_len == 0U;
}

static bool test_decode_state_snapshot_frame_loopback(void)
{
    const snapshot_test_field_t fields[] = {
        { LINK_FIELD_MACHINE_STATE, LINK_MACHINE_STATE_HOMING_MEASURING_TRAVEL },
        { LINK_FIELD_TRAVEL_RANGE_MM, 15000 },
        { LINK_FIELD_JOB_MASTER_SPEED, 1000 },
        { LINK_FIELD_MASTER_SPEED_RPS, 1234 },
        { LINK_FIELD_JOB_WINDING_PITCH, 50 },
        { LINK_FIELD_JOB_TARGET_LENGTH, 100000 },
        { LINK_FIELD_JOB_SHIFT_EVERY, 3 },
        { LINK_FIELD_JOB_RIGHT_EDGE_SHIFT, 100 },
    };
    hmi_controller_link_decoded_t decoded = {0};

    if (!decode_snapshot_fields(fields, sizeof(fields) / sizeof(fields[0]), &decoded)) {
        return false;
    }

    const hmi_controller_link_state_snapshot_t *state = &decoded.data.state_snapshot;
    return decoded.type == HMI_CONTROLLER_LINK_DECODED_STATE_SNAPSHOT &&
           state->machine_state_present &&
           state->machine_state == LINK_MACHINE_STATE_HOMING_MEASURING_TRAVEL &&
           state->travel_range_mm_present &&
           state->travel_range_mm == 150.0 &&
           state->job_master_speed_present && state->job_master_speed == 10.0 &&
           state->master_speed_rps_present && state->master_speed_rps == 12.34 &&
           state->job_winding_pitch_present && state->job_winding_pitch == 0.5 &&
           state->job_target_length_present && state->job_target_length == 100.0 &&
           state->job_shift_every_present && state->job_shift_every == 3.0 &&
           state->job_right_edge_shift_present && state->job_right_edge_shift == 1.0;
}

static bool test_decode_runtime_speed_values(void)
{
    const snapshot_test_field_t zero_field[] = {
        { LINK_FIELD_MASTER_SPEED_RPS, 0 },
    };
    const snapshot_test_field_t running_field[] = {
        { LINK_FIELD_MASTER_SPEED_RPS, 625 },
    };
    hmi_controller_link_decoded_t zero = {0};
    hmi_controller_link_decoded_t running = {0};

    return decode_snapshot_fields(zero_field, 1U, &zero) &&
           zero.data.state_snapshot.master_speed_rps_present &&
           zero.data.state_snapshot.master_speed_rps == 0.0 &&
           !zero.data.state_snapshot.job_master_speed_present &&
           decode_snapshot_fields(running_field, 1U, &running) &&
           running.data.state_snapshot.master_speed_rps_present &&
           running.data.state_snapshot.master_speed_rps == 6.25;
}

static bool test_decode_snapshot_truncated_payload_rejected(void)
{
    uint8_t payload[7] = {0};
    winder_link_payload_writer_t writer;
    if (!winder_link_payload_writer_init(&writer, payload, sizeof(payload)) ||
        !winder_link_payload_write_u8(&writer, 2U) ||
        !winder_link_payload_write_u16_le(&writer, LINK_FIELD_MASTER_SPEED_RPS) ||
        !winder_link_payload_write_i32_le(&writer, 625)) {
        return false;
    }

    hmi_controller_link_decoded_t decoded = {0};
    return !hmi_controller_link_decode_message(
        WINDER_LINK_MSG_STATE_SNAPSHOT,
        payload,
        winder_link_payload_writer_len(&writer),
        &decoded);
}

static bool test_decode_travel_range_values(void)
{
    const snapshot_test_field_t zero_field[] = {
        { LINK_FIELD_TRAVEL_RANGE_MM, 0 },
    };
    const snapshot_test_field_t negative_field[] = {
        { LINK_FIELD_TRAVEL_RANGE_MM, -125 },
    };
    hmi_controller_link_decoded_t zero = {0};
    hmi_controller_link_decoded_t negative = {0};

    return decode_snapshot_fields(zero_field, 1U, &zero) &&
           zero.data.state_snapshot.travel_range_mm_present &&
           zero.data.state_snapshot.travel_range_mm == 0.0 &&
           decode_snapshot_fields(negative_field, 1U, &negative) &&
           negative.data.state_snapshot.travel_range_mm_present &&
           negative.data.state_snapshot.travel_range_mm == -1.25;
}

static bool test_unknown_and_missing_fields(void)
{
    const snapshot_test_field_t fields[] = {
        { 7U, 3 },
        { 0x9999U, 123 },
        { LINK_FIELD_MACHINE_STATE, LINK_MACHINE_STATE_RUNNING },
    };
    hmi_controller_link_decoded_t decoded = {0};
    hmi_controller_link_decoded_t empty = {0};

    if (!decode_snapshot_fields(fields, sizeof(fields) / sizeof(fields[0]), &decoded) ||
        !decode_snapshot_fields(NULL, 0U, &empty)) {
        return false;
    }

    return decoded.data.state_snapshot.machine_state_present &&
           decoded.data.state_snapshot.machine_state == LINK_MACHINE_STATE_RUNNING &&
           !decoded.data.state_snapshot.travel_range_mm_present &&
           !empty.data.state_snapshot.travel_range_mm_present;
}

static bool test_unknown_machine_state_does_not_map(void)
{
    const snapshot_test_field_t fields[] = {
        { LINK_FIELD_MACHINE_STATE, 99 },
    };
    hmi_controller_link_decoded_t decoded = {0};
    hmi_machine_state_t local_state = HMI_MACHINE_READY;

    return decode_snapshot_fields(fields, 1U, &decoded) &&
           decoded.data.state_snapshot.machine_state_present &&
           !hmi_link_state_mapper_machine_state(
               decoded.data.state_snapshot.machine_state,
               &local_state) &&
           local_state == HMI_MACHINE_READY;
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
           test_encode_abort_homing_frame_loopback() &&
           test_encode_get_telemetry_frame() &&
           test_encode_set_speed_override_frame_loopback() &&
           test_encode_start_job_keyed_payload() &&
           test_encode_start_job_invalid_wire_mapping_rejected() &&
           test_decode_command_accepted_frame_loopback() &&
           test_decode_command_rejected_frame_loopback() &&
           test_machine_state_numeric_contract() &&
           test_machine_state_mapper() &&
           test_decode_state_snapshot_frame_loopback() &&
           test_decode_runtime_speed_values() &&
           test_decode_snapshot_truncated_payload_rejected() &&
           test_decode_travel_range_values() &&
           test_unknown_and_missing_fields() &&
           test_unknown_machine_state_does_not_map() &&
           test_decode_command_accepted_truncated_payload_rejected() &&
           test_decode_command_accepted_extra_payload_rejected();
}
