#pragma once

#include "lvgl.h"
#include "hmi_state.h"

typedef struct {
    lv_obj_t *label;
} hmi_status_badge_t;

void widget_status_badge_create(lv_obj_t *parent, hmi_status_badge_t *badge);
void widget_status_badge_update(hmi_status_badge_t *badge, const hmi_state_t *state);
