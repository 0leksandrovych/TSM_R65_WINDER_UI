#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HMI_MACHINE_HOMING_REQUIRED = 0,
    HMI_MACHINE_HOMING_SEARCHING_RIGHT_REFERENCE = 1,
    HMI_MACHINE_HOMING_BACKING_OFF_RIGHT_REFERENCE = 2,
    HMI_MACHINE_HOMING_SEARCHING_LEFT_REFERENCE = 3,
    HMI_MACHINE_HOMING_BACKING_OFF_LEFT_REFERENCE = 4,
    HMI_MACHINE_HOMING_MEASURING_TRAVEL = 5,
    HMI_MACHINE_HOMING_APPLYING_OFFSET = 6,
    HMI_MACHINE_HOMING_COMPLETING = 7,
    HMI_MACHINE_READY = 8,
    HMI_MACHINE_ACCELERATING = 9,
    HMI_MACHINE_RUNNING = 10,
    HMI_MACHINE_PAUSED = 11,
    HMI_MACHINE_STOPPING = 12,
    HMI_MACHINE_FINISHED = 13,
    HMI_MACHINE_ALARM = 14,
} hmi_machine_state_t;

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
    bool machine_state_known;
    hmi_job_state_t job_state;
    const char *selected_mode;
    float unwound_length_m;
    float wound_length_m;
    float target_length_m;
    bool target_length_known;
    float progress_percent;
    float carriage_position_mm;
    double travel_range_mm;
    bool travel_range_known;
    float job_master_speed_rps;
    bool job_master_speed_known;
    float master_speed_rps;
    bool master_speed_known;
    float winding_pitch_mm;
    bool winding_pitch_known;
    float speed_override_percent;
    float right_edge_offset_mm;
    float eta_min;
    uint32_t current_layer;
    uint32_t shift_every_layers;
    uint32_t encoder_count;
    hmi_carriage_direction_t carriage_direction;
    const char *motor_state;
    const char *last_event;
    const char *last_error;
    bool safety_ok;
} hmi_state_t;
