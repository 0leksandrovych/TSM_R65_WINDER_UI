#include "screen_run.h"

#include <math.h>
#include <stdio.h>

#include "hmi_actions.h"
#include "hmi_model.h"
#include "hmi_paused_job_draft_model.h"
#include "hmi_pending_command.h"
#include "hmi_styles.h"
#include "hmi_types.h"
#include "modal_confirm.h"
#include "modal_edge_trim.h"
#include "modal_paused_job_edit.h"
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
    lv_obj_t *pause_summary_label;
    lv_obj_t *metrics_panel;
    run_value_card_t actual_speed;
    run_value_card_t job_speed;
    run_value_card_t winding_pitch;
    run_value_card_t completed_layers;
    run_value_card_t applied_edge_offset;
    run_value_card_t edge_trim;
    lv_obj_t *feedback_label;
    lv_obj_t *primary_button;   /* PAUSE / RESUME (also shows run-state text) */
    lv_obj_t *primary_label;
    lv_obj_t *secondary_button; /* ABORT / FINISH JOB */
    lv_obj_t *secondary_label;
    lv_obj_t *edit_button;
    lv_obj_t *edit_label;
    lv_obj_t *edge_trim_button;
    lv_obj_t *edge_trim_label;
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

static void primary_event_cb(lv_event_t *event)
{
    (void)event;

    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL || !state->machine_state_known) {
        return;
    }

    /* Both actions re-check state and pending internally. */
    if (state->machine_state == HMI_MACHINE_RUNNING) {
        hmi_actions_pause_job();
    } else if (state->machine_state == HMI_MACHINE_PAUSED) {
        hmi_actions_resume_job();
    }
}

static void finish_confirm_cb(void *user_ctx)
{
    (void)user_ctx;
    hmi_actions_finish_job();
}

static void secondary_event_cb(lv_event_t *event)
{
    (void)event;

    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL || !state->machine_state_known) {
        return;
    }

    if (state->machine_state == HMI_MACHINE_PAUSED) {
        /* FINISH JOB always confirms first; the job cannot be resumed after. */
        const modal_confirm_config_t config = {
            .title = "Finish current job?",
            .body = "The current measured length and statistics will be saved. "
                    "The job cannot be resumed afterward.",
            .cancel_text = "CANCEL",
            .confirm_text = "FINISH JOB",
            .confirm_role = HMI_COLOR_AMBER,
            .confirm_cb = finish_confirm_cb,
        };
        modal_confirm_open(&config);
    } else {
        /* ABORT is destructive but immediate — no confirmation. State and
         * pending gating live entirely inside hmi_actions_abort_job(). */
        hmi_actions_abort_job();
    }
}

static void edge_trim_event_cb(lv_event_t *event)
{
    (void)event;
    modal_edge_trim_open();
}

static void edit_job_event_cb(lv_event_t *event)
{
    (void)event;
    modal_paused_job_edit_open();
}

static void set_button_enabled_color(lv_obj_t *button, hmi_color_role_t role)
{
    lv_obj_set_style_bg_color(button, hmi_color_for_role(role), 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(
        button,
        role == HMI_COLOR_AMBER ? hmi_palette_get()->device : lv_color_white(),
        0);
}

static void set_button_dimmed(lv_obj_t *button, bool dimmed)
{
    if (button == NULL) {
        return;
    }

    if (dimmed) {
        lv_obj_clear_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(button, LV_STATE_DISABLED);
        lv_obj_set_style_opa(button, LV_OPA_60, 0);
    } else {
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_state(button, LV_STATE_DISABLED);
        lv_obj_set_style_opa(button, LV_OPA_COVER, 0);
    }
}

static lv_obj_t *create_button(
    lv_obj_t *parent,
    const char *text,
    int32_t width,
    hmi_color_role_t role,
    lv_event_cb_t cb)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->primary_button, 0);
    lv_obj_set_size(button, width, 52);
    set_button_enabled_color(button, role);
    if (cb != NULL) {
        lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return button;
}

