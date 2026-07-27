#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HMI_PENDING_NONE = 0,

    HMI_PENDING_START_HOMING,
    HMI_PENDING_ABORT_HOMING,

    HMI_PENDING_VALIDATE_JOB,
    HMI_PENDING_START_JOB,

    HMI_PENDING_PAUSE_JOB,
    HMI_PENDING_RESUME_JOB,
    HMI_PENDING_ABORT_JOB,
    HMI_PENDING_FINISH_JOB,
    HMI_PENDING_RESET_JOB,

    HMI_PENDING_RESET_UNWOUND_COUNTER,
    HMI_PENDING_RESET_ALARM,
    HMI_PENDING_EDGE_TRIM
} hmi_pending_command_t;

typedef struct {
    int32_t left_centi_mm;
    int32_t right_centi_mm;
} hmi_edge_trim_pair_t;

void hmi_pending_command_clear(void);

/* Set a pending command with an explicit start timestamp and timeout.
 * started_at_ms and timeout_ms are used to detect lost ACK/REJECT responses.
 * A timeout clears the pending state and records a communication error;
 * it does not produce fake success. */
void hmi_pending_command_set(hmi_pending_command_t command,
                             const char *message,
                             uint32_t started_at_ms,
                             uint32_t timeout_ms);

/* Convenience wrapper that applies HMI_PENDING_DEFAULT_TIMEOUT_MS. */
void hmi_pending_command_set_default(hmi_pending_command_t command,
                                     const char *message,
                                     uint32_t started_at_ms);

hmi_pending_command_t hmi_pending_command_get(void);
const char           *hmi_pending_command_get_message(void);
bool                  hmi_pending_command_is_active(void);

/* Returns true if a pending command is active and its timeout has elapsed.
 * Uses unsigned 32-bit subtraction — safe across tick count wrap-around. */
bool     hmi_pending_command_is_expired(uint32_t now_ms);
uint32_t hmi_pending_command_elapsed_ms(uint32_t now_ms);

/* Edge Trim has two phases: a short command awaiting ACK and a staged pair
 * awaiting matching active telemetry. This state is intentionally independent
 * from the generic command-busy slot so other pending policies stay intact. */
void hmi_edge_trim_pending_clear(void);
void hmi_edge_trim_pending_stage_candidate(int32_t left_centi_mm,
                                           int32_t right_centi_mm);
void hmi_edge_trim_pending_accept_candidate(void);
void hmi_edge_trim_pending_reject_candidate(void);
bool hmi_edge_trim_pending_has_candidate(void);
bool hmi_edge_trim_pending_is_valid(void);
bool hmi_edge_trim_pending_get(hmi_edge_trim_pair_t *out_pair);
void hmi_edge_trim_pending_reconcile(int32_t active_left_centi_mm,
                                     int32_t active_right_centi_mm);
