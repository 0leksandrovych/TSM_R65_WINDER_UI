#include "hmi_coordinator.h"

#include <stdio.h>

#include "hmi_job_draft_model.h"
#include "hmi_model.h"
#include "hmi_navigation.h"
#include "hmi_pending_command.h"

static char s_command_rejected_reason[HMI_TEXT_MESSAGE_MAX];
static const char s_timeout_reason[] = "Controller response timeout";

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
    case HMI_CMD_STOP_JOB:
        return HMI_PENDING_STOP_JOB;
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

void hmi_coordinator_init(void)
{
    s_command_rejected_reason[0] = '\0';
}

void hmi_coordinator_on_state_update(const hmi_state_t *state)
{
    if (state == NULL) {
        return;
    }

    hmi_model_set_state(state);
    handle_alarm_interrupt(hmi_model_get_state());
    hmi_navigation_update(hmi_model_get_state());
}

void hmi_coordinator_on_command_accepted(hmi_command_t command)
{
    hmi_pending_command_t expected = expected_pending_for_command(command);
    if (expected == HMI_PENDING_NONE ||
        !hmi_pending_command_is_active() ||
        hmi_pending_command_get() != expected) {
        return;
    }

    hmi_pending_command_clear();
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

    hmi_pending_command_clear();

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
