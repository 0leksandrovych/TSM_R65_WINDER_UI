#include "hmi_model.h"

#include <stddef.h>

static hmi_state_t s_state;
static hmi_connection_state_t s_connection_state;

void hmi_model_init(void)
{
    s_state.machine_state = HMI_MACHINE_HOMING_REQUIRED;
    s_state.machine_state_known = false;
    s_state.job_state = HMI_JOB_NOT_CONFIGURED;
    s_state.selected_mode = "Not selected";
    s_state.unwound_length_m = 0.0f;
    s_state.wound_length_m = 0.0f;
    s_state.wound_length_known = false;
    s_state.target_length_m = 0.0f;
    s_state.target_length_known = false;
    s_state.progress_percent = 0.0f;
    s_state.carriage_position_mm = 0.0f;
    s_state.carriage_reference_position = HMI_CARRIAGE_POSITION_UNKNOWN;
    s_state.carriage_reference_position_known = false;
    s_state.homing_alarm_code = 0U;
    s_state.homing_alarm_code_known = false;
    s_state.left_edge_sample_count = 0U;
    s_state.left_edge_sample_count_known = false;
    s_state.right_edge_sample_count = 0U;
    s_state.right_edge_sample_count_known = false;
    s_state.homing_sample_target_count = 0U;
    s_state.homing_sample_target_count_known = false;
    s_state.travel_range_mm = 0.0;
    s_state.travel_range_known = false;
    s_state.job_master_speed_rps = 0.0f;
    s_state.job_master_speed_known = false;
    s_state.master_speed_rps = 0.0f;
    s_state.master_speed_known = false;
    s_state.winding_pitch_mm = 0.0f;
    s_state.winding_pitch_known = false;
    s_state.shift_every_layers = 0;
    s_state.shift_every_layers_known = false;
    s_state.job_right_edge_shift_mm = 0.0f;
    s_state.job_right_edge_shift_known = false;
    s_state.pause_reason = HMI_JOB_PAUSE_REASON_NONE;
    s_state.pause_reason_known = false;
    s_state.speed_override_percent = 0.0f;
    s_state.right_edge_offset_mm = 0.0f;
    s_state.active_left_edge_trim_mm = 0.0f;
    s_state.active_left_edge_trim_known = false;
    s_state.active_right_edge_trim_mm = 0.0f;
    s_state.active_right_edge_trim_known = false;
    s_state.eta_min = 0.0f;
    s_state.current_layer = 0;
    s_state.encoder_count = 0;
    s_state.carriage_direction = HMI_CARRIAGE_STOPPED;
    s_state.motor_state = "Idle";
    s_state.last_event = "Waiting for controller";
    s_state.last_error = NULL;
    s_state.safety_ok = true;
    s_connection_state = HMI_CONNECTION_DISCONNECTED;
}

void hmi_model_set_state(const hmi_state_t *state)
{
    if (state == NULL) {
        return;
    }

    s_state = *state;
}

const hmi_state_t *hmi_model_get_state(void)
{
    return &s_state;
}

void hmi_model_set_connection_state(hmi_connection_state_t connection)
{
    s_connection_state = connection;
}

hmi_connection_state_t hmi_model_get_connection_state(void)
{
    return s_connection_state;
}
