#pragma once

#include "lvgl.h"
#include "hmi_state.h"

void screen_jobs_create(lv_obj_t *root);
void screen_jobs_update(const hmi_state_t *state);
