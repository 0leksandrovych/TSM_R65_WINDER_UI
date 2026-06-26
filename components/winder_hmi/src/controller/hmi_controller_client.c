#include "hmi_controller_client.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "hmi_capability_model.h"
#include "hmi_command_bus.h"
#include "hmi_controller_messages.h"
#include "hmi_controller_rx_queue.h"
#include "hmi_controller_transport.h"
#include "hmi_event_queue.h"
#include "hmi_job_draft_model.h"
#include "hmi_model.h"
#include "winder_link_protocol.h"

#define HMI_CONTROLLER_SEQ_TRACK_CAPACITY 8U
#define HMI_CONTROLLER_RX_DRAIN_LIMIT     8U
#define HMI_CONTROLLER_RX_QUEUE_FULL      1001

typedef struct {
    bool active;
    uint16_t seq;
    hmi_command_t command;
    hmi_controller_msg_type_t message_type;
} hmi_controller_seq_entry_t;

static bool s_initialized;
static uint16_t s_next_seq = 1U;
static hmi_controller_seq_entry_t s_seq_entries[HMI_CONTROLLER_SEQ_TRACK_CAPACITY];
static hmi_uart_transport_error_callback_t s_uart_error_callback;
static void *s_uart_user_ctx;

static bool uart_transport_send_adapter(
    const hmi_controller_message_t *message,
    uint16_t seq,
    void *user_ctx);

static const hmi_controller_transport_t s_uart_controller_transport = {
    .send = uart_transport_send_adapter,
    .user_ctx = NULL,
};

static bool post_command_accepted(hmi_command_t command)
{
    hmi_internal_event_t event = {
        .type = HMI_INTERNAL_EVENT_COMMAND_ACCEPTED,
        .data.command_accepted = command,
    };

    return hmi_event_queue_post(&event);
}

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

