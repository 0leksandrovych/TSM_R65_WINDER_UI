#include "winder_link_protocol_selftest.h"
#include "winder_link_payload.h"
#include "winder_link_protocol.h"

#include <string.h>

/* =========================================================================
 * Helper
 * ========================================================================= */

static winder_link_decode_result_t push_all(
    winder_link_decoder_t *dec,
    const uint8_t         *buf,
    size_t                 len,
    winder_link_frame_t   *out_frame,
    winder_link_error_t   *out_error)
{
    winder_link_decode_result_t result = WINDER_LINK_DECODE_RESULT_NONE;
    for (size_t i = 0; i < len; i++) {
        result = winder_link_decoder_push_byte(dec, buf[i], out_frame, out_error);
    }
    return result;
}

/* =========================================================================
 * Test cases
 * ========================================================================= */

/* Test 1 — CRC-16/CCITT-FALSE known vector: "123456789" → 0x29B1 */
static bool test_crc_known_vector(void)
{
    const uint8_t input[] = {'1','2','3','4','5','6','7','8','9'};
    return winder_link_crc16_ccitt_false(input, sizeof(input)) == 0x29B1U;
}

/* Test 2 — Encode then decode a zero-payload frame */
static bool test_roundtrip_empty_payload(void)
{
    uint8_t buf[WINDER_LINK_FRAME_OVERHEAD];
    size_t  len = 0;

    if (!winder_link_encode_frame(WINDER_LINK_MSG_PING, 0x0001U,
                                  NULL, 0, buf, sizeof(buf), &len)) {
        return false;
    }
    if (len != WINDER_LINK_FRAME_OVERHEAD) {
        return false;
    }

    winder_link_decoder_t       dec;
    winder_link_decoder_init(&dec);
    winder_link_frame_t  frame  = {0};
    winder_link_error_t  error  = WINDER_LINK_ERROR_NONE;
    winder_link_decode_result_t result;

    /* All intermediate bytes must return NONE. */
    for (size_t i = 0; i + 1U < len; i++) {
        result = winder_link_decoder_push_byte(&dec, buf[i], &frame, &error);
        if (result != WINDER_LINK_DECODE_RESULT_NONE) {
            return false;
        }
    }
    /* Last byte (CRC_H) must yield FRAME. */
    result = winder_link_decoder_push_byte(&dec, buf[len - 1U], &frame, &error);
    if (result != WINDER_LINK_DECODE_RESULT_FRAME) {
        return false;
    }
    if (frame.type != WINDER_LINK_MSG_PING) {
        return false;
    }
    if (frame.seq != 0x0001U) {
        return false;
    }
    if (frame.payload_len != 0U) {
        return false;
    }
    return true;
}

/* Test 3 — Encode then decode a frame carrying a payload */
static bool test_roundtrip_with_payload(void)
{
    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint8_t buf[WINDER_LINK_FRAME_OVERHEAD + sizeof(payload)];
    size_t  len = 0;

    if (!winder_link_encode_frame(WINDER_LINK_MSG_STATE_SNAPSHOT, 0xABCDU,
                                  payload, (uint16_t)sizeof(payload),
                                  buf, sizeof(buf), &len)) {
        return false;
    }
    if (len != WINDER_LINK_FRAME_OVERHEAD + sizeof(payload)) {
        return false;
    }

    winder_link_decoder_t dec;
    winder_link_decoder_init(&dec);
    winder_link_frame_t  frame = {0};
    winder_link_error_t  error = WINDER_LINK_ERROR_NONE;

    winder_link_decode_result_t result = push_all(&dec, buf, len, &frame, &error);

    if (result != WINDER_LINK_DECODE_RESULT_FRAME)          { return false; }
    if (frame.type        != WINDER_LINK_MSG_STATE_SNAPSHOT) { return false; }
    if (frame.seq         != 0xABCDU)                        { return false; }
    if (frame.payload_len != sizeof(payload))                 { return false; }
    if (memcmp(frame.payload, payload, sizeof(payload)) != 0) { return false; }
    return true;
}

/* Test 4 — Corrupted CRC byte must yield CRC_MISMATCH */
static bool test_crc_mismatch_detected(void)
{
    uint8_t buf[WINDER_LINK_FRAME_OVERHEAD];
    size_t  len = 0;

    if (!winder_link_encode_frame(WINDER_LINK_MSG_PING, 0U,
                                  NULL, 0, buf, sizeof(buf), &len)) {
        return false;
    }
    buf[len - 1U] ^= 0xFFU;   /* corrupt CRC_H */

    winder_link_decoder_t dec;
    winder_link_decoder_init(&dec);
    winder_link_frame_t  frame = {0};
    winder_link_error_t  error = WINDER_LINK_ERROR_NONE;

    winder_link_decode_result_t result = push_all(&dec, buf, len, &frame, &error);

    return result == WINDER_LINK_DECODE_RESULT_ERROR &&
           error  == WINDER_LINK_ERROR_CRC_MISMATCH;
}

