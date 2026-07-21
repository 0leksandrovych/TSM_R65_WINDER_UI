#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hmi_events.h"
#include "hmi_state.h"

#define HMI_CONTROLLER_MAX_JOB_PARAMS 16

typedef enum {
    HMI_CONTROLLER_MSG_NONE = 0,

    HMI_CONTROLLER_MSG_GET_CAPABILITIES,
    HMI_CONTROLLER_MSG_VALIDATE_JOB,
    HMI_CONTROLLER_MSG_START_JOB,

    HMI_CONTROLLER_MSG_START_HOMING,
    HMI_CONTROLLER_MSG_ABORT_HOMING,

    HMI_CONTROLLER_MSG_PAUSE_JOB,
    HMI_CONTROLLER_MSG_RESUME_JOB,
    HMI_CONTROLLER_MSG_STOP_JOB,
    HMI_CONTROLLER_MSG_RESET_JOB,

    HMI_CONTROLLER_MSG_RESET_ALARM,
    HMI_CONTROLLER_MSG_RESET_UNWOUND_COUNTER,

    HMI_CONTROLLER_MSG_SET_SPEED_OVERRIDE,
    HMI_CONTROLLER_MSG_APPLY_EDGE_TRIM,
    HMI_CONTROLLER_MSG_GET_TELEMETRY
} hmi_controller_msg_type_t;

typedef enum {
    HMI_CONTROLLER_EVENT_NONE = 0,

    HMI_CONTROLLER_EVENT_CAPABILITIES_SNAPSHOT,
    HMI_CONTROLLER_EVENT_STATE_SNAPSHOT,
    HMI_CONTROLLER_EVENT_JOB_VALIDATION_RESULT,
    HMI_CONTROLLER_EVENT_COMMAND_ACCEPTED,
    HMI_CONTROLLER_EVENT_COMMAND_REJECTED,
    HMI_CONTROLLER_EVENT_ALARM,
    HMI_CONTROLLER_EVENT_HEARTBEAT,
    HMI_CONTROLLER_EVENT_CONNECTION_LOST
} hmi_controller_event_type_t;

typedef struct {
    uint16_t wire_param_id;
    double wire_scale;
    hmi_param_type_t type;
    hmi_param_value_t value;
} hmi_controller_param_value_t;

typedef struct {
    hmi_job_mode_id_t mode_id;
    size_t param_count;
    hmi_controller_param_value_t params[HMI_CONTROLLER_MAX_JOB_PARAMS];
} hmi_controller_job_payload_t;

typedef struct {
    hmi_controller_msg_type_t type;
    union {
        hmi_controller_job_payload_t job;
        float speed_override_percent;
        float edge_trim_mm;
    } data;
} hmi_controller_message_t;

typedef struct {
    hmi_controller_event_type_t type;
    union {
        hmi_state_t state;
        hmi_job_validation_t validation;
        hmi_command_rejected_t command_rejected;
        hmi_connection_state_t connection;
    } data;
} hmi_controller_event_t;

/* Pure helpers — no global reads.
 * Callers are responsible for building complete payloads before calling these. */

/* Initialize a simple command message with no payload data. */
bool hmi_controller_message_init(hmi_controller_message_t *out,
                                 hmi_controller_msg_type_t type);

/* Initialize a job message from a caller-supplied, fully populated job payload. */
bool hmi_controller_message_init_job(hmi_controller_message_t *out,
                                     hmi_controller_msg_type_t type,
                                     const hmi_controller_job_payload_t *job_payload);
