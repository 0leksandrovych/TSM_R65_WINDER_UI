#include "screen_diagnostics.h"

#include <stdio.h>

#include "hmi_actions.h"
#include "hmi_model.h"
#include "hmi_styles.h"
#include "hmi_types.h"
#include "widget_stat_row.h"
#include "widget_status_badge.h"

typedef struct {
    lv_obj_t *root;
    hmi_status_badge_t badge;
    lv_obj_t *banner;
    hmi_stat_row_t left_limit_row;
    hmi_stat_row_t right_limit_row;
    hmi_stat_row_t encoder_row;
    hmi_stat_row_t connection_row;
    hmi_stat_row_t unwound_row;
    hmi_stat_row_t motor_row;
    hmi_stat_row_t last_event_row;
    lv_obj_t *reset_button;
    lv_obj_t *reset_label;
} diagnostics_screen_t;

static diagnostics_screen_t s_screen;

static void back_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_go_home();
}

static void reset_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_reset_unwound_counter();
}

static const char *connection_text(hmi_connection_state_t connection)
{
    switch (connection) {
    case HMI_CONNECTION_CONNECTING:
        return "Connecting";
    case HMI_CONNECTION_CONNECTED:
        return "Connected";
    case HMI_CONNECTION_LOST:
        return "Lost";
    case HMI_CONNECTION_DISCONNECTED:
    default:
        return "Disconnected";
    }
}

static hmi_color_role_t connection_color(hmi_connection_state_t connection)
{
    switch (connection) {
    case HMI_CONNECTION_CONNECTED:
        return HMI_COLOR_GREEN;
    case HMI_CONNECTION_CONNECTING:
        return HMI_COLOR_AMBER;
    case HMI_CONNECTION_LOST:
        return HMI_COLOR_RED;
    case HMI_CONNECTION_DISCONNECTED:
    default:
        return HMI_COLOR_DIM;
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
    lv_obj_set_flex_align(topbar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(topbar, 12, 0);

    lv_obj_t *back = lv_btn_create(topbar);
    lv_obj_remove_style_all(back);
    lv_obj_add_style(back, &styles->nav_button, 0);
    lv_obj_set_size(back, 64, 42);
    lv_obj_add_event_cb(back, back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, "<");
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(topbar);
    lv_obj_add_style(title, &styles->topbar_title, 0);
    lv_label_set_text(title, "DIAGNOSTICS");

    lv_obj_t *spacer = lv_obj_create(topbar);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 1);

    widget_status_badge_create(topbar, &s_screen.badge);
}

void screen_diagnostics_create(lv_obj_t *root)
{
    hmi_styles_t *styles = hmi_styles_get();
    s_screen = (diagnostics_screen_t){0};
    s_screen.root = root;

    create_topbar(root);

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), HMI_DISPLAY_HEIGHT - HMI_TOPBAR_HEIGHT - 80);
    lv_obj_set_style_pad_all(content, 14, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(content);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &styles->panel, 0);
    lv_obj_set_size(panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &styles->panel_title, 0);
    lv_label_set_text(title, "READ ONLY SIGNALS");

    s_screen.banner = lv_label_create(panel);
    lv_obj_add_style(s_screen.banner, &styles->status_text, 0);
    lv_obj_set_width(s_screen.banner, LV_PCT(100));
    lv_obj_set_style_pad_all(s_screen.banner, 9, 0);
    lv_obj_set_style_bg_opa(s_screen.banner, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_screen.banner, 0, 0);
    lv_label_set_text(s_screen.banner, "");

    widget_stat_row_create(panel, &s_screen.left_limit_row, "Left limit sensor");
    widget_stat_row_create(panel, &s_screen.right_limit_row, "Right limit sensor");
    widget_stat_row_create(panel, &s_screen.encoder_row, "Encoder count");
    widget_stat_row_create(panel, &s_screen.connection_row, "Controller connection");
    widget_stat_row_create(panel, &s_screen.unwound_row, "Unwound fiber counter");
    widget_stat_row_create(panel, &s_screen.motor_row, "Motor state");
    widget_stat_row_create(panel, &s_screen.last_event_row, "Last event");

    lv_obj_t *buttons = lv_obj_create(root);
    lv_obj_remove_style_all(buttons);
    lv_obj_set_size(buttons, LV_PCT(100), 80);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(buttons, LV_OBJ_FLAG_SCROLLABLE);

    s_screen.reset_button = lv_btn_create(buttons);
    lv_obj_remove_style_all(s_screen.reset_button);
    lv_obj_add_style(s_screen.reset_button, &styles->primary_button, 0);
    lv_obj_set_size(s_screen.reset_button, 230, 56);
    lv_obj_add_event_cb(s_screen.reset_button, reset_event_cb, LV_EVENT_CLICKED, NULL);

    s_screen.reset_label = lv_label_create(s_screen.reset_button);
    lv_label_set_text(s_screen.reset_label, "RESET COUNTER");
    lv_obj_center(s_screen.reset_label);
}

