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

#define LINK_MAX_JOB_PARAMS 16U

typedef enum {
    LINK_PARAM_JOB_MASTER_SPEED     = 1,  /* float, scale x100  -> centi-rps */
    LINK_PARAM_JOB_WINDING_PITCH    = 2,  /* float, scale x100  -> centi-mm  */
    LINK_PARAM_JOB_TARGET_LENGTH    = 3,  /* uint,  scale x1    -> meters    */
    LINK_PARAM_JOB_SHIFT_EVERY      = 4,  /* uint,  scale x1    -> layers    */
    LINK_PARAM_JOB_RIGHT_EDGE_SHIFT = 5,  /* float, scale x100  -> centi-mm  */
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
} link_field_id_t;

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
} link_machine_state_t;

typedef enum {
    LINK_COMMAND_REJECT_NONE                 = 0,
    LINK_COMMAND_REJECT_UNKNOWN_COMMAND      = 1,
    LINK_COMMAND_REJECT_INVALID_ARGUMENT     = 2,
    LINK_COMMAND_REJECT_INBOX_FULL           = 3,
    LINK_COMMAND_REJECT_INBOX_EMPTY          = 4,
    LINK_COMMAND_REJECT_CONTEXT_WRITE_FAILED = 5,
    LINK_COMMAND_REJECT_EVENT_PUBLISH_FAILED = 6,
} link_command_reject_reason_t;
