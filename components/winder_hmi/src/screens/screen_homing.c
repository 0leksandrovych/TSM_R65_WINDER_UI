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
    hmi_stat_row_t left_samples_row;
    hmi_stat_row_t right_samples_row;
    hmi_stat_row_t carriage_row;
    hmi_stat_row_t alarm_row;
    hmi_stat_row_t travel_row;
    lv_obj_t *start_button;
    lv_obj_t *start_label;
    lv_obj_t *next_button;
    lv_obj_t *zero_button;
    lv_obj_t *left_button;
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

static void next_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_homing_next_measurement();
}

static void zero_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_move_carriage_to_zero();
}

static void left_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_move_carriage_to_left_edge();
}

static lv_obj_t *create_button(lv_obj_t *parent, const char *text, hmi_color_role_t role, lv_event_cb_t cb)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->primary_button, 0);
    lv_obj_set_size(button, 140, 56);
    lv_obj_set_style_bg_color(button, hmi_color_for_role(role), 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, 128);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);

    return button;
}

static void set_button_visible(lv_obj_t *button, bool visible)
{
    if (button == NULL) {
        return;
    }

    if (visible) {
        lv_obj_clear_flag(button, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(button, LV_OBJ_FLAG_HIDDEN);
    }
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

static const char *homing_stage_text(const hmi_state_t *state)
{
    if (state == NULL || !state->machine_state_known) {
        return "Waiting for controller";
    }

    switch (state->machine_state) {
    case HMI_MACHINE_HOMING_REQUIRED:
        return "Homing required";
    case HMI_MACHINE_HOMING_SEARCHING_LEFT:
        return "Searching left edge";
    case HMI_MACHINE_HOMING_LEFT_BACKOFF:
        return "Establishing precise left reference";
    case HMI_MACHINE_HOMING_LEFT_MEASUREMENT:
        return "Measuring left edge";
    case HMI_MACHINE_HOMING_MASTER_POSITIONING:
        return "Positioning master";
    case HMI_MACHINE_HOMING_SEARCHING_RIGHT:
        return "Searching right edge";
    case HMI_MACHINE_HOMING_RIGHT_BACKOFF:
        return "Establishing precise right reference";
    case HMI_MACHINE_HOMING_RIGHT_MEASUREMENT:
        return "Measuring right edge";
    case HMI_MACHINE_HOMING_WAITING_NEXT_MEASUREMENT:
        return "Waiting for next measurement";
    case HMI_MACHINE_HOMING_WAITING_ZERO_COMMAND:
        return "Waiting for move to zero";
    case HMI_MACHINE_HOMING_MOVING_TO_ZERO:
        return "Moving to calibrated zero";
    case HMI_MACHINE_READY:
        return "Homing completed successfully";
    case HMI_MACHINE_POSITIONING:
        return "Positioning carriage";
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
    lv_label_set_text(s_screen.message_label, "Homing locates the carriage using the left and right optical reference points and measures the usable travel range.");

    lv_obj_t *panel = lv_obj_create(content);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &styles->panel, 0);
    lv_obj_set_size(panel, 374, LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 16, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &styles->panel_title, 0);
    lv_label_set_text(title, "HOMING STATUS");

    widget_stat_row_create(panel, &s_screen.state_row, "Current stage");
    widget_stat_row_create(panel, &s_screen.left_samples_row, "Left measurements");
    widget_stat_row_create(panel, &s_screen.right_samples_row, "Right measurements");
    widget_stat_row_create(panel, &s_screen.carriage_row, "Carriage reference");
    widget_stat_row_create(panel, &s_screen.alarm_row, "Homing alarm");
    widget_stat_row_create(panel, &s_screen.travel_row, "Travel range");

    lv_obj_t *buttons = lv_obj_create(root);
    lv_obj_remove_style_all(buttons);
    lv_obj_set_size(buttons, LV_PCT(100), 76);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(buttons, 16, 0);
    lv_obj_clear_flag(buttons, LV_OBJ_FLAG_SCROLLABLE);

    s_screen.start_button = create_button(buttons, "START HOMING", HMI_COLOR_BLUE, start_event_cb);
    s_screen.start_label = lv_obj_get_child(s_screen.start_button, 0);
    s_screen.next_button = create_button(buttons, "NEXT MEASUREMENT", HMI_COLOR_BLUE, next_event_cb);
    s_screen.zero_button = create_button(buttons, "MOVE TO ZERO", HMI_COLOR_BLUE, zero_event_cb);
    s_screen.left_button = create_button(buttons, "MOVE TO LEFT EDGE", HMI_COLOR_BLUE, left_event_cb);
    s_screen.abort_button = create_button(buttons, "ABORT HOMING", HMI_COLOR_RED, abort_event_cb);
    lv_obj_t *continue_button = create_button(buttons, "BACK HOME", HMI_COLOR_DIM, back_event_cb);
    s_screen.continue_label = lv_obj_get_child(continue_button, 0);
}

static void format_sample_progress(
    char *buffer,
    size_t buffer_size,
    uint32_t current,
    bool current_known,
    const hmi_state_t *state)
{
    if (!current_known) {
        snprintf(buffer, buffer_size, "--");
    } else if (state->homing_sample_target_count_known &&
               state->homing_sample_target_count > 0U) {
        snprintf(buffer, buffer_size, "%lu / %lu",
                 (unsigned long)current,
                 (unsigned long)state->homing_sample_target_count);
    } else {
        snprintf(buffer, buffer_size, "%lu", (unsigned long)current);
    }
}

void screen_homing_update(const hmi_state_t *state)
{
    if (state == NULL || s_screen.root == NULL) {
        return;
    }

    char value[64];
    char alarm_buffer[64];
    bool running = state->machine_state_known &&
                   hmi_machine_state_is_homing(state->machine_state);
    bool complete = state->machine_state_known && state->machine_state == HMI_MACHINE_READY;
    bool required = state->machine_state_known && state->machine_state == HMI_MACHINE_HOMING_REQUIRED;
    bool positioning = state->machine_state_known &&
                       state->machine_state == HMI_MACHINE_POSITIONING;
    bool waiting_next = state->machine_state_known &&
                        state->machine_state == HMI_MACHINE_HOMING_WAITING_NEXT_MEASUREMENT;
    bool waiting_zero = state->machine_state_known &&
                        state->machine_state == HMI_MACHINE_HOMING_WAITING_ZERO_COMMAND;
    hmi_pending_command_t pending = hmi_pending_command_get();
    bool start_pending = pending == HMI_PENDING_START_HOMING;
    bool abort_pending = pending == HMI_PENDING_ABORT_HOMING;
    bool command_blocked = hmi_pending_command_is_active();
    bool homing_alarm_active = state->homing_alarm_code_known &&
                               state->homing_alarm_code != 0U;

    widget_status_badge_update(&s_screen.badge, state);
    const char *stage_text = homing_stage_text(state);
    lv_label_set_text(s_screen.step_label, stage_text);
    lv_obj_set_style_text_color(s_screen.step_label, complete ? hmi_palette_get()->green : hmi_palette_get()->blue, 0);
    if (homing_alarm_active) {
        lv_label_set_text(
            s_screen.message_label,
            hmi_homing_alarm_text(
                state->homing_alarm_code, alarm_buffer, sizeof(alarm_buffer)));
    } else if (start_pending || abort_pending) {
        lv_label_set_text(s_screen.message_label, hmi_pending_command_get_message());
    } else if (waiting_next) {
        lv_label_set_text(s_screen.message_label, "Press NEXT MEASUREMENT to start the next right-edge sample.");
    } else if (waiting_zero) {
        lv_label_set_text(s_screen.message_label, "Measurements are complete. Move the carriage to calibrated zero.");
    } else if (positioning) {
        lv_label_set_text(s_screen.message_label, "Carriage positioning is in progress. Position commands are disabled.");
    } else if (running) {
        lv_label_set_text(s_screen.message_label, "Homing is in progress. Use ABORT HOMING only when motion must be interrupted.");
    } else if (complete) {
        if (state->carriage_reference_position_known &&
            state->carriage_reference_position == HMI_CARRIAGE_POSITION_LEFT_EDGE) {
            lv_label_set_text(s_screen.message_label, "The machine is homed at the left edge. Move to zero before starting a job.");
        } else {
            lv_label_set_text(s_screen.message_label, "The machine is homed. The carriage can move between zero and the left edge.");
        }
    } else if (required) {
        lv_label_set_text(s_screen.message_label, "Start homing to establish the machine travel references.");
    } else {
        lv_label_set_text(s_screen.message_label, "Homing is unavailable in the current machine state.");
    }

    widget_stat_row_set_value(&s_screen.state_row, stage_text,
                              complete ? HMI_COLOR_GREEN :
                              (running ? HMI_COLOR_BLUE : HMI_COLOR_AMBER));

    format_sample_progress(
        value, sizeof(value), state->left_edge_sample_count,
        state->left_edge_sample_count_known, state);
    widget_stat_row_set_value(
        &s_screen.left_samples_row, value,
        state->left_edge_sample_count_known ? HMI_COLOR_NEUTRAL : HMI_COLOR_DIM);

    format_sample_progress(
        value, sizeof(value), state->right_edge_sample_count,
        state->right_edge_sample_count_known, state);
    widget_stat_row_set_value(
        &s_screen.right_samples_row, value,
        state->right_edge_sample_count_known ? HMI_COLOR_NEUTRAL : HMI_COLOR_DIM);

    widget_stat_row_set_value(
        &s_screen.carriage_row,
        state->carriage_reference_position_known ?
            hmi_carriage_reference_position_text(state->carriage_reference_position) : "--",
        state->carriage_reference_position_known ?
            (state->carriage_reference_position == HMI_CARRIAGE_POSITION_ZERO ?
                HMI_COLOR_GREEN : HMI_COLOR_AMBER) : HMI_COLOR_DIM);

    widget_stat_row_set_value(
        &s_screen.alarm_row,
        state->homing_alarm_code_known ?
            hmi_homing_alarm_text(
                state->homing_alarm_code, alarm_buffer, sizeof(alarm_buffer)) : "--",
        homing_alarm_active ? HMI_COLOR_RED :
            (state->homing_alarm_code_known ? HMI_COLOR_GREEN : HMI_COLOR_DIM));

    if (state->travel_range_known) {
        snprintf(value, sizeof(value), "%.2f mm", state->travel_range_mm);
        widget_stat_row_set_value(&s_screen.travel_row, value, HMI_COLOR_NEUTRAL);
    } else {
        widget_stat_row_set_value(&s_screen.travel_row, "Unknown", HMI_COLOR_DIM);
    }

    /* START_HOMING is offered both when references are missing (HOMING_REQUIRED)
     * and when the machine is idle and can redefine them (READY -> RE-HOME). */
    if (s_screen.start_label != NULL) {
        lv_label_set_text(s_screen.start_label, complete ? "RE-HOME" : "START HOMING");
        lv_obj_center(s_screen.start_label);
    }
    bool can_start = (required || complete) && !running && !command_blocked;
    set_button_enabled(s_screen.start_button, can_start, HMI_COLOR_BLUE);
    set_button_enabled(s_screen.next_button, waiting_next && !command_blocked, HMI_COLOR_BLUE);
    set_button_enabled(
        s_screen.zero_button,
        !command_blocked &&
            (waiting_zero ||
             (complete && state->carriage_reference_position_known &&
              state->carriage_reference_position == HMI_CARRIAGE_POSITION_LEFT_EDGE)),
        HMI_COLOR_BLUE);
    set_button_enabled(
        s_screen.left_button,
        !command_blocked && complete && state->carriage_reference_position_known &&
            state->carriage_reference_position == HMI_CARRIAGE_POSITION_ZERO,
        HMI_COLOR_BLUE);
    set_button_enabled(s_screen.abort_button, running && !command_blocked, HMI_COLOR_RED);

    set_button_visible(s_screen.start_button, required || complete);
    set_button_visible(s_screen.next_button, waiting_next);
    set_button_visible(
        s_screen.zero_button,
        waiting_zero || positioning ||
            (complete && state->carriage_reference_position_known &&
             state->carriage_reference_position == HMI_CARRIAGE_POSITION_LEFT_EDGE));
    set_button_visible(
        s_screen.left_button,
        positioning ||
            (complete && state->carriage_reference_position_known &&
             state->carriage_reference_position == HMI_CARRIAGE_POSITION_ZERO));
    set_button_visible(s_screen.abort_button, running);
    if (s_screen.continue_label != NULL) {
        lv_label_set_text(s_screen.continue_label, complete ? "CONTINUE" : "BACK HOME");
        lv_obj_center(s_screen.continue_label);
    }
}
