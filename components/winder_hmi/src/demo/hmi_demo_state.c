#include "hmi_demo_state.h"

#include <stddef.h>

static const hmi_state_t s_ready_state = {
    .machine_state = HMI_MACHINE_HOMING_REQUIRED,
    .machine_state_known = true,
    .job_state = HMI_JOB_NOT_CONFIGURED,
    .selected_mode = "Conical Winding",
    .unwound_length_m = 0.0f,
    .wound_length_m = 0.0f,
    .target_length_m = 125.0f,
    .target_length_known = true,
    .progress_percent = 0.0f,
    .carriage_position_mm = 0.0f,
    .travel_range_mm = 0.0,
    .travel_range_known = false,
    .job_master_speed_rps = 0.0f,
    .job_master_speed_known = false,
    .master_speed_rps = 0.0f,
    .master_speed_known = true,
    .winding_pitch_mm = 0.0f,
    .winding_pitch_known = false,
    .speed_override_percent = 100.0f,
    .right_edge_offset_mm = 0.0f,
    .eta_min = 0.0f,
    .current_layer = 0,
    .encoder_count = 0,
    .carriage_direction = HMI_CARRIAGE_STOPPED,
    .left_limit_active = false,
    .right_limit_active = false,
    .motor_state = "Idle",
    .last_event = "Demo ready",
    .last_error = NULL,
    .safety_ok = true,
};

const hmi_state_t *hmi_demo_state_ready(void)
{
    return &s_ready_state;
}
