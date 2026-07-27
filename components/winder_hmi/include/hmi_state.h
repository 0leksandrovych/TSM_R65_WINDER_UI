#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HMI_MACHINE_HOMING_REQUIRED = 0,
    HMI_MACHINE_HOMING_SEARCHING_RIGHT = 1,
    HMI_MACHINE_HOMING_RIGHT_BACKOFF = 2,
    HMI_MACHINE_HOMING_SEARCHING_LEFT = 3,
    HMI_MACHINE_HOMING_LEFT_BACKOFF = 4,
    HMI_MACHINE_HOMING_LEFT_MEASUREMENT = 5,
    HMI_MACHINE_HOMING_WAITING_ZERO_COMMAND = 6,
    HMI_MACHINE_HOMING_MOVING_TO_ZERO = 7,
    HMI_MACHINE_READY = 8,
    HMI_MACHINE_ACCELERATING = 9,
    HMI_MACHINE_RUNNING = 10,
    HMI_MACHINE_PAUSED = 11,
    HMI_MACHINE_STOPPING = 12,
    HMI_MACHINE_FINISHED = 13,
    HMI_MACHINE_ALARM = 14,
    HMI_MACHINE_HOMING_MASTER_POSITIONING = 15,
    HMI_MACHINE_HOMING_RIGHT_MEASUREMENT = 16,
    HMI_MACHINE_HOMING_WAITING_NEXT_MEASUREMENT = 17,
    HMI_MACHINE_POSITIONING = 18,

    /* Compatibility aliases retained only for the frozen mock/self-test code.
     * Product HMI code uses the homing-v3 names above. */
    HMI_MACHINE_HOMING_SEARCHING_RIGHT_REFERENCE = HMI_MACHINE_HOMING_SEARCHING_RIGHT,
    HMI_MACHINE_HOMING_BACKING_OFF_RIGHT_REFERENCE = HMI_MACHINE_HOMING_RIGHT_BACKOFF,
    HMI_MACHINE_HOMING_SEARCHING_LEFT_REFERENCE = HMI_MACHINE_HOMING_SEARCHING_LEFT,
    HMI_MACHINE_HOMING_BACKING_OFF_LEFT_REFERENCE = HMI_MACHINE_HOMING_LEFT_BACKOFF,
    HMI_MACHINE_HOMING_MEASURING_TRAVEL = HMI_MACHINE_HOMING_LEFT_MEASUREMENT,
    HMI_MACHINE_HOMING_APPLYING_OFFSET = HMI_MACHINE_HOMING_WAITING_ZERO_COMMAND,
    HMI_MACHINE_HOMING_COMPLETING = HMI_MACHINE_HOMING_MOVING_TO_ZERO,
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

typedef enum {
    HMI_CARRIAGE_POSITION_UNKNOWN = 0,
    HMI_CARRIAGE_POSITION_ZERO,
    HMI_CARRIAGE_POSITION_LEFT_EDGE,
    HMI_CARRIAGE_POSITION_MOVING,
} hmi_carriage_reference_position_t;

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
    hmi_carriage_reference_position_t carriage_reference_position;
    bool carriage_reference_position_known;
    uint32_t homing_alarm_code;
    bool homing_alarm_code_known;
    uint32_t left_edge_sample_count;
    bool left_edge_sample_count_known;
    uint32_t right_edge_sample_count;
    bool right_edge_sample_count_known;
    uint32_t homing_sample_target_count;
    bool homing_sample_target_count_known;
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
    float active_left_edge_trim_mm;
    bool active_left_edge_trim_known;
    float active_right_edge_trim_mm;
    bool active_right_edge_trim_known;
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

bool hmi_machine_state_is_homing(hmi_machine_state_t state);
bool hmi_machine_state_is_positioning(hmi_machine_state_t state);
bool hmi_state_can_start_job(const hmi_state_t *state);
const char *hmi_carriage_reference_position_text(
    hmi_carriage_reference_position_t position);
const char *hmi_homing_alarm_text(uint32_t code, char *buffer, size_t buffer_size);
