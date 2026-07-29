#include "hmi_controller_rx_handler.h"

#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "hmi_controller_client.h"
#include "hmi_controller_rx_queue.h"
#include "hmi_event_queue.h"
#include "hmi_link_state_mapper.h"
#include "hmi_model.h"

#define HMI_CONTROLLER_RX_DRAIN_LIMIT 8U
#define HMI_CONTROLLER_RX_QUEUE_FULL  1001

static const char *TAG = "hmi_controller_rx";

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

static bool post_connection_state(hmi_connection_state_t connection)
{
    hmi_internal_event_t event = {
        .type = HMI_INTERNAL_EVENT_CONNECTION_STATE_CHANGED,
        .data.connection = connection,
    };

    return hmi_event_queue_post(&event);
}

static bool post_resume_rejected(uint16_t reason_code)
{
    hmi_internal_event_t event = {
        .type = HMI_INTERNAL_EVENT_RESUME_REJECTED,
        .data.resume_rejected_reason = reason_code,
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

    (void)post_connection_state(HMI_CONNECTION_LOST);

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
        if (hmi_link_state_mapper_machine_state(
                snapshot->machine_state,
                &state.machine_state)) {
            state.machine_state_known = true;
        } else {
            ESP_LOGW(
                TAG,
                "Ignoring unsupported link machine state: %d",
                (int)snapshot->machine_state);
        }
    }
    if (snapshot->carriage_reference_position_present) {
        if (hmi_link_state_mapper_carriage_position(
                snapshot->carriage_reference_position,
                &state.carriage_reference_position)) {
            state.carriage_reference_position_known = true;
        } else {
            ESP_LOGW(
                TAG,
                "Ignoring unsupported carriage reference position: %d",
                (int)snapshot->carriage_reference_position);
        }
    }
    if (snapshot->homing_alarm_code_present) {
        state.homing_alarm_code = snapshot->homing_alarm_code;
        state.homing_alarm_code_known = true;
    }
    if (snapshot->left_edge_sample_count_present) {
        state.left_edge_sample_count = snapshot->left_edge_sample_count;
        state.left_edge_sample_count_known = true;
    }
    if (snapshot->right_edge_sample_count_present) {
        state.right_edge_sample_count = snapshot->right_edge_sample_count;
        state.right_edge_sample_count_known = true;
    }
    if (snapshot->homing_sample_target_count_present) {
        state.homing_sample_target_count = snapshot->homing_sample_target_count;
        state.homing_sample_target_count_known = true;
    }
    if (snapshot->job_master_speed_present) {
        state.job_master_speed_rps = (float)snapshot->job_master_speed;
        state.job_master_speed_known = true;
    }
    if (snapshot->master_speed_rps_present) {
        state.master_speed_rps = (float)snapshot->master_speed_rps;
        state.master_speed_known = true;
    }
    if (snapshot->job_winding_pitch_present) {
        state.winding_pitch_mm = (float)snapshot->job_winding_pitch;
        state.winding_pitch_known = true;
    }
    if (snapshot->job_target_length_present) {
        state.target_length_m = (float)snapshot->job_target_length;
        state.target_length_known = true;
    }
    if (snapshot->job_shift_every_present) {
        state.shift_every_layers = (uint32_t)snapshot->job_shift_every;
        state.shift_every_layers_known = true;
    }
    if (snapshot->job_right_edge_shift_present) {
        state.job_right_edge_shift_mm = (float)snapshot->job_right_edge_shift;
        state.job_right_edge_shift_known = true;
    }
    if (snapshot->wound_length_m_present) {
        state.wound_length_m = (float)snapshot->wound_length_m;
        state.wound_length_known = true;
    }
    if (snapshot->completed_layers_present) {
        state.current_layer = (uint32_t)snapshot->completed_layers;
    }
    if (snapshot->applied_right_edge_offset_mm_present) {
        state.right_edge_offset_mm = (float)snapshot->applied_right_edge_offset_mm;
    }
    if (snapshot->active_left_edge_trim_mm_present) {
        state.active_left_edge_trim_mm =
            (float)snapshot->active_left_edge_trim_mm;
        state.active_left_edge_trim_known = true;
    }
    if (snapshot->active_right_edge_trim_mm_present) {
        state.active_right_edge_trim_mm =
            (float)snapshot->active_right_edge_trim_mm;
        state.active_right_edge_trim_known = true;
    }
    if (snapshot->travel_range_mm_present) {
        state.travel_range_mm = snapshot->travel_range_mm;
        state.travel_range_known = true;
    }
    if (snapshot->job_pause_reason_present) {
        if (snapshot->job_pause_reason <=
            (uint32_t)LINK_JOB_PAUSE_REASON_LENGTH_WATCHDOG) {
            state.pause_reason =
                (hmi_job_pause_reason_t)snapshot->job_pause_reason;
            state.pause_reason_known = true;
        } else {
            ESP_LOGW(TAG,
                     "Ignoring unsupported pause reason: %lu",
                     (unsigned long)snapshot->job_pause_reason);
        }
    }

    /* job_state, progress, override, error, and event are not
     * available from the current controller telemetry contract. */

    (void)post_state_update(&state);
    (void)post_connection_state(HMI_CONNECTION_CONNECTED);
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
        case HMI_CONTROLLER_LINK_DECODED_RESUME_REJECTED:
            (void)post_resume_rejected(decoded.data.resume_rejected.reason_code);
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
