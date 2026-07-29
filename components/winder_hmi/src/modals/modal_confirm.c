#include "modal_confirm.h"

#include "hmi_styles.h"

typedef struct {
    lv_obj_t *overlay;
    modal_confirm_cb_t confirm_cb;
    void *user_ctx;
} confirm_modal_t;

static confirm_modal_t s_modal;

bool modal_confirm_is_open(void)
{
    return s_modal.overlay != NULL && lv_obj_is_valid(s_modal.overlay);
}

static void reset_modal_state(void)
{
    s_modal = (confirm_modal_t){0};
}

void modal_confirm_close(void)
{
    lv_obj_t *overlay = s_modal.overlay;

    reset_modal_state();

    if (overlay != NULL && lv_obj_is_valid(overlay)) {
        lv_obj_del_async(overlay);
    }
}

static void overlay_delete_event_cb(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);

    if (target == s_modal.overlay) {
        reset_modal_state();
    }
}

static void cancel_event_cb(lv_event_t *event)
{
    (void)event;
    modal_confirm_close();
}

static void confirm_event_cb(lv_event_t *event)
{
    (void)event;

    modal_confirm_cb_t confirm_cb = s_modal.confirm_cb;
    void *user_ctx = s_modal.user_ctx;
    modal_confirm_close();

    if (confirm_cb != NULL) {
        confirm_cb(user_ctx);
    }
}

static lv_obj_t *create_action_button(lv_obj_t *parent,
                                      const char *text,
                                      hmi_color_role_t role,
                                      lv_event_cb_t cb)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->primary_button, 0);
    lv_obj_set_size(button, 168, 50);
    lv_obj_set_style_bg_color(button, hmi_color_for_role(role), 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(
        button,
        role == HMI_COLOR_AMBER ? hmi_palette_get()->device : lv_color_white(),
        0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return button;
}

void modal_confirm_open(const modal_confirm_config_t *config)
{
    if (config == NULL) {
        return;
    }

    modal_confirm_close();

    s_modal.confirm_cb = config->confirm_cb;
    s_modal.user_ctx   = config->user_ctx;

    hmi_styles_t *styles = hmi_styles_get();

    s_modal.overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_modal.overlay);
    lv_obj_set_size(s_modal.overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_modal.overlay, hmi_palette_get()->bg, 0);
    lv_obj_set_style_bg_opa(s_modal.overlay, LV_OPA_70, 0);
    lv_obj_clear_flag(s_modal.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(s_modal.overlay);
    lv_obj_add_event_cb(s_modal.overlay, overlay_delete_event_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t *dialog = lv_obj_create(s_modal.overlay);
    lv_obj_remove_style_all(dialog);
    lv_obj_add_style(dialog, &styles->panel, 0);
    lv_obj_set_size(dialog, 460, 250);
    lv_obj_center(dialog);
    lv_obj_set_flex_flow(dialog, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(dialog, 24, 0);
    lv_obj_set_style_pad_row(dialog, 14, 0);
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(dialog);
    lv_obj_add_style(title, &styles->topbar_title, 0);
    lv_obj_set_width(title, LV_PCT(100));
    lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
    lv_label_set_text(title, config->title != NULL ? config->title : "Confirm");

    lv_obj_t *body = lv_label_create(dialog);
    lv_obj_add_style(body, &styles->status_text, 0);
    lv_obj_set_style_text_color(body, hmi_palette_get()->text_dim, 0);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_flex_grow(body, 1);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_label_set_text(body, config->body != NULL ? config->body : "");

    lv_obj_t *actions = lv_obj_create(dialog);
    lv_obj_remove_style_all(actions);
    lv_obj_set_size(actions, LV_PCT(100), 50);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(actions, 12, 0);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);

    create_action_button(
        actions,
        config->cancel_text != NULL ? config->cancel_text : "CANCEL",
        HMI_COLOR_DIM,
        cancel_event_cb);
    create_action_button(
        actions,
        config->confirm_text != NULL ? config->confirm_text : "CONFIRM",
        config->confirm_role,
        confirm_event_cb);
}
