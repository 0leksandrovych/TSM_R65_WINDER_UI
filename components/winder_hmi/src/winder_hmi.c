#include "winder_hmi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hmi_capability_model.h"
#include "hmi_command_bus.h"
#include "hmi_config.h"
#include "hmi_coordinator.h"
#include "hmi_controller_client.h"
#include "hmi_controller_rx_handler.h"
#include "hmi_controller_transport.h"
#include "hmi_event_queue.h"
#include "hmi_job_draft_model.h"
#include "hmi_model.h"
#include "hmi_mock_controller.h"
#include "hmi_navigation.h"
#include "hmi_styles.h"

#include <stdio.h>

static lv_timer_t *s_tick_timer;
static bool s_demo_enabled;
static uint32_t s_last_telemetry_request_ms;

static inline uint32_t hmi_now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void poll_controller_telemetry(uint32_t now_ms)
{
    if (hmi_model_get_connection_state() != HMI_CONNECTION_CONNECTED ||
        (now_ms - s_last_telemetry_request_ms) < HMI_TELEMETRY_POLL_INTERVAL_MS) {
        return;
    }

    if (hmi_controller_client_request_telemetry()) {
        s_last_telemetry_request_ms = now_ms;
    }
}

static void hmi_tick_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    winder_hmi_tick();
}

static void handle_internal_event(const hmi_internal_event_t *event)
{
    if (event == NULL) {
        return;
    }

    switch (event->type) {
    case HMI_INTERNAL_EVENT_STATE_UPDATE:
        hmi_coordinator_on_state_update(&event->data.state);
        break;
    case HMI_INTERNAL_EVENT_CAPABILITIES_UPDATE:
        hmi_coordinator_on_state_update(hmi_model_get_state());
        break;
    case HMI_INTERNAL_EVENT_COMMAND_ACCEPTED:
        hmi_coordinator_on_command_accepted(event->data.command_accepted);
        break;
    case HMI_INTERNAL_EVENT_JOB_VALIDATION_RESULT:
        hmi_coordinator_on_validation_result(&event->data.validation);
        break;
    case HMI_INTERNAL_EVENT_COMMAND_REJECTED:
        hmi_coordinator_on_command_rejected(&event->data.command_rejected);
        break;
    case HMI_INTERNAL_EVENT_CONNECTION_STATE_CHANGED:
        hmi_coordinator_on_connection_changed(event->data.connection);
        break;
    case HMI_INTERNAL_EVENT_NONE:
    default:
        break;
    }
}

/* Bounded event drain: limits processing to HMI_MAX_EVENTS_PER_TICK events per call
 * to prevent a burst of internal events from monopolizing the LVGL/UI context.
 * Any remaining events are handled in the next tick. */
static void process_internal_events(void)
{
    hmi_internal_event_t event;
    uint32_t processed = 0;

    while (processed < HMI_MAX_EVENTS_PER_TICK &&
           hmi_event_queue_poll(&event)) {
        handle_internal_event(&event);
        processed++;
    }
}

void winder_hmi_init(lv_obj_t *root)
{
    if (root == NULL) {
        root = lv_scr_act();
    }

    hmi_styles_init();
    hmi_event_queue_init();
    hmi_model_init();
    hmi_capability_model_init_demo();
    hmi_job_draft_model_init();
    hmi_coordinator_init();
    hmi_mock_controller_init();
    hmi_controller_client_init();
    winder_hmi_enable_demo_mode(false);
    hmi_navigation_init(root);
    hmi_navigation_update(hmi_model_get_state());

    if (s_tick_timer == NULL) {
        s_tick_timer = lv_timer_create(hmi_tick_timer_cb, HMI_TICK_PERIOD_MS, NULL);
    }
}

void winder_hmi_enable_demo_mode(bool enabled)
{
    s_demo_enabled = enabled;
    if (enabled) {
        hmi_mock_controller_set_enabled(true);
        hmi_controller_transport_set(hmi_mock_controller_get_transport());
        hmi_model_set_connection_state(HMI_CONNECTION_CONNECTED);
    } else {
        hmi_mock_controller_set_enabled(false);
        hmi_controller_transport_set(NULL);
        hmi_model_set_connection_state(HMI_CONNECTION_DISCONNECTED);
    }
}

bool winder_hmi_use_uart_controller(const winder_hmi_uart_controller_config_t *config)
{
    if (config == NULL) {
        return false;
    }

    hmi_uart_transport_config_t uart_config = {
        .uart_num = (uart_port_t)config->uart_num,
        .tx_gpio = config->tx_gpio,
        .rx_gpio = config->rx_gpio,
        .baud_rate = config->baud_rate,
        .rx_buffer_size = config->rx_buffer_size,
        .tx_buffer_size = config->tx_buffer_size,
        .rx_task_stack_size = config->rx_task_stack_size,
        .rx_task_priority = config->rx_task_priority,
        .rx_callback = NULL,
        .error_callback = NULL,
        .user_ctx = NULL,
    };

    if (!hmi_controller_client_use_uart_transport(&uart_config)) {
        return false;
    }

    hmi_model_set_connection_state(HMI_CONNECTION_CONNECTED);
    return true;
}

void winder_hmi_set_command_callback(winder_hmi_command_cb_t callback, void *user_ctx)
{
    hmi_command_bus_set_callback(callback, user_ctx);
}

void winder_hmi_set_state(const hmi_state_t *state)
{
    hmi_coordinator_on_state_update(state);
}

bool winder_hmi_post_state(const hmi_state_t *state)
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

bool winder_hmi_post_job_validation_result(const hmi_job_validation_t *result)
{
    if (result == NULL) {
        return false;
    }

    hmi_internal_event_t event = {
        .type = HMI_INTERNAL_EVENT_JOB_VALIDATION_RESULT,
        .data.validation = *result,
    };

    return hmi_event_queue_post(&event);
}

bool winder_hmi_post_command_accepted(hmi_command_t command)
{
    hmi_internal_event_t event = {
        .type = HMI_INTERNAL_EVENT_COMMAND_ACCEPTED,
        .data.command_accepted = command,
    };

    return hmi_event_queue_post(&event);
}

bool winder_hmi_post_command_rejected(hmi_command_t command, const char *reason)
{
    hmi_internal_event_t event = {
        .type = HMI_INTERNAL_EVENT_COMMAND_REJECTED,
    };

    event.data.command_rejected.command = command;
    snprintf(
        event.data.command_rejected.reason,
        sizeof(event.data.command_rejected.reason),
        "%s",
        reason != NULL ? reason : "Command rejected"
    );

    return hmi_event_queue_post(&event);
}

void winder_hmi_tick(void)
{
    uint32_t now_ms = hmi_now_ms();
    poll_controller_telemetry(now_ms);
    hmi_controller_rx_handler_process();
    process_internal_events();
    hmi_coordinator_on_tick(now_ms);
    if (s_demo_enabled) {
        hmi_mock_controller_tick(HMI_MOCK_TICK_PERIOD_MS);
        process_internal_events();
    }
    hmi_navigation_update(hmi_model_get_state());
}
