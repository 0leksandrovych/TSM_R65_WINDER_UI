#include "screen_run.h"

#include <stdio.h>

#include "hmi_actions.h"
#include "hmi_model.h"
#include "hmi_pending_command.h"
#include "hmi_styles.h"
#include "hmi_types.h"
#include "widget_status_badge.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *label;
    lv_obj_t *value;
} run_value_card_t;

typedef struct {
    lv_obj_t *root;
    hmi_status_badge_t badge;
    lv_obj_t *length_label;
    lv_obj_t *percent_label;
    lv_obj_t *progress_bar;
    run_value_card_t master_speed;
    run_value_card_t override;
    run_value_card_t layer;
    run_value_card_t edge;
    run_value_card_t unwound;
    run_value_card_t eta;
    lv_obj_t *carriage_dot;
    lv_obj_t *direction_label;
    lv_obj_t *position_label;
    lv_obj_t *right_edge_label;
    lv_obj_t *feedback_label;
    lv_obj_t *pause_button;
    lv_obj_t *pause_label;
    lv_obj_t *speed_button;
    lv_obj_t *stop_button;
    lv_obj_t *stop_label;
} run_screen_t;

static run_screen_t s_screen;

static void back_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_go_home();
}

static void status_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_open_diagnostics();
}

static void pause_event_cb(lv_event_t *event)
{
    (void)event;

    if (hmi_pending_command_is_active()) {
        return;
    }

    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL || !state->machine_state_known) {
        return;
    }

    if (state->machine_state == HMI_MACHINE_RUNNING) {
        hmi_actions_pause_job();
    } else if (state->machine_state == HMI_MACHINE_PAUSED) {
        hmi_actions_resume_job();
    }
}

static void stop_event_cb(lv_event_t *event)
{
    (void)event;

    if (hmi_pending_command_is_active()) {
        return;
    }

    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL || !state->machine_state_known) {
        return;
    }

    if (state->machine_state == HMI_MACHINE_ACCELERATING ||
        state->machine_state == HMI_MACHINE_RUNNING ||
        state->machine_state == HMI_MACHINE_PAUSED) {
        hmi_actions_stop_job();
    }
}

static void set_button_enabled_color(lv_obj_t *button, hmi_color_role_t role)
{
    lv_obj_set_style_bg_color(button, hmi_color_for_role(role), 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, role == HMI_COLOR_AMBER ? hmi_palette_get()->device : lv_color_white(), 0);
}

static void set_button_dimmed(lv_obj_t *button, bool dimmed)
{
    if (button == NULL) {
        return;
    }

    if (dimmed) {
        lv_obj_add_state(button, LV_STATE_DISABLED);
        lv_obj_set_style_opa(button, LV_OPA_60, 0);
    } else {
        lv_obj_clear_state(button, LV_STATE_DISABLED);
        lv_obj_set_style_opa(button, LV_OPA_COVER, 0);
    }
}

