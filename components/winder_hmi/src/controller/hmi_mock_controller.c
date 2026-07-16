#include "hmi_mock_controller.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "hmi_capability_model.h"
#include "hmi_param.h"
#include "winder_hmi.h"

#define MOCK_VALIDATE_DELAY_MS          150U
#define MOCK_START_JOB_DELAY_MS         200U
#define MOCK_SIMPLE_COMMAND_DELAY_MS    100U
#define MOCK_STOP_DELAY_MS              200U
#define MOCK_START_HOMING_DELAY_MS      150U
#define MOCK_HOMING_COMPLETE_DELAY_MS  1400U
#define MOCK_RUN_UPDATE_MS              250U

typedef enum {
    MOCK_OP_NONE = 0,
    MOCK_OP_VALIDATE_JOB,
    MOCK_OP_START_JOB,
    MOCK_OP_START_HOMING,
    MOCK_OP_ABORT_HOMING,
    MOCK_OP_PAUSE_JOB,
    MOCK_OP_RESUME_JOB,
    MOCK_OP_STOP_JOB,
    MOCK_OP_RESET_UNWOUND_COUNTER,
    MOCK_OP_RESET_ALARM,
} mock_operation_t;

static hmi_state_t s_state;
static bool s_enabled;
static bool s_job_valid;
static bool s_homing_active;
static bool s_run_active;
static uint32_t s_homing_elapsed_ms;
static uint32_t s_run_update_elapsed_ms;
static mock_operation_t s_pending_op;
static uint32_t s_pending_elapsed_ms;
static uint32_t s_pending_delay_ms;
static hmi_controller_message_t s_pending_message;

static const hmi_state_t s_ready_state = {
    .machine_state          = HMI_MACHINE_HOMING_REQUIRED,
    .machine_state_known    = true,
    .job_state              = HMI_JOB_NOT_CONFIGURED,
    .selected_mode          = "Conical Winding",
    .unwound_length_m       = 0.0f,
    .wound_length_m         = 0.0f,
    .target_length_m        = 125.0f,
    .progress_percent       = 0.0f,
    .carriage_position_mm   = 0.0f,
    .travel_range_mm        = 0.0,
    .travel_range_known     = false,
    .master_speed_rps       = 0.0f,
    .speed_override_percent = 100.0f,
    .right_edge_offset_mm   = 0.0f,
    .eta_min                = 0.0f,
    .current_layer          = 0,
    .encoder_count          = 0,
    .carriage_direction     = HMI_CARRIAGE_STOPPED,
    .left_limit_active      = false,
    .right_limit_active     = false,
    .motor_state            = "Idle",
    .last_event             = "Mock controller ready",
    .last_error             = NULL,
    .safety_ok              = true,
};

static bool mock_send(const hmi_controller_message_t *message, uint16_t seq, void *user_ctx);

static const hmi_controller_transport_t s_transport = {
    .send     = mock_send,
    .user_ctx = NULL,
};

static void publish_state(void)
{
    (void)winder_hmi_post_state(&s_state);
}

static const char *active_mode_title(hmi_job_mode_id_t mode_id)
{
    const hmi_mode_capability_t *mode = hmi_capability_model_get_mode_by_id(mode_id);
    return (mode != NULL && mode->title != NULL) ? mode->title : "No job mode";
}

/* Look up a parameter value from the received job payload only.
 * Returns false if the parameter is absent or has an unexpected type. */
static bool payload_value_by_key(const hmi_controller_job_payload_t *job,
                                 const char *key,
                                 hmi_param_value_t *out_value,
                                 const hmi_param_descriptor_t **out_descriptor)
{
    if (job == NULL || out_value == NULL || out_descriptor == NULL) {
        return false;
    }

    const hmi_param_descriptor_t *descriptor =
        hmi_capability_model_get_param_by_key(job->mode_id, key);
    if (descriptor == NULL) {
        return false;
    }

    for (size_t i = 0; i < job->param_count; i++) {
        if (job->params[i].param_id == descriptor->id &&
            job->params[i].type == descriptor->type) {
            *out_value = job->params[i].value;
            *out_descriptor = descriptor;
            return true;
        }
    }

    return false;
}

