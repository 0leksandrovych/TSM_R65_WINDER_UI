#pragma once

#include "lvgl.h"
#include "hmi_types.h"

typedef struct {
    lv_style_t root;
    lv_style_t topbar;
    lv_style_t topbar_title;
    lv_style_t topbar_text;
    lv_style_t topbar_system_ok;
    lv_style_t panel;
    lv_style_t panel_secondary;
    lv_style_t panel_title;
    lv_style_t status_big;
    lv_style_t status_text;
    lv_style_t stat_label;
    lv_style_t stat_value;
    lv_style_t primary_button;
    lv_style_t nav_button;
    lv_style_t badge_base;
    lv_style_t badge_green;
    lv_style_t badge_amber;
    lv_style_t badge_red;
    lv_style_t badge_blue;
    lv_style_t badge_dim;
} hmi_styles_t;

typedef struct {
    lv_color_t bg;
    lv_color_t device;
    lv_color_t panel;
    lv_color_t panel_secondary;
    lv_color_t border;
    lv_color_t border_strong;
    lv_color_t text;
    lv_color_t text_dim;
    lv_color_t text_muted;
    lv_color_t blue;
    lv_color_t green;
    lv_color_t amber;
    lv_color_t red;
} hmi_palette_t;

void hmi_styles_init(void);
hmi_styles_t *hmi_styles_get(void);
const hmi_palette_t *hmi_palette_get(void);
lv_color_t hmi_color_for_role(hmi_color_role_t role);
