#pragma once

#include "lvgl.h"
#include "hmi_state.h"

void screen_homing_create(lv_obj_t *root);
void screen_homing_update(const hmi_state_t *state);
