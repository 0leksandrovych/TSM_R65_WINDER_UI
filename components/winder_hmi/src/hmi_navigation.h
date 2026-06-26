#pragma once

#include "lvgl.h"
#include "hmi_state.h"

typedef enum {
    HMI_SCREEN_HOME,
    HMI_SCREEN_JOBS,
    HMI_SCREEN_CONICAL_SETUP,
    HMI_SCREEN_CONFIRM_START,
    HMI_SCREEN_HOMING,
    HMI_SCREEN_RUN,
    HMI_SCREEN_DIAGNOSTICS,
    HMI_SCREEN_SETTINGS
} hmi_screen_id_t;

void hmi_navigation_init(lv_obj_t *root);
void hmi_navigation_show(hmi_screen_id_t screen_id);
void hmi_navigation_home(void);
void hmi_navigation_update(const hmi_state_t *state);
hmi_screen_id_t hmi_navigation_current(void);
