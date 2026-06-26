#include "hmi_controller_client.h"

#include "hmi_capability_model.h"
#include "hmi_command_bus.h"
#include "hmi_controller_messages.h"
#include "hmi_controller_transport.h"
#include "hmi_job_draft_model.h"
#include "winder_hmi.h"

static bool s_initialized;

/* Build a complete job payload snapshot from the current draft and capability models.
 * This is the single authoritative conversion point from UI draft → wire payload. */
static bool build_current_job_payload(hmi_controller_job_payload_t *out)
{
    if (out == NULL) {
        return false;
    }

    hmi_job_mode_id_t mode_id = hmi_job_draft_model_get_mode();
    if (mode_id == HMI_JOB_MODE_NONE) {
        return false;
    }

    const hmi_mode_capability_t *mode = hmi_capability_model_get_mode_by_id(mode_id);
    if (mode == NULL || !mode->enabled) {
        return false;
    }

    if (mode->param_count > HMI_CONTROLLER_MAX_JOB_PARAMS) {
        return false;
    }

    out->mode_id = mode_id;
    out->param_count = 0;

    for (size_t i = 0; i < mode->param_count; i++) {
        const hmi_param_descriptor_t *descriptor = &mode->params[i];
        hmi_param_value_t value = {0};

        if (!hmi_job_draft_model_get_value(descriptor->id, &value)) {
            return false;
        }

        out->params[out->param_count].param_id = descriptor->id;
        out->params[out->param_count].type     = descriptor->type;
        out->params[out->param_count].value    = value;
        out->param_count++;
    }

    return true;
}

static void on_command(hmi_command_t command,
                       const hmi_command_payload_t *payload,
                       void *user_ctx)
{
    (void)user_ctx;

    if (!hmi_controller_transport_is_available()) {
        (void)winder_hmi_post_command_rejected(command, "Controller not connected");
        return;
    }

    hmi_controller_message_t message = {0};
    bool ok = false;

    switch (command) {
    case HMI_CMD_VALIDATE_JOB:
    case HMI_CMD_START_JOB: {
        hmi_controller_job_payload_t job = {0};
        if (!build_current_job_payload(&job)) {
            (void)winder_hmi_post_command_rejected(command, "Failed to build job payload");
            return;
        }
        hmi_controller_msg_type_t type = (command == HMI_CMD_VALIDATE_JOB)
            ? HMI_CONTROLLER_MSG_VALIDATE_JOB
            : HMI_CONTROLLER_MSG_START_JOB;
        ok = hmi_controller_message_init_job(&message, type, &job);
        break;
    }
    case HMI_CMD_START_HOMING:
        ok = hmi_controller_message_init(&message, HMI_CONTROLLER_MSG_START_HOMING);
        break;
    case HMI_CMD_ABORT_HOMING:
        ok = hmi_controller_message_init(&message, HMI_CONTROLLER_MSG_ABORT_HOMING);
        break;
    case HMI_CMD_PAUSE_JOB:
        ok = hmi_controller_message_init(&message, HMI_CONTROLLER_MSG_PAUSE_JOB);
        break;
    case HMI_CMD_RESUME_JOB:
        ok = hmi_controller_message_init(&message, HMI_CONTROLLER_MSG_RESUME_JOB);
        break;
    case HMI_CMD_STOP_JOB:
        ok = hmi_controller_message_init(&message, HMI_CONTROLLER_MSG_STOP_JOB);
        break;
    case HMI_CMD_RESET_ALARM:
        ok = hmi_controller_message_init(&message, HMI_CONTROLLER_MSG_RESET_ALARM);
        break;
    case HMI_CMD_RESET_UNWOUND_COUNTER:
        ok = hmi_controller_message_init(&message, HMI_CONTROLLER_MSG_RESET_UNWOUND_COUNTER);
        break;
    case HMI_CMD_SET_SPEED_OVERRIDE:
        message.type = HMI_CONTROLLER_MSG_SET_SPEED_OVERRIDE;
        message.data.speed_override_percent = payload != NULL ? payload->value.f32 : 0.0f;
        ok = true;
        break;
    case HMI_CMD_APPLY_EDGE_TRIM:
        message.type = HMI_CONTROLLER_MSG_APPLY_EDGE_TRIM;
        message.data.edge_trim_mm = payload != NULL ? payload->value.f32 : 0.0f;
        ok = true;
        break;
    default:
        ok = false;
        break;
    }

    if (!ok) {
        (void)winder_hmi_post_command_rejected(command, "Command not supported");
        return;
    }

    if (!hmi_controller_transport_send(&message)) {
        (void)winder_hmi_post_command_rejected(command, "Transport send failed");
    }
}

void hmi_controller_client_init(void)
{
    if (s_initialized) {
        return;
    }

    if (hmi_command_bus_add_listener(on_command, NULL)) {
        s_initialized = true;
    }
}

void hmi_controller_client_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    (void)hmi_command_bus_remove_listener(on_command, NULL);
    s_initialized = false;
}
