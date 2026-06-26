#include "winder_link_protocol.h"

#include <string.h>

/* =========================================================================
 * CRC-16/CCITT-FALSE
 * Polynomial 0x1021, initial value 0xFFFF, no reflection, no final XOR.
 * Known test vector: "123456789" (9 bytes) → 0x29B1
 * ========================================================================= */

static uint16_t crc16_update_byte(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)((uint16_t)byte << 8);
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000U) {
            crc = (uint16_t)((crc << 1) ^ 0x1021U);
        } else {
            crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

uint16_t winder_link_crc16_ccitt_false(const uint8_t *data, size_t len)
{
    if (data == NULL) {
        return 0xFFFFU;
    }

    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < len; i++) {
        crc = crc16_update_byte(crc, data[i]);
    }
    return crc;
}

/* =========================================================================
 * Encoder
 * ========================================================================= */

bool winder_link_encode_frame(
    uint8_t        type,
    uint16_t       seq,
    const uint8_t *payload,
    uint16_t       payload_len,
    uint8_t       *out,
    size_t         out_capacity,
    size_t        *out_len)
{
    if (out == NULL || out_len == NULL) {
        return false;
    }

    if (payload_len > WINDER_LINK_MAX_PAYLOAD_SIZE) {
        return false;
    }

    if (payload == NULL && payload_len > 0U) {
        return false;
    }

    size_t frame_len = WINDER_LINK_FRAME_OVERHEAD + (size_t)payload_len;
    if (out_capacity < frame_len) {
        return false;
    }

    /* SOF (not covered by CRC) */
    out[0] = WINDER_LINK_SOF1;
    out[1] = WINDER_LINK_SOF2;

    /* Header — CRC calculation starts from out[2] */
    out[2] = WINDER_LINK_VERSION;
    out[3] = type;
    out[4] = (uint8_t)(seq & 0xFFU);
    out[5] = (uint8_t)(seq >> 8);
    out[6] = (uint8_t)(payload_len & 0xFFU);
    out[7] = (uint8_t)(payload_len >> 8);

    /* Payload */
    if (payload_len > 0U) {
        (void)memcpy(&out[8], payload, payload_len);
    }

    /* CRC over VERSION..end-of-payload (out[2..7+payload_len]) */
    uint16_t crc = winder_link_crc16_ccitt_false(
        &out[2], WINDER_LINK_HEADER_SIZE + (size_t)payload_len);

    out[8U + payload_len] = (uint8_t)(crc & 0xFFU);
    out[9U + payload_len] = (uint8_t)(crc >> 8);

    *out_len = frame_len;
    return true;
}

/* =========================================================================
 * Decoder state machine
 * ========================================================================= */

static void decoder_resync(winder_link_decoder_t *decoder)
{
    decoder->state       = WINDER_LINK_DECODER_STATE_WAIT_SOF1;
    decoder->crc_accum   = 0;
    decoder->payload_len = 0;
    decoder->payload_pos = 0;
    decoder->seq_l       = 0;
    decoder->len_l       = 0;
}

void winder_link_decoder_init(winder_link_decoder_t *decoder)
{
    if (decoder == NULL) {
        return;
    }
    *decoder = (winder_link_decoder_t){0};
}

