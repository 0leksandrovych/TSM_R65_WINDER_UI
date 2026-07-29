#include "modal_paused_job_edit.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hmi_actions.h"
#include "hmi_model.h"
#include "hmi_navigation.h"
#include "hmi_param.h"
#include "hmi_paused_job_draft_model.h"
#include "hmi_pending_command.h"
#include "hmi_styles.h"
#include "hmi_types.h"
#include "modal_confirm.h"
#include "modal_numeric_keypad.h"

#define EDIT_FIELD_COUNT HMI_PAUSED_JOB_EDIT_PARAM_COUNT

typedef struct {
    const hmi_param_descriptor_t *descriptor;
    lv_obj_t *button;
    lv_obj_t *value_label;
} edit_field_t;

typedef enum {
    PREVIEW_TARGET_UNAVAILABLE,
    PREVIEW_TARGET_DRAFT,
    PREVIEW_TARGET_UNCHANGED,
} preview_target_mode_t;

typedef enum {
    MODAL_STATUS_IDLE,
    MODAL_STATUS_DIRTY,
    MODAL_STATUS_UNINITIALIZED,
    MODAL_STATUS_CONFIRMING,
    MODAL_STATUS_APPLYING,
} modal_status_mode_t;

typedef enum {
    PRESENTATION_COLOR_TEXT,
    PRESENTATION_COLOR_DIM,
    PRESENTATION_COLOR_AMBER,
    PRESENTATION_COLOR_BLUE,
    PRESENTATION_COLOR_RED,
} presentation_color_t;

typedef struct {
    bool wound_known;
    float wound_length_m;
    bool target_known;
    float target_length_m;
    bool pause_reason_known;
    hmi_job_pause_reason_t pause_reason;
    bool field_known[EDIT_FIELD_COUNT];
    hmi_param_value_t field_values[EDIT_FIELD_COUNT];
    bool has_additional;
    float additional_length_m;
    preview_target_mode_t preview_target_mode;
    float preview_target_m;
    bool additional_invalid;
    bool fields_dimmed;
    bool additional_dimmed;
    bool clear_dimmed;
    bool close_dimmed;
    bool cancel_dimmed;
    bool apply_dimmed;
    modal_status_mode_t status_mode;
} presentation_snapshot_t;

typedef struct {
    bool initialized;
    presentation_snapshot_t snapshot;
    char wound_text[32];
    char target_text[32];
    char remaining_title[16];
    char remaining_text[32];
    char pause_reason_text[32];
    char field_text[EDIT_FIELD_COUNT][64];
    char additional_text[32];
    char preview_wound_text[40];
    char preview_add_text[40];
    char preview_target_text[64];
    char status_text[64];
    char apply_text[32];
    char error_text[HMI_TEXT_MESSAGE_MAX];
    presentation_color_t remaining_color;
    presentation_color_t pause_reason_color;
    presentation_color_t status_color;
} presentation_cache_t;

typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *close_button;
    lv_obj_t *wound_value;
    lv_obj_t *target_value;
    lv_obj_t *remaining_title;
    lv_obj_t *remaining_value;
    lv_obj_t *pause_reason_value;
    edit_field_t fields[EDIT_FIELD_COUNT];
    lv_obj_t *additional_card;
    lv_obj_t *additional_button;
    lv_obj_t *additional_value;
    lv_obj_t *draft_badge;
    lv_obj_t *clear_button;
    lv_obj_t *preview_wound;
    lv_obj_t *preview_add;
    lv_obj_t *preview_target;
    lv_obj_t *error_label;
    lv_obj_t *cancel_button;
    lv_obj_t *apply_button;
    lv_obj_t *apply_label;
    lv_obj_t *status_label;
    bool apply_requested;
    bool additional_invalid;
    uint32_t apply_confirmation_revision;
    char local_error[HMI_TEXT_MESSAGE_MAX];
    bool render_suppressed;
    bool force_refresh;
    presentation_cache_t render_cache;
} paused_job_edit_modal_t;

static paused_job_edit_modal_t s_modal;

bool modal_paused_job_edit_is_open(void)
{
    return s_modal.overlay != NULL && lv_obj_is_valid(s_modal.overlay);
}

static void reset_modal_state(void)
{
    s_modal = (paused_job_edit_modal_t){0};
}

static void set_button_dimmed(lv_obj_t *button, bool dimmed)
{
    if (button == NULL) {
        return;
    }

    if (dimmed) {
        lv_obj_clear_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_state(button, LV_STATE_DISABLED);
        lv_obj_set_style_opa(button, LV_OPA_50, 0);
    } else {
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_state(button, LV_STATE_DISABLED);
        lv_obj_set_style_opa(button, LV_OPA_COVER, 0);
    }
}

static bool float_value_equal(float left, float right)
{
    return memcmp(&left, &right, sizeof(left)) == 0;
}

static bool known_float_changed(bool force,
                                bool known,
                                float value,
                                bool cached_known,
                                float cached_value)
{
    return force || known != cached_known ||
           (known && !float_value_equal(value, cached_value));
}

static void format_length_m(char *buffer, size_t buffer_size, float value_m)
{
    const float rounded_m = roundf(value_m);
    if (isfinite(value_m) && fabsf(value_m - rounded_m) < 0.0005f) {
        snprintf(buffer, buffer_size, "%.0f m", (double)rounded_m);
    } else {
        snprintf(buffer, buffer_size, "%.3f m", (double)value_m);
    }
}

