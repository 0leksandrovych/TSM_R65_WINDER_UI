#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HMI_MACHINE_BOOTING = 0,
    HMI_MACHINE_HOMING_REQUIRED,
    HMI_MACHINE_READY,
    HMI_MACHINE_RUNNING,
    HMI_MACHINE_PAUSED,
    HMI_MACHINE_STOPPING,
    HMI_MACHINE_FINISHED,
    HMI_MACHINE_ALARM,
} hmi_machine_state_t;

typedef enum {
    HMI_HOMING_REQUIRED = 0,
    HMI_HOMING_IN_PROGRESS,
    HMI_HOMING_OK,
    HMI_HOMING_FAILED,
} hmi_homing_state_t;

typedef enum {
    HMI_JOB_NOT_CONFIGURED = 0,
    HMI_JOB_VALID,
    HMI_JOB_INVALID,
} hmi_job_state_t;

typedef enum {
    HMI_CARRIAGE_STOPPED = 0,
    HMI_CARRIAGE_LEFT,
    HMI_CARRIAGE_RIGHT,
} hmi_carriage_direction_t;

typedef struct {
    hmi_machine_state_t machine_state;
    hmi_homing_state_t homing_state;
    hmi_job_state_t job_state;
    const char *selected_mode;
    const char *homing_step;
    float unwound_length_m;
    float wound_length_m;
    float target_length_m;
    float progress_percent;
    float carriage_position_mm;
    float travel_range_mm;
    float master_speed_rps;
    float winding_pitch_mm;
    float speed_override_percent;
    float right_edge_offset_mm;
    float eta_min;
    uint32_t current_layer;
    uint32_t shift_every_layers;
    uint32_t encoder_count;
    hmi_carriage_direction_t carriage_direction;
    bool left_limit_active;
    bool right_limit_active;
    const char *motor_state;
    const char *last_event;
    const char *last_error;
    bool safety_ok;
} hmi_state_t;