winder_link_decode_result_t winder_link_decoder_push_byte(
    winder_link_decoder_t *decoder,
    uint8_t                byte,
    winder_link_frame_t   *out_frame,
    winder_link_error_t   *out_error)
{
    if (out_error != NULL) {
        *out_error = WINDER_LINK_ERROR_NONE;
    }

    if (decoder == NULL) {
        return WINDER_LINK_DECODE_RESULT_NONE;
    }

    switch (decoder->state) {

    case WINDER_LINK_DECODER_STATE_WAIT_SOF1:
        if (byte == WINDER_LINK_SOF1) {
            decoder->state = WINDER_LINK_DECODER_STATE_WAIT_SOF2;
        }
        break;

    case WINDER_LINK_DECODER_STATE_WAIT_SOF2:
        if (byte == WINDER_LINK_SOF2) {
            decoder->state = WINDER_LINK_DECODER_STATE_READ_VERSION;
        } else if (byte == WINDER_LINK_SOF1) {
            /* 0xAA 0xAA 0x55 is valid — treat the second 0xAA as a new SOF1. */
        } else {
            decoder->state = WINDER_LINK_DECODER_STATE_WAIT_SOF1;
        }
        break;

    case WINDER_LINK_DECODER_STATE_READ_VERSION:
        /* First byte covered by CRC — initialise the running accumulator here. */
        decoder->crc_accum = crc16_update_byte(0xFFFFU, byte);
        if (byte != WINDER_LINK_VERSION) {
            decoder_resync(decoder);
            if (out_error != NULL) {
                *out_error = WINDER_LINK_ERROR_UNSUPPORTED_VERSION;
            }
            return WINDER_LINK_DECODE_RESULT_ERROR;
        }
        decoder->state = WINDER_LINK_DECODER_STATE_READ_TYPE;
        break;

    case WINDER_LINK_DECODER_STATE_READ_TYPE:
        decoder->crc_accum = crc16_update_byte(decoder->crc_accum, byte);
        decoder->type      = byte;
        decoder->state     = WINDER_LINK_DECODER_STATE_READ_SEQ_L;
        break;

    case WINDER_LINK_DECODER_STATE_READ_SEQ_L:
        decoder->crc_accum = crc16_update_byte(decoder->crc_accum, byte);
        decoder->seq_l     = byte;
        decoder->state     = WINDER_LINK_DECODER_STATE_READ_SEQ_H;
        break;

    case WINDER_LINK_DECODER_STATE_READ_SEQ_H:
        decoder->crc_accum = crc16_update_byte(decoder->crc_accum, byte);
        decoder->seq       = (uint16_t)decoder->seq_l | ((uint16_t)byte << 8);
        decoder->state     = WINDER_LINK_DECODER_STATE_READ_LEN_L;
        break;

    case WINDER_LINK_DECODER_STATE_READ_LEN_L:
        decoder->crc_accum = crc16_update_byte(decoder->crc_accum, byte);
        decoder->len_l     = byte;
        decoder->state     = WINDER_LINK_DECODER_STATE_READ_LEN_H;
        break;

    case WINDER_LINK_DECODER_STATE_READ_LEN_H:
        decoder->crc_accum   = crc16_update_byte(decoder->crc_accum, byte);
        decoder->payload_len = (uint16_t)decoder->len_l | ((uint16_t)byte << 8);
        if (decoder->payload_len > WINDER_LINK_MAX_PAYLOAD_SIZE) {
            decoder_resync(decoder);
            if (out_error != NULL) {
                *out_error = WINDER_LINK_ERROR_PAYLOAD_TOO_LARGE;
            }
            return WINDER_LINK_DECODE_RESULT_ERROR;
        }
        decoder->payload_pos = 0;
        decoder->state = (decoder->payload_len == 0U)
                         ? WINDER_LINK_DECODER_STATE_READ_CRC_L
                         : WINDER_LINK_DECODER_STATE_READ_PAYLOAD;
        break;

    case WINDER_LINK_DECODER_STATE_READ_PAYLOAD:
        decoder->crc_accum = crc16_update_byte(decoder->crc_accum, byte);
        decoder->payload[decoder->payload_pos++] = byte;
        if (decoder->payload_pos >= decoder->payload_len) {
            decoder->state = WINDER_LINK_DECODER_STATE_READ_CRC_L;
        }
        break;

    case WINDER_LINK_DECODER_STATE_READ_CRC_L:
        decoder->crc_l = byte;
        decoder->state = WINDER_LINK_DECODER_STATE_READ_CRC_H;
        break;

    case WINDER_LINK_DECODER_STATE_READ_CRC_H: {
        uint16_t received_crc = (uint16_t)decoder->crc_l | ((uint16_t)byte << 8);
        if (received_crc != decoder->crc_accum) {
            decoder_resync(decoder);
            if (out_error != NULL) {
                *out_error = WINDER_LINK_ERROR_CRC_MISMATCH;
            }
            return WINDER_LINK_DECODE_RESULT_ERROR;
        }

        if (out_frame != NULL) {
            out_frame->type        = decoder->type;
            out_frame->seq         = decoder->seq;
            out_frame->payload_len = decoder->payload_len;
            if (decoder->payload_len > 0U) {
                (void)memcpy(out_frame->payload, decoder->payload, decoder->payload_len);
            }
        }

        decoder_resync(decoder);
        return WINDER_LINK_DECODE_RESULT_FRAME;
    }

    default:
        decoder_resync(decoder);
        break;
    }

    return WINDER_LINK_DECODE_RESULT_NONE;
}
