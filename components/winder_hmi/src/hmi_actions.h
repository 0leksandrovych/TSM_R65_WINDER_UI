#pragma once

#include <stdint.h>

#include "hmi_events.h"
#include "hmi_capability_model.h"
#include "hmi_param.h"

void hmi_actions_home_primary(void);
void hmi_actions_go_home(void);
void hmi_actions_open_jobs(void);
void hmi_actions_select_job_mode(hmi_job_mode_id_t mode_id);
void hmi_actions_open_active_setup(void);
void hmi_actions_open_confirm_start(void);
void hmi_actions_validate_job(void);
void hmi_actions_update_job_param(uint16_t param_id, hmi_param_value_t value);
void hmi_actions_confirm_start_job(void);
void hmi_actions_open_homing(void);
void hmi_actions_open_run(void);
void hmi_actions_open_finished(void);
void hmi_actions_open_diagnostics(void);
void hmi_actions_open_settings(void);
void hmi_actions_placeholder_machine(void);
void hmi_actions_placeholder_jobs(void);
void hmi_actions_start_homing(void);
void hmi_actions_abort_homing(void);
void hmi_actions_homing_next_measurement(void);
void hmi_actions_move_carriage_to_zero(void);
void hmi_actions_move_carriage_to_left_edge(void);
void hmi_actions_start_job(void);
void hmi_actions_pause_job(void);
void hmi_actions_resume_job(void);
void hmi_actions_abort_job(void);
void hmi_actions_finish_job(void);
void hmi_actions_reset_job(void);
void hmi_actions_set_speed_override(float percent);
bool hmi_actions_apply_edge_trim(float left_trim_mm, float right_trim_mm);
void hmi_actions_reset_unwound_counter(void);
void hmi_actions_reset_alarm(void);
