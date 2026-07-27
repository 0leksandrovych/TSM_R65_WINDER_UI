#include "modal_edge_trim.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "hmi_actions.h"
#include "hmi_capability_model.h"
#include "hmi_model.h"
#include "hmi_pending_command.h"
#include "hmi_styles.h"

#define EDGE_TRIM_SIDE_COUNT 2U
#define EDGE_TRIM_BUTTON_COUNT 6U

typedef struct {
    uint8_t side;
    int32_t delta_centi_mm;
} trim_button_context_t;

typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *active_labels[EDGE_TRIM_SIDE_COUNT];
    lv_obj_t *draft_labels[EDGE_TRIM_SIDE_COUNT];
    lv_obj_t *step_buttons[EDGE_TRIM_SIDE_COUNT][EDGE_TRIM_BUTTON_COUNT];
    lv_obj_t *apply_button;
    int32_t draft_centi_mm[EDGE_TRIM_SIDE_COUNT];
    int32_t initial_centi_mm[EDGE_TRIM_SIDE_COUNT];
    int32_t min_centi_mm[EDGE_TRIM_SIDE_COUNT];
    int32_t max_centi_mm[EDGE_TRIM_SIDE_COUNT];
    trim_button_context_t button_contexts[EDGE_TRIM_SIDE_COUNT][EDGE_TRIM_BUTTON_COUNT];
} edge_trim_modal_t;

static edge_trim_modal_t s_modal;

static const int32_t s_step_multipliers[EDGE_TRIM_BUTTON_COUNT] = {
    -100, -10, -1, 1, 10, 100,
};

static const char *s_step_labels[EDGE_TRIM_BUTTON_COUNT] = {
    "-10", "-1", "-0.1", "+0.1", "+1", "+10",
};

static void format_trim(char *buffer, size_t buffer_size, int32_t centi_mm)
{
    if (buffer == NULL || buffer_size == 0U) {
        return;
    }

    if (centi_mm == 0) {
        snprintf(buffer, buffer_size, "0.0 mm");
    } else if ((centi_mm % 10) == 0) {
        snprintf(buffer, buffer_size, "%+.1f mm", (double)centi_mm / 100.0);
    } else {
        snprintf(buffer, buffer_size, "%+.2f mm", (double)centi_mm / 100.0);
    }
}

static bool state_allows_edge_trim(const hmi_state_t *state)
{
    return state != NULL && state->machine_state_known &&
           (state->machine_state == HMI_MACHINE_RUNNING ||
            state->machine_state == HMI_MACHINE_PAUSED);
}

static void set_button_disabled(lv_obj_t *button, bool disabled)
{
    if (button == NULL) {
        return;
    }

    if (disabled) {
        lv_obj_add_state(button, LV_STATE_DISABLED);
        lv_obj_clear_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(button, LV_OPA_40, 0);
    } else {
        lv_obj_clear_state(button, LV_STATE_DISABLED);
        lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(button, LV_OPA_COVER, 0);
    }
}

static void update_draft_controls(void)
{
    bool dirty = false;

    for (uint8_t side = 0U; side < EDGE_TRIM_SIDE_COUNT; side++) {
        char value[24];
        format_trim(value, sizeof(value), s_modal.draft_centi_mm[side]);
        if (s_modal.draft_labels[side] != NULL) {
            lv_label_set_text(s_modal.draft_labels[side], value);
        }

        dirty = dirty ||
                s_modal.draft_centi_mm[side] != s_modal.initial_centi_mm[side];

        for (uint8_t i = 0U; i < EDGE_TRIM_BUTTON_COUNT; i++) {
            int64_t candidate = (int64_t)s_modal.draft_centi_mm[side] +
                                s_modal.button_contexts[side][i].delta_centi_mm;
            bool outside = candidate < s_modal.min_centi_mm[side] ||
                           candidate > s_modal.max_centi_mm[side];
            set_button_disabled(s_modal.step_buttons[side][i], outside);
        }
    }

    bool busy = hmi_pending_command_is_active();
    bool allowed = state_allows_edge_trim(hmi_model_get_state());
    set_button_disabled(s_modal.apply_button, !dirty || busy || !allowed);
}

static void reset_modal_state(void)
{
    s_modal = (edge_trim_modal_t){0};
}

void modal_edge_trim_close(void)
{
    lv_obj_t *overlay = s_modal.overlay;
    reset_modal_state();

    if (overlay != NULL && lv_obj_is_valid(overlay)) {
        lv_obj_del_async(overlay);
    }
}

static void overlay_delete_event_cb(lv_event_t *event)
{
    if (lv_event_get_target(event) == s_modal.overlay) {
        reset_modal_state();
    }
}

static void cancel_event_cb(lv_event_t *event)
{
    (void)event;
    modal_edge_trim_close();
}

