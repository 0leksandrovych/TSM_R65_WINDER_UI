#include "screen_jobs.h"

#include "hmi_actions.h"
#include "hmi_capability_model.h"
#include "hmi_styles.h"
#include "hmi_types.h"
#include "widget_status_badge.h"

typedef struct {
    lv_obj_t *root;
    hmi_status_badge_t badge;
} jobs_screen_t;

static jobs_screen_t s_screen;

static void back_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_go_home();
}

static void mode_event_cb(lv_event_t *event)
{
    const hmi_mode_capability_t *mode = (const hmi_mode_capability_t *)lv_event_get_user_data(event);
    if (mode == NULL || !mode->enabled) {
        return;
    }

    hmi_actions_select_job_mode(mode->id);
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
    lv_label_set_text(title, "SELECT JOB");

    lv_obj_t *spacer = lv_obj_create(topbar);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 1);

    widget_status_badge_create(topbar, &s_screen.badge);
}

static lv_obj_t *create_card(lv_obj_t *parent, const hmi_mode_capability_t *mode)
{
    hmi_styles_t *styles = hmi_styles_get();
    bool enabled = mode != NULL && mode->enabled;

    lv_obj_t *card = lv_btn_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_add_style(card, enabled ? &styles->panel : &styles->panel_secondary, 0);
    lv_obj_set_size(card, 244, 246);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(card, 10, 0);
    lv_obj_set_style_bg_color(card, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    if (!enabled) {
        lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_opa(card, LV_OPA_60, 0);
    }

    lv_obj_t *title = lv_label_create(card);
    lv_obj_add_style(title, &styles->status_text, 0);
    lv_obj_set_style_text_color(title, enabled ? hmi_palette_get()->text : hmi_palette_get()->text_muted, 0);
    lv_label_set_text(title, mode != NULL && mode->title != NULL ? mode->title : "--");

    lv_obj_t *body = lv_label_create(card);
    lv_obj_add_style(body, &styles->topbar_text, 0);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(body, LV_PCT(100));
    lv_label_set_text(body, mode != NULL && mode->description != NULL ? mode->description : "");

    lv_obj_t *spacer = lv_obj_create(card);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 1);

    lv_obj_t *status = lv_label_create(card);
    lv_obj_add_style(status, &styles->panel_title, 0);
    lv_obj_set_style_text_color(status, enabled ? hmi_palette_get()->green : hmi_palette_get()->amber, 0);
    lv_label_set_text(status, enabled ? "AVAILABLE" : "COMING LATER");

    if (enabled) {
        lv_obj_add_event_cb(card, mode_event_cb, LV_EVENT_CLICKED, (void *)mode);
    }

    return card;
}

void screen_jobs_create(lv_obj_t *root)
{
    s_screen = (jobs_screen_t){0};
    s_screen.root = root;
    create_topbar(root);

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), HMI_DISPLAY_HEIGHT - HMI_TOPBAR_HEIGHT);
    lv_obj_set_style_pad_all(content, 14, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(content, 12, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    for (size_t i = 0; i < hmi_capability_model_get_mode_count(); i++) {
        create_card(content, hmi_capability_model_get_mode_by_index(i));
    }
}

void screen_jobs_update(const hmi_state_t *state)
{
    if (state != NULL) {
        widget_status_badge_update(&s_screen.badge, state->machine_state);
    }
}