/* Test 5 — Encoder rejects payload larger than the maximum */
static bool test_encoder_rejects_oversized_payload(void)
{
    uint8_t oversized[WINDER_LINK_MAX_PAYLOAD_SIZE + 1U];
    uint8_t buf[WINDER_LINK_FRAME_OVERHEAD + sizeof(oversized)];
    size_t  len = 0;

    (void)memset(oversized, 0xABU, sizeof(oversized));

    return !winder_link_encode_frame(WINDER_LINK_MSG_PING, 0U,
                                     oversized, (uint16_t)sizeof(oversized),
                                     buf, sizeof(buf), &len);
}

/* Test 5b — Decoder rejects a stream whose LENGTH field claims > max payload */
static bool test_decoder_rejects_oversized_length(void)
{
    /* Encode a valid zero-payload frame to get correct SOF / VERSION / TYPE / SEQ. */
    uint8_t buf[WINDER_LINK_FRAME_OVERHEAD];
    size_t  len = 0;
    if (!winder_link_encode_frame(WINDER_LINK_MSG_PING, 0U,
                                  NULL, 0, buf, sizeof(buf), &len)) {
        return false;
    }

    /* Patch LEN field to 0x0101 = 257 > WINDER_LINK_MAX_PAYLOAD_SIZE (256). */
    buf[6] = 0x01U;
    buf[7] = 0x01U;

    winder_link_decoder_t dec;
    winder_link_decoder_init(&dec);
    winder_link_frame_t  frame  = {0};
    winder_link_error_t  error  = WINDER_LINK_ERROR_NONE;
    winder_link_decode_result_t result = WINDER_LINK_DECODE_RESULT_NONE;

    for (size_t i = 0; i < len; i++) {
        result = winder_link_decoder_push_byte(&dec, buf[i], &frame, &error);
        if (result != WINDER_LINK_DECODE_RESULT_NONE) {
            break;
        }
    }

    return result == WINDER_LINK_DECODE_RESULT_ERROR &&
           error  == WINDER_LINK_ERROR_PAYLOAD_TOO_LARGE;
}

/* Test 6 — Decoder skips garbage bytes and then decodes a valid frame */
static bool test_decoder_skips_garbage(void)
{
    uint8_t frame_buf[WINDER_LINK_FRAME_OVERHEAD];
    size_t  frame_len = 0;
    if (!winder_link_encode_frame(WINDER_LINK_MSG_PING, 42U,
                                  NULL, 0, frame_buf, sizeof(frame_buf), &frame_len)) {
        return false;
    }

    /* Stray 0xAA exercises the SOF1-restart path in WAIT_SOF2. */
    const uint8_t garbage[] = {0x00U, 0xFFU, 0x12U, 0xAAU, 0x00U, 0xBBU};

    winder_link_decoder_t dec;
    winder_link_decoder_init(&dec);
    winder_link_frame_t  frame = {0};
    winder_link_error_t  error = WINDER_LINK_ERROR_NONE;
    winder_link_decode_result_t result;

    for (size_t i = 0; i < sizeof(garbage); i++) {
        result = winder_link_decoder_push_byte(&dec, garbage[i], &frame, &error);
        if (result != WINDER_LINK_DECODE_RESULT_NONE) {
            return false;
        }
    }

    result = push_all(&dec, frame_buf, frame_len, &frame, &error);

    return result == WINDER_LINK_DECODE_RESULT_FRAME &&
           frame.seq  == 42U                         &&
           frame.type == WINDER_LINK_MSG_PING;
}

/* Test 7 - Payload writer emits little-endian primitive fields */
static bool test_payload_writer_little_endian(void)
{
    uint8_t buf[11] = {0};
    winder_link_payload_writer_t writer;

    if (!winder_link_payload_writer_init(&writer, buf, sizeof(buf))) {
        return false;
    }

    if (!winder_link_payload_write_u8(&writer, 0x12U)) {
        return false;
    }
    if (!winder_link_payload_write_u16_le(&writer, 0x3456U)) {
        return false;
    }
    if (!winder_link_payload_write_u32_le(&writer, 0x789ABCDEU)) {
        return false;
    }
    if (!winder_link_payload_write_i32_le(&writer, -123456)) {
        return false;
    }

    const uint8_t expected[] = {
        0x12U,
        0x56U, 0x34U,
        0xDEU, 0xBCU, 0x9AU, 0x78U,
        0xC0U, 0x1DU, 0xFEU, 0xFFU,
    };

    if (winder_link_payload_writer_len(&writer) != sizeof(expected)) {
        return false;
    }
    if (winder_link_payload_writer_remaining(&writer) != 0U) {
        return false;
    }

    return memcmp(buf, expected, sizeof(expected)) == 0;
}

