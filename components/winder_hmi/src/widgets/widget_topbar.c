#include "widget_topbar.h"

#include "hmi_styles.h"

void widget_topbar_create(lv_obj_t *parent, hmi_topbar_t *topbar)
{
    if (topbar == NULL) {
        return;
    }

    hmi_styles_t *styles = hmi_styles_get();

    topbar->root = lv_obj_create(parent);
    lv_obj_remove_style_all(topbar->root);
    lv_obj_add_style(topbar->root, &styles->topbar, 0);
    lv_obj_set_size(topbar->root, LV_PCT(100), HMI_TOPBAR_HEIGHT);
    lv_obj_set_flex_flow(topbar->root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(topbar->root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(topbar->root, 12, 0);

    lv_obj_t *title = lv_label_create(topbar->root);
    lv_obj_add_style(title, &styles->topbar_title, 0);
    lv_label_set_text(title, "WINDER CONTROL");

    lv_obj_t *spacer = lv_obj_create(topbar->root);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 1);

    widget_status_badge_create(topbar->root, &topbar->badge);

    topbar->system_ok_dot = lv_obj_create(topbar->root);
    lv_obj_remove_style_all(topbar->system_ok_dot);
    lv_obj_set_size(topbar->system_ok_dot, 9, 9);
    lv_obj_set_style_radius(topbar->system_ok_dot, 5, 0);
    lv_obj_set_style_bg_opa(topbar->system_ok_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(topbar->system_ok_dot, hmi_palette_get()->green, 0);

    topbar->system_ok_label = lv_label_create(topbar->root);
    lv_obj_add_style(topbar->system_ok_label, &styles->topbar_system_ok, 0);
    lv_label_set_text(topbar->system_ok_label, "SYSTEM OK");

    topbar->clock_label = lv_label_create(topbar->root);
    lv_obj_add_style(topbar->clock_label, &styles->topbar_text, 0);
    lv_label_set_text(topbar->clock_label, "--:--");
}

void widget_topbar_update(hmi_topbar_t *topbar, const hmi_state_t *state)
{
    if (topbar == NULL || state == NULL) {
        return;
    }

    widget_status_badge_update(&topbar->badge, state->machine_state);
    lv_label_set_text(topbar->system_ok_label, state->safety_ok ? "SYSTEM OK" : "SYSTEM FAULT");
    lv_obj_set_style_text_color(
        topbar->system_ok_label,
        state->safety_ok ? hmi_palette_get()->green : hmi_palette_get()->red,
        0
    );
    if (topbar->system_ok_dot != NULL) {
        lv_obj_set_style_bg_color(
            topbar->system_ok_dot,
            state->safety_ok ? hmi_palette_get()->green : hmi_palette_get()->red,
            0
        );
    }
}