static bool param_value_equal(const hmi_param_descriptor_t *descriptor,
                              hmi_param_value_t left,
                              hmi_param_value_t right)
{
    if (descriptor == NULL) {
        return false;
    }

    switch (descriptor->type) {
    case HMI_PARAM_TYPE_FLOAT:
        return float_value_equal(left.f32, right.f32);
    case HMI_PARAM_TYPE_UINT32:
        return left.u32 == right.u32;
    case HMI_PARAM_TYPE_BOOL:
        return left.boolean == right.boolean;
    case HMI_PARAM_TYPE_ENUM:
        return left.enum_value == right.enum_value;
    default:
        return false;
    }
}

static void set_label_text_cached(lv_obj_t *label,
                                  char *cached_text,
                                  size_t cached_text_size,
                                  const char *text,
                                  bool force)
{
    const char *desired = text != NULL ? text : "";
    if (label == NULL || cached_text == NULL || cached_text_size == 0U ||
        (!force && strcmp(cached_text, desired) == 0)) {
        return;
    }

    lv_label_set_text(label, desired);
    snprintf(cached_text, cached_text_size, "%s", desired);
}

static lv_color_t color_for_presentation(presentation_color_t color)
{
    switch (color) {
    case PRESENTATION_COLOR_DIM:
        return hmi_palette_get()->text_dim;
    case PRESENTATION_COLOR_AMBER:
        return hmi_palette_get()->amber;
    case PRESENTATION_COLOR_BLUE:
        return hmi_palette_get()->blue;
    case PRESENTATION_COLOR_RED:
        return hmi_palette_get()->red;
    case PRESENTATION_COLOR_TEXT:
    default:
        return hmi_palette_get()->text;
    }
}

static void set_text_color_cached(lv_obj_t *object,
                                  presentation_color_t color,
                                  presentation_color_t *cached_color,
                                  bool force)
{
    if (object == NULL || cached_color == NULL ||
        (!force && *cached_color == color)) {
        return;
    }

    lv_obj_set_style_text_color(object, color_for_presentation(color), 0);
    *cached_color = color;
}

static void set_button_dimmed_cached(lv_obj_t *button,
                                     bool dimmed,
                                     bool cached_dimmed,
                                     bool force)
{
    if (force || dimmed != cached_dimmed) {
        set_button_dimmed(button, dimmed);
    }
}

static void set_hidden_cached(lv_obj_t *object,
                              bool hidden,
                              bool cached_hidden,
                              bool force)
{
    if (object == NULL || (!force && hidden == cached_hidden)) {
        return;
    }

    if (hidden) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

void modal_paused_job_edit_close(void)
{
    lv_obj_t *overlay = s_modal.overlay;

    modal_numeric_keypad_close();
    modal_confirm_close();
    reset_modal_state();

    if (overlay != NULL && lv_obj_is_valid(overlay)) {
        lv_obj_del_async(overlay);
    }
}

static void close_and_refresh_run(void)
{
    modal_paused_job_edit_close();
    if (hmi_navigation_current() == HMI_SCREEN_RUN) {
        hmi_navigation_update(hmi_model_get_state());
    }
}

static void overlay_delete_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == s_modal.overlay) {
        reset_modal_state();
    }
}

static void discard_and_close_cb(void *user_ctx)
{
    (void)user_ctx;
    (void)hmi_paused_job_draft_model_begin_edit(hmi_model_get_state());
    close_and_refresh_run();
}

static void request_close(void)
{
    if (!modal_paused_job_edit_is_open()) {
        return;
    }

    if (hmi_pending_command_is_active() ||
        hmi_paused_job_draft_model_update_awaiting_confirmation()) {
        snprintf(s_modal.local_error,
                 sizeof(s_modal.local_error),
                 "Wait for the controller update to finish");
        modal_paused_job_edit_update(hmi_model_get_state());
        return;
    }

    if (!hmi_paused_job_draft_model_is_dirty()) {
        close_and_refresh_run();
        return;
    }

    const modal_confirm_config_t config = {
        .title = "Discard paused job changes?",
        .body = "Your edited parameters and additional length have not been applied.",
        .cancel_text = "KEEP EDITING",
        .confirm_text = "DISCARD",
        .confirm_role = HMI_COLOR_AMBER,
        .confirm_cb = discard_and_close_cb,
    };
    modal_confirm_open(&config);
}

static void close_event_cb(lv_event_t *event)
{
    (void)event;
    request_close();
}

static lv_obj_t *create_plain_container(lv_obj_t *parent)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

