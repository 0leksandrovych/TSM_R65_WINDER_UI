#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Winder Link Protocol — shared binary frame codec
 *
 * Shared communication contract between the HMI UI controller and the main
 * machine controller.  This component is independent of both sides: it has
 * no dependencies on LVGL, FreeRTOS, ESP-IDF UART driver, winder_hmi, or
 * the motor runtime.
 *
 * Frame layout (all multi-byte fields are little-endian):
 *
 *   Offset  Field    Size  Notes
 *   ------  -------  ----  ----------------------------------------
 *     0     SOF1       1   0xAA  (not included in CRC)
 *     1     SOF2       1   0x55  (not included in CRC)
 *     2     VERSION    1   0x01
 *     3     TYPE       1   message type (winder_link_msg_type_t)
 *     4     SEQ_L      1   sequence number, low byte
 *     5     SEQ_H      1   sequence number, high byte
 *     6     LEN_L      1   payload length, low byte
 *     7     LEN_H      1   payload length, high byte
 *     8..N  PAYLOAD  0..WINDER_LINK_MAX_PAYLOAD_SIZE bytes
 *   N+1     CRC_L      1   CRC-16 low byte
 *   N+2     CRC_H      1   CRC-16 high byte
 *
 * CRC algorithm: CRC-16/CCITT-FALSE
 *   Polynomial : 0x1021
 *   Initial    : 0xFFFF
 *   RefIn/Out  : false (no bit reflection)
 *   Final XOR  : 0x0000
 * CRC covers: VERSION, TYPE, SEQ_L, SEQ_H, LEN_L, LEN_H, PAYLOAD.
 * SOF bytes are NOT included in the CRC.
 */

/* --- Frame constants ------------------------------------------------------- */

#define WINDER_LINK_SOF1             0xAAU
#define WINDER_LINK_SOF2             0x55U
#define WINDER_LINK_VERSION          0x01U
#define WINDER_LINK_MAX_PAYLOAD_SIZE 256U
#define WINDER_LINK_HEADER_SIZE      6U    /* VERSION + TYPE + SEQ(2) + LENGTH(2) */
#define WINDER_LINK_CRC_SIZE         2U
#define WINDER_LINK_FRAME_OVERHEAD   10U   /* SOF(2) + HEADER(6) + CRC(2) */

/* --- Message type identifiers ---------------------------------------------- */

typedef enum {
    /* HMI → Controller commands */
    WINDER_LINK_MSG_PING                  = 0x01,
    WINDER_LINK_MSG_GET_CAPABILITIES      = 0x02,
    WINDER_LINK_MSG_VALIDATE_JOB          = 0x03,
    WINDER_LINK_MSG_START_JOB             = 0x04,
    WINDER_LINK_MSG_START_HOMING          = 0x05,
    WINDER_LINK_MSG_ABORT_HOMING          = 0x06,
    WINDER_LINK_MSG_PAUSE_JOB             = 0x07,
    WINDER_LINK_MSG_RESUME_JOB            = 0x08,
    WINDER_LINK_MSG_ABORT_JOB             = 0x09,
    WINDER_LINK_MSG_RESET_ALARM           = 0x0A,
    WINDER_LINK_MSG_RESET_UNWOUND_COUNTER = 0x0B,
    WINDER_LINK_MSG_SET_SPEED_OVERRIDE    = 0x0C,
    WINDER_LINK_MSG_APPLY_EDGE_TRIM       = 0x0D,
    WINDER_LINK_MSG_GET_TELEMETRY         = 0x0E,
    WINDER_LINK_MSG_RESET_JOB             = 0x0F,
    WINDER_LINK_MSG_FINISH_JOB            = 0x10,
    WINDER_LINK_MSG_HOMING_NEXT_MEASUREMENT = 0x11,
    WINDER_LINK_MSG_MOVE_CARRIAGE_TO_ZERO   = 0x12,
    WINDER_LINK_MSG_MOVE_CARRIAGE_TO_LEFT_EDGE = 0x13,

    /* Controller → HMI responses / unsolicited messages */
    WINDER_LINK_MSG_PONG                  = 0x80,
    WINDER_LINK_MSG_HEARTBEAT             = 0x81,
    WINDER_LINK_MSG_CAPABILITIES_SNAPSHOT = 0x82,
    WINDER_LINK_MSG_STATE_SNAPSHOT        = 0x83,
    WINDER_LINK_MSG_COMMAND_ACCEPTED      = 0x84,
    WINDER_LINK_MSG_COMMAND_REJECTED      = 0x85,
    WINDER_LINK_MSG_JOB_VALIDATION_RESULT = 0x86,
    WINDER_LINK_MSG_ALARM_EVENT           = 0x87,
} winder_link_msg_type_t;

