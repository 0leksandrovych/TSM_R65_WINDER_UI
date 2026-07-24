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

static void show_action_error(const char *message)
{
    const hmi_state_t *current = hmi_model_get_state();
    if (current == NULL || message == NULL) {
        return;
    }

    hmi_state_t state = *current;
    state.last_error = message;
    state.last_event = message;
    hmi_model_set_state(&state);
    hmi_navigation_update(hmi_model_get_state());
}

static const char *start_job_block_reason(const hmi_state_t *state)
{
    if (state == NULL || !state->machine_state_known) {
        return "Machine state unavailable";
    }
    if (!state->carriage_reference_position_known ||
        state->carriage_reference_position == HMI_CARRIAGE_POSITION_UNKNOWN) {
        return "Carriage position unavailable";
    }
    if (state->carriage_reference_position == HMI_CARRIAGE_POSITION_LEFT_EDGE) {
        return "Return carriage to zero before starting";
    }
    if (state->carriage_reference_position == HMI_CARRIAGE_POSITION_MOVING ||
        hmi_machine_state_is_positioning(state->machine_state)) {
        return "Wait until carriage positioning completes";
    }
    if (state->machine_state != HMI_MACHINE_READY) {
        return "Machine is not ready to start";
    }
    if (!state->safety_ok) {
        return "Safety circuit is not ready";
    }
    if (hmi_model_get_connection_state() != HMI_CONNECTION_CONNECTED) {
        return "Controller not connected";
    }
    if (hmi_pending_command_is_active()) {
        return "Another command is in progress";
    }
    return NULL;
}

static void start_job_if_allowed(void)
{
    const hmi_state_t *state = hmi_model_get_state();
    const char *reason = start_job_block_reason(state);
    if (reason != NULL || !hmi_state_can_start_job(state)) {
        show_action_error(reason != NULL ? reason : "Winding start is unavailable");
        return;
    }

    if (!hmi_job_draft_model_validate_local()) {
        const hmi_job_validation_t *validation =
            hmi_job_draft_model_get_validation();

        show_action_error(
            validation != NULL && validation->message[0] != '\0'
                ? validation->message
                : "Job parameters are invalid");
        return;
    }

    if (!hmi_command_bus_emit(HMI_CMD_START_JOB, NULL)) {
        return;
    }

    hmi_pending_command_set(HMI_PENDING_START_JOB, "Starting job...",
                            hmi_now_ms(), HMI_PENDING_START_TIMEOUT_MS);
    hmi_navigation_update(state);
}

void hmi_actions_home_primary(void)
{
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL) {
        return;
    }

    if (state->machine_state == HMI_MACHINE_HOMING_REQUIRED ||
        hmi_machine_state_is_homing(state->machine_state) ||
        hmi_machine_state_is_positioning(state->machine_state)) {
        hmi_navigation_show(HMI_SCREEN_HOMING);
        return;
    }

    if (state->machine_state == HMI_MACHINE_ALARM) {
        hmi_actions_open_diagnostics();
        return;
    }

    if (state->machine_state == HMI_MACHINE_READY &&
        state->carriage_reference_position_known &&
        state->carriage_reference_position == HMI_CARRIAGE_POSITION_LEFT_EDGE) {
        hmi_actions_move_carriage_to_zero();
        return;
    }

    if (state->machine_state == HMI_MACHINE_READY &&
        state->carriage_reference_position_known &&
        state->carriage_reference_position == HMI_CARRIAGE_POSITION_ZERO) {
        if (hmi_job_draft_model_get_mode() == HMI_JOB_MODE_NONE) {
            hmi_actions_open_jobs();
        } else {
            hmi_actions_open_confirm_start();
        }
        return;
    }

    if (state->machine_state == HMI_MACHINE_FINISHED) {
        hmi_actions_open_finished();
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
    start_job_if_allowed();
}

void hmi_actions_open_homing(void)
{
    hmi_navigation_show(HMI_SCREEN_HOMING);
}

void hmi_actions_open_run(void)
{
    hmi_navigation_show(HMI_SCREEN_RUN);
}

void hmi_actions_open_finished(void)
{
    hmi_navigation_show(HMI_SCREEN_FINISHED);
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
    /* START_HOMING is only meaningful when the machine has no travel references
     * yet (HOMING_REQUIRED) or is idle and can re-home (READY). It must never be
     * sent during an active job, stopping, finished, alarm, or active homing. */
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL ||
        !state->machine_state_known ||
        (state->machine_state != HMI_MACHINE_HOMING_REQUIRED &&
         state->machine_state != HMI_MACHINE_READY) ||
        hmi_pending_command_is_active() ||
        !hmi_command_bus_emit(HMI_CMD_START_HOMING, NULL)) {
        return;
    }

    hmi_pending_command_set(HMI_PENDING_START_HOMING, "Starting homing...",
                            hmi_now_ms(), HMI_PENDING_START_TIMEOUT_MS);
    hmi_navigation_update(state);
}

void hmi_actions_abort_homing(void)
{
    hmi_pending_command_set(HMI_PENDING_ABORT_HOMING, "Aborting homing...",
                            hmi_now_ms(), HMI_PENDING_DEFAULT_TIMEOUT_MS);
    hmi_navigation_update(hmi_model_get_state());
    hmi_command_bus_emit(HMI_CMD_ABORT_HOMING, NULL);
}