static void create_home_button(lv_obj_t *parent)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->nav_button, 0);
    lv_obj_set_size(button, 150, 52);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->device, LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, "HOME");
    lv_obj_center(label);
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

    lv_obj_t *spacer = lv_obj_create(topbar);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 1);

    s_screen.feedback_label = lv_label_create(topbar);
    lv_obj_add_style(s_screen.feedback_label, &styles->status_text, 0);
    lv_label_set_long_mode(s_screen.feedback_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_screen.feedback_label, 150);
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

static lv_obj_t *create_panel(lv_obj_t *parent, int32_t height, const char *title_text)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &styles->panel, 0);
    lv_obj_set_size(panel, LV_PCT(100), height);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 6, 0);
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
    lv_obj_set_size(card->root, 243, 76);
    lv_obj_set_flex_flow(card->root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card->root, 8, 0);
    lv_obj_set_style_pad_row(card->root, 4, 0);
    lv_obj_clear_flag(card->root, LV_OBJ_FLAG_SCROLLABLE);

    card->label = lv_label_create(card->root);
    lv_obj_add_style(card->label, &styles->panel_title, 0);
    lv_label_set_long_mode(card->label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(card->label, LV_PCT(100));
    lv_label_set_text(card->label, label_text);

    card->value = lv_label_create(card->root);
    lv_obj_add_style(card->value, &styles->stat_value, 0);
    lv_label_set_long_mode(card->value, LV_LABEL_LONG_DOT);
    lv_obj_set_width(card->value, LV_PCT(100));
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
    lv_obj_set_size(content, LV_PCT(100), HMI_DISPLAY_HEIGHT - HMI_TOPBAR_HEIGHT - 70);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 12, 0);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *progress = create_panel(content, 108, "WINDING PROGRESS");

    lv_obj_t *length_row = lv_obj_create(progress);
    lv_obj_remove_style_all(length_row);
    lv_obj_set_size(length_row, LV_PCT(100), 40);
    lv_obj_set_flex_flow(length_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        length_row,
        LV_FLEX_ALIGN_SPACE_BETWEEN,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(length_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *length_block = lv_obj_create(length_row);
    lv_obj_remove_style_all(length_block);
    lv_obj_set_size(length_block, 650, 40);
    lv_obj_set_flex_flow(length_block, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(length_block, 1, 0);
    lv_obj_clear_flag(length_block, LV_OBJ_FLAG_SCROLLABLE);

    s_screen.length_label = lv_label_create(length_block);
    lv_obj_add_style(s_screen.length_label, &styles->status_text, 0);
    lv_label_set_text(s_screen.length_label, "0.000 / -- m");

    s_screen.pause_summary_label = lv_label_create(length_block);
    lv_obj_add_style(s_screen.pause_summary_label, &styles->panel_title, 0);
    lv_obj_set_width(s_screen.pause_summary_label, LV_PCT(100));
    lv_label_set_long_mode(s_screen.pause_summary_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(s_screen.pause_summary_label,
                                hmi_palette_get()->amber,
                                0);
    lv_label_set_text(s_screen.pause_summary_label, "");
    lv_obj_add_flag(s_screen.pause_summary_label, LV_OBJ_FLAG_HIDDEN);

    s_screen.percent_label = lv_label_create(length_row);
    lv_obj_add_style(s_screen.percent_label, &styles->status_text, 0);
    lv_obj_set_style_text_color(s_screen.percent_label, hmi_palette_get()->text_dim, 0);
    lv_label_set_text(s_screen.percent_label, "--");

    s_screen.progress_bar = lv_bar_create(progress);
    lv_obj_set_size(s_screen.progress_bar, LV_PCT(100), 14);
    lv_bar_set_range(s_screen.progress_bar, 0, 100);
    lv_obj_set_style_bg_color(s_screen.progress_bar, hmi_palette_get()->panel_secondary, 0);
    lv_obj_set_style_border_width(s_screen.progress_bar, 1, 0);
    lv_obj_set_style_border_color(s_screen.progress_bar, hmi_palette_get()->border_strong, 0);
    lv_obj_set_style_bg_color(s_screen.progress_bar, hmi_palette_get()->text_dim, LV_PART_INDICATOR);

    s_screen.metrics_panel = create_panel(content, 214, "RUNTIME VALUES");
    lv_obj_t *card_grid = lv_obj_create(s_screen.metrics_panel);
    lv_obj_remove_style_all(card_grid);
    lv_obj_set_size(card_grid, LV_PCT(100), 164);
    lv_obj_set_flex_flow(card_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(card_grid, 8, 0);
    lv_obj_set_style_pad_column(card_grid, 8, 0);
    lv_obj_clear_flag(card_grid, LV_OBJ_FLAG_SCROLLABLE);

    value_card_create(card_grid, &s_screen.actual_speed, "ACTUAL SPEED");
    value_card_create(card_grid, &s_screen.job_speed, "JOB SPEED");
    value_card_create(card_grid, &s_screen.winding_pitch, "WINDING PITCH");
    value_card_create(card_grid, &s_screen.completed_layers, "COMPLETED LAYERS");
    value_card_create(card_grid, &s_screen.applied_edge_offset, "CONICAL OFFSET / INTERVAL");
    value_card_create(card_grid, &s_screen.edge_trim, "EDGE TRIM");
    lv_obj_add_style(s_screen.edge_trim.value, &styles->status_text, 0);

    lv_obj_t *buttons = lv_obj_create(root);
    lv_obj_remove_style_all(buttons);
    lv_obj_set_size(buttons, LV_PCT(100), 70);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(buttons, 8, 0);
    lv_obj_clear_flag(buttons, LV_OBJ_FLAG_SCROLLABLE);

    s_screen.primary_button = create_button(buttons, "PAUSE", 145, HMI_COLOR_AMBER, primary_event_cb);
    s_screen.primary_label = lv_obj_get_child(s_screen.primary_button, 0);
    s_screen.secondary_button = create_button(buttons, "ABORT", 150, HMI_COLOR_RED, secondary_event_cb);
    s_screen.secondary_label = lv_obj_get_child(s_screen.secondary_button, 0);
    s_screen.edit_button = create_button(
        buttons, "EDIT JOB", 140, HMI_COLOR_BLUE, edit_job_event_cb);
    s_screen.edit_label = lv_obj_get_child(s_screen.edit_button, 0);
    s_screen.edge_trim_button = create_button(
        buttons, "EDGE TRIM", 140, HMI_COLOR_BLUE, edge_trim_event_cb);
    s_screen.edge_trim_label = lv_obj_get_child(s_screen.edge_trim_button, 0);
    create_home_button(buttons);

    set_button_dimmed(s_screen.primary_button, true);
    set_button_dimmed(s_screen.secondary_button, true);
    set_button_dimmed(s_screen.edit_button, true);
    set_button_dimmed(s_screen.edge_trim_button, true);
}

static void format_trim_value(char *buffer, size_t buffer_size, int32_t centi_mm)
{
    if (centi_mm == 0) {
        snprintf(buffer, buffer_size, "0.0");
    } else if ((centi_mm % 10) == 0) {
        snprintf(buffer, buffer_size, "%+.1f", (double)centi_mm / 100.0);
    } else {
        snprintf(buffer, buffer_size, "%+.2f", (double)centi_mm / 100.0);
    }
}

static const char *pause_reason_text(const hmi_state_t *state)
{
    if (state == NULL || !state->pause_reason_known) {
        return "PAUSE REASON NOT REPORTED";
    }

    switch (state->pause_reason) {
    case HMI_JOB_PAUSE_REASON_OPERATOR:
        return "OPERATOR PAUSE";
    case HMI_JOB_PAUSE_REASON_TARGET_REACHED:
        return "TARGET REACHED";
    case HMI_JOB_PAUSE_REASON_LENGTH_WATCHDOG:
        return "LENGTH WATCHDOG";
    case HMI_JOB_PAUSE_REASON_NONE:
    default:
        return "PAUSED";
    }
}

void screen_run_update(const hmi_state_t *state)
{
    if (state == NULL || s_screen.root == NULL) {
        return;
    }

    /* The paused-job editor is an opaque fullscreen layer. Keep delivering
     * current state to its lifecycle/presentation logic, but do not invalidate
     * and relayout the covered Run tree. If the modal closes while processing
     * this state, fall through once and fully synchronize Run. */
    if (modal_paused_job_edit_is_open()) {
        modal_paused_job_edit_update(state);
        if (modal_paused_job_edit_is_open()) {
            return;
        }
    }

    char value[96];
    widget_status_badge_update(&s_screen.badge, state);
    hmi_pending_command_t pending = hmi_pending_command_get();
    bool any_pending = hmi_pending_command_is_active();
    bool pause_pending = pending == HMI_PENDING_PAUSE_JOB;
    bool resume_pending = pending == HMI_PENDING_RESUME_JOB;
    bool abort_pending = pending == HMI_PENDING_ABORT_JOB;
    bool finish_pending = pending == HMI_PENDING_FINISH_JOB;
    bool machine_accelerating = state->machine_state_known &&
                                state->machine_state == HMI_MACHINE_ACCELERATING;
    bool machine_running = state->machine_state_known &&
                           state->machine_state == HMI_MACHINE_RUNNING;
    bool machine_paused = state->machine_state_known &&
                          state->machine_state == HMI_MACHINE_PAUSED;
    bool machine_stopping = state->machine_state_known &&
                            state->machine_state == HMI_MACHINE_STOPPING;
    bool resume_allowed = machine_paused &&
                          state->wound_length_known &&
                          state->target_length_known &&
                          state->target_length_m > state->wound_length_m &&
                          !hmi_paused_job_draft_model_update_awaiting_confirmation();

    if (s_screen.feedback_label != NULL) {
        bool has_error = state->last_error != NULL && state->last_error[0] != '\0';
        lv_label_set_text(s_screen.feedback_label, has_error ? state->last_error : "");
        lv_obj_set_style_text_color(
            s_screen.feedback_label,
            has_error ? hmi_palette_get()->red : hmi_palette_get()->text_dim,
            0);
    }

    bool target_available = state->wound_length_known &&
                            state->target_length_known &&
                            state->target_length_m > 0.0f;
    double calculated_percent = 0.0;
    if (target_available) {
        calculated_percent = ((double)state->wound_length_m / (double)state->target_length_m) * 100.0;
        if (calculated_percent < 0.0) {
            calculated_percent = 0.0;
        } else if (calculated_percent > 100.0) {
            calculated_percent = 100.0;
        }

        snprintf(
            value,
            sizeof(value),
            "%.3f / %.3f m",
            (double)state->wound_length_m,
            (double)state->target_length_m);
        lv_label_set_text(s_screen.length_label, value);
        lv_obj_set_style_text_color(s_screen.length_label, hmi_palette_get()->text, 0);

        snprintf(value, sizeof(value), "%.0f%%", calculated_percent);
        lv_label_set_text(s_screen.percent_label, value);
        lv_obj_set_style_text_color(s_screen.percent_label, hmi_palette_get()->green, 0);
        lv_obj_set_style_bg_color(s_screen.progress_bar, hmi_palette_get()->green, LV_PART_INDICATOR);
    } else if (state->wound_length_known) {
        snprintf(value, sizeof(value), "%.3f / -- m", (double)state->wound_length_m);
        lv_label_set_text(s_screen.length_label, value);
        lv_obj_set_style_text_color(s_screen.length_label, hmi_palette_get()->text_dim, 0);
        lv_label_set_text(s_screen.percent_label, "--");
        lv_obj_set_style_text_color(s_screen.percent_label, hmi_palette_get()->text_dim, 0);
        lv_obj_set_style_bg_color(s_screen.progress_bar, hmi_palette_get()->text_dim, LV_PART_INDICATOR);
    } else {
        lv_label_set_text(s_screen.length_label, "-- / -- m");
        lv_obj_set_style_text_color(s_screen.length_label, hmi_palette_get()->text_dim, 0);
        lv_label_set_text(s_screen.percent_label, "--");
        lv_obj_set_style_text_color(s_screen.percent_label, hmi_palette_get()->text_dim, 0);
        lv_obj_set_style_bg_color(s_screen.progress_bar, hmi_palette_get()->text_dim, LV_PART_INDICATOR);
    }
    lv_bar_set_value(s_screen.progress_bar, (int32_t)(calculated_percent + 0.5), LV_ANIM_OFF);

    if (machine_paused) {
        if (state->wound_length_known && state->target_length_known) {
            const double difference = (double)state->target_length_m -
                                      (double)state->wound_length_m;
            snprintf(value,
                     sizeof(value),
                     difference >= 0.0
                         ? "Remaining %.3f m | %s"
                         : "Exceeded %.3f m | %s",
                     fabs(difference),
                     pause_reason_text(state));
        } else {
            snprintf(value,
                     sizeof(value),
                     "Remaining unavailable | %s",
                     pause_reason_text(state));
        }
        lv_label_set_text(s_screen.pause_summary_label, value);
        lv_obj_set_style_text_color(
            s_screen.pause_summary_label,
            state->pause_reason_known &&
                    state->pause_reason == HMI_JOB_PAUSE_REASON_LENGTH_WATCHDOG
                ? hmi_palette_get()->red
                : hmi_palette_get()->amber,
            0);
        lv_obj_clear_flag(s_screen.pause_summary_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_screen.pause_summary_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (state->master_speed_known) {
        snprintf(value, sizeof(value), "%.2f rps", (double)state->master_speed_rps);
        value_card_set(&s_screen.actual_speed, value, HMI_COLOR_GREEN);
    } else {
        value_card_set(&s_screen.actual_speed, "--", HMI_COLOR_DIM);
    }

    if (state->job_master_speed_known) {
        snprintf(value, sizeof(value), "%.2f rps", (double)state->job_master_speed_rps);
        value_card_set(&s_screen.job_speed, value, HMI_COLOR_BLUE);
    } else {
        value_card_set(&s_screen.job_speed, "--", HMI_COLOR_DIM);
    }

    if (state->winding_pitch_known) {
        snprintf(value, sizeof(value), "%.3f mm", (double)state->winding_pitch_mm);
        value_card_set(&s_screen.winding_pitch, value, HMI_COLOR_NEUTRAL);
    } else {
        value_card_set(&s_screen.winding_pitch, "--", HMI_COLOR_DIM);
    }

    snprintf(value, sizeof(value), "%lu", (unsigned long)state->current_layer);
    value_card_set(&s_screen.completed_layers, value, HMI_COLOR_NEUTRAL);

    snprintf(
        value,
        sizeof(value),
        state->shift_every_layers == 1U
            ? "%+.2f mm / %lu layer"
            : "%+.2f mm / %lu layers",
        (double)state->right_edge_offset_mm,
        (unsigned long)state->shift_every_layers);
    value_card_set(&s_screen.applied_edge_offset, value, HMI_COLOR_NEUTRAL);

    hmi_edge_trim_pair_t pending_trim;
    bool trim_known = state->active_left_edge_trim_known &&
                      state->active_right_edge_trim_known;
    if (trim_known) {
        int32_t active_left =
            (int32_t)lroundf(state->active_left_edge_trim_mm * 100.0f);
        int32_t active_right =
            (int32_t)lroundf(state->active_right_edge_trim_mm * 100.0f);
        char active_left_text[16];
        char active_right_text[16];
        format_trim_value(active_left_text, sizeof(active_left_text), active_left);
        format_trim_value(active_right_text, sizeof(active_right_text), active_right);

        if (hmi_edge_trim_pending_get(&pending_trim)) {
            char pending_left_text[16];
            char pending_right_text[16];
            format_trim_value(
                pending_left_text,
                sizeof(pending_left_text),
                pending_trim.left_centi_mm);
            format_trim_value(
                pending_right_text,
                sizeof(pending_right_text),
                pending_trim.right_centi_mm);
            snprintf(
                value,
                sizeof(value),
                "L %s > %s\nR %s > %s  PENDING",
                active_left_text,
                pending_left_text,
                active_right_text,
                pending_right_text);
            value_card_set(&s_screen.edge_trim, value, HMI_COLOR_AMBER);
        } else {
            snprintf(
                value,
                sizeof(value),
                "L %s mm   R %s mm",
                active_left_text,
                active_right_text);
            value_card_set(&s_screen.edge_trim, value, HMI_COLOR_NEUTRAL);
        }
    } else {
        value_card_set(&s_screen.edge_trim, "L --   R --", HMI_COLOR_DIM);
    }

    /* Primary button: PAUSE / RESUME when actionable, otherwise a disabled
     * run-state indicator. STOPPING shows "STOPPING..." — never "FINISHING..." —
     * because the STOPPING machine state is also used while pausing. */
    if (s_screen.primary_label != NULL) {
        const char *text = "PAUSE";
        hmi_color_role_t color = HMI_COLOR_AMBER;
        bool enabled = false;

        if (pause_pending) {
            text = "PAUSING...";
        } else if (resume_pending) {
            text = "RESUMING...";
            color = HMI_COLOR_GREEN;
        } else if (machine_running) {
            text = "PAUSE";
            enabled = !any_pending;
        } else if (machine_paused) {
            text = "RESUME";
            color = HMI_COLOR_GREEN;
            enabled = !any_pending && resume_allowed;
        } else if (machine_accelerating) {
            text = "STARTING...";
        } else if (machine_stopping) {
            text = "STOPPING...";
        }

        lv_label_set_text(s_screen.primary_label, text);
        lv_obj_center(s_screen.primary_label);
        set_button_enabled_color(s_screen.primary_button, color);
        set_button_dimmed(s_screen.primary_button, !enabled);
    }

    /* Secondary button: destructive ABORT while in motion, or FINISH JOB while
     * PAUSED (ABORT is hidden there). Repeated presses are blocked by dimming;
     * the exception is STOPPING, where ABORT stays available so a decelerating
     * PAUSE can still be escalated to an abort. */
    if (s_screen.secondary_label != NULL) {
        const char *text = "ABORT";
        hmi_color_role_t color = HMI_COLOR_RED;
        bool enabled = false;
        bool visible = true;

        if (abort_pending) {
            text = "ABORTING...";
        } else if (finish_pending) {
            text = "FINISHING...";
            color = HMI_COLOR_AMBER;
        } else if (machine_paused) {
            text = "FINISH JOB";
            color = HMI_COLOR_AMBER;
            enabled = !any_pending;
        } else if (machine_accelerating || machine_running) {
            text = "ABORT";
            enabled = !any_pending;
        } else if (machine_stopping) {
            text = "ABORT";
            enabled = !any_pending || pause_pending;
        } else {
            visible = false;
        }

        lv_label_set_text(s_screen.secondary_label, text);
        lv_obj_center(s_screen.secondary_label);
        set_button_enabled_color(s_screen.secondary_button, color);
        set_button_dimmed(s_screen.secondary_button, !enabled);
        if (visible) {
            lv_obj_clear_flag(s_screen.secondary_button, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_screen.secondary_button, LV_OBJ_FLAG_HIDDEN);
        }
    }

    const bool edit_visible = machine_paused;
    const bool edit_enabled = edit_visible &&
                              hmi_paused_job_draft_model_is_initialized() &&
                              !any_pending &&
                              !hmi_paused_job_draft_model_update_awaiting_confirmation();
    if (s_screen.edit_label != NULL) {
        lv_label_set_text(s_screen.edit_label, "EDIT JOB");
        lv_obj_center(s_screen.edit_label);
        set_button_enabled_color(s_screen.edit_button, HMI_COLOR_BLUE);
    }
    set_button_dimmed(s_screen.edit_button, !edit_enabled);
    if (edit_visible) {
        lv_obj_clear_flag(s_screen.edit_button, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_screen.edit_button, LV_OBJ_FLAG_HIDDEN);
    }

    const bool edge_trim_visible = machine_running || machine_paused;
    const bool edge_trim_enabled = edge_trim_visible && !any_pending;
    if (s_screen.edge_trim_label != NULL) {
        lv_label_set_text(s_screen.edge_trim_label, "EDGE TRIM");
        lv_obj_center(s_screen.edge_trim_label);
        set_button_enabled_color(s_screen.edge_trim_button, HMI_COLOR_BLUE);
    }
    set_button_dimmed(s_screen.edge_trim_button, !edge_trim_enabled);
    if (edge_trim_visible) {
        lv_obj_clear_flag(s_screen.edge_trim_button, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_screen.edge_trim_button, LV_OBJ_FLAG_HIDDEN);
    }

    modal_edge_trim_update(state);
}
