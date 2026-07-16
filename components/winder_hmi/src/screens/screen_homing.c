#include "screen_homing.h"

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
    lv_obj_t *step_label;
    lv_obj_t *message_label;
    hmi_stat_row_t state_row;
    hmi_stat_row_t left_limit_row;
    hmi_stat_row_t right_limit_row;
    hmi_stat_row_t travel_row;
    lv_obj_t *start_button;
    lv_obj_t *abort_button;
    lv_obj_t *continue_label;
} homing_screen_t;

static homing_screen_t s_screen;

static void back_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_go_home();
}

static void start_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_start_homing();
}

static void abort_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_abort_homing();
}

static lv_obj_t *create_button(lv_obj_t *parent, const char *text, hmi_color_role_t role, lv_event_cb_t cb)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->primary_button, 0);
    lv_obj_set_size(button, 190, 56);
    lv_obj_set_style_bg_color(button, hmi_color_for_role(role), 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return button;
}

static void set_button_enabled(lv_obj_t *button, bool enabled, hmi_color_role_t role)
{
    if (button == NULL) {
        return;
    }

    if (enabled) {
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_state(button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(button, hmi_color_for_role(role), 0);
        lv_obj_set_style_text_color(button, role == HMI_COLOR_AMBER ? hmi_palette_get()->device : lv_color_white(), 0);
    } else {
        lv_obj_clear_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(button, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, 0);
        lv_obj_set_style_text_color(button, hmi_palette_get()->text_muted, 0);
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
    lv_label_set_text(title, "HOMING");

    lv_obj_t *spacer = lv_obj_create(topbar);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 1);

    widget_status_badge_create(topbar, &s_screen.badge);
}

static bool homing_is_active(hmi_machine_state_t state)
{
    switch (state) {
    case HMI_MACHINE_HOMING_SEARCHING_RIGHT_REFERENCE:
    case HMI_MACHINE_HOMING_BACKING_OFF_RIGHT_REFERENCE:
    case HMI_MACHINE_HOMING_SEARCHING_LEFT_REFERENCE:
    case HMI_MACHINE_HOMING_BACKING_OFF_LEFT_REFERENCE:
    case HMI_MACHINE_HOMING_MEASURING_TRAVEL:
    case HMI_MACHINE_HOMING_APPLYING_OFFSET:
    case HMI_MACHINE_HOMING_COMPLETING:
        return true;
    default:
        return false;
    }
}

static const char *homing_stage_text(const hmi_state_t *state)
{
    if (state == NULL || !state->machine_state_known) {
        return "Waiting for controller";
    }

    switch (state->machine_state) {
    case HMI_MACHINE_HOMING_REQUIRED:
        return "Homing required";
    case HMI_MACHINE_HOMING_SEARCHING_RIGHT_REFERENCE:
        return "Searching right reference";
    case HMI_MACHINE_HOMING_BACKING_OFF_RIGHT_REFERENCE:
        return "Backing off right reference";
    case HMI_MACHINE_HOMING_SEARCHING_LEFT_REFERENCE:
        return "Searching left reference";
    case HMI_MACHINE_HOMING_BACKING_OFF_LEFT_REFERENCE:
        return "Backing off left reference";
    case HMI_MACHINE_HOMING_MEASURING_TRAVEL:
        return "Measuring travel";
    case HMI_MACHINE_HOMING_APPLYING_OFFSET:
        return "Applying offset";
    case HMI_MACHINE_HOMING_COMPLETING:
        return "Completing homing";
    case HMI_MACHINE_READY:
        return "Homing completed successfully";
    default:
        return "Homing unavailable in current state";
    }
}

void screen_homing_create(lv_obj_t *root)
{
    hmi_styles_t *styles = hmi_styles_get();
    s_screen = (homing_screen_t){0};
    s_screen.root = root;

    create_topbar(root);

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), HMI_DISPLAY_HEIGHT - HMI_TOPBAR_HEIGHT - 76);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(content, 14, 0);
    lv_obj_set_style_pad_column(content, 12, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sequence_panel = lv_obj_create(content);
    lv_obj_remove_style_all(sequence_panel);
    lv_obj_add_style(sequence_panel, &styles->panel, 0);
    lv_obj_set_size(sequence_panel, 374, LV_PCT(100));
    lv_obj_set_flex_flow(sequence_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sequence_panel, 12, 0);
    lv_obj_clear_flag(sequence_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *sequence_title = lv_label_create(sequence_panel);
    lv_obj_add_style(sequence_title, &styles->panel_title, 0);
    lv_label_set_text(sequence_title, "REFERENCE SEQUENCE");

    s_screen.step_label = lv_label_create(sequence_panel);
    lv_obj_add_style(s_screen.step_label, &styles->status_big, 0);
    lv_obj_set_style_text_color(s_screen.step_label, hmi_palette_get()->blue, 0);
    lv_label_set_long_mode(s_screen.step_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_screen.step_label, LV_PCT(100));
    lv_label_set_text(s_screen.step_label, "Ready to start");

    s_screen.message_label = lv_label_create(sequence_panel);
    lv_obj_add_style(s_screen.message_label, &styles->status_text, 0);
    lv_label_set_long_mode(s_screen.message_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_screen.message_label, LV_PCT(100));
    lv_label_set_text(s_screen.message_label, "Homing references the carriage against the limit sensors so positions are known.");

    lv_obj_t *panel = lv_obj_create(content);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &styles->panel, 0);
    lv_obj_set_size(panel, 374, LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 10, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &styles->panel_title, 0);
    lv_label_set_text(title, "HOMING STATUS");

    widget_stat_row_create(panel, &s_screen.state_row, "Current stage");
    widget_stat_row_create(panel, &s_screen.left_limit_row, "Left limit sensor");
    widget_stat_row_create(panel, &s_screen.right_limit_row, "Right limit sensor");
    widget_stat_row_create(panel, &s_screen.travel_row, "Travel range");

    lv_obj_t *buttons = lv_obj_create(root);
    lv_obj_remove_style_all(buttons);
    lv_obj_set_size(buttons, LV_PCT(100), 76);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(buttons, 16, 0);
    lv_obj_clear_flag(buttons, LV_OBJ_FLAG_SCROLLABLE);

    s_screen.start_button = create_button(buttons, "START HOMING", HMI_COLOR_BLUE, start_event_cb);
    s_screen.abort_button = create_button(buttons, "ABORT HOMING", HMI_COLOR_RED, abort_event_cb);
    lv_obj_t *continue_button = create_button(buttons, "BACK HOME", HMI_COLOR_DIM, back_event_cb);
    s_screen.continue_label = lv_obj_get_child(continue_button, 0);
}