static lv_obj_t *create_button(lv_obj_t *parent, const char *text, hmi_color_role_t role, lv_event_cb_t cb)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->primary_button, 0);
    lv_obj_set_size(button, 176, 52);
    set_button_enabled_color(button, role);
    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return button;
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
    lv_obj_set_style_bg_color(back, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_add_event_cb(back, back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, "<");
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(topbar);
    lv_obj_add_style(title, &styles->topbar_title, 0);
    lv_label_set_text(title, "CONICAL WINDING");

    s_screen.feedback_label = lv_label_create(topbar);
    lv_obj_add_style(s_screen.feedback_label, &styles->status_text, 0);
    lv_label_set_long_mode(s_screen.feedback_label, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(s_screen.feedback_label, 1);
    lv_obj_set_width(s_screen.feedback_label, 0);
    lv_obj_set_style_text_align(s_screen.feedback_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(s_screen.feedback_label, "");

    widget_status_badge_create(topbar, &s_screen.badge);

    lv_obj_t *status = lv_btn_create(topbar);
    lv_obj_remove_style_all(status);
    lv_obj_add_style(status, &styles->nav_button, 0);
    lv_obj_set_size(status, 98, 42);
    lv_obj_set_style_text_color(status, hmi_palette_get()->blue, 0);
    lv_obj_set_style_border_color(status, hmi_palette_get()->blue, 0);
    lv_obj_set_style_bg_color(status, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_add_event_cb(status, status_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *status_label = lv_label_create(status);
    lv_label_set_text(status_label, "STATUS");
    lv_obj_center(status_label);
}

static lv_obj_t *create_panel(lv_obj_t *parent, int32_t width, int32_t height, const char *title_text)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &styles->panel, 0);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 9, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &styles->panel_title, 0);
    lv_label_set_text(title, title_text);

    return panel;
}

static void value_card_create(lv_obj_t *parent, run_value_card_t *card, const char *label_text)
{
    hmi_styles_t *styles = hmi_styles_get();

    card->root = lv_obj_create(parent);
    lv_obj_remove_style_all(card->root);
    lv_obj_add_style(card->root, &styles->panel_secondary, 0);
    lv_obj_set_size(card->root, 204, 58);
    lv_obj_set_flex_flow(card->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card->root, 8, 0);
    lv_obj_set_style_pad_row(card->root, 3, 0);
    lv_obj_clear_flag(card->root, LV_OBJ_FLAG_SCROLLABLE);

    card->label = lv_label_create(card->root);
    lv_obj_add_style(card->label, &styles->panel_title, 0);
    lv_label_set_text(card->label, label_text);

    card->value = lv_label_create(card->root);
    lv_obj_add_style(card->value, &styles->stat_value, 0);
    lv_label_set_text(card->value, "--");
}

static void value_card_set(run_value_card_t *card, const char *value, hmi_color_role_t color)
{
    if (card == NULL || card->value == NULL) {
        return;
    }

    lv_label_set_text(card->value, value != NULL ? value : "--");
    lv_obj_set_style_text_color(card->value, hmi_color_for_role(color), 0);
}

void screen_run_create(lv_obj_t *root)
{
    hmi_styles_t *styles = hmi_styles_get();
    s_screen = (run_screen_t){0};
    s_screen.root = root;

    create_topbar(root);

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), HMI_DISPLAY_HEIGHT - HMI_TOPBAR_HEIGHT - 76);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 14, 0);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *progress = create_panel(content, LV_PCT(100), 86, "PROGRESS");
    lv_obj_set_style_pad_row(progress, 6, 0);

    lv_obj_t *length_row = lv_obj_create(progress);
    lv_obj_remove_style_all(length_row);
    lv_obj_set_size(length_row, LV_PCT(100), 30);
    lv_obj_set_flex_flow(length_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(length_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(length_row, LV_OBJ_FLAG_SCROLLABLE);

    s_screen.length_label = lv_label_create(length_row);
    lv_obj_add_style(s_screen.length_label, &styles->status_text, 0);
    lv_label_set_text(s_screen.length_label, "0.0 / 125.0 m");

    s_screen.percent_label = lv_label_create(length_row);
    lv_obj_add_style(s_screen.percent_label, &styles->status_text, 0);
    lv_obj_set_style_text_color(s_screen.percent_label, hmi_palette_get()->green, 0);
    lv_label_set_text(s_screen.percent_label, "0%");

    s_screen.progress_bar = lv_bar_create(progress);
    lv_obj_set_size(s_screen.progress_bar, LV_PCT(100), 14);
    lv_bar_set_range(s_screen.progress_bar, 0, 100);
    lv_obj_set_style_bg_color(s_screen.progress_bar, hmi_palette_get()->panel_secondary, 0);
    lv_obj_set_style_border_width(s_screen.progress_bar, 1, 0);
    lv_obj_set_style_border_color(s_screen.progress_bar, hmi_palette_get()->border_strong, 0);
    lv_obj_set_style_bg_color(s_screen.progress_bar, hmi_palette_get()->green, LV_PART_INDICATOR);

    lv_obj_t *mid = lv_obj_create(content);
    lv_obj_remove_style_all(mid);
    lv_obj_set_size(mid, LV_PCT(100), 226);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(mid, 10, 0);
    lv_obj_clear_flag(mid, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *stats = create_panel(mid, 444, LV_PCT(100), "RUNTIME VALUES");
    lv_obj_t *card_grid = lv_obj_create(stats);
    lv_obj_remove_style_all(card_grid);
    lv_obj_set_size(card_grid, LV_PCT(100), 190);
    lv_obj_set_flex_flow(card_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(card_grid, 8, 0);
    lv_obj_set_style_pad_column(card_grid, 8, 0);
    lv_obj_clear_flag(card_grid, LV_OBJ_FLAG_SCROLLABLE);

    value_card_create(card_grid, &s_screen.master_speed, "MASTER SPEED");
    value_card_create(card_grid, &s_screen.override, "SPEED OVERRIDE");
    value_card_create(card_grid, &s_screen.layer, "CURRENT LAYER");
    value_card_create(card_grid, &s_screen.edge, "RIGHT EDGE OFFSET");
    value_card_create(card_grid, &s_screen.unwound, "UNWOUND FIBER");
    value_card_create(card_grid, &s_screen.eta, "ETA");

    lv_obj_t *carriage = create_panel(mid, 318, LV_PCT(100), "CARRIAGE");

    lv_obj_t *track = lv_obj_create(carriage);
    lv_obj_remove_style_all(track);
    lv_obj_set_size(track, LV_PCT(100), 128);
    lv_obj_set_style_bg_color(track, hmi_palette_get()->panel_secondary, 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(track, 1, 0);
    lv_obj_set_style_border_color(track, hmi_palette_get()->border, 0);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *rail = lv_obj_create(track);
    lv_obj_remove_style_all(rail);
    lv_obj_set_size(rail, 250, 4);
    lv_obj_set_style_bg_color(rail, hmi_palette_get()->border_strong, 0);
    lv_obj_set_style_bg_opa(rail, LV_OPA_COVER, 0);
    lv_obj_align(rail, LV_ALIGN_CENTER, 0, -4);

    s_screen.carriage_dot = lv_obj_create(track);
    lv_obj_remove_style_all(s_screen.carriage_dot);
    lv_obj_set_size(s_screen.carriage_dot, 18, 18);
    lv_obj_set_style_radius(s_screen.carriage_dot, 9, 0);
    lv_obj_set_style_bg_color(s_screen.carriage_dot, hmi_palette_get()->blue, 0);
    lv_obj_set_style_bg_opa(s_screen.carriage_dot, LV_OPA_COVER, 0);

    s_screen.direction_label = lv_label_create(track);
    lv_obj_add_style(s_screen.direction_label, &styles->status_text, 0);
    lv_obj_set_style_text_color(s_screen.direction_label, hmi_palette_get()->blue, 0);
    lv_label_set_text(s_screen.direction_label, ">");

    lv_obj_t *left_edge = lv_label_create(track);
    lv_obj_add_style(left_edge, &styles->topbar_text, 0);
    lv_label_set_text(left_edge, "0.0 mm");
    lv_obj_align(left_edge, LV_ALIGN_BOTTOM_LEFT, 12, -8);

    s_screen.right_edge_label = lv_label_create(track);
    lv_obj_add_style(s_screen.right_edge_label, &styles->topbar_text, 0);
    lv_label_set_text(s_screen.right_edge_label, "250.0 mm");
    lv_obj_align(s_screen.right_edge_label, LV_ALIGN_BOTTOM_RIGHT, -12, -8);

    s_screen.position_label = lv_label_create(carriage);
    lv_obj_add_style(s_screen.position_label, &styles->stat_value, 0);
    lv_obj_set_style_text_color(s_screen.position_label, hmi_palette_get()->blue, 0);
    lv_label_set_text(s_screen.position_label, "Position 0.0 mm");
    lv_obj_set_width(s_screen.position_label, LV_PCT(100));
    lv_obj_set_style_text_align(s_screen.position_label, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *buttons = lv_obj_create(root);
    lv_obj_remove_style_all(buttons);
    lv_obj_set_size(buttons, LV_PCT(100), 76);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(buttons, 12, 0);
    lv_obj_clear_flag(buttons, LV_OBJ_FLAG_SCROLLABLE);

    s_screen.pause_button = create_button(buttons, "PAUSE", HMI_COLOR_AMBER, pause_event_cb);
    s_screen.pause_label = lv_obj_get_child(s_screen.pause_button, 0);
    s_screen.speed_button = create_button(buttons, "SPEED", HMI_COLOR_BLUE, NULL);
    s_screen.stop_button = create_button(buttons, "STOP", HMI_COLOR_RED, stop_event_cb);
    s_screen.stop_label = lv_obj_get_child(s_screen.stop_button, 0);
    create_button(buttons, "HOME", HMI_COLOR_DIM, back_event_cb);

    set_button_dimmed(s_screen.pause_button, true);
    set_button_dimmed(s_screen.speed_button, true);
}

void screen_run_update(const hmi_state_t *state)
{
    if (state == NULL || s_screen.root == NULL) {
        return;
    }

    char value[40];
    widget_status_badge_update(&s_screen.badge, state);
    hmi_pending_command_t pending = hmi_pending_command_get();
    bool any_pending = hmi_pending_command_is_active();
    bool pause_pending = pending == HMI_PENDING_PAUSE_JOB;
    bool resume_pending = pending == HMI_PENDING_RESUME_JOB;
    bool stop_pending = pending == HMI_PENDING_STOP_JOB;
    bool machine_accelerating = state->machine_state_known &&
                                state->machine_state == HMI_MACHINE_ACCELERATING;
    bool machine_running = state->machine_state_known &&
                           state->machine_state == HMI_MACHINE_RUNNING;
    bool machine_paused = state->machine_state_known &&
                          state->machine_state == HMI_MACHINE_PAUSED;
    bool machine_stopping = state->machine_state_known &&
                            state->machine_state == HMI_MACHINE_STOPPING;

    if (s_screen.feedback_label != NULL) {
        bool has_error = state->last_error != NULL && state->last_error[0] != '\0';
        lv_label_set_text(s_screen.feedback_label, has_error ? state->last_error : "");
        lv_obj_set_style_text_color(
            s_screen.feedback_label,
            has_error ? hmi_palette_get()->red : hmi_palette_get()->text_dim,
            0);
    }

    snprintf(value, sizeof(value), "%.1f / %.1f m", (double)state->wound_length_m, (double)state->target_length_m);
    lv_label_set_text(s_screen.length_label, value);
    snprintf(value, sizeof(value), "%.0f%%", (double)state->progress_percent);
    lv_label_set_text(s_screen.percent_label, value);
    lv_bar_set_value(s_screen.progress_bar, (int32_t)state->progress_percent, LV_ANIM_OFF);

    if (state->master_speed_known) {
        snprintf(value, sizeof(value), "%.1f rps", (double)state->master_speed_rps);
        value_card_set(&s_screen.master_speed, value, HMI_COLOR_GREEN);
    } else {
        value_card_set(&s_screen.master_speed, "Unknown", HMI_COLOR_DIM);
    }
    snprintf(value, sizeof(value), "%.0f %%", (double)state->speed_override_percent);
    value_card_set(&s_screen.override, value, HMI_COLOR_BLUE);
    snprintf(value, sizeof(value), "%lu", (unsigned long)state->current_layer);
    value_card_set(&s_screen.layer, value, HMI_COLOR_NEUTRAL);
    snprintf(value, sizeof(value), "%.1f mm", (double)state->right_edge_offset_mm);
    value_card_set(&s_screen.edge, value, HMI_COLOR_NEUTRAL);
    snprintf(value, sizeof(value), "%.1f m", (double)state->unwound_length_m);
    value_card_set(&s_screen.unwound, value, HMI_COLOR_NEUTRAL);
    snprintf(value, sizeof(value), "%.1f min", (double)state->eta_min);
    value_card_set(&s_screen.eta, value, HMI_COLOR_NEUTRAL);

    double range = state->travel_range_known && state->travel_range_mm > 1.0 ?
        state->travel_range_mm : 250.0;
    int32_t x = 22 + (int32_t)(((double)state->carriage_position_mm / range) * 250.0);
    if (x < 22) {
        x = 22;
    } else if (x > 272) {
        x = 272;
    }
    lv_obj_set_pos(s_screen.carriage_dot, x, 52);
    lv_obj_set_pos(s_screen.direction_label, x + 12, 44);

    if (state->carriage_direction == HMI_CARRIAGE_LEFT) {
        lv_label_set_text(s_screen.direction_label, "<");
    } else if (state->carriage_direction == HMI_CARRIAGE_RIGHT) {
        lv_label_set_text(s_screen.direction_label, ">");
    } else {
        lv_label_set_text(s_screen.direction_label, "STOP");
    }

    snprintf(value, sizeof(value), "%.1f mm", range);
    lv_label_set_text(s_screen.right_edge_label, value);
    lv_obj_align(s_screen.right_edge_label, LV_ALIGN_BOTTOM_RIGHT, -12, -8);

    snprintf(value, sizeof(value), "Position %.1f mm", (double)state->carriage_position_mm);
    lv_label_set_text(s_screen.position_label, value);

    if (s_screen.pause_label != NULL) {
        if (pause_pending || machine_stopping) {
            lv_label_set_text(s_screen.pause_label, "PAUSING...");
        } else if (resume_pending) {
            lv_label_set_text(s_screen.pause_label, "RESUMING...");
        } else {
            lv_label_set_text(s_screen.pause_label, machine_paused ? "RESUME" : "PAUSE");
        }
        lv_obj_center(s_screen.pause_label);
        set_button_enabled_color(
            s_screen.pause_button,
            machine_paused ? HMI_COLOR_GREEN : HMI_COLOR_AMBER);
        set_button_dimmed(
            s_screen.pause_button,
            any_pending || (!machine_running && !machine_paused));
    }

    set_button_dimmed(s_screen.speed_button, true);

    if (s_screen.stop_label != NULL) {
        lv_label_set_text(
            s_screen.stop_label,
            (stop_pending || machine_stopping) ? "STOPPING..." : "STOP");
        set_button_enabled_color(s_screen.stop_button, HMI_COLOR_RED);
        set_button_dimmed(
            s_screen.stop_button,
            any_pending ||
            machine_stopping ||
            (!machine_accelerating && !machine_running && !machine_paused));
        lv_obj_center(s_screen.stop_label);
    }
}
