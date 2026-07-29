#include "hmi_navigation.h"

#include "hmi_model.h"
#include "hmi_styles.h"
#include "modal_confirm.h"
#include "modal_numeric_keypad.h"
#include "modal_paused_job_edit.h"
#include "screen_diagnostics.h"
#include "screen_confirm_start.h"
#include "screen_conical_setup.h"
#include "screen_home.h"
#include "screen_homing.h"
#include "screen_jobs.h"
#include "screen_finished.h"
#include "screen_run.h"
#include "screen_settings.h"

static lv_obj_t *s_root;
static hmi_screen_id_t s_current_screen = HMI_SCREEN_HOME;

static void set_root_opa(void *obj, int32_t opa)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)opa, 0);
}

static void animate_screen(lv_obj_t *root)
{
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, root);
    lv_anim_set_values(&anim, LV_OPA_70, LV_OPA_COVER);
    lv_anim_set_time(&anim, 120);
    lv_anim_set_exec_cb(&anim, set_root_opa);
    lv_anim_start(&anim);
}

static void prepare_root(void)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_clean(s_root);
    lv_obj_remove_style_all(s_root);
    lv_obj_add_style(s_root, &styles->root, 0);
    lv_obj_set_size(s_root, HMI_DISPLAY_WIDTH, HMI_DISPLAY_HEIGHT);
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_opa(s_root, LV_OPA_COVER, 0);
}

void hmi_navigation_init(lv_obj_t *root)
{
    s_root = root != NULL ? root : lv_scr_act();
    hmi_navigation_show(HMI_SCREEN_HOME);
}

void hmi_navigation_show(hmi_screen_id_t screen_id)
{
    if (s_root == NULL) {
        return;
    }

    modal_paused_job_edit_close();
    modal_numeric_keypad_close();
    modal_confirm_close();
    prepare_root();
    s_current_screen = screen_id;

    switch (screen_id) {
    case HMI_SCREEN_JOBS:
        screen_jobs_create(s_root);
        break;
    case HMI_SCREEN_CONICAL_SETUP:
        screen_conical_setup_create(s_root);
        break;
    case HMI_SCREEN_CONFIRM_START:
        screen_confirm_start_create(s_root);
        break;
    case HMI_SCREEN_HOMING:
        screen_homing_create(s_root);
        break;
    case HMI_SCREEN_RUN:
        screen_run_create(s_root);
        break;
    case HMI_SCREEN_FINISHED:
        screen_finished_create(s_root);
        break;
    case HMI_SCREEN_DIAGNOSTICS:
        screen_diagnostics_create(s_root);
        break;
    case HMI_SCREEN_SETTINGS:
        screen_settings_create(s_root);
        break;
    case HMI_SCREEN_HOME:
    default:
        screen_home_create(s_root);
        break;
    }

    hmi_navigation_update(hmi_model_get_state());
    animate_screen(s_root);
}

void hmi_navigation_home(void)
{
    hmi_navigation_show(HMI_SCREEN_HOME);
}

void hmi_navigation_update(const hmi_state_t *state)
{
    if (state == NULL) {
        return;
    }

    switch (s_current_screen) {
    case HMI_SCREEN_JOBS:
        screen_jobs_update(state);
        break;
    case HMI_SCREEN_CONICAL_SETUP:
        screen_conical_setup_update(state);
        break;
    case HMI_SCREEN_CONFIRM_START:
        screen_confirm_start_update(state);
        break;
    case HMI_SCREEN_HOMING:
        screen_homing_update(state);
        break;
    case HMI_SCREEN_RUN:
        screen_run_update(state);
        break;
    case HMI_SCREEN_FINISHED:
        screen_finished_update(state);
        break;
    case HMI_SCREEN_DIAGNOSTICS:
        screen_diagnostics_update(state);
        break;
    case HMI_SCREEN_SETTINGS:
        screen_settings_update(state);
        break;
    case HMI_SCREEN_HOME:
    default:
        screen_home_update(state);
        break;
    }
}

hmi_screen_id_t hmi_navigation_current(void)
{
    return s_current_screen;
}
