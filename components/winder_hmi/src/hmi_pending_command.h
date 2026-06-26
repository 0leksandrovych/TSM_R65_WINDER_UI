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
    HMI_PENDING_STOP_JOB,

    HMI_PENDING_RESET_UNWOUND_COUNTER,
    HMI_PENDING_RESET_ALARM
} hmi_pending_command_t;

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
