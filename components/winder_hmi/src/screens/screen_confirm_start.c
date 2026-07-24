#include "screen_confirm_start.h"

#include <stdio.h>

#include "hmi_actions.h"
#include "hmi_capability_model.h"
#include "hmi_job_draft_model.h"
#include "hmi_model.h"
#include "hmi_param.h"
#include "hmi_pending_command.h"
#include "hmi_styles.h"
#include "hmi_types.h"
#include "widget_stat_row.h"
#include "widget_status_badge.h"

#define CONFIRM_START_MAX_PARAMS 8

typedef struct {
    const hmi_param_descriptor_t *descriptor;
    hmi_stat_row_t row;
} confirm_param_row_t;

typedef struct {
    lv_obj_t *root;
    hmi_status_badge_t badge;
    hmi_stat_row_t mode;
    confirm_param_row_t params[CONFIRM_START_MAX_PARAMS];
    hmi_stat_row_t homing;
    hmi_stat_row_t carriage;
    hmi_stat_row_t safety;
    lv_obj_t *message_label;
    lv_obj_t *start_button;
    lv_obj_t *start_label;
} confirm_start_screen_t;

static confirm_start_screen_t s_screen;

static void cancel_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_open_active_setup();
}

static bool can_start(const hmi_state_t *state)
{
    const hmi_job_validation_t *validation =
        hmi_job_draft_model_get_validation();

    return hmi_state_can_start_job(state) &&
           hmi_model_get_connection_state() == HMI_CONNECTION_CONNECTED &&
           !hmi_pending_command_is_active() &&
           hmi_job_draft_model_get_mode() != HMI_JOB_MODE_NONE &&
           validation != NULL &&
           validation->valid;
}

static const char *start_block_reason(const hmi_state_t *state)
{
    if (state == NULL || !state->machine_state_known) {
        return "Machine state unavailable.";
    }
    if (!state->carriage_reference_position_known ||
        state->carriage_reference_position == HMI_CARRIAGE_POSITION_UNKNOWN) {
        return "Carriage position unavailable.";
    }
    if (state->carriage_reference_position == HMI_CARRIAGE_POSITION_LEFT_EDGE) {
        return "Return carriage to zero before starting.";
    }
    if (state->carriage_reference_position == HMI_CARRIAGE_POSITION_MOVING ||
        hmi_machine_state_is_positioning(state->machine_state)) {
        return "Wait until carriage positioning completes.";
    }
    if (state->machine_state != HMI_MACHINE_READY) {
        return "Machine is not ready to start.";
    }
    if (hmi_job_draft_model_get_mode() == HMI_JOB_MODE_NONE) {
        return "Configure a winding job first.";
    }
    const hmi_job_validation_t *validation =
        hmi_job_draft_model_get_validation();
    if (validation == NULL || !validation->valid) {
        return validation != NULL && validation->message[0] != '\0' ?
            validation->message : "Local job validation is required.";
    }
    if (!state->safety_ok) {
        return "Safety circuit is not ready.";
    }
    if (hmi_model_get_connection_state() != HMI_CONNECTION_CONNECTED) {
        return "Controller is not connected.";
    }
    if (hmi_pending_command_is_active()) {
        return hmi_pending_command_get_message();
    }
    return "Start winding is unavailable.";
}

static void start_event_cb(lv_event_t *event)
{
    (void)event;
    if (can_start(hmi_model_get_state())) {
        hmi_actions_start_job();
    }
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

static const char *homing_text(const hmi_state_t *state)
{
    return state != NULL && state->machine_state_known &&
           state->machine_state == HMI_MACHINE_READY ? "OK" : "REQUIRED";
}

static void back_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_open_active_setup();
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
    lv_label_set_text(title, "START CONFIRMATION");

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
    lv_obj_set_style_pad_row(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &styles->panel_title, 0);
    lv_label_set_text(title, title_text);

    return panel;
}

static lv_obj_t *create_button(lv_obj_t *parent, const char *text, hmi_color_role_t role, lv_event_cb_t cb)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->primary_button, 0);
    lv_obj_set_size(button, 260, 52);
    lv_obj_set_style_bg_color(button, hmi_color_for_role(role), 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, role == HMI_COLOR_AMBER ? hmi_palette_get()->device : lv_color_white(), 0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return button;
}

