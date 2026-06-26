#pragma once

#include "lvgl.h"
#include "hmi_state.h"
#include "widget_status_badge.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *system_ok_dot;
    lv_obj_t *system_ok_label;
    lv_obj_t *clock_label;
    hmi_status_badge_t badge;
} hmi_topbar_t;

void widget_topbar_create(lv_obj_t *parent, hmi_topbar_t *topbar);
void widget_topbar_update(hmi_topbar_t *topbar, const hmi_state_t *state);
