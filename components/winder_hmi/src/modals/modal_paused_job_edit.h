#pragma once

#include <stdbool.h>

#include "hmi_state.h"

void modal_paused_job_edit_open(void);
void modal_paused_job_edit_close(void);
bool modal_paused_job_edit_is_open(void);
void modal_paused_job_edit_update(const hmi_state_t *state);