static lv_obj_t *create_action_button(lv_obj_t *parent,
                                      const char *text,
                                      int32_t width,
                                      hmi_color_role_t role,
                                      lv_event_cb_t callback)
{
    hmi_styles_t *styles = hmi_styles_get();
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->primary_button, 0);
    lv_obj_set_size(button, width, 50);
    lv_obj_set_style_bg_color(button, hmi_color_for_role(role), 0);
    lv_obj_set_style_bg_color(button,
                              hmi_palette_get()->panel_secondary,
                              LV_STATE_PRESSED);
    lv_obj_set_style_text_color(
        button,
        role == HMI_COLOR_AMBER ? hmi_palette_get()->device : lv_color_white(),
        0);
    if (callback != NULL) {
        lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

static lv_obj_t *create_status_cell(lv_obj_t *parent,
                                    int32_t width,
                                    const char *title,
                                    lv_obj_t **title_out,
                                    lv_obj_t **value_out)
{
    hmi_styles_t *styles = hmi_styles_get();
    lv_obj_t *cell = create_plain_container(parent);
    lv_obj_set_size(cell, width, 50);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cell, 3, 0);

    lv_obj_t *title_label = lv_label_create(cell);
    lv_obj_add_style(title_label, &styles->panel_title, 0);
    lv_label_set_text(title_label, title);

    lv_obj_t *value_label = lv_label_create(cell);
    lv_obj_add_style(value_label, &styles->status_text, 0);
    lv_obj_set_style_text_color(value_label, hmi_palette_get()->text, 0);
    lv_label_set_long_mode(value_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(value_label, LV_PCT(100));
    lv_label_set_text(value_label, "--");

    if (title_out != NULL) {
        *title_out = title_label;
    }
    if (value_out != NULL) {
        *value_out = value_label;
    }
    return cell;
}

static void clear_local_feedback(void)
{
    s_modal.local_error[0] = '\0';
    s_modal.additional_invalid = false;
    s_modal.apply_requested = false;
}

static void field_keypad_apply_cb(float value, uint32_t u32_value, void *user_ctx)
{
    edit_field_t *field = (edit_field_t *)user_ctx;
    hmi_param_value_t param_value = {0};
    if (field == NULL || field->descriptor == NULL ||
        !hmi_param_from_keypad_float(field->descriptor,
                                     value,
                                     u32_value,
                                     &param_value) ||
        !hmi_actions_update_paused_job_param(field->descriptor->id, param_value)) {
        return;
    }

    clear_local_feedback();
    modal_paused_job_edit_update(hmi_model_get_state());
}

static void field_event_cb(lv_event_t *event)
{
    edit_field_t *field = (edit_field_t *)lv_event_get_user_data(event);
    hmi_param_value_t current = {0};
    if (field == NULL || field->descriptor == NULL ||
        hmi_pending_command_is_active() ||
        hmi_paused_job_draft_model_update_awaiting_confirmation() ||
        !hmi_paused_job_draft_model_get_value(field->descriptor->id, &current)) {
        return;
    }

    const hmi_param_descriptor_t *descriptor = field->descriptor;
    const modal_numeric_keypad_config_t config = {
        .title = descriptor->label,
        .unit = descriptor->unit,
        .initial_value = hmi_param_value_as_float(descriptor, current),
        .min_value = hmi_param_value_as_float(descriptor, descriptor->min_value),
        .max_value = hmi_param_value_as_float(descriptor, descriptor->max_value),
        .decimals = descriptor->decimals,
        .integer_only = hmi_param_type_uses_integer_keypad(descriptor->type),
        .apply_cb = field_keypad_apply_cb,
        .user_ctx = field,
    };
    modal_numeric_keypad_open(NULL, &config);
}

static void additional_keypad_apply_cb(float value,
                                       uint32_t u32_value,
                                       void *user_ctx)
{
    (void)value;
    (void)user_ctx;
    if (!hmi_actions_set_additional_length((float)u32_value)) {
        return;
    }

    clear_local_feedback();
    modal_paused_job_edit_update(hmi_model_get_state());
}

static void additional_event_cb(lv_event_t *event)
{
    (void)event;
    if (hmi_pending_command_is_active() ||
        hmi_paused_job_draft_model_update_awaiting_confirmation()) {
        return;
    }

    const float initial = hmi_paused_job_draft_model_has_additional_length()
                              ? hmi_paused_job_draft_model_get_additional_length_m()
                              : 1.0f;
    const modal_numeric_keypad_config_t config = {
        .title = "Additional Length",
        .unit = "m",
        .initial_value = initial,
        .min_value = 1.0f,
        .max_value = 2147483.0f,
        .decimals = 0,
        .integer_only = true,
        .apply_cb = additional_keypad_apply_cb,
    };
    modal_numeric_keypad_open(NULL, &config);
}

static void clear_additional_event_cb(lv_event_t *event)
{
    (void)event;
    if (hmi_pending_command_is_active() ||
        hmi_paused_job_draft_model_update_awaiting_confirmation()) {
        return;
    }

    hmi_actions_clear_additional_length();
    clear_local_feedback();
    modal_paused_job_edit_update(hmi_model_get_state());
}

static void apply_event_cb(lv_event_t *event)
{
    (void)event;
    if (hmi_pending_command_is_active() ||
        hmi_paused_job_draft_model_update_awaiting_confirmation()) {
        return;
    }

    char validation_message[HMI_TEXT_MESSAGE_MAX];
    if (!hmi_paused_job_draft_model_validate(validation_message,
                                              sizeof(validation_message))) {
        snprintf(s_modal.local_error,
                 sizeof(s_modal.local_error),
                 "%s",
                 validation_message[0] != '\0'
                     ? validation_message
                     : "Paused job parameters are invalid");
        s_modal.additional_invalid = strstr(s_modal.local_error, "Additional") != NULL;
        modal_paused_job_edit_update(hmi_model_get_state());
        return;
    }

    if (!hmi_actions_apply_paused_job_changes()) {
        const hmi_state_t *state = hmi_model_get_state();
        snprintf(s_modal.local_error,
                 sizeof(s_modal.local_error),
                 "%s",
                 state != NULL && state->last_error != NULL &&
                         state->last_error[0] != '\0'
                     ? state->last_error
                     : "Unable to send paused job update");
        s_modal.additional_invalid = strstr(s_modal.local_error, "Additional") != NULL;
        modal_paused_job_edit_update(state);
        return;
    }

    s_modal.apply_requested = true;
    s_modal.apply_confirmation_revision =
        hmi_paused_job_draft_model_confirmation_revision();
    s_modal.local_error[0] = '\0';
    s_modal.additional_invalid = false;
    modal_paused_job_edit_update(hmi_model_get_state());
}

static void create_header(lv_obj_t *parent)
{
    hmi_styles_t *styles = hmi_styles_get();
    lv_obj_t *header = create_plain_container(parent);
    lv_obj_set_size(header, LV_PCT(100), 58);
    lv_obj_set_style_bg_color(header, hmi_palette_get()->device, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(header, hmi_palette_get()->border, 0);
    lv_obj_set_style_border_width(header, 2, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_left(header, 16, 0);
    lv_obj_set_style_pad_right(header, 12, 0);
    lv_obj_set_style_pad_column(header, 12, 0);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(header);
    lv_obj_add_style(title, &styles->topbar_title, 0);
    lv_label_set_text(title, "EDIT PAUSED JOB");

    lv_obj_t *spacer = create_plain_container(header);
    lv_obj_set_size(spacer, 0, 1);
    lv_obj_set_flex_grow(spacer, 1);

    lv_obj_t *badge = lv_label_create(header);
    lv_obj_add_style(badge, &styles->badge_base, 0);
    lv_obj_add_style(badge, &styles->badge_amber, 0);
    lv_label_set_text(badge, "PAUSED");

    s_modal.close_button = lv_btn_create(header);
    lv_obj_remove_style_all(s_modal.close_button);
    lv_obj_add_style(s_modal.close_button, &styles->nav_button, 0);
    lv_obj_set_size(s_modal.close_button, 54, 44);
    lv_obj_set_style_border_color(s_modal.close_button, hmi_palette_get()->border_strong, 0);
    lv_obj_set_style_bg_color(s_modal.close_button,
                              hmi_palette_get()->panel_secondary,
                              LV_STATE_PRESSED);
    lv_obj_set_style_text_color(s_modal.close_button, hmi_palette_get()->text, 0);
    lv_obj_add_event_cb(s_modal.close_button, close_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_label = lv_label_create(s_modal.close_button);
    lv_obj_add_style(close_label, &styles->topbar_title, 0);
    lv_obj_set_style_text_color(close_label, hmi_palette_get()->text, 0);
    lv_label_set_text(close_label, "X");
    lv_obj_center(close_label);
}

static void create_status_strip(lv_obj_t *parent)
{
    lv_obj_t *strip = create_plain_container(parent);
    lv_obj_set_size(strip, LV_PCT(100), 62);
    lv_obj_set_style_bg_color(strip, hmi_palette_get()->panel_secondary, 0);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(strip, hmi_palette_get()->border, 0);
    lv_obj_set_style_border_width(strip, 1, 0);
    lv_obj_set_style_border_side(strip, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_left(strip, 12, 0);
    lv_obj_set_style_pad_right(strip, 12, 0);
    lv_obj_set_style_pad_top(strip, 6, 0);
    lv_obj_set_style_pad_column(strip, 6, 0);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);

    create_status_cell(strip, 140, "WOUND", NULL, &s_modal.wound_value);
    create_status_cell(strip, 140, "TARGET", NULL, &s_modal.target_value);
    create_status_cell(strip,
                       140,
                       "REMAINING",
                       &s_modal.remaining_title,
                       &s_modal.remaining_value);
    create_status_cell(strip,
                       338,
                       "PAUSE REASON",
                       NULL,
                       &s_modal.pause_reason_value);
    lv_obj_set_style_text_color(s_modal.pause_reason_value,
                                hmi_palette_get()->amber,
                                0);
}

static void create_edit_field(lv_obj_t *parent,
                              edit_field_t *field,
                              const char *title)
{
    hmi_styles_t *styles = hmi_styles_get();
    field->button = lv_btn_create(parent);
    lv_obj_remove_style_all(field->button);
    lv_obj_add_style(field->button, &styles->panel_secondary, 0);
    lv_obj_set_size(field->button, LV_PCT(100), 61);
    lv_obj_set_style_pad_all(field->button, 10, 0);
    lv_obj_set_flex_flow(field->button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(field->button,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(field->button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(field->button,
                              hmi_palette_get()->border_strong,
                              LV_STATE_PRESSED);
    lv_obj_add_event_cb(field->button, field_event_cb, LV_EVENT_CLICKED, field);

    lv_obj_t *label = lv_label_create(field->button);
    lv_obj_add_style(label, &styles->panel_title, 0);
    /* Let the title consume the space that remains after the right-aligned
     * value column. This removes the obsolete EDIT-slot gap while keeping
     * every value on the same right edge. */
    lv_obj_set_width(label, 0);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(label, title);

    field->value_label = lv_label_create(field->button);
    lv_obj_add_style(field->value_label, &styles->status_text, 0);
    lv_obj_set_width(field->value_label, 190);
    lv_obj_set_style_text_align(field->value_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(field->value_label, hmi_palette_get()->blue, 0);
    lv_obj_clear_flag(field->value_label, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_long_mode(field->value_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(field->value_label, "--");
}

static void create_editor_body(lv_obj_t *parent)
{
    hmi_styles_t *styles = hmi_styles_get();
    lv_obj_t *body = create_plain_container(parent);
    lv_obj_set_size(body, LV_PCT(100), 288);
    lv_obj_set_style_pad_all(body, 12, 0);
    lv_obj_set_style_pad_column(body, 12, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);

    lv_obj_t *fields = create_plain_container(body);
    lv_obj_set_size(fields, 456, 264);
    lv_obj_set_flex_flow(fields, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(fields, 6, 0);

    static const char *const field_titles[EDIT_FIELD_COUNT] = {
        "MASTER SPEED",
        "WINDING PITCH",
        "LAYERS BEFORE OFFSET",
        "RIGHT EDGE OFFSET",
    };
    for (size_t i = 0; i < EDIT_FIELD_COUNT; i++) {
        s_modal.fields[i].descriptor =
            hmi_paused_job_draft_model_get_descriptor(i);
        create_edit_field(fields,
                          &s_modal.fields[i],
                          field_titles[i]);
    }

    lv_obj_t *right = create_plain_container(body);
    lv_obj_set_size(right, 308, 264);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right, 8, 0);

    s_modal.additional_card = lv_obj_create(right);
    lv_obj_remove_style_all(s_modal.additional_card);
    lv_obj_add_style(s_modal.additional_card, &styles->panel, 0);
    lv_obj_set_size(s_modal.additional_card, LV_PCT(100), 150);
    lv_obj_set_style_pad_all(s_modal.additional_card, 10, 0);
    lv_obj_set_style_pad_row(s_modal.additional_card, 6, 0);
    lv_obj_set_flex_flow(s_modal.additional_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s_modal.additional_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *additional_header = create_plain_container(s_modal.additional_card);
    lv_obj_set_size(additional_header, LV_PCT(100), 20);
    lv_obj_set_flex_flow(additional_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(additional_header,
                          LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *additional_title = lv_label_create(additional_header);
    lv_obj_add_style(additional_title, &styles->panel_title, 0);
    lv_label_set_text(additional_title, "ADDITIONAL LENGTH");

    s_modal.draft_badge = lv_label_create(additional_header);
    lv_obj_add_style(s_modal.draft_badge, &styles->panel_title, 0);
    lv_obj_set_style_text_color(s_modal.draft_badge, hmi_palette_get()->blue, 0);
    lv_label_set_text(s_modal.draft_badge, "DRAFT");
    lv_obj_add_flag(s_modal.draft_badge, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *additional_row = create_plain_container(s_modal.additional_card);
    lv_obj_set_size(additional_row, LV_PCT(100), 48);
    lv_obj_set_flex_flow(additional_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(additional_row, 8, 0);

    s_modal.additional_button = lv_btn_create(additional_row);
    lv_obj_remove_style_all(s_modal.additional_button);
    lv_obj_add_style(s_modal.additional_button, &styles->nav_button, 0);
    lv_obj_set_size(s_modal.additional_button, 190, 48);
    lv_obj_set_style_border_color(s_modal.additional_button,
                                  hmi_palette_get()->blue,
                                  0);
    lv_obj_set_style_bg_color(s_modal.additional_button,
                              hmi_palette_get()->panel_secondary,
                              0);
    lv_obj_set_style_bg_color(s_modal.additional_button,
                              hmi_palette_get()->border_strong,
                              LV_STATE_PRESSED);
    lv_obj_add_event_cb(s_modal.additional_button,
                        additional_event_cb,
                        LV_EVENT_CLICKED,
                        NULL);

    s_modal.additional_value = lv_label_create(s_modal.additional_button);
    lv_obj_add_style(s_modal.additional_value, &styles->status_text, 0);
    lv_obj_set_style_text_color(s_modal.additional_value,
                                hmi_palette_get()->blue,
                                0);
    lv_label_set_text(s_modal.additional_value, "NOT SET");
    lv_obj_center(s_modal.additional_value);

    s_modal.clear_button = lv_btn_create(additional_row);
    lv_obj_remove_style_all(s_modal.clear_button);
    lv_obj_add_style(s_modal.clear_button, &styles->nav_button, 0);
    lv_obj_set_size(s_modal.clear_button, 82, 48);
    lv_obj_set_style_bg_color(s_modal.clear_button,
                              hmi_palette_get()->panel_secondary,
                              LV_STATE_PRESSED);
    lv_obj_add_event_cb(s_modal.clear_button,
                        clear_additional_event_cb,
                        LV_EVENT_CLICKED,
                        NULL);
    lv_obj_t *clear_label = lv_label_create(s_modal.clear_button);
    lv_label_set_text(clear_label, "CLEAR");
    lv_obj_center(clear_label);

    lv_obj_t *helper = lv_label_create(s_modal.additional_card);
    lv_obj_add_style(helper, &styles->panel_title, 0);
    lv_obj_set_width(helper, LV_PCT(100));
    lv_label_set_long_mode(helper, LV_LABEL_LONG_WRAP);
    lv_label_set_text(helper,
                      "Adds to the measured wound length.\nMinimum 1 meter.");

    lv_obj_t *preview = lv_obj_create(right);
    lv_obj_remove_style_all(preview);
    lv_obj_add_style(preview, &styles->panel_secondary, 0);
    lv_obj_set_size(preview, LV_PCT(100), 106);
    lv_obj_set_style_pad_all(preview, 9, 0);
    lv_obj_set_flex_flow(preview, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(preview, 2, 0);
    lv_obj_clear_flag(preview, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *preview_title = lv_label_create(preview);
    lv_obj_add_style(preview_title, &styles->panel_title, 0);
    lv_obj_set_style_text_color(preview_title, hmi_palette_get()->blue, 0);
    lv_label_set_text(preview_title, "PREVIEW - NOT APPLIED");

    s_modal.preview_wound = lv_label_create(preview);
    lv_obj_add_style(s_modal.preview_wound, &styles->stat_label, 0);
    lv_label_set_text(s_modal.preview_wound, "Wound: --");
    s_modal.preview_add = lv_label_create(preview);
    lv_obj_add_style(s_modal.preview_add, &styles->stat_label, 0);
    lv_label_set_text(s_modal.preview_add, "Add: --");
    s_modal.preview_target = lv_label_create(preview);
    lv_obj_add_style(s_modal.preview_target, &styles->stat_value, 0);
    lv_obj_set_style_text_color(s_modal.preview_target,
                                hmi_palette_get()->blue,
                                0);
    lv_label_set_text(s_modal.preview_target, "New target: --");
}

static void create_footer(lv_obj_t *parent)
{
    hmi_styles_t *styles = hmi_styles_get();
    lv_obj_t *footer = create_plain_container(parent);
    lv_obj_set_size(footer, LV_PCT(100), 72);
    lv_obj_set_style_bg_color(footer, hmi_palette_get()->device, 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(footer, hmi_palette_get()->border, 0);
    lv_obj_set_style_border_width(footer, 2, 0);
    lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_left(footer, 14, 0);
    lv_obj_set_style_pad_right(footer, 14, 0);
    lv_obj_set_style_pad_column(footer, 10, 0);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *feedback = create_plain_container(footer);
    lv_obj_set_size(feedback, 330, 52);
    lv_obj_set_flex_flow(feedback, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(feedback, 2, 0);

    s_modal.status_label = lv_label_create(feedback);
    lv_obj_add_style(s_modal.status_label, &styles->stat_label, 0);
    lv_obj_set_width(s_modal.status_label, LV_PCT(100));
    lv_label_set_long_mode(s_modal.status_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_modal.status_label, "Edit values, then apply atomically.");

    s_modal.error_label = lv_label_create(feedback);
    lv_obj_add_style(s_modal.error_label, &styles->stat_label, 0);
    lv_obj_set_width(s_modal.error_label, LV_PCT(100));
    lv_label_set_long_mode(s_modal.error_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(s_modal.error_label, hmi_palette_get()->red, 0);
    lv_label_set_text(s_modal.error_label, "");

    s_modal.cancel_button = create_action_button(
        footer, "CANCEL", 165, HMI_COLOR_DIM, close_event_cb);
    s_modal.apply_button = create_action_button(
        footer, "APPLY CHANGES", 235, HMI_COLOR_BLUE, apply_event_cb);
    s_modal.apply_label = lv_obj_get_child(s_modal.apply_button, 0);
}

void modal_paused_job_edit_open(void)
{
    const hmi_state_t *state = hmi_model_get_state();
    if (state == NULL || !state->machine_state_known ||
        state->machine_state != HMI_MACHINE_PAUSED ||
        hmi_pending_command_is_active() ||
        hmi_paused_job_draft_model_update_awaiting_confirmation()) {
        return;
    }

    modal_paused_job_edit_close();
    (void)hmi_paused_job_draft_model_begin_edit(state);

    s_modal.overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_modal.overlay);
    lv_obj_set_size(s_modal.overlay, HMI_DISPLAY_WIDTH, HMI_DISPLAY_HEIGHT);
    lv_obj_set_style_bg_color(s_modal.overlay, hmi_palette_get()->bg, 0);
    lv_obj_set_style_bg_opa(s_modal.overlay, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(s_modal.overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(s_modal.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(s_modal.overlay);
    lv_obj_add_event_cb(s_modal.overlay,
                        overlay_delete_event_cb,
                        LV_EVENT_DELETE,
                        NULL);

    create_header(s_modal.overlay);
    create_status_strip(s_modal.overlay);
    create_editor_body(s_modal.overlay);
    create_footer(s_modal.overlay);
    s_modal.force_refresh = true;
    modal_paused_job_edit_update(state);
}

static const char *pause_reason_text(const hmi_state_t *state)
{
    if (state == NULL || !state->pause_reason_known) {
        return "NOT REPORTED";
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

static presentation_snapshot_t capture_presentation(const hmi_state_t *state)
{
    presentation_snapshot_t presentation = {
        .wound_known = state->wound_length_known,
        .wound_length_m = state->wound_length_m,
        .target_known = state->target_length_known,
        .target_length_m = state->target_length_m,
        .pause_reason_known = state->pause_reason_known,
        .pause_reason = state->pause_reason,
        .additional_invalid = s_modal.additional_invalid,
    };

    for (size_t i = 0; i < EDIT_FIELD_COUNT; i++) {
        const edit_field_t *field = &s_modal.fields[i];
        if (field->descriptor != NULL) {
            presentation.field_known[i] =
                hmi_paused_job_draft_model_get_value(
                    field->descriptor->id,
                    &presentation.field_values[i]);
        }
    }

    presentation.has_additional =
        hmi_paused_job_draft_model_has_additional_length();
    if (presentation.has_additional) {
        presentation.additional_length_m =
            hmi_paused_job_draft_model_get_additional_length_m();
    }

    if (hmi_paused_job_draft_model_preview_target_m(
            state,
            &presentation.preview_target_m)) {
        presentation.preview_target_mode = PREVIEW_TARGET_DRAFT;
    } else if (state->target_length_known) {
        presentation.preview_target_mode = PREVIEW_TARGET_UNCHANGED;
        presentation.preview_target_m = state->target_length_m;
    } else {
        presentation.preview_target_mode = PREVIEW_TARGET_UNAVAILABLE;
    }

    const bool command_pending = hmi_pending_command_is_active();
    const bool confirmation_pending =
        hmi_paused_job_draft_model_update_awaiting_confirmation();
    const bool busy = command_pending || confirmation_pending;
    const bool initialized = hmi_paused_job_draft_model_is_initialized();
    const bool dirty = hmi_paused_job_draft_model_is_dirty();

    presentation.fields_dimmed = busy || !initialized;
    presentation.additional_dimmed = busy || !initialized;
    presentation.clear_dimmed = busy || !presentation.has_additional;
    presentation.close_dimmed = busy;
    presentation.cancel_dimmed = busy;
    presentation.apply_dimmed = busy || !initialized || !dirty;

    if (command_pending &&
        hmi_pending_command_get() == HMI_PENDING_UPDATE_PAUSED_JOB) {
        presentation.status_mode = MODAL_STATUS_APPLYING;
    } else if (confirmation_pending) {
        presentation.status_mode = MODAL_STATUS_CONFIRMING;
    } else if (!initialized) {
        presentation.status_mode = MODAL_STATUS_UNINITIALIZED;
    } else if (dirty) {
        presentation.status_mode = MODAL_STATUS_DIRTY;
    } else {
        presentation.status_mode = MODAL_STATUS_IDLE;
    }

    return presentation;
}

static bool preview_target_changed(bool force,
                                   const presentation_snapshot_t *current,
                                   const presentation_snapshot_t *cached)
{
    if (force ||
        current->preview_target_mode != cached->preview_target_mode) {
        return true;
    }

    return current->preview_target_mode != PREVIEW_TARGET_UNAVAILABLE &&
           !float_value_equal(current->preview_target_m,
                              cached->preview_target_m);
}

static void render_presentation(const hmi_state_t *state,
                                const presentation_snapshot_t *current,
                                bool force)
{
    presentation_cache_t *cache = &s_modal.render_cache;
    const presentation_snapshot_t *cached = &cache->snapshot;
    char text[96];

    const bool wound_changed = known_float_changed(
        force,
        current->wound_known,
        current->wound_length_m,
        cached->wound_known,
        cached->wound_length_m);
    if (wound_changed) {
        if (current->wound_known) {
            snprintf(text,
                     sizeof(text),
                     "%.3f m",
                     (double)current->wound_length_m);
            set_label_text_cached(s_modal.wound_value,
                                  cache->wound_text,
                                  sizeof(cache->wound_text),
                                  text,
                                  force);
            snprintf(text,
                     sizeof(text),
                     "Wound: %.3f m",
                     (double)current->wound_length_m);
        } else {
            set_label_text_cached(s_modal.wound_value,
                                  cache->wound_text,
                                  sizeof(cache->wound_text),
                                  "--",
                                  force);
            snprintf(text, sizeof(text), "Wound: --");
        }
        set_label_text_cached(s_modal.preview_wound,
                              cache->preview_wound_text,
                              sizeof(cache->preview_wound_text),
                              text,
                              force);
    }

    const bool target_changed = known_float_changed(
        force,
        current->target_known,
        current->target_length_m,
        cached->target_known,
        cached->target_length_m);
    if (target_changed) {
        if (current->target_known) {
            format_length_m(text,
                            sizeof(text),
                            current->target_length_m);
        } else {
            snprintf(text, sizeof(text), "--");
        }
        set_label_text_cached(s_modal.target_value,
                              cache->target_text,
                              sizeof(cache->target_text),
                              text,
                              force);
    }

    if (wound_changed || target_changed) {
        const char *remaining_title = "REMAINING";
        presentation_color_t remaining_color = PRESENTATION_COLOR_DIM;
        if (current->wound_known && current->target_known) {
            const double delta = (double)current->target_length_m -
                                 (double)current->wound_length_m;
            if (delta >= 0.0) {
                snprintf(text, sizeof(text), "%.3f m", delta);
                remaining_color = PRESENTATION_COLOR_TEXT;
            } else {
                remaining_title = "EXCEEDED";
                snprintf(text, sizeof(text), "+%.3f m", -delta);
                remaining_color = PRESENTATION_COLOR_AMBER;
            }
        } else {
            snprintf(text, sizeof(text), "--");
        }

        set_label_text_cached(s_modal.remaining_title,
                              cache->remaining_title,
                              sizeof(cache->remaining_title),
                              remaining_title,
                              force);
        set_label_text_cached(s_modal.remaining_value,
                              cache->remaining_text,
                              sizeof(cache->remaining_text),
                              text,
                              force);
        set_text_color_cached(s_modal.remaining_value,
                              remaining_color,
                              &cache->remaining_color,
                              force);
    }

    if (force ||
        current->pause_reason_known != cached->pause_reason_known ||
        (current->pause_reason_known &&
         current->pause_reason != cached->pause_reason)) {
        const presentation_color_t pause_color =
            current->pause_reason_known &&
                    current->pause_reason ==
                        HMI_JOB_PAUSE_REASON_LENGTH_WATCHDOG
                ? PRESENTATION_COLOR_RED
                : PRESENTATION_COLOR_AMBER;
        set_label_text_cached(s_modal.pause_reason_value,
                              cache->pause_reason_text,
                              sizeof(cache->pause_reason_text),
                              pause_reason_text(state),
                              force);
        set_text_color_cached(s_modal.pause_reason_value,
                              pause_color,
                              &cache->pause_reason_color,
                              force);
    }

    for (size_t i = 0; i < EDIT_FIELD_COUNT; i++) {
        edit_field_t *field = &s_modal.fields[i];
        const bool field_changed =
            force || current->field_known[i] != cached->field_known[i] ||
            (current->field_known[i] &&
             !param_value_equal(field->descriptor,
                                current->field_values[i],
                                cached->field_values[i]));
        if (!field_changed) {
            continue;
        }

        const bool formatted = current->field_known[i] &&
                               field->descriptor != NULL &&
                               hmi_param_format_value(
                                   field->descriptor,
                                   current->field_values[i],
                                   text,
                                   sizeof(text),
                                   true);
        set_label_text_cached(field->value_label,
                              cache->field_text[i],
                              sizeof(cache->field_text[i]),
                              formatted ? text : "--",
                              force);
    }

    const bool additional_changed = known_float_changed(
        force,
        current->has_additional,
        current->additional_length_m,
        cached->has_additional,
        cached->additional_length_m);
    if (additional_changed) {
        if (current->has_additional) {
            format_length_m(text,
                            sizeof(text),
                            current->additional_length_m);
        } else {
            snprintf(text, sizeof(text), "NOT SET");
        }
        set_label_text_cached(s_modal.additional_value,
                              cache->additional_text,
                              sizeof(cache->additional_text),
                              text,
                              force);
        set_hidden_cached(s_modal.draft_badge,
                          !current->has_additional,
                          !cached->has_additional,
                          force);

        if (current->has_additional) {
            snprintf(text,
                     sizeof(text),
                     "Add: +%.0f m",
                     (double)current->additional_length_m);
        } else {
            snprintf(text, sizeof(text), "Add: --");
        }
        set_label_text_cached(s_modal.preview_add,
                              cache->preview_add_text,
                              sizeof(cache->preview_add_text),
                              text,
                              force);
    }

    if (force || current->additional_invalid != cached->additional_invalid) {
        lv_obj_set_style_border_color(
            s_modal.additional_card,
            current->additional_invalid ? hmi_palette_get()->red
                                        : hmi_palette_get()->border,
            0);
        lv_obj_set_style_border_width(s_modal.additional_card,
                                      current->additional_invalid ? 2 : 1,
                                      0);
    }

    if (preview_target_changed(force, current, cached)) {
        char target_text[32];
        switch (current->preview_target_mode) {
        case PREVIEW_TARGET_DRAFT:
            format_length_m(target_text,
                            sizeof(target_text),
                            current->preview_target_m);
            snprintf(text,
                     sizeof(text),
                     "New target: %s",
                     target_text);
            break;
        case PREVIEW_TARGET_UNCHANGED:
            format_length_m(target_text,
                            sizeof(target_text),
                            current->preview_target_m);
            snprintf(text,
                     sizeof(text),
                     "New target: %s (unchanged)",
                     target_text);
            break;
        case PREVIEW_TARGET_UNAVAILABLE:
        default:
            snprintf(text, sizeof(text), "New target: --");
            break;
        }
        set_label_text_cached(s_modal.preview_target,
                              cache->preview_target_text,
                              sizeof(cache->preview_target_text),
                              text,
                              force);
    }

    for (size_t i = 0; i < EDIT_FIELD_COUNT; i++) {
        set_button_dimmed_cached(s_modal.fields[i].button,
                                 current->fields_dimmed,
                                 cached->fields_dimmed,
                                 force);
    }
    set_button_dimmed_cached(s_modal.additional_button,
                             current->additional_dimmed,
                             cached->additional_dimmed,
                             force);
    set_button_dimmed_cached(s_modal.clear_button,
                             current->clear_dimmed,
                             cached->clear_dimmed,
                             force);
    set_button_dimmed_cached(s_modal.close_button,
                             current->close_dimmed,
                             cached->close_dimmed,
                             force);
    set_button_dimmed_cached(s_modal.cancel_button,
                             current->cancel_dimmed,
                             cached->cancel_dimmed,
                             force);
    set_button_dimmed_cached(s_modal.apply_button,
                             current->apply_dimmed,
                             cached->apply_dimmed,
                             force);

    const char *status_text = "EDIT VALUES, THEN APPLY ATOMICALLY.";
    const char *apply_text = "APPLY CHANGES";
    presentation_color_t status_color = PRESENTATION_COLOR_DIM;
    switch (current->status_mode) {
    case MODAL_STATUS_APPLYING:
        status_text = "APPLYING ATOMIC UPDATE...";
        apply_text = "APPLYING...";
        status_color = PRESENTATION_COLOR_AMBER;
        break;
    case MODAL_STATUS_CONFIRMING:
        status_text = "ACCEPTED - WAITING FOR MATCHING TELEMETRY...";
        apply_text = "APPLIED - VERIFYING";
        status_color = PRESENTATION_COLOR_AMBER;
        break;
    case MODAL_STATUS_UNINITIALIZED:
        status_text = "WAITING FOR CONFIRMED ACTIVE PARAMETERS...";
        status_color = PRESENTATION_COLOR_AMBER;
        break;
    case MODAL_STATUS_DIRTY:
        status_text = "DRAFT CHANGED - REVIEW AND APPLY.";
        status_color = PRESENTATION_COLOR_BLUE;
        break;
    case MODAL_STATUS_IDLE:
    default:
        break;
    }
    set_label_text_cached(s_modal.status_label,
                          cache->status_text,
                          sizeof(cache->status_text),
                          status_text,
                          force);
    set_label_text_cached(s_modal.apply_label,
                          cache->apply_text,
                          sizeof(cache->apply_text),
                          apply_text,
                          force);
    set_text_color_cached(s_modal.status_label,
                          status_color,
                          &cache->status_color,
                          force);
    set_label_text_cached(s_modal.error_label,
                          cache->error_text,
                          sizeof(cache->error_text),
                          s_modal.local_error,
                          force);

    cache->snapshot = *current;
    cache->initialized = true;
}

void modal_paused_job_edit_update(const hmi_state_t *state)
{
    if (!modal_paused_job_edit_is_open() || state == NULL) {
        return;
    }

    if (!state->machine_state_known ||
        state->machine_state != HMI_MACHINE_PAUSED) {
        modal_paused_job_edit_close();
        return;
    }

    if (s_modal.apply_requested &&
        hmi_paused_job_draft_model_confirmation_revision() !=
            s_modal.apply_confirmation_revision) {
        modal_paused_job_edit_close();
        return;
    }
    const bool busy = hmi_pending_command_is_active() ||
                      hmi_paused_job_draft_model_update_awaiting_confirmation();
    if (s_modal.apply_requested && !busy &&
        s_modal.local_error[0] == '\0') {
        const char *controller_error = state->last_error;
        if (controller_error != NULL && controller_error[0] != '\0') {
            snprintf(s_modal.local_error,
                     sizeof(s_modal.local_error),
                     "%s",
                      controller_error);
        }
    }

    if (modal_numeric_keypad_is_open() || modal_confirm_is_open()) {
        s_modal.render_suppressed = true;
        return;
    }

    const bool force = s_modal.force_refresh || s_modal.render_suppressed ||
                       !s_modal.render_cache.initialized;
    s_modal.force_refresh = false;
    s_modal.render_suppressed = false;

    const presentation_snapshot_t presentation = capture_presentation(state);
    render_presentation(state, &presentation, force);
}