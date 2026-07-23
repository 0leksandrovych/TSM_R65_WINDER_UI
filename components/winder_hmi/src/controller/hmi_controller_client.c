#include "hmi_controller_client.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "hmi_capability_model.h"
#include "hmi_command_bus.h"
#include "hmi_controller_messages.h"
#include "hmi_controller_rx_handler.h"
#include "hmi_controller_transport.h"
#include "hmi_event_queue.h"
#include "hmi_job_draft_model.h"
#include "winder_link_protocol.h"

#define HMI_CONTROLLER_SEQ_TRACK_CAPACITY 8U

typedef struct {
    bool active;
    uint16_t seq;
    hmi_command_t command;
    hmi_controller_msg_type_t message_type;
} hmi_controller_seq_entry_t;

typedef enum {
    CMD_PAYLOAD_NONE,
    CMD_PAYLOAD_JOB,
    CMD_PAYLOAD_SINGLE_FLOAT,
} command_payload_kind_t;

typedef enum {
    CMD_FLOAT_FIELD_NONE,
    CMD_FLOAT_FIELD_SPEED_OVERRIDE_PERCENT,
    CMD_FLOAT_FIELD_EDGE_TRIM_MM,
} command_float_field_t;

typedef struct {
    hmi_command_t command;
    hmi_controller_msg_type_t message_type;
    command_payload_kind_t payload_kind;
    command_float_field_t float_field;
} command_binding_t;

static const command_binding_t s_command_bindings[] = {
    { HMI_CMD_START_HOMING,          HMI_CONTROLLER_MSG_START_HOMING,
      CMD_PAYLOAD_NONE, CMD_FLOAT_FIELD_NONE },
    { HMI_CMD_ABORT_HOMING,          HMI_CONTROLLER_MSG_ABORT_HOMING,
      CMD_PAYLOAD_NONE, CMD_FLOAT_FIELD_NONE },
    { HMI_CMD_VALIDATE_JOB,          HMI_CONTROLLER_MSG_VALIDATE_JOB,
      CMD_PAYLOAD_JOB, CMD_FLOAT_FIELD_NONE },
    { HMI_CMD_START_JOB,             HMI_CONTROLLER_MSG_START_JOB,
      CMD_PAYLOAD_JOB, CMD_FLOAT_FIELD_NONE },
    { HMI_CMD_PAUSE_JOB,             HMI_CONTROLLER_MSG_PAUSE_JOB,
      CMD_PAYLOAD_NONE, CMD_FLOAT_FIELD_NONE },
    { HMI_CMD_RESUME_JOB,            HMI_CONTROLLER_MSG_RESUME_JOB,
      CMD_PAYLOAD_NONE, CMD_FLOAT_FIELD_NONE },
    { HMI_CMD_ABORT_JOB,             HMI_CONTROLLER_MSG_ABORT_JOB,
      CMD_PAYLOAD_NONE, CMD_FLOAT_FIELD_NONE },
    { HMI_CMD_FINISH_JOB,            HMI_CONTROLLER_MSG_FINISH_JOB,
      CMD_PAYLOAD_NONE, CMD_FLOAT_FIELD_NONE },
    { HMI_CMD_RESET_JOB,             HMI_CONTROLLER_MSG_RESET_JOB,
      CMD_PAYLOAD_NONE, CMD_FLOAT_FIELD_NONE },
    { HMI_CMD_RESET_ALARM,           HMI_CONTROLLER_MSG_RESET_ALARM,
      CMD_PAYLOAD_NONE, CMD_FLOAT_FIELD_NONE },
    { HMI_CMD_RESET_UNWOUND_COUNTER, HMI_CONTROLLER_MSG_RESET_UNWOUND_COUNTER,
      CMD_PAYLOAD_NONE, CMD_FLOAT_FIELD_NONE },
    { HMI_CMD_SET_SPEED_OVERRIDE,    HMI_CONTROLLER_MSG_SET_SPEED_OVERRIDE,
      CMD_PAYLOAD_SINGLE_FLOAT, CMD_FLOAT_FIELD_SPEED_OVERRIDE_PERCENT },
    { HMI_CMD_APPLY_EDGE_TRIM,       HMI_CONTROLLER_MSG_APPLY_EDGE_TRIM,
      CMD_PAYLOAD_SINGLE_FLOAT, CMD_FLOAT_FIELD_EDGE_TRIM_MM },
};

