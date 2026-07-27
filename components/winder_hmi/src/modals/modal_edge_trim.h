#pragma once

#include "hmi_state.h"

void modal_edge_trim_open(void);
void modal_edge_trim_close(void);
void modal_edge_trim_update(const hmi_state_t *state);
