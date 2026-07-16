#include "hmi_actions.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hmi_command_bus.h"
#include "hmi_config.h"
#include "hmi_job_draft_model.h"
#include "hmi_model.h"
#include "hmi_navigation.h"
#include "hmi_pending_command.h"

static inline uint32_t hmi_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static bool machine_is_homing(hmi_machine_state_t state)
{
    switch (state) {
    case HMI_MACHINE_HOMING_SEARCHING_RIGHT_REFERENCE:
    case HMI_MACHINE_HOMING_BACKING_OFF_RIGHT_REFERENCE:
    case HMI_MACHINE_HOMING_SEARCHING_LEFT_REFERENCE:
    case HMI_MACHINE_HOMING_BACKING_OFF_LEFT_REFERENCE:
    case HMI_MACHINE_HOMING_MEASURING_TRAVEL:
    case HMI_MACHINE_HOMING_APPLYING_OFFSET:
    case HMI_MACHINE_HOMING_COMPLETING:
        return true;
    default:
        return false;
    }
}

void hmi_actions_home_primary(void)
{
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL) {
        return;
    }

    if (state->machine_state == HMI_MACHINE_HOMING_REQUIRED ||
        machine_is_homing(state->machine_state)) {
        hmi_navigation_show(HMI_SCREEN_HOMING);
        return;
    }

    if (state->machine_state == HMI_MACHINE_ALARM) {
        hmi_actions_open_diagnostics();
        return;
    }

    if (state->machine_state == HMI_MACHINE_READY && state->job_state == HMI_JOB_VALID) {
        hmi_actions_open_confirm_start();
        return;
    }

    if (state->machine_state == HMI_MACHINE_ACCELERATING ||
        state->machine_state == HMI_MACHINE_RUNNING ||
        state->machine_state == HMI_MACHINE_PAUSED ||
        state->machine_state == HMI_MACHINE_STOPPING) {
        hmi_actions_open_run();
        return;
    }

    hmi_actions_open_jobs();
}

void hmi_actions_go_home(void)
{
    hmi_navigation_home();
}

void hmi_actions_open_jobs(void)
{
    hmi_navigation_show(HMI_SCREEN_JOBS);
}

void hmi_actions_select_job_mode(hmi_job_mode_id_t mode_id)
{
    if (!hmi_job_draft_model_select_mode(mode_id)) {
        return;
    }

    hmi_navigation_show(HMI_SCREEN_CONICAL_SETUP);
}

void hmi_actions_open_active_setup(void)
{
    if (hmi_job_draft_model_get_mode() == HMI_JOB_MODE_NONE) {
        (void)hmi_job_draft_model_select_mode(HMI_JOB_MODE_CONICAL);
    }
    hmi_navigation_show(HMI_SCREEN_CONICAL_SETUP);
}

void hmi_actions_open_confirm_start(void)
{
    hmi_navigation_show(HMI_SCREEN_CONFIRM_START);
}

void hmi_actions_validate_job(void)
{
    hmi_command_payload_t payload = {
        .mode_id = hmi_job_draft_model_get_mode(),
    };
    hmi_pending_command_set(HMI_PENDING_VALIDATE_JOB, "Validating job...",
                            hmi_now_ms(), HMI_PENDING_VALIDATE_TIMEOUT_MS);
    hmi_navigation_update(hmi_model_get_state());
    hmi_command_bus_emit(HMI_CMD_VALIDATE_JOB, &payload);
}

void hmi_actions_update_job_param(uint16_t param_id, hmi_param_value_t value)
{
    hmi_job_mode_id_t mode_id = hmi_job_draft_model_get_mode();
    if (hmi_capability_model_get_param_by_id(mode_id, param_id) == NULL) {
        return;
    }

    (void)hmi_job_draft_model_set_value(param_id, value);
}

void hmi_actions_confirm_start_job(void)
{
    hmi_pending_command_set(HMI_PENDING_START_JOB, "Starting job...",
                            hmi_now_ms(), HMI_PENDING_START_TIMEOUT_MS);
    hmi_navigation_update(hmi_model_get_state());
    hmi_command_bus_emit(HMI_CMD_START_JOB, NULL);
}

void hmi_actions_open_homing(void)
{
    hmi_navigation_show(HMI_SCREEN_HOMING);
}

