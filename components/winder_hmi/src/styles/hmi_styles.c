#include "hmi_styles.h"

static hmi_styles_t s_styles;
static bool s_initialized;

#if LV_FONT_MONTSERRAT_40
#define HMI_FONT_STATUS (&lv_font_montserrat_40)
#else
#define HMI_FONT_STATUS LV_FONT_DEFAULT
#endif

#if LV_FONT_MONTSERRAT_18
#define HMI_FONT_LARGE (&lv_font_montserrat_18)
#else
#define HMI_FONT_LARGE LV_FONT_DEFAULT
#endif

#if LV_FONT_MONTSERRAT_16
#define HMI_FONT_BODY (&lv_font_montserrat_16)
#else
#define HMI_FONT_BODY LV_FONT_DEFAULT
#endif

#if LV_FONT_MONTSERRAT_14
#define HMI_FONT_SMALL (&lv_font_montserrat_14)
#else
#define HMI_FONT_SMALL LV_FONT_DEFAULT
#endif

#if LV_FONT_MONTSERRAT_12
#define HMI_FONT_TINY (&lv_font_montserrat_12)
#else
#define HMI_FONT_TINY LV_FONT_DEFAULT
#endif

static const hmi_palette_t s_palette = {
    .bg = LV_COLOR_MAKE(0x07, 0x0A, 0x0F),
    .device = LV_COLOR_MAKE(0x0C, 0x11, 0x18),
    .panel = LV_COLOR_MAKE(0x14, 0x1A, 0x24),
    .panel_secondary = LV_COLOR_MAKE(0x1A, 0x21, 0x2D),
    .border = LV_COLOR_MAKE(0x28, 0x31, 0x3F),
    .border_strong = LV_COLOR_MAKE(0x3A, 0x46, 0x58),
    .text = LV_COLOR_MAKE(0xEA, 0xF0, 0xF8),
    .text_dim = LV_COLOR_MAKE(0x9A, 0xA7, 0xBA),
    .text_muted = LV_COLOR_MAKE(0x63, 0x70, 0x85),
    .blue = LV_COLOR_MAKE(0x2F, 0x7F, 0xF0),
    .green = LV_COLOR_MAKE(0x2F, 0xAB, 0x4D),
    .amber = LV_COLOR_MAKE(0xE3, 0xA1, 0x1D),
    .red = LV_COLOR_MAKE(0xE6, 0x3D, 0x3D),
};

static void init_common_obj_style(lv_style_t *style, lv_color_t bg, lv_color_t border)
{
    lv_style_init(style);
    lv_style_set_bg_color(style, bg);
    lv_style_set_bg_opa(style, LV_OPA_COVER);
    lv_style_set_border_color(style, border);
    lv_style_set_border_width(style, 1);
    lv_style_set_radius(style, 2);
    lv_style_set_pad_all(style, 14);
}

