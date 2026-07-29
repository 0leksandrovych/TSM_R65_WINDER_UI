#pragma once

/*
 * winder_link_contract.h - shared wire-stable IDs between HMI and controller.
 * Hand-authored today in the exact style a future configurator-generated
 * file would use. Values are explicit and must never be renumbered -
 * treat as a stable wire contract. Must stay byte-identical to the copy
 * that will eventually live in Winder_UI.
 *
 * Keyed command payload format:
 * payload := param_count:u8,
 *            param_count * (param_id:u16_le, scaled_value:i32_le)
 *
 * scaled_value is always int32, even for unsigned logical fields. The type
 * and scale are defined by param_id below; no value_type byte is sent.
 */

#define LINK_START_JOB_PARAM_COUNT 5U
#define LINK_UPDATE_PAUSED_JOB_PARAM_COUNT 6U
#define LINK_MAX_JOB_PARAMS LINK_UPDATE_PAUSED_JOB_PARAM_COUNT
#define LINK_START_JOB_PARAM_ENCODED_SIZE (2U + 4U)
#define LINK_START_JOB_PAYLOAD_ENCODED_SIZE \
    (1U + (LINK_START_JOB_PARAM_COUNT * LINK_START_JOB_PARAM_ENCODED_SIZE))

#define LINK_UPDATE_PAUSED_JOB_PAYLOAD_ENCODED_SIZE \
    (1U + (LINK_UPDATE_PAUSED_JOB_PARAM_COUNT * \
           LINK_START_JOB_PARAM_ENCODED_SIZE))

/*
 * UPDATE_PAUSED_JOB uses the keyed encoding in this canonical order:
 * JOB_MASTER_SPEED, JOB_WINDING_PITCH, JOB_SHIFT_EVERY,
 * JOB_RIGHT_EDGE_SHIFT, ADDITIONAL_LENGTH_PRESENT, ADDITIONAL_LENGTH_M.
 */

#define LINK_EDGE_TRIM_PARAM_COUNT 2U
#define LINK_EDGE_TRIM_PAYLOAD_ENCODED_SIZE \
    (1U + (LINK_EDGE_TRIM_PARAM_COUNT * LINK_START_JOB_PARAM_ENCODED_SIZE))

#define LINK_JOB_MASTER_SPEED_WIRE_MIN 10
#define LINK_JOB_MASTER_SPEED_WIRE_MAX 3000
#define LINK_JOB_WINDING_PITCH_WIRE_MIN 10
#define LINK_JOB_WINDING_PITCH_WIRE_MAX 3000
#define LINK_JOB_TARGET_LENGTH_WIRE_MIN 1
#define LINK_JOB_TARGET_LENGTH_WIRE_MAX 100000
#define LINK_JOB_SHIFT_EVERY_WIRE_MIN 2
#define LINK_JOB_SHIFT_EVERY_WIRE_MAX 100
#define LINK_JOB_RIGHT_EDGE_SHIFT_WIRE_MIN 6
#define LINK_JOB_RIGHT_EDGE_SHIFT_WIRE_MAX 5000

typedef enum {
    LINK_PARAM_JOB_MASTER_SPEED     = 1, /* float, centi-rps, wire 10..3000 */
    LINK_PARAM_JOB_WINDING_PITCH    = 2, /* float, centi-mm, wire 10..3000 */
    LINK_PARAM_JOB_TARGET_LENGTH    = 3, /* uint32, meters x1, wire 1..100000 */
    LINK_PARAM_JOB_SHIFT_EVERY      = 4, /* uint32, layers x1, wire 2..100 */
    LINK_PARAM_JOB_RIGHT_EDGE_SHIFT = 5, /* float, centi-mm, wire 6..5000 */
    LINK_PARAM_LEFT_EDGE_TRIM_MM    = 6, /* signed centi-mm */
    LINK_PARAM_RIGHT_EDGE_TRIM_MM   = 7, /* signed centi-mm */
    LINK_PARAM_ADDITIONAL_LENGTH_PRESENT = 8, /* bool/code, scale x1 */
    LINK_PARAM_ADDITIONAL_LENGTH_M = 9, /* float meters, scale x1000 -> mm */
} link_param_id_t;

