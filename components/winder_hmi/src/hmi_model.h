#pragma once

#include "hmi_events.h"
#include "hmi_state.h"

void hmi_model_init(void);
void hmi_model_set_state(const hmi_state_t *state);
const hmi_state_t *hmi_model_get_state(void);
void hmi_model_set_connection_state(hmi_connection_state_t connection);
hmi_connection_state_t hmi_model_get_connection_state(void);