static bool s_initialized;
static uint16_t s_next_seq = 1U;
static hmi_controller_seq_entry_t s_seq_entries[HMI_CONTROLLER_SEQ_TRACK_CAPACITY];

static bool uart_transport_send_adapter(
    const hmi_controller_message_t *message,
    uint16_t seq,
    void *user_ctx);

static const hmi_controller_transport_t s_uart_controller_transport = {
    .send = uart_transport_send_adapter,
    .user_ctx = NULL,
};

static bool post_command_rejected(hmi_command_t command, const char *reason)
{
    hmi_internal_event_t event = {
        .type = HMI_INTERNAL_EVENT_COMMAND_REJECTED,
    };

    event.data.command_rejected.command = command;
    snprintf(
        event.data.command_rejected.reason,
        sizeof(event.data.command_rejected.reason),
        "%s",
        reason != NULL ? reason : "Command rejected");

    return hmi_event_queue_post(&event);
}

static uint16_t allocate_seq(void)
{
    uint16_t seq = s_next_seq++;
    if (s_next_seq == 0U) {
        s_next_seq = 1U;
    }
    return seq;
}

static void forget_seq(uint16_t seq)
{
    for (size_t i = 0; i < HMI_CONTROLLER_SEQ_TRACK_CAPACITY; i++) {
        if (s_seq_entries[i].active && s_seq_entries[i].seq == seq) {
            s_seq_entries[i] = (hmi_controller_seq_entry_t){0};
            return;
        }
    }
}

static bool remember_seq(uint16_t seq,
                         hmi_command_t command,
                         hmi_controller_msg_type_t message_type)
{
    for (size_t i = 0; i < HMI_CONTROLLER_SEQ_TRACK_CAPACITY; i++) {
        if (!s_seq_entries[i].active) {
            s_seq_entries[i] = (hmi_controller_seq_entry_t){
                .active = true,
                .seq = seq,
                .command = command,
                .message_type = message_type,
            };
            return true;
        }
    }

    return false;
}

static bool command_for_seq(uint16_t seq, hmi_command_t *out_command)
{
    if (out_command == NULL) {
        return false;
    }

    for (size_t i = 0; i < HMI_CONTROLLER_SEQ_TRACK_CAPACITY; i++) {
        if (s_seq_entries[i].active && s_seq_entries[i].seq == seq) {
            *out_command = s_seq_entries[i].command;
            return true;
        }
    }

    return false;
}

static bool command_for_wire_type(winder_link_msg_type_t type, hmi_command_t *out_command)
{
    if (out_command == NULL) {
        return false;
    }

    switch (type) {
    case WINDER_LINK_MSG_START_HOMING:
        *out_command = HMI_CMD_START_HOMING;
        return true;
    case WINDER_LINK_MSG_ABORT_HOMING:
        *out_command = HMI_CMD_ABORT_HOMING;
        return true;
    case WINDER_LINK_MSG_PAUSE_JOB:
        *out_command = HMI_CMD_PAUSE_JOB;
        return true;
    case WINDER_LINK_MSG_RESUME_JOB:
        *out_command = HMI_CMD_RESUME_JOB;
        return true;
    case WINDER_LINK_MSG_ABORT_JOB:
        *out_command = HMI_CMD_ABORT_JOB;
        return true;
    case WINDER_LINK_MSG_FINISH_JOB:
        *out_command = HMI_CMD_FINISH_JOB;
        return true;
    case WINDER_LINK_MSG_RESET_JOB:
        *out_command = HMI_CMD_RESET_JOB;
        return true;
    case WINDER_LINK_MSG_RESET_UNWOUND_COUNTER:
        *out_command = HMI_CMD_RESET_UNWOUND_COUNTER;
        return true;
    case WINDER_LINK_MSG_RESET_ALARM:
        *out_command = HMI_CMD_RESET_ALARM;
        return true;
    case WINDER_LINK_MSG_SET_SPEED_OVERRIDE:
        *out_command = HMI_CMD_SET_SPEED_OVERRIDE;
        return true;
    case WINDER_LINK_MSG_APPLY_EDGE_TRIM:
        *out_command = HMI_CMD_APPLY_EDGE_TRIM;
        return true;
    default:
        return false;
    }
}

bool hmi_controller_client_resolve_response(
    uint16_t original_seq,
    winder_link_msg_type_t original_type,
    hmi_command_t *out_command)
{
    if (command_for_seq(original_seq, out_command)) {
        forget_seq(original_seq);
        return true;
    }

    return command_for_wire_type(original_type, out_command);
}

