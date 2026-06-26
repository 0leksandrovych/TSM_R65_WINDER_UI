#pragma once

#include "lvgl.h"
#include "hmi_state.h"

void screen_diagnostics_create(lv_obj_t *root);
void screen_diagnostics_update(const hmi_state_t *state);
