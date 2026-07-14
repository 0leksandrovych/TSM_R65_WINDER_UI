#include "hmi_controller_rx_handler.h"

#include <stdint.h>
#include <stdio.h>

#include "hmi_controller_client.h"
#include "hmi_controller_rx_queue.h"
#include "hmi_event_queue.h"
#include "hmi_model.h"

#define HMI_CONTROLLER_RX_DRAIN_LIMIT 8U
#define HMI_CONTROLLER_RX_QUEUE_FULL  1001

static hmi_uart_transport_error_callback_t s_uart_error_callback;
static void *s_uart_user_ctx;

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
        !hmi_controller_client_resolve_response(
            accepted->original_seq,
            accepted->original_type,
            &command)) {
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
        !hmi_controller_client_resolve_response(
            rejected->original_seq,
            rejected->original_type,
            &command)) {
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

    if (snapshot->machine_state_present) {
        state.machine_state = (hmi_machine_state_t)snapshot->machine_state;
    }
    if (snapshot->job_master_speed_present) {
        state.master_speed_rps = (float)snapshot->job_master_speed;
    }
    if (snapshot->job_target_length_present) {
        state.target_length_m = (float)(snapshot->job_target_length / 1000.0);
    }
    if (snapshot->job_right_edge_shift_present) {
        state.right_edge_offset_mm = (float)snapshot->job_right_edge_shift;
    }

    /* job_winding_pitch/job_shift_every are decoded for Phase H3, but the
     * current hmi_state_t has no dedicated fields for them yet. homing_state,
     * job_state, progress, wound length, override, error, and event are not
     * available from controller telemetry yet - Phase H3/future. */

    (void)post_state_update(&state);
}

bool hmi_controller_rx_handler_prepare_uart_config(
    hmi_uart_transport_config_t *config)
{
    if (config == NULL || !hmi_controller_rx_queue_init()) {
        return false;
    }

    s_uart_error_callback = config->error_callback;
    s_uart_user_ctx = config->user_ctx;

    config->rx_callback = on_uart_decoded_response;
    config->error_callback = on_uart_error;
    config->user_ctx = NULL;
    return true;
}

void hmi_controller_rx_handler_process(void)
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
