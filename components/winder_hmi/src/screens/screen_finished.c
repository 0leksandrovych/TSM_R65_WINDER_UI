#include "screen_finished.h"

#include <stdio.h>

#include "hmi_actions.h"
#include "hmi_pending_command.h"
#include "hmi_styles.h"
#include "hmi_types.h"
#include "widget_stat_row.h"
#include "widget_status_badge.h"

typedef struct {
    lv_obj_t *root;
    hmi_status_badge_t badge;
    lv_obj_t *status_label;
    hmi_stat_row_t wound_length;
    hmi_stat_row_t target_length;
    hmi_stat_row_t completed_layers;
    hmi_stat_row_t applied_offset;
    lv_obj_t *finish_button;
    lv_obj_t *finish_label;
} finished_screen_t;

static finished_screen_t s_screen;

static void finish_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_reset_job();
}

static void set_button_enabled(lv_obj_t *button, bool enabled)
{
    if (button == NULL) {
        return;
    }

    if (enabled) {
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_state(button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(button, hmi_palette_get()->green, 0);
        lv_obj_set_style_text_color(button, lv_color_white(), 0);
        lv_obj_set_style_opa(button, LV_OPA_COVER, 0);
    } else {
        lv_obj_clear_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, 0);
        lv_obj_set_style_text_color(button, hmi_palette_get()->text_muted, 0);
        lv_obj_set_style_opa(button, LV_OPA_60, 0);
    }
}

static void create_topbar(lv_obj_t *root)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *topbar = lv_obj_create(root);
    lv_obj_remove_style_all(topbar);
    lv_obj_add_style(topbar, &styles->topbar, 0);
    lv_obj_set_size(topbar, LV_PCT(100), HMI_TOPBAR_HEIGHT);
    lv_obj_set_flex_flow(topbar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        topbar,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(topbar, 12, 0);

    lv_obj_t *title = lv_label_create(topbar);
    lv_obj_add_style(title, &styles->topbar_title, 0);
    lv_obj_set_style_text_color(title, hmi_palette_get()->text, 0);
    lv_label_set_text(title, "JOB FINISHED");

    lv_obj_t *spacer = lv_obj_create(topbar);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 1);

    widget_status_badge_create(topbar, &s_screen.badge);
}

static lv_obj_t *create_panel(lv_obj_t *parent, int32_t width, const char *title_text)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &styles->panel, 0);
    lv_obj_set_size(panel, width, LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &styles->panel_title, 0);
    lv_label_set_text(title, title_text);

    return panel;
}

static lv_obj_t *create_finish_button(lv_obj_t *parent)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->primary_button, 0);
    lv_obj_set_size(button, 240, 56);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->green, 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, finish_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, "CLOSE JOB");
    lv_obj_center(label);

    return button;
}

void screen_finished_create(lv_obj_t *root)
{
    hmi_styles_t *styles = hmi_styles_get();
    s_screen = (finished_screen_t){0};
    s_screen.root = root;

    create_topbar(root);

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), HMI_DISPLAY_HEIGHT - HMI_TOPBAR_HEIGHT - 76);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(content, 14, 0);
    lv_obj_set_style_pad_column(content, 12, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *status_panel = create_panel(content, 330, "STATUS");
    s_screen.status_label = lv_label_create(status_panel);
    lv_obj_add_style(s_screen.status_label, &styles->status_text, 0);
    lv_obj_set_style_text_color(s_screen.status_label, hmi_palette_get()->text, 0);
    lv_label_set_long_mode(s_screen.status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_screen.status_label, LV_PCT(100));
    lv_label_set_text(s_screen.status_label, "Job finished");

    lv_obj_t *summary_panel = create_panel(content, 430, "SUMMARY");
    widget_stat_row_create(summary_panel, &s_screen.wound_length, "Wound length");
    widget_stat_row_create(summary_panel, &s_screen.target_length, "Target length");
    widget_stat_row_create(summary_panel, &s_screen.completed_layers, "Completed layers");
    widget_stat_row_create(summary_panel, &s_screen.applied_offset, "Applied right-edge offset");

    lv_obj_t *buttons = lv_obj_create(root);
    lv_obj_remove_style_all(buttons);
    lv_obj_set_size(buttons, LV_PCT(100), 76);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        buttons,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(buttons, LV_OBJ_FLAG_SCROLLABLE);

    s_screen.finish_button = create_finish_button(buttons);
    s_screen.finish_label = lv_obj_get_child(s_screen.finish_button, 0);
}

void screen_finished_update(const hmi_state_t *state)
{
    if (state == NULL || s_screen.root == NULL) {
        return;
    }

    char value[48];
    hmi_pending_command_t pending = hmi_pending_command_get();
    bool pending_active = hmi_pending_command_is_active();
    bool reset_pending = pending == HMI_PENDING_RESET_JOB;
    bool finished = state->machine_state_known &&
                    state->machine_state == HMI_MACHINE_FINISHED;

    widget_status_badge_update(&s_screen.badge, state);

    snprintf(value, sizeof(value), "%.3f m", (double)state->wound_length_m);
    widget_stat_row_set_value(&s_screen.wound_length, value, HMI_COLOR_NEUTRAL);

    snprintf(value, sizeof(value), "%.3f m", (double)state->target_length_m);
    widget_stat_row_set_value(&s_screen.target_length, value, HMI_COLOR_NEUTRAL);

    snprintf(value, sizeof(value), "%lu", (unsigned long)state->current_layer);
    widget_stat_row_set_value(&s_screen.completed_layers, value, HMI_COLOR_NEUTRAL);

    snprintf(value, sizeof(value), "%+.2f mm", (double)state->right_edge_offset_mm);
    widget_stat_row_set_value(&s_screen.applied_offset, value, HMI_COLOR_NEUTRAL);

    if (reset_pending) {
        lv_label_set_text(s_screen.status_label, "Closing completed job...");
        lv_obj_set_style_text_color(s_screen.status_label, hmi_palette_get()->amber, 0);
    } else if (finished) {
        /* Reason (target reached / manual finish / abort) is not carried by
         * telemetry, so the copy stays neutral and does not imply success. */
        lv_label_set_text(s_screen.status_label, "Job finished");
        lv_obj_set_style_text_color(s_screen.status_label, hmi_palette_get()->text, 0);
    } else {
        lv_label_set_text(s_screen.status_label, "Waiting for controller");
        lv_obj_set_style_text_color(s_screen.status_label, hmi_palette_get()->text_dim, 0);
    }

    if (s_screen.finish_label != NULL) {
        lv_label_set_text(
            s_screen.finish_label,
            reset_pending ? "CLOSING..." : "CLOSE JOB");
        lv_obj_center(s_screen.finish_label);
    }
    set_button_enabled(s_screen.finish_button, finished && !pending_active);
}