static void apply_event_cb(lv_event_t *event)
{
    (void)event;
    if (hmi_actions_apply_edge_trim(
            (float)s_modal.draft_centi_mm[HMI_EDGE_TRIM_LEFT] / 100.0f,
            (float)s_modal.draft_centi_mm[HMI_EDGE_TRIM_RIGHT] / 100.0f)) {
        modal_edge_trim_close();
    }
}

static void step_event_cb(lv_event_t *event)
{
    const trim_button_context_t *context =
        (const trim_button_context_t *)lv_event_get_user_data(event);
    if (context == NULL || context->side >= EDGE_TRIM_SIDE_COUNT) {
        return;
    }

    int64_t candidate = (int64_t)s_modal.draft_centi_mm[context->side] +
                        context->delta_centi_mm;
    if (candidate < s_modal.min_centi_mm[context->side] ||
        candidate > s_modal.max_centi_mm[context->side]) {
        return;
    }

    s_modal.draft_centi_mm[context->side] = (int32_t)candidate;
    update_draft_controls();
}

static lv_obj_t *create_button(lv_obj_t *parent,
                               const char *text,
                               int32_t width,
                               int32_t height,
                               hmi_color_role_t role,
                               lv_event_cb_t callback,
                               void *user_data)
{
    hmi_styles_t *styles = hmi_styles_get();
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->nav_button, 0);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, hmi_color_for_role(role), 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->border_strong, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(
        button,
        role == HMI_COLOR_AMBER ? hmi_palette_get()->device : lv_color_white(),
        0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

static lv_obj_t *create_edge_card(lv_obj_t *parent,
                                  hmi_edge_trim_side_t side,
                                  const char *title_text,
                                  int32_t capability_step_centi_mm)
{
    hmi_styles_t *styles = hmi_styles_get();
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, &styles->panel_secondary, 0);
    lv_obj_set_size(card, 352, 248);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 5, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(card);
    lv_obj_add_style(title, &styles->panel_title, 0);
    lv_label_set_text(title, title_text);

    s_modal.active_labels[side] = lv_label_create(card);
    lv_obj_add_style(s_modal.active_labels[side], &styles->status_text, 0);
    lv_obj_set_style_text_color(
        s_modal.active_labels[side], hmi_palette_get()->text_dim, 0);
    lv_label_set_text(s_modal.active_labels[side], "Active trim: --");

    lv_obj_t *new_label = lv_label_create(card);
    lv_obj_add_style(new_label, &styles->status_text, 0);
    lv_label_set_text(new_label, "New trim");

    s_modal.draft_labels[side] = lv_label_create(card);
    lv_obj_add_style(s_modal.draft_labels[side], &styles->stat_value, 0);
    lv_obj_set_style_text_color(
        s_modal.draft_labels[side], hmi_palette_get()->blue, 0);
    lv_label_set_text(s_modal.draft_labels[side], "0.0 mm");

    lv_obj_t *controls = lv_obj_create(card);
    lv_obj_remove_style_all(controls);
    lv_obj_set_size(controls, LV_PCT(100), 88);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(
        controls,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(controls, 6, 0);
    lv_obj_set_style_pad_column(controls, 6, 0);
    lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);

    for (uint8_t i = 0U; i < EDGE_TRIM_BUTTON_COUNT; i++) {
        trim_button_context_t *context = &s_modal.button_contexts[side][i];
        context->side = (uint8_t)side;
        context->delta_centi_mm =
            s_step_multipliers[i] * capability_step_centi_mm;
        s_modal.step_buttons[side][i] = create_button(
            controls,
            s_step_labels[i],
            96,
            40,
            HMI_COLOR_DIM,
            step_event_cb,
            context);
    }

    return card;
}

