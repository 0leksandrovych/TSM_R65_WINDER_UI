#pragma once

#include <stdint.h>

#include "hmi_events.h"
#include "hmi_state.h"

void hmi_coordinator_init(void);

void hmi_coordinator_on_state_update(const hmi_state_t *state);
void hmi_coordinator_on_command_accepted(hmi_command_t command);
void hmi_coordinator_on_command_rejected(const hmi_command_rejected_t *rejected);
void hmi_coordinator_on_validation_result(const hmi_job_validation_t *validation);
void hmi_coordinator_on_connection_changed(hmi_connection_state_t connection);

/* Called once per HMI tick to check for pending command timeouts.
 * If the pending command has expired, the pending state is cleared and a
 * communication error is stored. Does not produce fake command success. */
void hmi_coordinator_on_tick(uint32_t now_ms);
