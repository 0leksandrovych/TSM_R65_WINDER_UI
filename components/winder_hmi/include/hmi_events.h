#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hmi_param.h"
#include "hmi_state.h"

#define HMI_TEXT_MESSAGE_MAX 96

typedef enum {
    HMI_JOB_MODE_NONE = 0,
    HMI_JOB_MODE_CONICAL,
    HMI_JOB_MODE_STRAIGHT,
    HMI_JOB_MODE_TEST
} hmi_job_mode_id_t;

typedef enum {
    HMI_CMD_START_HOMING = 0,
    HMI_CMD_ABORT_HOMING,

    HMI_CMD_VALIDATE_JOB,
    HMI_CMD_START_JOB,

    HMI_CMD_PAUSE_JOB,
    HMI_CMD_RESUME_JOB,
    HMI_CMD_ABORT_JOB,
    HMI_CMD_FINISH_JOB,
    HMI_CMD_RESET_JOB,

    HMI_CMD_SET_SPEED_OVERRIDE,
    HMI_CMD_APPLY_EDGE_TRIM,

    HMI_CMD_RESET_UNWOUND_COUNTER,
    HMI_CMD_RESET_ALARM
} hmi_command_t;

typedef struct {
    hmi_job_mode_id_t mode_id;
    uint16_t param_id;
    hmi_param_value_t value;
} hmi_command_payload_t;

typedef enum {
    HMI_CONNECTION_DISCONNECTED = 0,
    HMI_CONNECTION_CONNECTING,
    HMI_CONNECTION_CONNECTED,
    HMI_CONNECTION_LOST
} hmi_connection_state_t;

typedef struct {
    bool valid;
    char message[HMI_TEXT_MESSAGE_MAX];
    uint32_t estimated_layers;
    float estimated_time_min;
    float estimated_offset_mm;
} hmi_job_validation_t;

typedef struct {
    hmi_command_t command;
    char reason[HMI_TEXT_MESSAGE_MAX];
} hmi_command_rejected_t;
