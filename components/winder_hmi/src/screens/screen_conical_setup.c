#include "screen_conical_setup.h"

#include <stdio.h>

#include "esp_log.h"
#include "hmi_actions.h"
#include "hmi_capability_model.h"
#include "hmi_job_draft_model.h"
#include "hmi_model.h"
#include "hmi_navigation.h"
#include "hmi_param.h"
#include "hmi_styles.h"
#include "hmi_types.h"
#include "modal_numeric_keypad.h"
#include "widget_stat_row.h"
#include "widget_status_badge.h"

#define CONICAL_SETUP_MAX_FIELDS 8

typedef struct {
    const hmi_param_descriptor_t *descriptor;
    lv_obj_t *value_label;
} conical_field_t;

typedef struct {
    lv_obj_t *root;
    hmi_status_badge_t badge;
    conical_field_t fields[CONICAL_SETUP_MAX_FIELDS];
    hmi_stat_row_t mode;
    hmi_stat_row_t homing;
    hmi_stat_row_t job;
    hmi_stat_row_t layers;
    hmi_stat_row_t time;
    hmi_stat_row_t offset;
    lv_obj_t *validation_label;
} conical_setup_screen_t;

static conical_setup_screen_t s_screen;
static const char *TAG = "conical_setup";

static void back_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_open_jobs();
}

static void start_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_open_confirm_start();
}

static bool machine_is_homing(hmi_machine_state_t state)
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

static const char *homing_text(const hmi_state_t *state)
{
    if (state == NULL || !state->machine_state_known) {
        return "UNKNOWN";
    }
    if (state->machine_state == HMI_MACHINE_HOMING_REQUIRED) {
        return "REQUIRED";
    }
    if (machine_is_homing(state->machine_state)) {
        return "IN PROGRESS";
    }
    return state->machine_state == HMI_MACHINE_ALARM ? "UNKNOWN" : "OK";
}

static hmi_color_role_t homing_color(const hmi_state_t *state)
{
    if (state != NULL && state->machine_state_known &&
        state->machine_state != HMI_MACHINE_HOMING_REQUIRED &&
        !machine_is_homing(state->machine_state) &&
        state->machine_state != HMI_MACHINE_ALARM) {
        return HMI_COLOR_GREEN;
    }
    return state != NULL && state->machine_state_known ? HMI_COLOR_AMBER : HMI_COLOR_DIM;
}

static void apply_param_value(const hmi_param_descriptor_t *descriptor, hmi_param_value_t value)
{
    if (descriptor == NULL) {
        return;
    }

    hmi_actions_update_job_param(descriptor->id, value);
    screen_conical_setup_update(hmi_model_get_state());
}

static void adjust_field(conical_field_t *field, int32_t dir)
{
    if (field == NULL || field->descriptor == NULL) {
        return;
    }

    const hmi_param_descriptor_t *descriptor = field->descriptor;
    if (!hmi_job_draft_model_step_value(descriptor->id, dir)) {
        return;
    }

    hmi_param_value_t value = {0};
    if (hmi_job_draft_model_get_value(descriptor->id, &value)) {
        hmi_actions_update_job_param(descriptor->id, value);
        screen_conical_setup_update(hmi_model_get_state());
    }
}

static void field_decrement_event_cb(lv_event_t *event)
{
    adjust_field((conical_field_t *)lv_event_get_user_data(event), -1);
}

static void field_increment_event_cb(lv_event_t *event)
{
    adjust_field((conical_field_t *)lv_event_get_user_data(event), 1);
}

static void keypad_apply_cb(float value, uint32_t u32_value, void *user_ctx)
{
    const hmi_param_descriptor_t *descriptor = (const hmi_param_descriptor_t *)user_ctx;
    hmi_param_value_t param_value = {0};
    if (descriptor == NULL) {
        return;
    }

    if (!hmi_param_from_keypad_float(descriptor, value, u32_value, &param_value)) {
        return;
    }

    apply_param_value(descriptor, param_value);
}

