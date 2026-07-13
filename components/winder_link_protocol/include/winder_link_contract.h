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
    LINK_PARAM_JOB_TARGET_LENGTH    = 3,  /* float, scale x1000 -> mm        */
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
} link_field_id_t;

typedef enum {
    LINK_COMMAND_REJECT_NONE                 = 0,
    LINK_COMMAND_REJECT_UNKNOWN_COMMAND      = 1,
    LINK_COMMAND_REJECT_INVALID_ARGUMENT     = 2,
    LINK_COMMAND_REJECT_INBOX_FULL           = 3,
    LINK_COMMAND_REJECT_INBOX_EMPTY          = 4,
    LINK_COMMAND_REJECT_CONTEXT_WRITE_FAILED = 5,
    LINK_COMMAND_REJECT_EVENT_PUBLISH_FAILED = 6,
} link_command_reject_reason_t;