typedef enum {
    LINK_FIELD_MACHINE_STATE        = 1,  /* enum/code, scale x1 */
    LINK_FIELD_JOB_MASTER_SPEED     = 2,  /* float, scale x100  -> centi-rps */
    LINK_FIELD_JOB_WINDING_PITCH    = 3,  /* float, scale x100  -> centi-mm  */
    LINK_FIELD_JOB_TARGET_LENGTH    = 4,  /* float, scale x1000 -> mm        */
    LINK_FIELD_JOB_SHIFT_EVERY      = 5,  /* uint,  scale x1    -> layers    */
    LINK_FIELD_JOB_RIGHT_EDGE_SHIFT = 6,  /* float, scale x100  -> centi-mm  */
    /* ID 7 was the legacy homing sub-state field and remains retired. */
    LINK_FIELD_TRAVEL_RANGE_MM      = 8,  /* double, scale x100 -> centi-mm  */
    LINK_FIELD_MASTER_SPEED_RPS     = 9,  /* runtime setpoint, x100 -> centi-rps */
    LINK_FIELD_WOUND_LENGTH_M       = 10, /* double, scale x1000 -> mm       */
    LINK_FIELD_COMPLETED_LAYERS     = 11, /* double, scale x1 -> layers      */
    LINK_FIELD_APPLIED_RIGHT_EDGE_OFFSET_MM = 12, /* double, x100 -> centi-mm */
    LINK_FIELD_HOMING_ALARM_CODE    = 13, /* enum/code, scale x1             */
    LINK_FIELD_CARRIAGE_REFERENCE_POSITION = 14, /* enum/code, scale x1        */
    LINK_FIELD_LEFT_EDGE_SAMPLE_COUNT = 15, /* double, scale x1 -> samples    */
    LINK_FIELD_RIGHT_EDGE_SAMPLE_COUNT = 16, /* double, scale x1 -> samples   */
    LINK_FIELD_HOMING_SAMPLE_TARGET_COUNT = 17, /* double, scale x1 -> samples */
    LINK_FIELD_ACTIVE_LEFT_EDGE_TRIM_MM = 18, /* double, x100 -> centi-mm      */
    LINK_FIELD_ACTIVE_RIGHT_EDGE_TRIM_MM = 19, /* double, x100 -> centi-mm     */
    LINK_FIELD_JOB_PAUSE_REASON = 20, /* link_job_pause_reason_t, scale x1    */
} link_field_id_t;

typedef enum {
    LINK_JOB_PAUSE_REASON_NONE             = 0,
    LINK_JOB_PAUSE_REASON_OPERATOR         = 1,
    LINK_JOB_PAUSE_REASON_TARGET_REACHED   = 2,
    LINK_JOB_PAUSE_REASON_LENGTH_WATCHDOG  = 3,
} link_job_pause_reason_t;

typedef enum {
    LINK_RESUME_REJECT_TARGET_REACHED = 1,
} link_resume_reject_reason_t;

typedef enum {
    LINK_MACHINE_STATE_HOMING_REQUIRED                  = 0,
    LINK_MACHINE_STATE_HOMING_SEARCHING_RIGHT_REFERENCE = 1,
    LINK_MACHINE_STATE_HOMING_BACKING_OFF_RIGHT_REFERENCE = 2,
    LINK_MACHINE_STATE_HOMING_SEARCHING_LEFT_REFERENCE  = 3,
    LINK_MACHINE_STATE_HOMING_BACKING_OFF_LEFT_REFERENCE = 4,
    LINK_MACHINE_STATE_HOMING_MEASURING_TRAVEL          = 5,
    LINK_MACHINE_STATE_HOMING_APPLYING_OFFSET           = 6,
    LINK_MACHINE_STATE_HOMING_COMPLETING                = 7,
    LINK_MACHINE_STATE_READY                             = 8,
    LINK_MACHINE_STATE_ACCELERATING                      = 9,
    LINK_MACHINE_STATE_RUNNING                           = 10,
    LINK_MACHINE_STATE_PAUSED                            = 11,
    LINK_MACHINE_STATE_STOPPING                          = 12,
    LINK_MACHINE_STATE_FINISHED                          = 13,
    LINK_MACHINE_STATE_ALARM                             = 14,
    LINK_MACHINE_STATE_HOMING_WAITING_NEXT_MEASUREMENT   = 15,
    LINK_MACHINE_STATE_HOMING_WAITING_ZERO_COMMAND       = 16,
    LINK_MACHINE_STATE_HOMING_MOVING_TO_ZERO             = 17,
    LINK_MACHINE_STATE_READY_AT_ZERO                     = 18,
    LINK_MACHINE_STATE_READY_AT_LEFT_EDGE                = 19,
    LINK_MACHINE_STATE_POSITIONING                       = 20,
    LINK_MACHINE_STATE_HOMING_MASTER_POSITIONING         = 21,
} link_machine_state_t;

typedef enum {
    LINK_CARRIAGE_REFERENCE_POSITION_UNKNOWN   = 0,
    LINK_CARRIAGE_REFERENCE_POSITION_ZERO      = 1,
    LINK_CARRIAGE_REFERENCE_POSITION_LEFT_EDGE = 2,
    LINK_CARRIAGE_REFERENCE_POSITION_MOVING    = 3,
} link_carriage_reference_position_t;

typedef enum {
    LINK_COMMAND_REJECT_NONE                 = 0,
    LINK_COMMAND_REJECT_UNKNOWN_COMMAND      = 1,
    LINK_COMMAND_REJECT_INVALID_ARGUMENT     = 2,
    LINK_COMMAND_REJECT_INBOX_FULL           = 3,
    LINK_COMMAND_REJECT_INBOX_EMPTY          = 4,
    LINK_COMMAND_REJECT_CONTEXT_WRITE_FAILED = 5,
    LINK_COMMAND_REJECT_EVENT_PUBLISH_FAILED = 6,
} link_command_reject_reason_t;