void hmi_actions_open_run(void)
{
    hmi_navigation_show(HMI_SCREEN_RUN);
}

void hmi_actions_open_diagnostics(void)
{
    hmi_navigation_show(HMI_SCREEN_DIAGNOSTICS);
}

void hmi_actions_open_settings(void)
{
    hmi_navigation_show(HMI_SCREEN_SETTINGS);
}

void hmi_actions_placeholder_machine(void)
{
    hmi_actions_open_diagnostics();
}

void hmi_actions_placeholder_jobs(void)
{
    hmi_actions_open_jobs();
}

void hmi_actions_start_homing(void)
{
    hmi_pending_command_set(HMI_PENDING_START_HOMING, "Starting homing...",
                            hmi_now_ms(), HMI_PENDING_START_TIMEOUT_MS);
    hmi_navigation_update(hmi_model_get_state());
    hmi_command_bus_emit(HMI_CMD_START_HOMING, NULL);
}

void hmi_actions_abort_homing(void)
{
    hmi_pending_command_set(HMI_PENDING_ABORT_HOMING, "Aborting homing...",
                            hmi_now_ms(), HMI_PENDING_DEFAULT_TIMEOUT_MS);
    hmi_navigation_update(hmi_model_get_state());
    hmi_command_bus_emit(HMI_CMD_ABORT_HOMING, NULL);
}

void hmi_actions_start_job(void)
{
    hmi_pending_command_set(HMI_PENDING_START_JOB, "Starting job...",
                            hmi_now_ms(), HMI_PENDING_START_TIMEOUT_MS);
    hmi_navigation_update(hmi_model_get_state());
    hmi_command_bus_emit(HMI_CMD_START_JOB, NULL);
}

void hmi_actions_pause_job(void)
{
    hmi_pending_command_set(HMI_PENDING_PAUSE_JOB, "Pausing...",
                            hmi_now_ms(), HMI_PENDING_DEFAULT_TIMEOUT_MS);
    hmi_navigation_update(hmi_model_get_state());
    hmi_command_bus_emit(HMI_CMD_PAUSE_JOB, NULL);
}

void hmi_actions_resume_job(void)
{
    hmi_pending_command_set(HMI_PENDING_RESUME_JOB, "Resuming...",
                            hmi_now_ms(), HMI_PENDING_DEFAULT_TIMEOUT_MS);
    hmi_navigation_update(hmi_model_get_state());
    hmi_command_bus_emit(HMI_CMD_RESUME_JOB, NULL);
}

void hmi_actions_stop_job(void)
{
    hmi_pending_command_set(HMI_PENDING_STOP_JOB, "Stopping...",
                            hmi_now_ms(), HMI_PENDING_STOP_TIMEOUT_MS);
    hmi_navigation_update(hmi_model_get_state());
    hmi_command_bus_emit(HMI_CMD_STOP_JOB, NULL);
}

void hmi_actions_set_speed_override(float percent)
{
    hmi_command_payload_t payload = {
        .value.f32 = percent,
    };
    hmi_command_bus_emit(HMI_CMD_SET_SPEED_OVERRIDE, &payload);
}

void hmi_actions_apply_edge_trim(float trim_mm)
{
    hmi_command_payload_t payload = {
        .value.f32 = trim_mm,
    };
    hmi_command_bus_emit(HMI_CMD_APPLY_EDGE_TRIM, &payload);
}

void hmi_actions_reset_unwound_counter(void)
{
    hmi_pending_command_set(HMI_PENDING_RESET_UNWOUND_COUNTER, "Resetting counter...",
                            hmi_now_ms(), HMI_PENDING_DEFAULT_TIMEOUT_MS);
    hmi_navigation_update(hmi_model_get_state());
    hmi_command_bus_emit(HMI_CMD_RESET_UNWOUND_COUNTER, NULL);
}

void hmi_actions_reset_alarm(void)
{
    hmi_pending_command_set(HMI_PENDING_RESET_ALARM, "Resetting alarm...",
                            hmi_now_ms(), HMI_PENDING_DEFAULT_TIMEOUT_MS);
    hmi_navigation_update(hmi_model_get_state());
    hmi_command_bus_emit(HMI_CMD_RESET_ALARM, NULL);
}