void hmi_styles_init(void)
{
    if (s_initialized) {
        return;
    }

    lv_style_init(&s_styles.root);
    lv_style_set_bg_color(&s_styles.root, s_palette.device);
    lv_style_set_bg_opa(&s_styles.root, LV_OPA_COVER);
    lv_style_set_pad_all(&s_styles.root, 0);
    lv_style_set_radius(&s_styles.root, 0);

    lv_style_init(&s_styles.topbar);
    lv_style_set_bg_color(&s_styles.topbar, s_palette.device);
    lv_style_set_bg_opa(&s_styles.topbar, LV_OPA_COVER);
    lv_style_set_border_color(&s_styles.topbar, s_palette.border);
    lv_style_set_border_width(&s_styles.topbar, 2);
    lv_style_set_border_side(&s_styles.topbar, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_pad_left(&s_styles.topbar, 14);
    lv_style_set_pad_right(&s_styles.topbar, 14);
    lv_style_set_pad_top(&s_styles.topbar, 0);
    lv_style_set_pad_bottom(&s_styles.topbar, 0);
    lv_style_set_radius(&s_styles.topbar, 0);

    lv_style_init(&s_styles.topbar_title);
    lv_style_set_text_color(&s_styles.topbar_title, s_palette.blue);
    lv_style_set_text_font(&s_styles.topbar_title, HMI_FONT_LARGE);
    lv_style_set_text_letter_space(&s_styles.topbar_title, 1);

    lv_style_init(&s_styles.topbar_text);
    lv_style_set_text_color(&s_styles.topbar_text, s_palette.text_dim);
    lv_style_set_text_font(&s_styles.topbar_text, HMI_FONT_SMALL);

    lv_style_init(&s_styles.topbar_system_ok);
    lv_style_set_text_color(&s_styles.topbar_system_ok, s_palette.green);
    lv_style_set_text_font(&s_styles.topbar_system_ok, HMI_FONT_TINY);
    lv_style_set_text_letter_space(&s_styles.topbar_system_ok, 1);

    init_common_obj_style(&s_styles.panel, s_palette.panel, s_palette.border);
    init_common_obj_style(&s_styles.panel_secondary, s_palette.panel_secondary, s_palette.border_strong);

    lv_style_init(&s_styles.panel_title);
    lv_style_set_text_color(&s_styles.panel_title, s_palette.text_muted);
    lv_style_set_text_font(&s_styles.panel_title, HMI_FONT_TINY);
    lv_style_set_text_letter_space(&s_styles.panel_title, 1);

    lv_style_init(&s_styles.status_big);
    lv_style_set_text_color(&s_styles.status_big, s_palette.text);
    lv_style_set_text_font(&s_styles.status_big, HMI_FONT_STATUS);

    lv_style_init(&s_styles.status_text);
    lv_style_set_text_color(&s_styles.status_text, s_palette.text_dim);
    lv_style_set_text_font(&s_styles.status_text, HMI_FONT_BODY);

    lv_style_init(&s_styles.stat_label);
    lv_style_set_text_color(&s_styles.stat_label, s_palette.text_dim);
    lv_style_set_text_font(&s_styles.stat_label, HMI_FONT_SMALL);

    lv_style_init(&s_styles.stat_value);
    lv_style_set_text_color(&s_styles.stat_value, s_palette.text);
    lv_style_set_text_font(&s_styles.stat_value, HMI_FONT_SMALL);

    lv_style_init(&s_styles.primary_button);
    lv_style_set_bg_color(&s_styles.primary_button, s_palette.blue);
    lv_style_set_bg_opa(&s_styles.primary_button, LV_OPA_COVER);
    lv_style_set_border_color(&s_styles.primary_button, s_palette.border_strong);
    lv_style_set_border_width(&s_styles.primary_button, 1);
    lv_style_set_radius(&s_styles.primary_button, 2);
    lv_style_set_text_color(&s_styles.primary_button, lv_color_white());
    lv_style_set_text_font(&s_styles.primary_button, HMI_FONT_LARGE);

    lv_style_init(&s_styles.nav_button);
    lv_style_set_bg_color(&s_styles.nav_button, s_palette.device);
    lv_style_set_bg_opa(&s_styles.nav_button, LV_OPA_COVER);
    lv_style_set_border_color(&s_styles.nav_button, s_palette.border);
    lv_style_set_border_width(&s_styles.nav_button, 1);
    lv_style_set_radius(&s_styles.nav_button, 0);
    lv_style_set_text_color(&s_styles.nav_button, s_palette.text_dim);
    lv_style_set_text_font(&s_styles.nav_button, HMI_FONT_SMALL);

    lv_style_init(&s_styles.badge_base);
    lv_style_set_bg_opa(&s_styles.badge_base, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_styles.badge_base, 2);
    lv_style_set_radius(&s_styles.badge_base, 2);
    lv_style_set_pad_left(&s_styles.badge_base, 10);
    lv_style_set_pad_right(&s_styles.badge_base, 10);
    lv_style_set_pad_top(&s_styles.badge_base, 5);
    lv_style_set_pad_bottom(&s_styles.badge_base, 5);
    lv_style_set_text_font(&s_styles.badge_base, HMI_FONT_SMALL);
    lv_style_set_text_letter_space(&s_styles.badge_base, 1);

    lv_style_init(&s_styles.badge_green);
    lv_style_set_border_color(&s_styles.badge_green, s_palette.green);
    lv_style_set_text_color(&s_styles.badge_green, s_palette.green);

    lv_style_init(&s_styles.badge_amber);
    lv_style_set_border_color(&s_styles.badge_amber, s_palette.amber);
    lv_style_set_text_color(&s_styles.badge_amber, s_palette.amber);

    lv_style_init(&s_styles.badge_red);
    lv_style_set_border_color(&s_styles.badge_red, s_palette.red);
    lv_style_set_text_color(&s_styles.badge_red, s_palette.red);

    lv_style_init(&s_styles.badge_blue);
    lv_style_set_border_color(&s_styles.badge_blue, s_palette.blue);
    lv_style_set_text_color(&s_styles.badge_blue, s_palette.blue);

    lv_style_init(&s_styles.badge_dim);
    lv_style_set_border_color(&s_styles.badge_dim, s_palette.border_strong);
    lv_style_set_text_color(&s_styles.badge_dim, s_palette.text_dim);

    s_initialized = true;
}

hmi_styles_t *hmi_styles_get(void)
{
    return &s_styles;
}

const hmi_palette_t *hmi_palette_get(void)
{
    return &s_palette;
}

lv_color_t hmi_color_for_role(hmi_color_role_t role)
{
    switch (role) {
    case HMI_COLOR_BLUE:
        return s_palette.blue;
    case HMI_COLOR_GREEN:
        return s_palette.green;
    case HMI_COLOR_AMBER:
        return s_palette.amber;
    case HMI_COLOR_RED:
        return s_palette.red;
    case HMI_COLOR_DIM:
        return s_palette.text_dim;
    case HMI_COLOR_NEUTRAL:
    default:
        return s_palette.text;
    }
}
