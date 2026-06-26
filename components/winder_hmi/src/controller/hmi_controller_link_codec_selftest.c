#include "hmi_controller_link_codec_selftest.h"

#include <stddef.h>
#include <stdint.h>

#include "hmi_controller_link_codec.h"
#include "hmi_controller_messages.h"
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

static bool test_decode_state_snapshot_frame_loopback(void)
{
    uint8_t payload[WINDER_LINK_MAX_PAYLOAD_SIZE] = {0};
    winder_link_payload_writer_t writer;
    if (!winder_link_payload_writer_init(&writer, payload, sizeof(payload))) {
        return false;
    }
    if (!winder_link_payload_write_u8(&writer, 1U)) {
        return false;
    }
    if (!winder_link_payload_write_u8(&writer, 2U)) {
        return false;
    }
    if (!winder_link_payload_write_u8(&writer, 3U)) {
        return false;
    }
    if (!winder_link_payload_write_u16_le(&writer, 500U)) {
        return false;
    }
    if (!winder_link_payload_write_u32_le(&writer, 1200U)) {
        return false;
    }
    if (!winder_link_payload_write_u32_le(&writer, 5000U)) {
        return false;
    }
    if (!winder_link_payload_write_u16_le(&writer, 250U)) {
        return false;
    }
    if (!winder_link_payload_write_u16_le(&writer, 850U)) {
        return false;
    }
    if (!winder_link_payload_write_u16_le(&writer, 0U)) {
        return false;
    }
    if (!winder_link_payload_write_u16_le(&writer, 7U)) {
        return false;
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

    hmi_controller_link_decoded_t decoded = {0};
    if (!hmi_controller_link_decode_message(
            (winder_link_msg_type_t)frame.type,
            frame.payload,
            frame.payload_len,
            &decoded)) {
        return false;
    }

    const hmi_controller_link_state_snapshot_t *state = &decoded.data.state_snapshot;
    return decoded.type == HMI_CONTROLLER_LINK_DECODED_STATE_SNAPSHOT &&
           state->machine_state == 1U &&
           state->homing_state == 2U &&
           state->job_state == 3U &&
           state->progress_permille == 500U &&
           state->wound_length_mm == 1200U &&
           state->target_length_mm == 5000U &&
           state->master_speed_centirps == 250U &&
           state->speed_override_permille == 850U &&
           state->error_code == 0U &&
           state->event_code == 7U;
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
           test_encode_set_speed_override_frame_loopback() &&
           test_decode_command_accepted_frame_loopback() &&
           test_decode_command_rejected_frame_loopback() &&
           test_decode_state_snapshot_frame_loopback() &&
           test_decode_command_accepted_truncated_payload_rejected() &&
           test_decode_command_accepted_extra_payload_rejected();
}