bool hmi_controller_client_request_telemetry(void)
{
    if (!hmi_controller_transport_is_available()) {
        return false;
    }

    hmi_controller_message_t message;
    if (!hmi_controller_message_init(
            &message,
            HMI_CONTROLLER_MSG_GET_TELEMETRY)) {
        return false;
    }

    /* STATE_SNAPSHOT has no correlation fields, so this frame sequence must
     * not consume a command-tracking slot. */
    const uint16_t seq = allocate_seq();
    return hmi_controller_transport_send(&message, seq);
}

/* Build a complete job payload snapshot from the current draft and capability models.
 * This is the single authoritative conversion point from UI draft to controller messages. */
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

        if (descriptor->wire.param_id == 0U ||
            !isfinite(descriptor->wire.scale) ||
            !(descriptor->wire.scale > 0.0)) {
            return false;
        }

        out->params[out->param_count].wire_param_id = descriptor->wire.param_id;
        out->params[out->param_count].wire_scale    = descriptor->wire.scale;
        out->params[out->param_count].type          = descriptor->type;
        out->params[out->param_count].value         = value;
        out->param_count++;
    }

    return true;
}

static const command_binding_t *find_command_binding(hmi_command_t command)
{
    for (size_t i = 0;
         i < sizeof(s_command_bindings) / sizeof(s_command_bindings[0]);
         i++) {
        if (s_command_bindings[i].command == command) {
            return &s_command_bindings[i];
        }
    }

    return NULL;
}

static bool on_command(hmi_command_t command,
                       const hmi_command_payload_t *payload,
                       void *user_ctx)
{
    (void)user_ctx;

    if (!hmi_controller_transport_is_available()) {
        (void)post_command_rejected(command, "Controller not connected");
        return false;
    }

    const command_binding_t *binding = find_command_binding(command);
    if (binding == NULL) {
        (void)post_command_rejected(command, "Command not supported");
        return false;
    }

    hmi_controller_message_t message = {0};
    bool ok = false;

    switch (binding->payload_kind) {
    case CMD_PAYLOAD_NONE:
        ok = hmi_controller_message_init(&message, binding->message_type);
        break;
    case CMD_PAYLOAD_JOB: {
        hmi_controller_job_payload_t job = {0};
        if (!build_current_job_payload(&job)) {
            (void)post_command_rejected(command, "Failed to build job payload");
            return false;
        }
        ok = hmi_controller_message_init_job(
            &message,
            binding->message_type,
            &job);
        break;
    }
    case CMD_PAYLOAD_SINGLE_FLOAT: {
        const float value = payload != NULL ? payload->value.f32 : 0.0f;
        message.type = binding->message_type;

        switch (binding->float_field) {
        case CMD_FLOAT_FIELD_SPEED_OVERRIDE_PERCENT:
            message.data.speed_override_percent = value;
            ok = true;
            break;
        case CMD_FLOAT_FIELD_EDGE_TRIM_MM:
            message.data.edge_trim_mm = value;
            ok = true;
            break;
        case CMD_FLOAT_FIELD_NONE:
        default:
            break;
        }
        break;
    }
    default:
        break;
    }

    if (!ok) {
        (void)post_command_rejected(command, "Command not supported");
        return false;
    }

    uint16_t seq = allocate_seq();
    if (!remember_seq(seq, command, message.type)) {
        (void)post_command_rejected(command, "Controller command tracking full");
        return false;
    }

    if (!hmi_controller_transport_send(&message, seq)) {
        forget_seq(seq);
        (void)post_command_rejected(command, "Transport send failed");
        return false;
    }

    return true;
}

static bool uart_transport_send_adapter(
    const hmi_controller_message_t *message,
    uint16_t seq,
    void *user_ctx)
{
    (void)user_ctx;
    return hmi_uart_transport_send_message(message, seq);
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

bool hmi_controller_client_use_uart_transport(
    const hmi_uart_transport_config_t *config)
{
    if (config == NULL) {
        return false;
    }
    hmi_uart_transport_config_t transport_config = *config;
    if (!hmi_controller_rx_handler_prepare_uart_config(&transport_config)) {
        return false;
    }

    if (!hmi_uart_transport_init(&transport_config)) {
        return false;
    }
    if (!hmi_uart_transport_start()) {
        return false;
    }

    hmi_controller_transport_set(&s_uart_controller_transport);
    return true;
}