void screen_diagnostics_update(const hmi_state_t *state)
{
    if (state == NULL || s_screen.root == NULL) {
        return;
    }

    char value[40];
    bool locked = state->machine_state == HMI_MACHINE_RUNNING || state->machine_state == HMI_MACHINE_PAUSED;

    widget_status_badge_update(&s_screen.badge, state->machine_state);
    lv_label_set_text(s_screen.banner, locked ? "READ ONLY - machine is running" : "Diagnostic values are read-only mock/state data.");
    lv_obj_set_style_text_color(s_screen.banner, locked ? hmi_palette_get()->amber : hmi_palette_get()->text_dim, 0);
    lv_obj_set_style_bg_opa(s_screen.banner, locked ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(s_screen.banner, hmi_palette_get()->panel_secondary, 0);
    lv_obj_set_style_border_width(s_screen.banner, locked ? 1 : 0, 0);
    lv_obj_set_style_border_color(s_screen.banner, hmi_palette_get()->amber, 0);

    widget_stat_row_set_value(&s_screen.left_limit_row, state->left_limit_active ? "ACTIVE" : "Open",
                              state->left_limit_active ? HMI_COLOR_GREEN : HMI_COLOR_DIM);
    widget_stat_row_set_value(&s_screen.right_limit_row, state->right_limit_active ? "ACTIVE" : "Open",
                              state->right_limit_active ? HMI_COLOR_GREEN : HMI_COLOR_DIM);
    snprintf(value, sizeof(value), "%lu", (unsigned long)state->encoder_count);
    widget_stat_row_set_value(&s_screen.encoder_row, value, HMI_COLOR_NEUTRAL);
    hmi_connection_state_t connection = hmi_model_get_connection_state();
    widget_stat_row_set_value(&s_screen.connection_row, connection_text(connection), connection_color(connection));
    snprintf(value, sizeof(value), "%.1f m", (double)state->unwound_length_m);
    widget_stat_row_set_value(&s_screen.unwound_row, value, HMI_COLOR_NEUTRAL);
    widget_stat_row_set_value(&s_screen.motor_row, state->motor_state != NULL ? state->motor_state : "--", HMI_COLOR_BLUE);
    widget_stat_row_set_value(&s_screen.last_event_row, state->last_event != NULL ? state->last_event : "--", HMI_COLOR_NEUTRAL);

    if (locked) {
        lv_obj_clear_flag(s_screen.reset_button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(s_screen.reset_button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(s_screen.reset_button, hmi_palette_get()->border_strong, 0);
        lv_obj_set_style_text_color(s_screen.reset_button, hmi_palette_get()->text_muted, 0);
    } else {
        lv_obj_add_flag(s_screen.reset_button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_state(s_screen.reset_button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(s_screen.reset_button, hmi_palette_get()->blue, 0);
        lv_obj_set_style_text_color(s_screen.reset_button, lv_color_white(), 0);
    }
}