void hmi_actions_homing_next_measurement(void)
{
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL ||
        !state->machine_state_known ||
        state->machine_state != HMI_MACHINE_HOMING_WAITING_NEXT_MEASUREMENT ||
        hmi_pending_command_is_active()) {
        return;
    }

    (void)hmi_command_bus_emit(HMI_CMD_HOMING_NEXT_MEASUREMENT, NULL);
}

void hmi_actions_move_carriage_to_zero(void)
{
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL || !state->machine_state_known ||
        hmi_pending_command_is_active()) {
        return;
    }

    bool allowed_after_homing =
        state->machine_state == HMI_MACHINE_HOMING_WAITING_ZERO_COMMAND;
    bool allowed_from_left_edge =
        state->machine_state == HMI_MACHINE_READY &&
        state->carriage_reference_position_known &&
        state->carriage_reference_position == HMI_CARRIAGE_POSITION_LEFT_EDGE;
    if (!allowed_after_homing && !allowed_from_left_edge) {
        return;
    }

    (void)hmi_command_bus_emit(HMI_CMD_MOVE_CARRIAGE_TO_ZERO, NULL);
}

void hmi_actions_move_carriage_to_left_edge(void)
{
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL ||
        !state->machine_state_known ||
        state->machine_state != HMI_MACHINE_READY ||
        !state->carriage_reference_position_known ||
        state->carriage_reference_position != HMI_CARRIAGE_POSITION_ZERO ||
        hmi_pending_command_is_active()) {
        return;
    }

    (void)hmi_command_bus_emit(HMI_CMD_MOVE_CARRIAGE_TO_LEFT_EDGE, NULL);
}

void hmi_actions_start_job(void)
{
    start_job_if_allowed();
}

void hmi_actions_pause_job(void)
{
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL ||
        !state->machine_state_known ||
        state->machine_state != HMI_MACHINE_RUNNING ||
        hmi_pending_command_is_active() ||
        !hmi_command_bus_emit(HMI_CMD_PAUSE_JOB, NULL)) {
        return;
    }

    hmi_pending_command_set(HMI_PENDING_PAUSE_JOB, "Pausing...",
                            hmi_now_ms(), HMI_PENDING_DEFAULT_TIMEOUT_MS);
    hmi_navigation_update(state);
}

void hmi_actions_resume_job(void)
{
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL ||
        !state->machine_state_known ||
        state->machine_state != HMI_MACHINE_PAUSED ||
        hmi_pending_command_is_active() ||
        !hmi_command_bus_emit(HMI_CMD_RESUME_JOB, NULL)) {
        return;
    }

    hmi_pending_command_set(HMI_PENDING_RESUME_JOB, "Resuming...",
                            hmi_now_ms(), HMI_PENDING_DEFAULT_TIMEOUT_MS);
    hmi_navigation_update(state);
}

void hmi_actions_abort_job(void)
{
    /* ABORT is only valid while the machine is in motion. FINISHED comes back
     * through telemetry; the HMI never synthesizes it locally. */
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL || !state->machine_state_known) {
        return;
    }

    bool abortable = state->machine_state == HMI_MACHINE_ACCELERATING ||
                     state->machine_state == HMI_MACHINE_RUNNING ||
                     state->machine_state == HMI_MACHINE_STOPPING;
    if (!abortable) {
        return;
    }

    hmi_pending_command_t pending = hmi_pending_command_get();
    bool pending_active = hmi_pending_command_is_active();

    /* Do not resend ABORT while one is already in flight. */
    if (pending_active && pending == HMI_PENDING_ABORT_JOB) {
        return;
    }

    /* The only pending command ABORT is allowed to preempt is a PAUSE that is
     * still decelerating (machine reports STOPPING). Any other in-flight command
     * blocks ABORT so pending state is never arbitrarily overwritten. */
    if (pending_active) {
        bool preempt_pause = state->machine_state == HMI_MACHINE_STOPPING &&
                             pending == HMI_PENDING_PAUSE_JOB;
        if (!preempt_pause) {
            return;
        }
    }

    if (!hmi_command_bus_emit(HMI_CMD_ABORT_JOB, NULL)) {
        return;
    }

    /* Only after a successful emit do we mark ABORT pending (replacing a
     * preempted PAUSE pending, if any). Cleared by telemetry FINISHED. */
    hmi_pending_command_set(HMI_PENDING_ABORT_JOB, "Aborting job...",
                            hmi_now_ms(), HMI_PENDING_STOP_TIMEOUT_MS);
    hmi_navigation_update(state);
}

void hmi_actions_finish_job(void)
{
    /* FINISH gracefully ends a paused job. It requires a clean PAUSED state and
     * no other pending command. FINISHED is confirmed only by telemetry. */
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL ||
        !state->machine_state_known ||
        state->machine_state != HMI_MACHINE_PAUSED ||
        hmi_pending_command_is_active() ||
        !hmi_command_bus_emit(HMI_CMD_FINISH_JOB, NULL)) {
        return;
    }

    hmi_pending_command_set(HMI_PENDING_FINISH_JOB, "Finishing job...",
                            hmi_now_ms(), HMI_PENDING_STOP_TIMEOUT_MS);
    hmi_navigation_update(state);
}

void hmi_actions_reset_job(void)
{
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL ||
        !state->machine_state_known ||
        state->machine_state != HMI_MACHINE_FINISHED ||
        hmi_pending_command_is_active() ||
        !hmi_command_bus_emit(HMI_CMD_RESET_JOB, NULL)) {
        return;
    }

    hmi_pending_command_set_default(
        HMI_PENDING_RESET_JOB,
        "Resetting job...",
        hmi_now_ms());
    hmi_navigation_update(state);
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
