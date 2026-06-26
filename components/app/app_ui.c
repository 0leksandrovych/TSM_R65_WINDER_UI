#include "app_ui.h"

#include "lvgl.h"

static lv_obj_t* status_label;

static void set_status(const char* text)
{
    if (status_label != NULL) {
        lv_label_set_text(status_label, text);
    }
}

static void home_btn_event_cb(lv_event_t* e)
{
    LV_UNUSED(e);
    set_status("Status: HOME pressed");
}

static void start_btn_event_cb(lv_event_t* e)
{
    LV_UNUSED(e);
    set_status("Status: START pressed");
}

static void stop_btn_event_cb(lv_event_t* e)
{
    LV_UNUSED(e);
    set_status("Status: STOP pressed");
}

static lv_obj_t* create_button(lv_obj_t* parent,
    const char* text,
    lv_coord_t x,
    lv_coord_t y,
    lv_event_cb_t event_cb)
{
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 160, 70);
    lv_obj_align(btn, LV_ALIGN_CENTER, x, y);
    lv_obj_add_event_cb(btn, event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return btn;
}

void app_ui_create(void)
{
    lv_obj_t* scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x202020), 0);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "Fiber Winder");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 25);

    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, "Status: IDLE");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_20, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 75);

    create_button(scr, "HOME", -220, 80, home_btn_event_cb);
    create_button(scr, "START", 0, 80, start_btn_event_cb);
    create_button(scr, "STOP", 220, 80, stop_btn_event_cb);
}
