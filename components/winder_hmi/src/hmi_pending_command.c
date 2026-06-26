#include "hmi_pending_command.h"
#include "hmi_config.h"

#include <stddef.h>

typedef struct {
    hmi_pending_command_t command;
    const char           *message;
    uint32_t              started_at_ms;
    uint32_t              timeout_ms;
    bool                  active;
} hmi_pending_command_state_t;

static hmi_pending_command_state_t s_state;

void hmi_pending_command_clear(void)
{
    s_state = (hmi_pending_command_state_t){0};
}

void hmi_pending_command_set(hmi_pending_command_t command,
                             const char *message,
                             uint32_t started_at_ms,
                             uint32_t timeout_ms)
{
    s_state.command       = command;
    s_state.message       = message;
    s_state.started_at_ms = started_at_ms;
    s_state.timeout_ms    = timeout_ms;
    s_state.active        = (command != HMI_PENDING_NONE);
}

void hmi_pending_command_set_default(hmi_pending_command_t command,
                                     const char *message,
                                     uint32_t started_at_ms)
{
    hmi_pending_command_set(command, message, started_at_ms, HMI_PENDING_DEFAULT_TIMEOUT_MS);
}

hmi_pending_command_t hmi_pending_command_get(void)
{
    return s_state.command;
}

const char *hmi_pending_command_get_message(void)
{
    return s_state.message != NULL ? s_state.message : "";
}

bool hmi_pending_command_is_active(void)
{
    return s_state.active;
}

bool hmi_pending_command_is_expired(uint32_t now_ms)
{
    if (!s_state.active || s_state.timeout_ms == 0) {
        return false;
    }
    /* Unsigned 32-bit subtraction is safe across tick count wrap-around. */
    return (now_ms - s_state.started_at_ms) >= s_state.timeout_ms;
}

uint32_t hmi_pending_command_elapsed_ms(uint32_t now_ms)
{
    if (!s_state.active) {
        return 0;
    }
    return now_ms - s_state.started_at_ms;
}