/* --- Decoded frame --------------------------------------------------------- */

typedef struct {
    uint8_t  type;
    uint16_t seq;
    uint16_t payload_len;
    uint8_t  payload[WINDER_LINK_MAX_PAYLOAD_SIZE];
} winder_link_frame_t;

/* --- Decoder result and error codes --------------------------------------- */

typedef enum {
    WINDER_LINK_DECODE_RESULT_NONE  = 0,
    WINDER_LINK_DECODE_RESULT_FRAME,
    WINDER_LINK_DECODE_RESULT_ERROR,
} winder_link_decode_result_t;

typedef enum {
    WINDER_LINK_ERROR_NONE               = 0,
    WINDER_LINK_ERROR_UNSUPPORTED_VERSION,
    WINDER_LINK_ERROR_PAYLOAD_TOO_LARGE,
    WINDER_LINK_ERROR_CRC_MISMATCH,
    WINDER_LINK_ERROR_BUFFER_TOO_SMALL,
    WINDER_LINK_ERROR_INVALID_ARGUMENT,
} winder_link_error_t;

/* --- Decoder state machine ------------------------------------------------- */

typedef enum {
    WINDER_LINK_DECODER_STATE_WAIT_SOF1 = 0,
    WINDER_LINK_DECODER_STATE_WAIT_SOF2,
    WINDER_LINK_DECODER_STATE_READ_VERSION,
    WINDER_LINK_DECODER_STATE_READ_TYPE,
    WINDER_LINK_DECODER_STATE_READ_SEQ_L,
    WINDER_LINK_DECODER_STATE_READ_SEQ_H,
    WINDER_LINK_DECODER_STATE_READ_LEN_L,
    WINDER_LINK_DECODER_STATE_READ_LEN_H,
    WINDER_LINK_DECODER_STATE_READ_PAYLOAD,
    WINDER_LINK_DECODER_STATE_READ_CRC_L,
    WINDER_LINK_DECODER_STATE_READ_CRC_H,
} winder_link_decoder_state_t;

/* Decoder context.  Callers allocate this (no heap); treat fields as opaque. */
typedef struct {
    winder_link_decoder_state_t state;
    uint16_t crc_accum;     /* running CRC, initialised on VERSION byte */
    uint16_t seq;
    uint16_t payload_len;
    uint16_t payload_pos;
    uint8_t  type;
    uint8_t  seq_l;         /* latched in READ_SEQ_L */
    uint8_t  len_l;         /* latched in READ_LEN_L */
    uint8_t  crc_l;         /* latched in READ_CRC_L */
    uint8_t  payload[WINDER_LINK_MAX_PAYLOAD_SIZE];
} winder_link_decoder_t;

/* --- Public API ------------------------------------------------------------ */

/* CRC-16/CCITT-FALSE over data[0..len-1].
 * Known test vector: "123456789" (9 bytes) → 0x29B1
 * Returns 0xFFFF if data is NULL. */
uint16_t winder_link_crc16_ccitt_false(const uint8_t *data, size_t len);

/* Encode one frame into out[0..out_capacity-1].
 * payload may be NULL only when payload_len == 0.
 * Sets *out_len to the total encoded byte count on success.
 * Returns false (writes nothing) on any validation failure. */
bool winder_link_encode_frame(
    uint8_t        type,
    uint16_t       seq,
    const uint8_t *payload,
    uint16_t       payload_len,
    uint8_t       *out,
    size_t         out_capacity,
    size_t        *out_len
);

/* Initialise (or reset) a decoder to the WAIT_SOF1 state. */
void winder_link_decoder_init(winder_link_decoder_t *decoder);

/* Push one byte into the streaming decoder.
 *
 * Returns:
 *   NONE  — frame still assembling; no output.
 *   FRAME — complete valid frame written to *out_frame; decoder ready for next.
 *   ERROR — protocol violation; *out_error set; decoder resyncs (searches for
 *           next SOF1 from the byte following the offending position).
 *
 * out_frame and out_error may be NULL if the caller does not need them. */
winder_link_decode_result_t winder_link_decoder_push_byte(
    winder_link_decoder_t *decoder,
    uint8_t                byte,
    winder_link_frame_t   *out_frame,
    winder_link_error_t   *out_error
);
