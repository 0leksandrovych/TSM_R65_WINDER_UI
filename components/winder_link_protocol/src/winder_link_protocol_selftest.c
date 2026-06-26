#include "winder_link_protocol_selftest.h"
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
    return true;
}
