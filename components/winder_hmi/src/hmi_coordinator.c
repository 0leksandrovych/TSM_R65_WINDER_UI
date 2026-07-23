#include "hmi_coordinator.h"

#include <stdio.h>

#include "hmi_job_draft_model.h"
#include "hmi_model.h"
#include "hmi_navigation.h"
#include "hmi_pending_command.h"

static char s_command_rejected_reason[HMI_TEXT_MESSAGE_MAX];
static const char s_timeout_reason[] = "Controller response timeout";

static bool machine_is_active_run_state(hmi_machine_state_t state)
{
    switch (state) {
    case HMI_MACHINE_ACCELERATING:
    case HMI_MACHINE_RUNNING:
    case HMI_MACHINE_PAUSED:
    case HMI_MACHINE_STOPPING:
        return true;
    default:
        return false;
    }
}

static hmi_pending_command_t expected_pending_for_command(hmi_command_t command)
{
    switch (command) {
    case HMI_CMD_START_HOMING:
        return HMI_PENDING_START_HOMING;
    case HMI_CMD_ABORT_HOMING:
        return HMI_PENDING_ABORT_HOMING;
    case HMI_CMD_VALIDATE_JOB:
        return HMI_PENDING_VALIDATE_JOB;
    case HMI_CMD_START_JOB:
        return HMI_PENDING_START_JOB;
    case HMI_CMD_PAUSE_JOB:
        return HMI_PENDING_PAUSE_JOB;
    case HMI_CMD_RESUME_JOB:
        return HMI_PENDING_RESUME_JOB;
    case HMI_CMD_ABORT_JOB:
        return HMI_PENDING_ABORT_JOB;
    case HMI_CMD_FINISH_JOB:
        return HMI_PENDING_FINISH_JOB;
    case HMI_CMD_RESET_JOB:
        return HMI_PENDING_RESET_JOB;
    case HMI_CMD_RESET_UNWOUND_COUNTER:
        return HMI_PENDING_RESET_UNWOUND_COUNTER;
    case HMI_CMD_RESET_ALARM:
        return HMI_PENDING_RESET_ALARM;
    case HMI_CMD_SET_SPEED_OVERRIDE:
    case HMI_CMD_APPLY_EDGE_TRIM:
    default:
        return HMI_PENDING_NONE;
    }
}

static void handle_alarm_interrupt(const hmi_state_t *state)
{
    if (state == NULL || !hmi_pending_command_is_active()) {
        return;
    }

    if (state->machine_state == HMI_MACHINE_ALARM) {
        hmi_pending_command_clear();
    }
}

static void handle_run_command_completion(const hmi_state_t *state)
{
    if (state == NULL ||
        !state->machine_state_known ||
        !hmi_pending_command_is_active()) {
        return;
    }

    /* Job-lifecycle commands are cleared only by the telemetry snapshot that
     * confirms the resulting machine state — never by the command ACK alone.
     * ABORT and FINISH both resolve to FINISHED; RESET resolves to
     * HOMING_REQUIRED (not READY). */
    hmi_pending_command_t pending = hmi_pending_command_get();
    if ((pending == HMI_PENDING_PAUSE_JOB &&
         state->machine_state == HMI_MACHINE_PAUSED) ||
        (pending == HMI_PENDING_RESUME_JOB &&
         state->machine_state == HMI_MACHINE_RUNNING) ||
        (pending == HMI_PENDING_ABORT_JOB &&
         state->machine_state == HMI_MACHINE_FINISHED) ||
        (pending == HMI_PENDING_FINISH_JOB &&
         state->machine_state == HMI_MACHINE_FINISHED) ||
        (pending == HMI_PENDING_RESET_JOB &&
         state->machine_state == HMI_MACHINE_HOMING_REQUIRED)) {
        hmi_pending_command_clear();
    }
}

void hmi_coordinator_init(void)
{
    s_command_rejected_reason[0] = '\0';
}