void screen_homing_update(const hmi_state_t *state)
{
    if (state == NULL || s_screen.root == NULL) {
        return;
    }

    char value[32];
    bool running = state->machine_state_known && homing_is_active(state->machine_state);
    bool complete = state->machine_state_known && state->machine_state == HMI_MACHINE_READY;
    bool required = state->machine_state_known && state->machine_state == HMI_MACHINE_HOMING_REQUIRED;
    hmi_pending_command_t pending = hmi_pending_command_get();
    bool start_pending = pending == HMI_PENDING_START_HOMING;
    bool abort_pending = pending == HMI_PENDING_ABORT_HOMING;

    widget_status_badge_update(&s_screen.badge, state);
    const char *stage_text = homing_stage_text(state);
    lv_label_set_text(s_screen.step_label, stage_text);
    lv_obj_set_style_text_color(s_screen.step_label, complete ? hmi_palette_get()->green : hmi_palette_get()->blue, 0);
    if (start_pending || abort_pending) {
        lv_label_set_text(s_screen.message_label, hmi_pending_command_get_message());
    } else if (complete) {
        lv_label_set_text(s_screen.message_label, "Homing completed successfully");
    } else if (running) {
        lv_label_set_text(s_screen.message_label, "Homing is in progress. Use ABORT HOMING only when motion must be interrupted.");
    } else if (required) {
        lv_label_set_text(s_screen.message_label, "Start homing to establish the machine travel references.");
    } else {
        lv_label_set_text(s_screen.message_label, "Homing status follows controller telemetry.");
    }

    widget_stat_row_set_value(&s_screen.state_row, stage_text,
                              complete ? HMI_COLOR_GREEN : (running ? HMI_COLOR_BLUE : HMI_COLOR_AMBER));
    widget_stat_row_set_value(&s_screen.left_limit_row, state->left_limit_active ? "ACTIVE" : "Open",
                              state->left_limit_active ? HMI_COLOR_GREEN : HMI_COLOR_DIM);
    widget_stat_row_set_value(&s_screen.right_limit_row, state->right_limit_active ? "ACTIVE" : "Open",
                              state->right_limit_active ? HMI_COLOR_GREEN : HMI_COLOR_DIM);

    if (state->travel_range_known) {
        snprintf(value, sizeof(value), "%.2f mm", state->travel_range_mm);
        widget_stat_row_set_value(&s_screen.travel_row, value, HMI_COLOR_NEUTRAL);
    } else {
        widget_stat_row_set_value(&s_screen.travel_row, "Unknown", HMI_COLOR_DIM);
    }

    set_button_enabled(s_screen.start_button, (required || complete) && !start_pending && !abort_pending, HMI_COLOR_BLUE);
    set_button_enabled(s_screen.abort_button, running && !start_pending && !abort_pending, HMI_COLOR_RED);
    if (running) {
        lv_obj_add_flag(s_screen.start_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_screen.abort_button, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_screen.start_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_screen.abort_button, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_screen.continue_label != NULL) {
        lv_label_set_text(s_screen.continue_label, complete ? "CONTINUE" : "BACK HOME");
        lv_obj_center(s_screen.continue_label);
    }
}