static bool post_state_update(const hmi_state_t *state)
{
    if (state == NULL) {
        return false;
    }

    hmi_internal_event_t event = {
        .type = HMI_INTERNAL_EVENT_STATE_UPDATE,
        .data.state = *state,
    };

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
    case WINDER_LINK_MSG_STOP_JOB:
        *out_command = HMI_CMD_STOP_JOB;
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

static bool command_from_response(uint16_t original_seq,
                                  winder_link_msg_type_t original_type,
                                  hmi_command_t *out_command)
{
    if (command_for_seq(original_seq, out_command)) {
        forget_seq(original_seq);
        return true;
    }

    return command_for_wire_type(original_type, out_command);
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
        (void)post_command_rejected(command, "Controller not connected");
        return;
    }

    hmi_controller_message_t message = {0};
    bool ok = false;

    switch (command) {
    case HMI_CMD_VALIDATE_JOB:
    case HMI_CMD_START_JOB: {
        hmi_controller_job_payload_t job = {0};
        if (!build_current_job_payload(&job)) {
            (void)post_command_rejected(command, "Failed to build job payload");
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
        (void)post_command_rejected(command, "Command not supported");
        return;
    }

    uint16_t seq = allocate_seq();
    if (!remember_seq(seq, command, message.type)) {
        (void)post_command_rejected(command, "Controller command tracking full");
        return;
    }

    if (!hmi_controller_transport_send(&message, seq)) {
        forget_seq(seq);
        (void)post_command_rejected(command, "Transport send failed");
    }
}

static bool uart_transport_send_adapter(
    const hmi_controller_message_t *message,
    uint16_t seq,
    void *user_ctx)
{
    (void)user_ctx;
    return hmi_uart_transport_send_message(message, seq);
}

static void on_uart_decoded_response(
    const hmi_controller_link_decoded_t *decoded,
    void *user_ctx)
{
    (void)user_ctx;

    if (!hmi_controller_rx_queue_push(decoded) && s_uart_error_callback != NULL) {
        s_uart_error_callback(HMI_CONTROLLER_RX_QUEUE_FULL, s_uart_user_ctx);
    }
}

static void on_uart_error(int error_code, void *user_ctx)
{
    (void)user_ctx;

    if (s_uart_error_callback != NULL) {
        s_uart_error_callback(error_code, s_uart_user_ctx);
    }
}

static void handle_command_accepted(
    const hmi_controller_link_command_accepted_t *accepted)
{
    hmi_command_t command;
    if (accepted == NULL ||
        !command_from_response(accepted->original_seq, accepted->original_type, &command)) {
        return;
    }

    (void)post_command_accepted(command);
}

static void handle_command_rejected(
    const hmi_controller_link_command_rejected_t *rejected)
{
    hmi_command_t command;
    char reason[HMI_TEXT_MESSAGE_MAX];

    if (rejected == NULL ||
        !command_from_response(rejected->original_seq, rejected->original_type, &command)) {
        return;
    }

    snprintf(
        reason,
        sizeof(reason),
        "Controller rejected command, code %u",
        (unsigned)rejected->reason_code);

    (void)post_command_rejected(command, reason);
}

static void handle_state_snapshot(
    const hmi_controller_link_state_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return;
    }

    hmi_state_t state = {0};
    const hmi_state_t *current = hmi_model_get_state();
    if (current != NULL) {
        state = *current;
    }

    state.machine_state = (hmi_machine_state_t)snapshot->machine_state;
    state.homing_state = (hmi_homing_state_t)snapshot->homing_state;
    state.job_state = (hmi_job_state_t)snapshot->job_state;
    state.progress_percent = (float)snapshot->progress_permille / 10.0f;
    state.wound_length_m = (float)snapshot->wound_length_mm / 1000.0f;
    state.target_length_m = (float)snapshot->target_length_mm / 1000.0f;
    state.master_speed_rps = (float)snapshot->master_speed_centirps / 100.0f;
    state.speed_override_percent = (float)snapshot->speed_override_permille / 10.0f;

    if (snapshot->error_code != 0U) {
        state.last_error = "Controller error";
    }
    if (snapshot->event_code != 0U) {
        state.last_event = "Controller state update";
    }

    (void)post_state_update(&state);
}

void hmi_controller_client_init(void)
{
    if (s_initialized) {
        return;
    }

    (void)hmi_controller_rx_queue_init();

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
    if (!hmi_controller_rx_queue_init()) {
        return false;
    }

    s_uart_error_callback = config->error_callback;
    s_uart_user_ctx = config->user_ctx;

    hmi_uart_transport_config_t transport_config = *config;
    transport_config.rx_callback = on_uart_decoded_response;
    transport_config.error_callback = on_uart_error;
    transport_config.user_ctx = NULL;

    if (!hmi_uart_transport_init(&transport_config)) {
        return false;
    }
    if (!hmi_uart_transport_start()) {
        return false;
    }

    hmi_controller_transport_set(&s_uart_controller_transport);
    return true;
}

void hmi_controller_client_process(void)
{
    hmi_controller_link_decoded_t decoded;
    uint32_t processed = 0U;

    while (processed < HMI_CONTROLLER_RX_DRAIN_LIMIT &&
           hmi_controller_rx_queue_pop(&decoded)) {
        switch (decoded.type) {
        case HMI_CONTROLLER_LINK_DECODED_COMMAND_ACCEPTED:
            handle_command_accepted(&decoded.data.command_accepted);
            break;
        case HMI_CONTROLLER_LINK_DECODED_COMMAND_REJECTED:
            handle_command_rejected(&decoded.data.command_rejected);
            break;
        case HMI_CONTROLLER_LINK_DECODED_STATE_SNAPSHOT:
            handle_state_snapshot(&decoded.data.state_snapshot);
            break;
        case HMI_CONTROLLER_LINK_DECODED_NONE:
        default:
            break;
        }
        processed++;
    }
}