static float payload_float_by_key(const hmi_controller_job_payload_t *job,
                                  const char *key,
                                  float fallback)
{
    hmi_param_value_t value = {0};
    const hmi_param_descriptor_t *descriptor = NULL;

    if (!payload_value_by_key(job, key, &value, &descriptor)) {
        return fallback;
    }

    return hmi_param_value_as_float(descriptor, value);
}

static void set_validation_message(hmi_job_validation_t *validation,
                                   bool valid,
                                   const char *message)
{
    if (validation == NULL) {
        return;
    }

    validation->valid = valid;
    snprintf(validation->message, sizeof(validation->message),
             "%s", message != NULL ? message : "");
}

/* Validate only what is present in the received payload.
 * Returns false with a message if any required parameter is missing or invalid. */
static bool validate_job_payload(const hmi_controller_job_payload_t *job,
                                 hmi_job_validation_t *out_validation)
{
    if (job == NULL || out_validation == NULL) {
        return false;
    }

    *out_validation = (hmi_job_validation_t){0};

    const hmi_mode_capability_t *mode = hmi_capability_model_get_mode_by_id(job->mode_id);
    if (mode == NULL || !mode->enabled) {
        set_validation_message(out_validation, false, "Selected mode is not available");
        return false;
    }

    for (size_t i = 0; i < mode->param_count; i++) {
        const hmi_param_descriptor_t *descriptor = &mode->params[i];
        hmi_param_value_t value = {0};
        char message[HMI_TEXT_MESSAGE_MAX];
        bool found = false;

        for (size_t j = 0; j < job->param_count; j++) {
            if (job->params[j].param_id == descriptor->id &&
                job->params[j].type == descriptor->type) {
                value = job->params[j].value;
                found = true;
                break;
            }
        }

        if (!found) {
            set_validation_message(out_validation, false, "Parameter missing in job payload");
            return false;
        }

        if (!hmi_param_validate_value(descriptor, value, message, sizeof(message))) {
            set_validation_message(out_validation, false, message);
            return false;
        }
    }

    float master_speed  = payload_float_by_key(job, "master_speed",    0.0f);
    float pitch         = payload_float_by_key(job, "winding_pitch",   0.0f);
    float target_length = payload_float_by_key(job, "target_length",   0.0f);
    float right_shift   = payload_float_by_key(job, "right_edge_shift", 0.0f);

    hmi_param_value_t shift_every_value = {0};
    const hmi_param_descriptor_t *shift_every_desc = NULL;
    uint32_t shift_every = 0U;
    if (payload_value_by_key(job, "shift_every", &shift_every_value, &shift_every_desc)) {
        if (shift_every_desc->type == HMI_PARAM_TYPE_UINT32) {
            shift_every = shift_every_value.u32;
        }
    }

    if (master_speed > 0.0f && pitch > 0.0f && shift_every > 0U) {
        float layers_f = target_length / (pitch * 4.0f);
        if (layers_f < 1.0f) {
            layers_f = 1.0f;
        }

        out_validation->estimated_layers    = (uint32_t)ceilf(layers_f);
        out_validation->estimated_time_min  = target_length / (master_speed * 60.0f);
        out_validation->estimated_offset_mm =
            ((float)out_validation->estimated_layers / (float)shift_every) * right_shift;
    }

    set_validation_message(out_validation, true, "Job configuration is valid");
    return true;
}

static void start_pending(mock_operation_t operation,
                          uint32_t delay_ms,
                          const hmi_controller_message_t *message)
{
    if (s_pending_op != MOCK_OP_NONE) {
        return;
    }

    s_pending_op         = operation;
    s_pending_elapsed_ms = 0;
    s_pending_delay_ms   = delay_ms;
    s_pending_message    = message != NULL ? *message : (hmi_controller_message_t){0};
}

static bool reject_if_busy(hmi_command_t command)
{
    if (s_pending_op == MOCK_OP_NONE) {
        return false;
    }

    (void)winder_hmi_post_command_rejected(command, "Controller is busy");
    return true;
}