/* Test 8 - Payload reader reads the same little-endian primitive fields */
static bool test_payload_reader_little_endian(void)
{
    const uint8_t buf[] = {
        0x12U,
        0x56U, 0x34U,
        0xDEU, 0xBCU, 0x9AU, 0x78U,
        0xC0U, 0x1DU, 0xFEU, 0xFFU,
    };
    winder_link_payload_reader_t reader;
    uint8_t  u8  = 0U;
    uint16_t u16 = 0U;
    uint32_t u32 = 0U;
    int32_t  i32 = 0;

    if (!winder_link_payload_reader_init(&reader, buf, sizeof(buf))) {
        return false;
    }
    if (winder_link_payload_reader_done(&reader)) {
        return false;
    }
    if (!winder_link_payload_read_u8(&reader, &u8)) {
        return false;
    }
    if (!winder_link_payload_read_u16_le(&reader, &u16)) {
        return false;
    }
    if (!winder_link_payload_read_u32_le(&reader, &u32)) {
        return false;
    }
    if (!winder_link_payload_read_i32_le(&reader, &i32)) {
        return false;
    }

    return u8 == 0x12U &&
           u16 == 0x3456U &&
           u32 == 0x789ABCDEU &&
           i32 == -123456 &&
           winder_link_payload_reader_remaining(&reader) == 0U &&
           winder_link_payload_reader_done(&reader);
}

/* Test 9 - Payload writer overflow does not advance pos */
static bool test_payload_writer_overflow_does_not_advance(void)
{
    uint8_t buf[2] = {0};
    winder_link_payload_writer_t writer;

    if (!winder_link_payload_writer_init(&writer, buf, sizeof(buf))) {
        return false;
    }
    if (!winder_link_payload_write_u8(&writer, 0xAAU)) {
        return false;
    }

    size_t before = writer.pos;
    if (winder_link_payload_write_u16_le(&writer, 0x1234U)) {
        return false;
    }

    return writer.pos == before &&
           winder_link_payload_writer_len(&writer) == 1U &&
           winder_link_payload_writer_remaining(&writer) == 1U;
}

/* Test 10 - Payload reader underflow does not advance pos */
static bool test_payload_reader_underflow_does_not_advance(void)
{
    const uint8_t buf[] = {0xAAU, 0xBBU, 0xCCU};
    winder_link_payload_reader_t reader;
    uint8_t dummy = 0U;
    uint32_t value = 0U;

    if (!winder_link_payload_reader_init(&reader, buf, sizeof(buf))) {
        return false;
    }
    if (!winder_link_payload_read_u8(&reader, &dummy)) {
        return false;
    }

    size_t before = reader.pos;
    if (winder_link_payload_read_u32_le(&reader, &value)) {
        return false;
    }

    return reader.pos == before &&
           winder_link_payload_reader_remaining(&reader) == 2U &&
           !winder_link_payload_reader_done(&reader);
}

/* Test 11 - Zero-length payload helpers are valid and controlled */
static bool test_payload_zero_length_init(void)
{
    winder_link_payload_writer_t writer;
    winder_link_payload_reader_t reader;
    uint8_t value = 0U;

    if (!winder_link_payload_writer_init(&writer, NULL, 0U)) {
        return false;
    }
    if (winder_link_payload_write_u8(&writer, 0x12U)) {
        return false;
    }
    if (winder_link_payload_writer_len(&writer) != 0U) {
        return false;
    }
    if (winder_link_payload_writer_remaining(&writer) != 0U) {
        return false;
    }

    if (!winder_link_payload_reader_init(&reader, NULL, 0U)) {
        return false;
    }
    if (!winder_link_payload_reader_done(&reader)) {
        return false;
    }
    if (winder_link_payload_read_u8(&reader, &value)) {
        return false;
    }

    return reader.pos == 0U &&
           winder_link_payload_reader_remaining(&reader) == 0U;
}

/* =========================================================================
 * Entry point
 * ========================================================================= */

bool winder_link_protocol_selftest(void)
{
    if (!test_crc_known_vector())                   { return false; }
    if (!test_roundtrip_empty_payload())             { return false; }
    if (!test_roundtrip_with_payload())              { return false; }
    if (!test_crc_mismatch_detected())               { return false; }
    if (!test_encoder_rejects_oversized_payload())   { return false; }
    if (!test_decoder_rejects_oversized_length())    { return false; }
    if (!test_decoder_skips_garbage())               { return false; }
    if (!test_payload_writer_little_endian())         { return false; }
    if (!test_payload_reader_little_endian())         { return false; }
    if (!test_payload_writer_overflow_does_not_advance()) { return false; }
    if (!test_payload_reader_underflow_does_not_advance()) { return false; }
    if (!test_payload_zero_length_init())             { return false; }
    return true;
}