void modal_edge_trim_open(void)
{
    const hmi_state_t *state = hmi_model_get_state();
    if (!state_allows_edge_trim(state)) {
        return;
    }

    const hmi_param_descriptor_t *left_capability =
        hmi_capability_model_get_edge_trim(HMI_EDGE_TRIM_LEFT);
    const hmi_param_descriptor_t *right_capability =
        hmi_capability_model_get_edge_trim(HMI_EDGE_TRIM_RIGHT);
    if (left_capability == NULL || right_capability == NULL) {
        return;
    }

    modal_edge_trim_close();

    const hmi_param_descriptor_t *capabilities[EDGE_TRIM_SIDE_COUNT] = {
        left_capability,
        right_capability,
    };
    for (uint8_t side = 0U; side < EDGE_TRIM_SIDE_COUNT; side++) {
        s_modal.min_centi_mm[side] =
            (int32_t)lroundf(capabilities[side]->min_value.f32 * 100.0f);
        s_modal.max_centi_mm[side] =
            (int32_t)lroundf(capabilities[side]->max_value.f32 * 100.0f);
    }

    hmi_edge_trim_pair_t pending_pair;
    if (hmi_edge_trim_pending_get(&pending_pair)) {
        s_modal.draft_centi_mm[HMI_EDGE_TRIM_LEFT] = pending_pair.left_centi_mm;
        s_modal.draft_centi_mm[HMI_EDGE_TRIM_RIGHT] = pending_pair.right_centi_mm;
    } else {
        s_modal.draft_centi_mm[HMI_EDGE_TRIM_LEFT] =
            state->active_left_edge_trim_known
                ? (int32_t)lroundf(state->active_left_edge_trim_mm * 100.0f)
                : 0;
        s_modal.draft_centi_mm[HMI_EDGE_TRIM_RIGHT] =
            state->active_right_edge_trim_known
                ? (int32_t)lroundf(state->active_right_edge_trim_mm * 100.0f)
                : 0;
    }
    s_modal.initial_centi_mm[HMI_EDGE_TRIM_LEFT] =
        s_modal.draft_centi_mm[HMI_EDGE_TRIM_LEFT];
    s_modal.initial_centi_mm[HMI_EDGE_TRIM_RIGHT] =
        s_modal.draft_centi_mm[HMI_EDGE_TRIM_RIGHT];

    hmi_styles_t *styles = hmi_styles_get();
    s_modal.overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_modal.overlay);
    lv_obj_set_size(s_modal.overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_modal.overlay, hmi_palette_get()->bg, 0);
    lv_obj_set_style_bg_opa(s_modal.overlay, LV_OPA_80, 0);
    lv_obj_clear_flag(s_modal.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(s_modal.overlay);
    lv_obj_add_event_cb(
        s_modal.overlay, overlay_delete_event_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t *dialog = lv_obj_create(s_modal.overlay);
    lv_obj_remove_style_all(dialog);
    lv_obj_add_style(dialog, &styles->panel, 0);
    lv_obj_set_size(dialog, 760, 440);
    lv_obj_center(dialog);
    lv_obj_set_flex_flow(dialog, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(dialog, 14, 0);
    lv_obj_set_style_pad_row(dialog, 6, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(dialog);
    lv_obj_add_style(title, &styles->topbar_title, 0);
    lv_label_set_text(title, "Edge Trim");

    lv_obj_t *hint = lv_label_create(dialog);
    lv_obj_add_style(hint, &styles->status_text, 0);
    lv_obj_set_style_text_color(hint, hmi_palette_get()->text_dim, 0);
    lv_label_set_text(hint, "+ moves left, - moves right");

    lv_obj_t *cards = lv_obj_create(dialog);
    lv_obj_remove_style_all(cards);
    lv_obj_set_size(cards, LV_PCT(100), 248);
    lv_obj_set_flex_flow(cards, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        cards,
        LV_FLEX_ALIGN_SPACE_BETWEEN,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(cards, LV_OBJ_FLAG_SCROLLABLE);

    create_edge_card(
        cards,
        HMI_EDGE_TRIM_LEFT,
        "LEFT EDGE",
        (int32_t)lroundf(left_capability->step.f32 * 100.0f));
    create_edge_card(
        cards,
        HMI_EDGE_TRIM_RIGHT,
        "RIGHT EDGE",
        (int32_t)lroundf(right_capability->step.f32 * 100.0f));

    lv_obj_t *footer = lv_obj_create(dialog);
    lv_obj_remove_style_all(footer);
    lv_obj_set_size(footer, LV_PCT(100), 52);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        footer,
        LV_FLEX_ALIGN_END,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(footer, 12, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    (void)create_button(
        footer, "CANCEL", 160, 50, HMI_COLOR_DIM, cancel_event_cb, NULL);
    s_modal.apply_button = create_button(
        footer, "APPLY", 160, 50, HMI_COLOR_GREEN, apply_event_cb, NULL);

    modal_edge_trim_update(state);
    update_draft_controls();
}

void modal_edge_trim_update(const hmi_state_t *state)
{
    if (s_modal.overlay == NULL || !lv_obj_is_valid(s_modal.overlay)) {
        return;
    }
    if (!state_allows_edge_trim(state)) {
        modal_edge_trim_close();
        return;
    }

    const bool known[EDGE_TRIM_SIDE_COUNT] = {
        state->active_left_edge_trim_known,
        state->active_right_edge_trim_known,
    };
    const float values[EDGE_TRIM_SIDE_COUNT] = {
        state->active_left_edge_trim_mm,
        state->active_right_edge_trim_mm,
    };

    for (uint8_t side = 0U; side < EDGE_TRIM_SIDE_COUNT; side++) {
        char text[40];
        if (known[side]) {
            char value[24];
            format_trim(
                value,
                sizeof(value),
                (int32_t)lroundf(values[side] * 100.0f));
            snprintf(text, sizeof(text), "Active trim: %s", value);
        } else {
            snprintf(text, sizeof(text), "Active trim: --");
        }
        lv_label_set_text(s_modal.active_labels[side], text);
    }

    update_draft_controls();
}