static bool command_for_message_type(hmi_controller_msg_type_t type,
                                     hmi_command_t *out_command)
{
    if (out_command == NULL) {
        return false;
    }

    switch (type) {
    case HMI_CONTROLLER_MSG_VALIDATE_JOB:
        *out_command = HMI_CMD_VALIDATE_JOB;
        return true;
    case HMI_CONTROLLER_MSG_START_JOB:
        *out_command = HMI_CMD_START_JOB;
        return true;
    case HMI_CONTROLLER_MSG_START_HOMING:
        *out_command = HMI_CMD_START_HOMING;
        return true;
    case HMI_CONTROLLER_MSG_ABORT_HOMING:
        *out_command = HMI_CMD_ABORT_HOMING;
        return true;
    case HMI_CONTROLLER_MSG_PAUSE_JOB:
        *out_command = HMI_CMD_PAUSE_JOB;
        return true;
    case HMI_CONTROLLER_MSG_RESUME_JOB:
        *out_command = HMI_CMD_RESUME_JOB;
        return true;
    case HMI_CONTROLLER_MSG_STOP_JOB:
        *out_command = HMI_CMD_STOP_JOB;
        return true;
    case HMI_CONTROLLER_MSG_RESET_ALARM:
        *out_command = HMI_CMD_RESET_ALARM;
        return true;
    case HMI_CONTROLLER_MSG_RESET_UNWOUND_COUNTER:
        *out_command = HMI_CMD_RESET_UNWOUND_COUNTER;
        return true;
    case HMI_CONTROLLER_MSG_SET_SPEED_OVERRIDE:
        *out_command = HMI_CMD_SET_SPEED_OVERRIDE;
        return true;
    case HMI_CONTROLLER_MSG_APPLY_EDGE_TRIM:
        *out_command = HMI_CMD_APPLY_EDGE_TRIM;
        return true;
    case HMI_CONTROLLER_MSG_GET_TELEMETRY:
    case HMI_CONTROLLER_MSG_GET_CAPABILITIES:
    case HMI_CONTROLLER_MSG_NONE:
    default:
        return false;
    }
}

static void enter_homing(void)
{
    s_homing_active = true;
    s_run_active    = false;
    s_homing_elapsed_ms = 0;
    s_state.machine_state       = HMI_MACHINE_HOMING_SEARCHING_RIGHT_REFERENCE;
    s_state.machine_state_known = true;
    s_state.left_limit_active   = false;
    s_state.right_limit_active  = false;
    s_state.carriage_position_mm = 125.0f;
    s_state.travel_range_mm     = 0.0;
    s_state.travel_range_known  = false;
    s_state.master_speed_rps    = 0.0f;
    s_state.carriage_direction  = HMI_CARRIAGE_STOPPED;
    s_state.motor_state         = "Homing";
    s_state.last_error          = NULL;
    s_state.last_event          = "Homing started";
    publish_state();
}

static void finish_homing(void)
{
    s_homing_active = false;
    s_state.machine_state        = HMI_MACHINE_READY;
    s_state.machine_state_known  = true;
    s_state.carriage_position_mm = 0.0f;
    s_state.travel_range_mm      = 250.0;
    s_state.travel_range_known   = true;
    s_state.left_limit_active    = true;
    s_state.right_limit_active   = false;
    s_state.master_speed_rps     = 0.0f;
    s_state.motor_state          = "Idle";
    s_state.last_event           = "Homing complete";
    publish_state();
}

static void abort_homing(void)
{
    s_homing_active = false;
    s_run_active    = false;
    s_state.machine_state        = HMI_MACHINE_HOMING_REQUIRED;
    s_state.machine_state_known  = true;
    s_state.master_speed_rps     = 0.0f;
    s_state.carriage_position_mm = 0.0f;
    s_state.left_limit_active    = false;
    s_state.right_limit_active   = false;
    s_state.motor_state          = "Idle";
    s_state.last_event           = "Homing aborted";
    publish_state();
}