static void open_keypad_event_cb(lv_event_t *event)
{
    conical_field_t *field = (conical_field_t *)lv_event_get_user_data(event);
    if (field == NULL || field->descriptor == NULL) {
        return;
    }
    if (hmi_navigation_current() != HMI_SCREEN_CONICAL_SETUP) {
        return;
    }
    if (s_screen.root == NULL || !lv_obj_is_valid(s_screen.root)) {
        return;
    }

    const hmi_param_descriptor_t *descriptor = field->descriptor;
    hmi_param_value_t current = {0};
    if (!hmi_job_draft_model_get_value(descriptor->id, &current)) {
        return;
    }
    ESP_LOGI(TAG, "Opening keypad for param %u", (unsigned)descriptor->id);

    modal_numeric_keypad_config_t config = {
        .title = descriptor->label,
        .unit = descriptor->unit,
        .initial_value = hmi_param_value_as_float(descriptor, current),
        .min_value = hmi_param_value_as_float(descriptor, descriptor->min_value),
        .max_value = hmi_param_value_as_float(descriptor, descriptor->max_value),
        .decimals = descriptor->decimals,
        .integer_only = hmi_param_type_uses_integer_keypad(descriptor->type),
        .apply_cb = keypad_apply_cb,
        .user_ctx = (void *)descriptor,
    };

    modal_numeric_keypad_open(NULL, &config);
}

static lv_obj_t *create_small_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, conical_field_t *field)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->nav_button, 0);
    lv_obj_set_size(button, 38, 36);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, field);

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
    lv_label_set_text(title, "CONICAL SETUP");

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
    lv_obj_set_style_pad_row(panel, 7, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &styles->panel_title, 0);
    lv_label_set_text(title, title_text);

    return panel;
}

static void create_field(lv_obj_t *parent, conical_field_t *field, const hmi_param_descriptor_t *descriptor)
{
    hmi_styles_t *styles = hmi_styles_get();

    field->descriptor = descriptor;

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), 47);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(row);
    lv_obj_add_style(label, &styles->stat_label, 0);
    lv_obj_set_width(label, 126);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_label_set_text(label, descriptor->label);

    lv_obj_t *decrement = create_small_button(row, "-", field_decrement_event_cb, field);

    lv_obj_t *value_button = lv_btn_create(row);
    lv_obj_remove_style_all(value_button);
    lv_obj_add_style(value_button, &styles->nav_button, 0);
    lv_obj_set_size(value_button, 124, 38);
    lv_obj_set_style_bg_color(value_button, hmi_palette_get()->panel_secondary, 0);
    lv_obj_set_style_bg_color(value_button, hmi_palette_get()->border_strong, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(value_button, hmi_palette_get()->text, 0);
    lv_obj_add_flag(value_button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(value_button, open_keypad_event_cb, LV_EVENT_CLICKED, field);

    field->value_label = lv_label_create(value_button);
    lv_label_set_text(field->value_label, "--");
    lv_obj_center(field->value_label);

    lv_obj_t *increment = create_small_button(row, "+", field_increment_event_cb, field);

    if (!descriptor->editable) {
        lv_obj_add_state(decrement, LV_STATE_DISABLED);
        lv_obj_add_state(value_button, LV_STATE_DISABLED);
        lv_obj_add_state(increment, LV_STATE_DISABLED);
        lv_obj_set_style_opa(decrement, LV_OPA_50, 0);
        lv_obj_set_style_opa(value_button, LV_OPA_50, 0);
        lv_obj_set_style_opa(increment, LV_OPA_50, 0);
    }
}

static lv_obj_t *create_bottom_button(lv_obj_t *parent, const char *text, hmi_color_role_t role, lv_event_cb_t cb)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->primary_button, 0);
    lv_obj_set_size(button, 230, 52);
    lv_obj_set_style_bg_color(button, hmi_color_for_role(role), 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, role == HMI_COLOR_AMBER ? hmi_palette_get()->device : lv_color_white(), 0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return button;
}

static void format_param_value(const hmi_param_descriptor_t *descriptor, char *buffer, size_t buffer_size)
{
    hmi_param_value_t value = {0};
    if (!hmi_job_draft_model_get_value(descriptor->id, &value) ||
        !hmi_param_format_value(descriptor, value, buffer, buffer_size, true)) {
        snprintf(buffer, buffer_size, "--");
    }
}