void hmi_coordinator_on_state_update(const hmi_state_t *state)
{
    if (state == NULL) {
        return;
    }

    const hmi_state_t *previous_state = hmi_model_get_state();
    bool was_active_run = previous_state != NULL &&
                          previous_state->machine_state_known &&
                          machine_is_active_run_state(previous_state->machine_state);
    bool was_finished = previous_state != NULL &&
                        previous_state->machine_state_known &&
                        previous_state->machine_state == HMI_MACHINE_FINISHED;

    hmi_model_set_state(state);
    const hmi_state_t *current_state = hmi_model_get_state();
    handle_run_command_completion(current_state);
    handle_alarm_interrupt(current_state);

    bool entered_finished = current_state->machine_state_known &&
                            current_state->machine_state == HMI_MACHINE_FINISHED &&
                            !was_finished;
    if (entered_finished &&
        hmi_navigation_current() != HMI_SCREEN_FINISHED) {
        hmi_navigation_show(HMI_SCREEN_FINISHED);
        return;
    }

    /* CLOSE JOB (RESET_JOB) drives FINISHED -> HOMING_REQUIRED on the
     * controller. Leave the Finished screen only once that telemetry arrives —
     * never optimistically on ACK or on a READY state. */
    if (current_state->machine_state_known &&
        current_state->machine_state == HMI_MACHINE_HOMING_REQUIRED &&
        hmi_navigation_current() == HMI_SCREEN_FINISHED) {
        hmi_navigation_home();
        return;
    }

    bool entered_active_run = current_state->machine_state_known &&
                              machine_is_active_run_state(current_state->machine_state) &&
                              !was_active_run;
    if (entered_active_run &&
        hmi_navigation_current() == HMI_SCREEN_CONFIRM_START) {
        hmi_navigation_show(HMI_SCREEN_RUN);
        return;
    }

    hmi_navigation_update(current_state);
}

void hmi_coordinator_on_command_accepted(hmi_command_t command)
{
    hmi_pending_command_t expected = expected_pending_for_command(command);
    if (expected == HMI_PENDING_NONE ||
        !hmi_pending_command_is_active() ||
        hmi_pending_command_get() != expected) {
        return;
    }

    /* COMMAND_ACCEPTED must not clear pending for the job-lifecycle commands:
     * their completion is defined by a telemetry snapshot, not the ACK. */
    if (expected != HMI_PENDING_PAUSE_JOB &&
        expected != HMI_PENDING_RESUME_JOB &&
        expected != HMI_PENDING_ABORT_JOB &&
        expected != HMI_PENDING_FINISH_JOB &&
        expected != HMI_PENDING_RESET_JOB) {
        hmi_pending_command_clear();
    }
    hmi_navigation_update(hmi_model_get_state());
}

void hmi_coordinator_on_command_rejected(const hmi_command_rejected_t *rejected)
{
    if (rejected == NULL) {
        return;
    }

    snprintf(
        s_command_rejected_reason,
        sizeof(s_command_rejected_reason),
        "%s",
        rejected->reason[0] != '\0' ? rejected->reason : "Command rejected"
    );

    hmi_pending_command_t expected = expected_pending_for_command(rejected->command);
    if (expected != HMI_PENDING_NONE &&
        hmi_pending_command_is_active() &&
        hmi_pending_command_get() == expected) {
        hmi_pending_command_clear();
    }

    hmi_state_t state = *hmi_model_get_state();
    state.last_error = s_command_rejected_reason;
    state.last_event = s_command_rejected_reason;
    hmi_model_set_state(&state);
    hmi_navigation_update(hmi_model_get_state());
}

void hmi_coordinator_on_validation_result(const hmi_job_validation_t *validation)
{
    if (validation == NULL) {
        return;
    }

    hmi_job_draft_model_apply_validation_result(validation);

    if (hmi_pending_command_is_active() &&
        hmi_pending_command_get() == HMI_PENDING_VALIDATE_JOB) {
        hmi_pending_command_clear();
    }

    hmi_state_t state = *hmi_model_get_state();
    state.job_state = validation->valid ? HMI_JOB_VALID : HMI_JOB_INVALID;
    state.last_event = validation->valid ? "Job validation accepted" : "Job validation rejected";
    hmi_model_set_state(&state);
    hmi_navigation_update(hmi_model_get_state());
}

void hmi_coordinator_on_connection_changed(hmi_connection_state_t connection)
{
    hmi_model_set_connection_state(connection);
    hmi_navigation_update(hmi_model_get_state());
}

void hmi_coordinator_on_tick(uint32_t now_ms)
{
    if (!hmi_pending_command_is_active()) {
        return;
    }

    if (!hmi_pending_command_is_expired(now_ms)) {
        return;
    }

    hmi_pending_command_clear();

    hmi_state_t state = *hmi_model_get_state();
    state.last_error = s_timeout_reason;
    state.last_event = s_timeout_reason;
    hmi_model_set_state(&state);
    hmi_navigation_update(hmi_model_get_state());
}