static void start_run(const hmi_controller_job_payload_t *job)
{
    float target_length = payload_float_by_key(job, "target_length", 120.0f);
    float master_speed  = payload_float_by_key(job, "master_speed",    2.5f);

    s_homing_active = false;
    s_run_active    = true;
    s_run_update_elapsed_ms = 0;
    s_state.machine_state           = HMI_MACHINE_RUNNING;
    s_state.machine_state_known     = true;
    s_state.job_state               = HMI_JOB_VALID;
    s_state.selected_mode           = active_mode_title(job->mode_id);
    s_state.wound_length_m          = 0.0f;
    s_state.target_length_m         = target_length;
    s_state.progress_percent        = 0.0f;
    s_state.master_speed_rps        = master_speed;
    s_state.speed_override_percent  = 100.0f;
    s_state.current_layer           = 1;
    s_state.carriage_position_mm    = 0.0f;
    s_state.travel_range_mm         = 250.0;
    s_state.travel_range_known      = true;
    s_state.right_edge_offset_mm    = 0.0f;
    s_state.carriage_direction      = HMI_CARRIAGE_RIGHT;
    s_state.motor_state             = "Running";
    s_state.last_error              = NULL;
    s_state.last_event              = "Mock run started";
    publish_state();
}

static void stop_run(void)
{
    s_run_active = false;
    s_state.machine_state       = HMI_MACHINE_READY;
    s_state.master_speed_rps    = 0.0f;
    s_state.carriage_direction  = HMI_CARRIAGE_STOPPED;
    s_state.motor_state         = "Idle";
    s_state.last_event          = "Mock run stopped";
    publish_state();
}