void screen_confirm_start_create(lv_obj_t *root)
{
    hmi_styles_t *styles = hmi_styles_get();
    s_screen = (confirm_start_screen_t){0};
    s_screen.root = root;

    create_topbar(root);

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), HMI_DISPLAY_HEIGHT - HMI_TOPBAR_HEIGHT - 76);
    lv_obj_set_style_pad_all(content, 14, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(content, 12, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *summary = create_panel(content, 382, "JOB SUMMARY");
    widget_stat_row_create(summary, &s_screen.mode, "Mode");

    const hmi_mode_capability_t *mode =
        hmi_capability_model_get_mode_by_id(hmi_job_draft_model_get_mode());
    size_t param_row = 0;
    if (mode != NULL) {
        for (size_t i = 0; i < mode->param_count && param_row < (sizeof(s_screen.params) / sizeof(s_screen.params[0])); i++) {
            if (mode->params[i].visible) {
                s_screen.params[param_row].descriptor = &mode->params[i];
                widget_stat_row_create(summary, &s_screen.params[param_row].row, mode->params[i].label);
                param_row++;
            }
        }
    }

    lv_obj_t *ready = create_panel(content, 378, "READINESS");
    widget_stat_row_create(ready, &s_screen.homing, "Homing");
    widget_stat_row_create(ready, &s_screen.carriage, "Carriage reference");
    widget_stat_row_create(ready, &s_screen.safety, "Safety");

    s_screen.message_label = lv_label_create(ready);
    lv_obj_add_style(s_screen.message_label, &styles->status_text, 0);
    lv_label_set_long_mode(s_screen.message_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_screen.message_label, LV_PCT(100));
    lv_label_set_text(s_screen.message_label, "--");

    lv_obj_t *spacer = lv_obj_create(ready);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 1);

    lv_obj_t *note = lv_label_create(ready);
    lv_obj_add_style(note, &styles->topbar_text, 0);
    lv_label_set_long_mode(note, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(note, LV_PCT(100));
    lv_label_set_text(note, "START WINDING emits HMI_CMD_START_JOB. Demo mode starts the mock run only.");

    lv_obj_t *buttons = lv_obj_create(root);
    lv_obj_remove_style_all(buttons);
    lv_obj_set_size(buttons, LV_PCT(100), 76);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(buttons, 14, 0);
    lv_obj_clear_flag(buttons, LV_OBJ_FLAG_SCROLLABLE);

    create_button(buttons, "CANCEL", HMI_COLOR_DIM, cancel_event_cb);
    s_screen.start_button = create_button(buttons, "START WINDING", HMI_COLOR_GREEN, start_event_cb);
    s_screen.start_label = lv_obj_get_child(s_screen.start_button, 0);
}

void screen_confirm_start_update(const hmi_state_t *state)
{
    if (state == NULL || s_screen.root == NULL) {
        return;
    }

    (void)hmi_job_draft_model_validate_local();

    char text[48];
    const hmi_mode_capability_t *mode =
        hmi_capability_model_get_mode_by_id(hmi_job_draft_model_get_mode());
    bool start_pending = hmi_pending_command_get() == HMI_PENDING_START_JOB;

    widget_status_badge_update(&s_screen.badge, state);
    widget_stat_row_set_value(&s_screen.mode,
                              mode != NULL && mode->title != NULL ? mode->title : "--",
                              HMI_COLOR_BLUE);

    for (size_t i = 0; i < sizeof(s_screen.params) / sizeof(s_screen.params[0]); i++) {
        const hmi_param_descriptor_t *descriptor = s_screen.params[i].descriptor;
        hmi_param_value_t value = {0};
        if (descriptor == NULL) {
            continue;
        }
        if (hmi_job_draft_model_get_value(descriptor->id, &value) &&
            hmi_param_format_value(descriptor, value, text, sizeof(text), true)) {
            widget_stat_row_set_value(&s_screen.params[i].row, text, HMI_COLOR_NEUTRAL);
        } else {
            widget_stat_row_set_value(&s_screen.params[i].row, "--", HMI_COLOR_DIM);
        }
    }

    widget_stat_row_set_value(&s_screen.homing, homing_text(state),
                              state->machine_state_known && state->machine_state == HMI_MACHINE_READY ?
                              HMI_COLOR_GREEN : HMI_COLOR_AMBER);
    widget_stat_row_set_value(
        &s_screen.carriage,
        state->carriage_reference_position_known ?
            hmi_carriage_reference_position_text(state->carriage_reference_position) : "--",
        state->carriage_reference_position_known &&
            state->carriage_reference_position == HMI_CARRIAGE_POSITION_ZERO ?
                HMI_COLOR_GREEN :
                (state->carriage_reference_position_known ? HMI_COLOR_AMBER : HMI_COLOR_DIM));
    widget_stat_row_set_value(&s_screen.safety, state->safety_ok ? "OK" : "FAULT",
                              state->safety_ok ? HMI_COLOR_GREEN : HMI_COLOR_RED);

    lv_label_set_text(s_screen.message_label,
                      start_pending ? hmi_pending_command_get_message() :
                      (can_start(state) ?
                        "Review parameters, then press START WINDING." :
                        start_block_reason(state)));
    lv_obj_set_style_text_color(s_screen.message_label,
                                can_start(state) && !start_pending ? hmi_palette_get()->green : hmi_palette_get()->amber,
                                0);

    bool ready = can_start(state);
    set_button_dimmed(s_screen.start_button, !ready);
    if (s_screen.start_label != NULL) {
        lv_label_set_text(s_screen.start_label, start_pending ? "STARTING..." : "START WINDING");
        lv_obj_center(s_screen.start_label);
    }
}
