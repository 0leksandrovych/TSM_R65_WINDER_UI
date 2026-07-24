#include "widget_status_badge.h"

#include "hmi_model.h"
#include "hmi_styles.h"

static const char *machine_state_text(hmi_machine_state_t state)
{
    if (hmi_machine_state_is_homing(state)) {
        return "HOMING";
    }
    if (hmi_machine_state_is_positioning(state)) {
        return "POSITIONING";
    }

    switch (state) {
    case HMI_MACHINE_HOMING_REQUIRED:
        return "HOMING REQUIRED";
    case HMI_MACHINE_READY:
        return "READY";
    case HMI_MACHINE_ACCELERATING:
        return "ACCELERATING";
    case HMI_MACHINE_RUNNING:
        return "RUNNING";
    case HMI_MACHINE_PAUSED:
        return "PAUSED";
    case HMI_MACHINE_STOPPING:
        return "STOPPING";
    case HMI_MACHINE_FINISHED:
        return "FINISHED";
    case HMI_MACHINE_ALARM:
        return "ALARM";
    default:
        return "UNKNOWN";
    }
}

static lv_style_t *badge_style_for_state(hmi_machine_state_t state)
{
    hmi_styles_t *styles = hmi_styles_get();

    if (hmi_machine_state_is_homing(state) ||
        hmi_machine_state_is_positioning(state)) {
        return &styles->badge_amber;
    }

    switch (state) {
    case HMI_MACHINE_READY:
    case HMI_MACHINE_ACCELERATING:
    case HMI_MACHINE_RUNNING:
    case HMI_MACHINE_FINISHED:
        return &styles->badge_green;
    case HMI_MACHINE_HOMING_REQUIRED:
    case HMI_MACHINE_PAUSED:
    case HMI_MACHINE_STOPPING:
        return &styles->badge_amber;
    case HMI_MACHINE_ALARM:
        return &styles->badge_red;
    default:
        return &styles->badge_dim;
    }
}

void widget_status_badge_create(lv_obj_t *parent, hmi_status_badge_t *badge)
{
    if (badge == NULL) {
        return;
    }

    hmi_styles_t *styles = hmi_styles_get();
    badge->label = lv_label_create(parent);
    lv_obj_add_style(badge->label, &styles->badge_base, 0);
    lv_label_set_text(badge->label, "CONNECTING");
}

void widget_status_badge_update(hmi_status_badge_t *badge, const hmi_state_t *state)
{
    if (badge == NULL || badge->label == NULL || state == NULL) {
        return;
    }

    hmi_styles_t *styles = hmi_styles_get();
    lv_obj_remove_style(badge->label, &styles->badge_green, 0);
    lv_obj_remove_style(badge->label, &styles->badge_amber, 0);
    lv_obj_remove_style(badge->label, &styles->badge_red, 0);
    lv_obj_remove_style(badge->label, &styles->badge_blue, 0);
    lv_obj_remove_style(badge->label, &styles->badge_dim, 0);

    hmi_connection_state_t connection = hmi_model_get_connection_state();
    if (connection == HMI_CONNECTION_LOST || connection == HMI_CONNECTION_DISCONNECTED) {
        lv_obj_add_style(badge->label, &styles->badge_red, 0);
        lv_label_set_text(badge->label, "DISCONNECTED");
    } else if (connection == HMI_CONNECTION_CONNECTING || !state->machine_state_known) {
        lv_obj_add_style(badge->label, &styles->badge_amber, 0);
        lv_label_set_text(badge->label, "CONNECTING");
    } else {
        lv_obj_add_style(badge->label, badge_style_for_state(state->machine_state), 0);
        lv_label_set_text(badge->label, machine_state_text(state->machine_state));
    }
}