static void execute_pending_operation(void)
{
    switch (s_pending_op) {
    case MOCK_OP_VALIDATE_JOB: {
        hmi_job_validation_t validation;
        s_job_valid = validate_job_payload(&s_pending_message.data.job, &validation);
        s_state.job_state     = s_job_valid ? HMI_JOB_VALID : HMI_JOB_INVALID;
        s_state.selected_mode = active_mode_title(s_pending_message.data.job.mode_id);
        s_state.last_event    = s_job_valid ? "Mock job validated" : "Mock job invalid";
        (void)winder_hmi_post_command_accepted(HMI_CMD_VALIDATE_JOB);
        (void)winder_hmi_post_job_validation_result(&validation);
        publish_state();
        break;
    }
    case MOCK_OP_START_JOB:
        if (!s_job_valid && s_state.job_state != HMI_JOB_VALID) {
            (void)winder_hmi_post_command_rejected(HMI_CMD_START_JOB, "Job is not valid");
        } else if (s_state.machine_state != HMI_MACHINE_READY) {
            (void)winder_hmi_post_command_rejected(HMI_CMD_START_JOB, "Machine is not ready");
        } else {
            (void)winder_hmi_post_command_accepted(HMI_CMD_START_JOB);
            start_run(&s_pending_message.data.job);
        }
        break;
    case MOCK_OP_START_HOMING:
        if (s_state.machine_state == HMI_MACHINE_RUNNING ||
            s_state.machine_state == HMI_MACHINE_PAUSED) {
            (void)winder_hmi_post_command_rejected(HMI_CMD_START_HOMING,
                                                   "Cannot home while job is active");
        } else if (s_homing_active) {
            (void)winder_hmi_post_command_rejected(HMI_CMD_START_HOMING,
                                                   "Homing already in progress");
        } else {
            (void)winder_hmi_post_command_accepted(HMI_CMD_START_HOMING);
            enter_homing();
        }
        break;
    case MOCK_OP_ABORT_HOMING:
        if (s_homing_active) {
            (void)winder_hmi_post_command_accepted(HMI_CMD_ABORT_HOMING);
            abort_homing();
        } else {
            (void)winder_hmi_post_command_rejected(HMI_CMD_ABORT_HOMING,
                                                   "Homing is not in progress");
        }
        break;
    case MOCK_OP_PAUSE_JOB:
        if (s_state.machine_state == HMI_MACHINE_RUNNING) {
            (void)winder_hmi_post_command_accepted(HMI_CMD_PAUSE_JOB);
            s_state.machine_state      = HMI_MACHINE_PAUSED;
            s_state.master_speed_rps   = 0.0f;
            s_state.carriage_direction = HMI_CARRIAGE_STOPPED;
            s_state.motor_state        = "Paused";
            s_state.last_event         = "Mock run paused";
            publish_state();
        } else {
            (void)winder_hmi_post_command_rejected(HMI_CMD_PAUSE_JOB, "Machine is not running");
        }
        break;
    case MOCK_OP_RESUME_JOB:
        if (s_state.machine_state == HMI_MACHINE_PAUSED) {
            (void)winder_hmi_post_command_accepted(HMI_CMD_RESUME_JOB);
            s_state.machine_state      = HMI_MACHINE_RUNNING;
            s_state.master_speed_rps   =
                payload_float_by_key(&s_pending_message.data.job, "master_speed", 2.5f);
            s_state.carriage_direction = HMI_CARRIAGE_RIGHT;
            s_state.motor_state        = "Running";
            s_state.last_event         = "Mock run resumed";
            publish_state();
        } else {
            (void)winder_hmi_post_command_rejected(HMI_CMD_RESUME_JOB, "Machine is not paused");
        }
        break;
    case MOCK_OP_STOP_JOB:
        if (s_state.machine_state == HMI_MACHINE_RUNNING ||
            s_state.machine_state == HMI_MACHINE_PAUSED) {
            (void)winder_hmi_post_command_accepted(HMI_CMD_STOP_JOB);
            stop_run();
        } else {
            (void)winder_hmi_post_command_rejected(HMI_CMD_STOP_JOB, "No active job to stop");
        }
        break;
    case MOCK_OP_RESET_UNWOUND_COUNTER:
        if (s_state.machine_state == HMI_MACHINE_RUNNING ||
            s_state.machine_state == HMI_MACHINE_PAUSED) {
            (void)winder_hmi_post_command_rejected(HMI_CMD_RESET_UNWOUND_COUNTER,
                                                   "Cannot reset counter while job is active");
        } else {
            (void)winder_hmi_post_command_accepted(HMI_CMD_RESET_UNWOUND_COUNTER);
            s_state.unwound_length_m = 0.0f;
            s_state.encoder_count    = 0;
            s_state.last_event       = "Unwound counter reset";
            publish_state();
        }
        break;
    case MOCK_OP_RESET_ALARM:
        (void)winder_hmi_post_command_accepted(HMI_CMD_RESET_ALARM);
        s_state.last_error = NULL;
        if (s_state.machine_state == HMI_MACHINE_ALARM) {
            s_state.machine_state = HMI_MACHINE_HOMING_REQUIRED;
        }
        s_state.last_event = "Alarm reset";
        publish_state();
        break;
    case MOCK_OP_NONE:
    default:
        break;
    }

    s_pending_op         = MOCK_OP_NONE;
    s_pending_elapsed_ms = 0;
    s_pending_delay_ms   = 0;
    s_pending_message    = (hmi_controller_message_t){0};
}

