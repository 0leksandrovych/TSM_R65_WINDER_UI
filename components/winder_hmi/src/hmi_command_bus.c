#include "hmi_command_bus.h"
#include "hmi_config.h"

typedef struct {
    hmi_command_listener_t fn;
    void *user_ctx;
} hmi_command_listener_entry_t;

static winder_hmi_command_cb_t s_callback;
static void *s_callback_ctx;
static hmi_command_listener_entry_t s_listeners[HMI_COMMAND_BUS_MAX_LISTENERS];
static size_t s_listener_count;

void hmi_command_bus_set_callback(winder_hmi_command_cb_t callback, void *user_ctx)
{
    s_callback = callback;
    s_callback_ctx = user_ctx;
}

bool hmi_command_bus_add_listener(hmi_command_listener_t listener, void *user_ctx)
{
    if (listener == NULL || s_listener_count >= HMI_COMMAND_BUS_MAX_LISTENERS) {
        return false;
    }

    s_listeners[s_listener_count].fn = listener;
    s_listeners[s_listener_count].user_ctx = user_ctx;
    s_listener_count++;
    return true;
}

bool hmi_command_bus_remove_listener(hmi_command_listener_t listener, void *user_ctx)
{
    if (listener == NULL) {
        return false;
    }

    for (size_t i = 0; i < s_listener_count; i++) {
        if (s_listeners[i].fn == listener && s_listeners[i].user_ctx == user_ctx) {
            for (size_t j = i; j + 1U < s_listener_count; j++) {
                s_listeners[j] = s_listeners[j + 1U];
            }
            s_listener_count--;
            s_listeners[s_listener_count] = (hmi_command_listener_entry_t){0};
            return true;
        }
    }

    return false;
}

void hmi_command_bus_clear_listeners(void)
{
    for (size_t i = 0; i < HMI_COMMAND_BUS_MAX_LISTENERS; i++) {
        s_listeners[i] = (hmi_command_listener_entry_t){0};
    }
    s_listener_count = 0;
}

/* hmi_command_bus_emit is synchronous.
 * The debug/notification callback (if set) is invoked first, followed by all registered
 * listeners in registration order. Calls return only after the last listener returns.
 * Listeners must not block — they run in the LVGL/UI context.
 * The public callback is a debug/notification hook only; it must not be used as
 * a second command execution path. */
bool hmi_command_bus_emit(hmi_command_t command, const hmi_command_payload_t *payload)
{
    hmi_command_payload_t empty_payload = {0};
    const hmi_command_payload_t *safe_payload = payload != NULL ? payload : &empty_payload;
    bool accepted = false;

    if (s_callback != NULL) {
        s_callback(command, safe_payload, s_callback_ctx);
    }

    for (size_t i = 0; i < s_listener_count; i++) {
        if (s_listeners[i].fn != NULL) {
            accepted = s_listeners[i].fn(
                command,
                safe_payload,
                s_listeners[i].user_ctx) || accepted;
        }
    }

    return accepted;
}
