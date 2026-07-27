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

typedef struct {
    hmi_edge_trim_pair_t pending;
    hmi_edge_trim_pair_t candidate;
    bool pending_valid;
    bool candidate_valid;
} hmi_edge_trim_pending_state_t;

static hmi_edge_trim_pending_state_t s_edge_trim;

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

void hmi_edge_trim_pending_clear(void)
{
    s_edge_trim = (hmi_edge_trim_pending_state_t){0};
}

void hmi_edge_trim_pending_stage_candidate(int32_t left_centi_mm,
                                           int32_t right_centi_mm)
{
    s_edge_trim.candidate = (hmi_edge_trim_pair_t){
        .left_centi_mm = left_centi_mm,
        .right_centi_mm = right_centi_mm,
    };
    s_edge_trim.candidate_valid = true;
}

void hmi_edge_trim_pending_accept_candidate(void)
{
    if (!s_edge_trim.candidate_valid) {
        return;
    }

    s_edge_trim.pending = s_edge_trim.candidate;
    s_edge_trim.pending_valid = true;
    s_edge_trim.candidate_valid = false;
}

void hmi_edge_trim_pending_reject_candidate(void)
{
    s_edge_trim.candidate = (hmi_edge_trim_pair_t){0};
    s_edge_trim.candidate_valid = false;
}

bool hmi_edge_trim_pending_has_candidate(void)
{
    return s_edge_trim.candidate_valid;
}

bool hmi_edge_trim_pending_is_valid(void)
{
    return s_edge_trim.pending_valid;
}

bool hmi_edge_trim_pending_get(hmi_edge_trim_pair_t *out_pair)
{
    if (out_pair == NULL || !s_edge_trim.pending_valid) {
        return false;
    }

    *out_pair = s_edge_trim.pending;
    return true;
}

void hmi_edge_trim_pending_reconcile(int32_t active_left_centi_mm,
                                     int32_t active_right_centi_mm)
{
    if (!s_edge_trim.pending_valid ||
        s_edge_trim.pending.left_centi_mm != active_left_centi_mm ||
        s_edge_trim.pending.right_centi_mm != active_right_centi_mm) {
        return;
    }

    s_edge_trim.pending = (hmi_edge_trim_pair_t){0};
    s_edge_trim.pending_valid = false;
}