static void update_homing(uint32_t elapsed_ms)
{
    if (!s_homing_active) {
        return;
    }

    s_homing_elapsed_ms += elapsed_ms;

    if (s_homing_elapsed_ms >= MOCK_HOMING_COMPLETE_DELAY_MS) {
        finish_homing();
        return;
    }

    if (s_homing_elapsed_ms < 200U) {
        s_state.machine_state        = HMI_MACHINE_HOMING_SEARCHING_RIGHT_REFERENCE;
        s_state.carriage_position_mm = 125.0f - ((float)s_homing_elapsed_ms * 0.18f);
        if (s_state.carriage_position_mm < 0.0f) {
            s_state.carriage_position_mm = 0.0f;
        }
        s_state.left_limit_active = false;
    } else if (s_homing_elapsed_ms < 400U) {
        s_state.machine_state        = HMI_MACHINE_HOMING_BACKING_OFF_RIGHT_REFERENCE;
        s_state.carriage_position_mm = 12.0f;
        s_state.right_limit_active   = true;
    } else if (s_homing_elapsed_ms < 600U) {
        s_state.machine_state        = HMI_MACHINE_HOMING_SEARCHING_LEFT_REFERENCE;
        s_state.carriage_position_mm = 6.0f;
        s_state.right_limit_active   = false;
    } else if (s_homing_elapsed_ms < 800U) {
        s_state.machine_state        = HMI_MACHINE_HOMING_BACKING_OFF_LEFT_REFERENCE;
        s_state.left_limit_active    = true;
    } else if (s_homing_elapsed_ms < 1000U) {
        s_state.machine_state        = HMI_MACHINE_HOMING_MEASURING_TRAVEL;
        s_state.travel_range_mm      = 250.0;
        s_state.travel_range_known   = true;
    } else if (s_homing_elapsed_ms < 1200U) {
        s_state.machine_state        = HMI_MACHINE_HOMING_APPLYING_OFFSET;
    } else {
        s_state.machine_state        = HMI_MACHINE_HOMING_COMPLETING;
    }
}

static void update_run(uint32_t elapsed_ms)
{
    if (!s_run_active || s_state.machine_state != HMI_MACHINE_RUNNING) {
        return;
    }

    s_run_update_elapsed_ms += elapsed_ms;
    if (s_run_update_elapsed_ms < MOCK_RUN_UPDATE_MS) {
        return;
    }
    s_run_update_elapsed_ms = 0;

    float speed_scale = s_state.speed_override_percent > 0.0f
        ? s_state.speed_override_percent / 100.0f
        : 1.0f;
    float increment = 0.45f * speed_scale;
    s_state.wound_length_m   += increment;
    s_state.unwound_length_m += increment;
    s_state.encoder_count    += 64;

    if (s_state.target_length_m > 0.0f) {
        s_state.progress_percent = (s_state.wound_length_m * 100.0f) / s_state.target_length_m;
        if (s_state.progress_percent > 100.0f) {
            s_state.progress_percent = 100.0f;
        }
    }

    if (s_state.carriage_direction == HMI_CARRIAGE_RIGHT) {
        s_state.carriage_position_mm += 12.0f;
        if ((double)s_state.carriage_position_mm >= s_state.travel_range_mm) {
            s_state.carriage_position_mm = (float)s_state.travel_range_mm;
            s_state.carriage_direction   = HMI_CARRIAGE_LEFT;
            s_state.current_layer++;
        }
    } else {
        s_state.carriage_position_mm -= 12.0f;
        if (s_state.carriage_position_mm <= 0.0f) {
            s_state.carriage_position_mm = 0.0f;
            s_state.carriage_direction   = HMI_CARRIAGE_RIGHT;
            s_state.current_layer++;
        }
    }

    s_state.right_edge_offset_mm = (float)(s_state.travel_range_mm - s_state.carriage_position_mm);
    s_state.eta_min = (s_state.target_length_m - s_state.wound_length_m) / 12.0f;
    if (s_state.eta_min < 0.0f) {
        s_state.eta_min = 0.0f;
    }

    if (s_state.progress_percent >= 100.0f) {
        s_run_active               = false;
        s_state.machine_state      = HMI_MACHINE_FINISHED;
        s_state.master_speed_rps   = 0.0f;
        s_state.carriage_direction = HMI_CARRIAGE_STOPPED;
        s_state.motor_state        = "Finished";
        s_state.last_event         = "Mock run complete";
    }

    publish_state();
}

