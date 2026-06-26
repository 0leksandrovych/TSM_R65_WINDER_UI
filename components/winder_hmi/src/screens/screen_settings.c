#include "screen_settings.h"

#include "hmi_actions.h"
#include "hmi_styles.h"
#include "hmi_types.h"
#include "widget_stat_row.h"
#include "widget_status_badge.h"

typedef struct {
    lv_obj_t *root;
    hmi_status_badge_t badge;
    lv_obj_t *banner;
    lv_obj_t *units_dropdown;
    lv_obj_t *brightness_slider;
    lv_obj_t *microstep_dropdown;
    lv_obj_t *polarity_dropdown;
    hmi_stat_row_t steps_row;
    hmi_stat_row_t max_speed_row;
    hmi_stat_row_t accel_row;
} settings_screen_t;

static settings_screen_t s_screen;

static void back_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_go_home();
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
    lv_obj_add_event_cb(back, back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, "<");
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(topbar);
    lv_obj_add_style(title, &styles->topbar_title, 0);
    lv_label_set_text(title, "SETTINGS");

    lv_obj_t *spacer = lv_obj_create(topbar);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 1);

    widget_status_badge_create(topbar, &s_screen.badge);
}

static lv_obj_t *create_panel(lv_obj_t *parent, const char *title_text)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &styles->panel, 0);
    lv_obj_set_size(panel, 374, 330);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &styles->panel_title, 0);
    lv_label_set_text(title, title_text);

    return panel;
}

static lv_obj_t *create_labeled_control(lv_obj_t *parent, const char *label_text)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), 48);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(row);
    lv_obj_add_style(label, &styles->stat_label, 0);
    lv_label_set_text(label, label_text);

    return row;
}

void screen_settings_create(lv_obj_t *root)
{
    hmi_styles_t *styles = hmi_styles_get();
    s_screen = (settings_screen_t){0};
    s_screen.root = root;

    create_topbar(root);

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), HMI_DISPLAY_HEIGHT - HMI_TOPBAR_HEIGHT);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 14, 0);
    lv_obj_set_style_pad_row(content, 10, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    s_screen.banner = lv_label_create(content);
    lv_obj_add_style(s_screen.banner, &styles->status_text, 0);
    lv_obj_set_width(s_screen.banner, LV_PCT(100));
    lv_obj_set_style_pad_all(s_screen.banner, 9, 0);
    lv_obj_set_style_bg_opa(s_screen.banner, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_screen.banner, 0, 0);
    lv_label_set_text(s_screen.banner, "");

    lv_obj_t *panels = lv_obj_create(content);
    lv_obj_remove_style_all(panels);
    lv_obj_set_size(panels, LV_PCT(100), 350);
    lv_obj_set_flex_flow(panels, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(panels, 12, 0);
    lv_obj_clear_flag(panels, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *user_panel = create_panel(panels, "USER SETTINGS");
    lv_obj_t *units_row = create_labeled_control(user_panel, "Units");
    s_screen.units_dropdown = lv_dropdown_create(units_row);
    lv_dropdown_set_options(s_screen.units_dropdown, "Metric\nImperial");
    lv_obj_set_size(s_screen.units_dropdown, 150, 38);

    lv_obj_t *brightness_row = create_labeled_control(user_panel, "Brightness");
    s_screen.brightness_slider = lv_slider_create(brightness_row);
    lv_obj_set_size(s_screen.brightness_slider, 150, 20);
    lv_slider_set_range(s_screen.brightness_slider, 20, 100);
    lv_slider_set_value(s_screen.brightness_slider, 80, LV_ANIM_OFF);

    lv_obj_t *machine_panel = create_panel(panels, "MACHINE PARAMETERS");
    lv_obj_t *microstep_row = create_labeled_control(machine_panel, "Microstepping");
    s_screen.microstep_dropdown = lv_dropdown_create(microstep_row);
    lv_dropdown_set_options(s_screen.microstep_dropdown, "1/8\n1/16\n1/32");
    lv_dropdown_set_selected(s_screen.microstep_dropdown, 1);
    lv_obj_set_size(s_screen.microstep_dropdown, 150, 38);

    widget_stat_row_create(machine_panel, &s_screen.steps_row, "Steps per mm");
    widget_stat_row_create(machine_panel, &s_screen.max_speed_row, "Max speed");
    widget_stat_row_create(machine_panel, &s_screen.accel_row, "Max acceleration");

    lv_obj_t *polarity_row = create_labeled_control(machine_panel, "Sensor polarity");
    s_screen.polarity_dropdown = lv_dropdown_create(polarity_row);
    lv_dropdown_set_options(s_screen.polarity_dropdown, "Active low\nActive high");
    lv_obj_set_size(s_screen.polarity_dropdown, 150, 38);

    widget_stat_row_set_value(&s_screen.steps_row, "80.0", HMI_COLOR_NEUTRAL);
    widget_stat_row_set_value(&s_screen.max_speed_row, "8.0 rps", HMI_COLOR_NEUTRAL);
    widget_stat_row_set_value(&s_screen.accel_row, "12.0 rps/s", HMI_COLOR_NEUTRAL);
}

static void set_control_locked(lv_obj_t *obj, bool locked)
{
    if (obj == NULL) {
        return;
    }

    if (locked) {
        lv_obj_add_state(obj, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(obj, LV_STATE_DISABLED);
    }
}

void screen_settings_update(const hmi_state_t *state)
{
    if (state == NULL || s_screen.root == NULL) {
        return;
    }

    bool locked = state->machine_state == HMI_MACHINE_RUNNING || state->machine_state == HMI_MACHINE_PAUSED;

    widget_status_badge_update(&s_screen.badge, state->machine_state);
    lv_label_set_text(s_screen.banner, locked ? "READ ONLY - machine is running" : "Demo values only. No persistent settings are written.");
    lv_obj_set_style_text_color(s_screen.banner, locked ? hmi_palette_get()->amber : hmi_palette_get()->text_dim, 0);
    lv_obj_set_style_bg_opa(s_screen.banner, locked ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(s_screen.banner, hmi_palette_get()->panel_secondary, 0);
    lv_obj_set_style_border_width(s_screen.banner, locked ? 1 : 0, 0);
    lv_obj_set_style_border_color(s_screen.banner, hmi_palette_get()->amber, 0);

    set_control_locked(s_screen.units_dropdown, locked);
    set_control_locked(s_screen.brightness_slider, locked);
    set_control_locked(s_screen.microstep_dropdown, locked);
    set_control_locked(s_screen.polarity_dropdown, locked);
}