void screen_conical_setup_create(lv_obj_t *root)
{
    hmi_styles_t *styles = hmi_styles_get();
    s_screen = (conical_setup_screen_t){0};
    s_screen.root = root;

    create_topbar(root);

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), HMI_DISPLAY_HEIGHT - HMI_TOPBAR_HEIGHT - 76);
    lv_obj_set_style_pad_all(content, 14, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(content, 12, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *left = create_panel(content, 382, "PARAMETERS");
    const hmi_mode_capability_t *mode =
        hmi_capability_model_get_mode_by_id(hmi_job_draft_model_get_mode());
    size_t field_index = 0;
    if (mode != NULL) {
        for (size_t i = 0; i < mode->param_count && field_index < (sizeof(s_screen.fields) / sizeof(s_screen.fields[0])); i++) {
            if (mode->params[i].visible) {
                create_field(left, &s_screen.fields[field_index], &mode->params[i]);
                field_index++;
            }
        }
    }

    lv_obj_t *right = create_panel(content, 378, "READINESS");
    widget_stat_row_create(right, &s_screen.mode, "Mode");
    widget_stat_row_create(right, &s_screen.homing, "Homing");
    widget_stat_row_create(right, &s_screen.job, "Job draft");
    widget_stat_row_create(right, &s_screen.layers, "Est layers");
    widget_stat_row_create(right, &s_screen.time, "Est time");
    widget_stat_row_create(right, &s_screen.offset, "Right shift");

    s_screen.validation_label = lv_label_create(right);
    lv_obj_add_style(s_screen.validation_label, &styles->topbar_text, 0);
    lv_label_set_long_mode(s_screen.validation_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_screen.validation_label, LV_PCT(100));
    lv_label_set_text(s_screen.validation_label, "--");

    lv_obj_t *buttons = lv_obj_create(root);
    lv_obj_remove_style_all(buttons);
    lv_obj_set_size(buttons, LV_PCT(100), 76);
    lv_obj_set_flex_flow(buttons, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(buttons, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(buttons, 12, 0);
    lv_obj_clear_flag(buttons, LV_OBJ_FLAG_SCROLLABLE);

    create_bottom_button(buttons, "BACK", HMI_COLOR_DIM, back_event_cb);
    create_bottom_button(buttons, "REVIEW", HMI_COLOR_GREEN, start_event_cb);
}

void screen_conical_setup_update(const hmi_state_t *state)
{
    if (state == NULL || s_screen.root == NULL) {
        return;
    }

    char text[64];
    const hmi_job_validation_t *validation = hmi_job_draft_model_get_validation();
    const hmi_mode_capability_t *mode =
        hmi_capability_model_get_mode_by_id(hmi_job_draft_model_get_mode());

    widget_status_badge_update(&s_screen.badge, state);

    for (size_t i = 0; i < sizeof(s_screen.fields) / sizeof(s_screen.fields[0]); i++) {
        if (s_screen.fields[i].descriptor != NULL && s_screen.fields[i].value_label != NULL) {
            format_param_value(s_screen.fields[i].descriptor, text, sizeof(text));
            lv_label_set_text(s_screen.fields[i].value_label, text);
        }
    }

    widget_stat_row_set_value(&s_screen.mode,
                              mode != NULL && mode->title != NULL ? mode->title : "--",
                              HMI_COLOR_BLUE);
    widget_stat_row_set_value(&s_screen.homing, homing_text(state), homing_color(state));
    widget_stat_row_set_value(&s_screen.job,
                              mode != NULL ? "CONFIGURED" : "NOT CONFIGURED",
                              mode != NULL ? HMI_COLOR_GREEN : HMI_COLOR_AMBER);

    if (validation->estimated_layers > 0) {
        snprintf(text, sizeof(text), "%lu", (unsigned long)validation->estimated_layers);
        widget_stat_row_set_value(&s_screen.layers, text, HMI_COLOR_NEUTRAL);
    } else {
        widget_stat_row_set_value(&s_screen.layers, "--", HMI_COLOR_DIM);
    }

    if (validation->estimated_time_min > 0.0f) {
        snprintf(text, sizeof(text), "%.1f min", (double)validation->estimated_time_min);
        widget_stat_row_set_value(&s_screen.time, text, HMI_COLOR_NEUTRAL);
    } else {
        widget_stat_row_set_value(&s_screen.time, "--", HMI_COLOR_DIM);
    }

    if (validation->estimated_offset_mm > 0.0f) {
        snprintf(text, sizeof(text), "%.1f mm", (double)validation->estimated_offset_mm);
        widget_stat_row_set_value(&s_screen.offset, text, HMI_COLOR_NEUTRAL);
    } else {
        widget_stat_row_set_value(&s_screen.offset, "--", HMI_COLOR_DIM);
    }

    lv_label_set_text(s_screen.validation_label,
                      "Review parameters, then continue to start confirmation.");
    lv_obj_set_style_text_color(s_screen.validation_label, hmi_palette_get()->text_dim, 0);
}