static bool mock_send(const hmi_controller_message_t *message, uint16_t seq, void *user_ctx)
{
    (void)user_ctx;
    (void)seq;

    if (!s_enabled || message == NULL) {
        return false;
    }

    if (message->type == HMI_CONTROLLER_MSG_GET_CAPABILITIES) {
        return true;
    }
    if (message->type == HMI_CONTROLLER_MSG_GET_TELEMETRY) {
        publish_state();
        return true;
    }

    hmi_command_t command = {0};
    if (!command_for_message_type(message->type, &command)) {
        return false;
    }

    if (reject_if_busy(command)) {
        return true;
    }

    switch (message->type) {
    case HMI_CONTROLLER_MSG_VALIDATE_JOB:
        start_pending(MOCK_OP_VALIDATE_JOB, MOCK_VALIDATE_DELAY_MS, message);
        return true;
    case HMI_CONTROLLER_MSG_START_JOB:
        start_pending(MOCK_OP_START_JOB, MOCK_START_JOB_DELAY_MS, message);
        return true;
    case HMI_CONTROLLER_MSG_START_HOMING:
        start_pending(MOCK_OP_START_HOMING, MOCK_START_HOMING_DELAY_MS, message);
        return true;
    case HMI_CONTROLLER_MSG_ABORT_HOMING:
        start_pending(MOCK_OP_ABORT_HOMING, MOCK_SIMPLE_COMMAND_DELAY_MS, message);
        return true;
    case HMI_CONTROLLER_MSG_PAUSE_JOB:
        start_pending(MOCK_OP_PAUSE_JOB, MOCK_SIMPLE_COMMAND_DELAY_MS, message);
        return true;
    case HMI_CONTROLLER_MSG_RESUME_JOB:
        start_pending(MOCK_OP_RESUME_JOB, MOCK_SIMPLE_COMMAND_DELAY_MS, message);
        return true;
    case HMI_CONTROLLER_MSG_STOP_JOB:
        start_pending(MOCK_OP_STOP_JOB, MOCK_STOP_DELAY_MS, message);
        return true;
    case HMI_CONTROLLER_MSG_RESET_UNWOUND_COUNTER:
        start_pending(MOCK_OP_RESET_UNWOUND_COUNTER, MOCK_SIMPLE_COMMAND_DELAY_MS, message);
        return true;
    case HMI_CONTROLLER_MSG_RESET_ALARM:
        start_pending(MOCK_OP_RESET_ALARM, MOCK_SIMPLE_COMMAND_DELAY_MS, message);
        return true;
    case HMI_CONTROLLER_MSG_SET_SPEED_OVERRIDE:
        if (message->data.speed_override_percent > 0.0f) {
            (void)winder_hmi_post_command_accepted(HMI_CMD_SET_SPEED_OVERRIDE);
            s_state.speed_override_percent = message->data.speed_override_percent;
            s_state.last_event = "Speed override updated";
            publish_state();
        }
        return true;
    case HMI_CONTROLLER_MSG_APPLY_EDGE_TRIM:
        (void)winder_hmi_post_command_accepted(HMI_CMD_APPLY_EDGE_TRIM);
        s_state.right_edge_offset_mm += message->data.edge_trim_mm;
        s_state.last_event = "Edge trim applied";
        publish_state();
        return true;
    case HMI_CONTROLLER_MSG_NONE:
    default:
        return false;
    }
}

void hmi_mock_controller_init(void)
{
    hmi_mock_controller_reset();
}

void hmi_mock_controller_reset(void)
{
    s_state                 = s_ready_state;
    s_job_valid             = false;
    s_homing_active         = false;
    s_run_active            = false;
    s_homing_elapsed_ms     = 0;
    s_run_update_elapsed_ms = 0;
    s_pending_op            = MOCK_OP_NONE;
    s_pending_elapsed_ms    = 0;
    s_pending_delay_ms      = 0;
    s_pending_message       = (hmi_controller_message_t){0};
}

void hmi_mock_controller_set_enabled(bool enabled)
{
    s_enabled = enabled;
    if (enabled) {
        hmi_mock_controller_reset();
        publish_state();
    }
}

const hmi_controller_transport_t *hmi_mock_controller_get_transport(void)
{
    return &s_transport;
}

void hmi_mock_controller_tick(uint32_t elapsed_ms)
{
    if (!s_enabled) {
        return;
    }

    if (s_pending_op != MOCK_OP_NONE) {
        s_pending_elapsed_ms += elapsed_ms;
        if (s_pending_elapsed_ms >= s_pending_delay_ms) {
            execute_pending_operation();
        }
    }

    update_homing(elapsed_ms);
    update_run(elapsed_ms);
}
