#include "screen_home.h"

#include <stdio.h>

#include "hmi_actions.h"
#include "hmi_styles.h"
#include "hmi_types.h"
#include "widget_stat_row.h"
#include "widget_topbar.h"

typedef struct {
    lv_obj_t *root;
    hmi_topbar_t topbar;
    lv_obj_t *status_label;
    lv_obj_t *explanation_label;
    hmi_stat_row_t mode_row;
    hmi_stat_row_t homing_row;
    hmi_stat_row_t job_row;
    hmi_stat_row_t unwound_row;
    hmi_stat_row_t error_row;
    hmi_stat_row_t safety_row;
    lv_obj_t *primary_button;
    lv_obj_t *primary_label;
} home_screen_t;

typedef enum {
    HOME_NAV_JOBS = 0,
    HOME_NAV_MACHINE,
    HOME_NAV_DIAGNOSTICS,
    HOME_NAV_SETTINGS,
} home_nav_action_t;

static home_screen_t s_home;

static const char *machine_big_text(hmi_machine_state_t state)
{
    switch (state) {
    case HMI_MACHINE_BOOTING:
        return "BOOTING";
    case HMI_MACHINE_HOMING_REQUIRED:
        return "HOMING REQUIRED";
    case HMI_MACHINE_READY:
        return "READY";
    case HMI_MACHINE_RUNNING:
        return "RUNNING";
    case HMI_MACHINE_PAUSED:
        return "PAUSED";
    case HMI_MACHINE_STOPPING:
        return "STOPPING";
    case HMI_MACHINE_FINISHED:
        return "FINISHED";
    case HMI_MACHINE_ALARM:
        return "ALARM";
    default:
        return "UNKNOWN";
    }
}

static const char *machine_explanation_text(const hmi_state_t *state)
{
    switch (state->machine_state) {
    case HMI_MACHINE_BOOTING:
        return "HMI runtime is starting. Waiting for the application state.";
    case HMI_MACHINE_HOMING_REQUIRED:
        return "Machine position is unknown. Homing is required before automatic winding.";
    case HMI_MACHINE_READY:
        if (state->job_state == HMI_JOB_VALID) {
            return "Machine is homed. A valid winding job is ready to start.";
        }
        return "Machine is homed. Configure a winding job to start.";
    case HMI_MACHINE_RUNNING:
    case HMI_MACHINE_PAUSED:
        return "A winding job is in progress.";
    case HMI_MACHINE_STOPPING:
        return "The machine is stopping the active winding job.";
    case HMI_MACHINE_FINISHED:
        return "The winding job has finished. Review results before starting a new job.";
    case HMI_MACHINE_ALARM:
        return "Machine is in alarm state. Check the alarm details before continuing.";
    default:
        return "Waiting for system state.";
    }
}

static const char *homing_text(hmi_homing_state_t state)
{
    switch (state) {
    case HMI_HOMING_REQUIRED:
        return "Required";
    case HMI_HOMING_IN_PROGRESS:
        return "In progress";
    case HMI_HOMING_OK:
        return "OK";
    case HMI_HOMING_FAILED:
        return "Failed";
    default:
        return "Unknown";
    }
}

static hmi_color_role_t homing_color(hmi_homing_state_t state)
{
    switch (state) {
    case HMI_HOMING_OK:
        return HMI_COLOR_GREEN;
    case HMI_HOMING_IN_PROGRESS:
    case HMI_HOMING_REQUIRED:
        return HMI_COLOR_AMBER;
    case HMI_HOMING_FAILED:
        return HMI_COLOR_RED;
    default:
        return HMI_COLOR_DIM;
    }
}

static const char *job_text(hmi_job_state_t state)
{
    switch (state) {
    case HMI_JOB_NOT_CONFIGURED:
        return "Not configured";
    case HMI_JOB_VALID:
        return "Valid";
    case HMI_JOB_INVALID:
        return "Invalid";
    default:
        return "Unknown";
    }
}

static hmi_color_role_t job_color(hmi_job_state_t state)
{
    switch (state) {
    case HMI_JOB_VALID:
        return HMI_COLOR_GREEN;
    case HMI_JOB_NOT_CONFIGURED:
        return HMI_COLOR_AMBER;
    case HMI_JOB_INVALID:
        return HMI_COLOR_RED;
    default:
        return HMI_COLOR_DIM;
    }
}

static const char *primary_text_for_state(const hmi_state_t *state)
{
    if (state->machine_state == HMI_MACHINE_HOMING_REQUIRED) {
        return "OPEN HOMING";
    }
    if (state->machine_state == HMI_MACHINE_READY && state->job_state == HMI_JOB_VALID) {
        return "CONFIRM START";
    }
    if (state->machine_state == HMI_MACHINE_RUNNING ||
        state->machine_state == HMI_MACHINE_PAUSED ||
        state->machine_state == HMI_MACHINE_STOPPING) {
        return "OPEN RUN SCREEN";
    }
    if (state->machine_state == HMI_MACHINE_ALARM) {
        return "VIEW ALARM";
    }
    return "CONFIGURE JOB";
}

static hmi_color_role_t machine_color_for_state(hmi_machine_state_t state)
{
    switch (state) {
    case HMI_MACHINE_READY:
    case HMI_MACHINE_RUNNING:
    case HMI_MACHINE_FINISHED:
        return HMI_COLOR_GREEN;
    case HMI_MACHINE_HOMING_REQUIRED:
    case HMI_MACHINE_PAUSED:
    case HMI_MACHINE_STOPPING:
    case HMI_MACHINE_BOOTING:
        return HMI_COLOR_AMBER;
    case HMI_MACHINE_ALARM:
        return HMI_COLOR_RED;
    default:
        return HMI_COLOR_NEUTRAL;
    }
}

static void primary_button_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_home_primary();
}

static void nav_event_cb(lv_event_t *event)
{
    home_nav_action_t action = (home_nav_action_t)(intptr_t)lv_event_get_user_data(event);
    switch (action) {
    case HOME_NAV_JOBS:
        hmi_actions_open_jobs();
        break;
    case HOME_NAV_MACHINE:
        hmi_actions_placeholder_machine();
        break;
    case HOME_NAV_DIAGNOSTICS:
        hmi_actions_open_diagnostics();
        break;
    case HOME_NAV_SETTINGS:
        hmi_actions_open_settings();
        break;
    default:
        hmi_actions_placeholder_machine();
        break;
    }
}

static lv_obj_t *create_panel(lv_obj_t *parent, const char *title_text)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &styles->panel, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &styles->panel_title, 0);
    lv_label_set_text(title, title_text);

    return panel;
}

static lv_obj_t *create_nav_button(lv_obj_t *parent, const char *text, bool enabled, home_nav_action_t action)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->nav_button, 0);
    lv_obj_set_size(button, HMI_DISPLAY_WIDTH / 5, HMI_BOTTOM_NAV_HEIGHT);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, hmi_palette_get()->text, LV_STATE_PRESSED);

    if (enabled) {
        lv_obj_add_event_cb(button, nav_event_cb, LV_EVENT_CLICKED, (void *)(intptr_t)action);
    } else {
        lv_obj_clear_flag(button, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_text_color(button, hmi_palette_get()->text_muted, 0);
    }

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return button;
}

void screen_home_create(lv_obj_t *root)
{
    hmi_styles_t *styles = hmi_styles_get();
    s_home = (home_screen_t){0};

    s_home.root = root;
    lv_obj_remove_style_all(root);
    lv_obj_add_style(root, &styles->root, 0);
    lv_obj_set_size(root, HMI_DISPLAY_WIDTH, HMI_DISPLAY_HEIGHT);
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    widget_topbar_create(root, &s_home.topbar);

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), HMI_DISPLAY_HEIGHT - HMI_TOPBAR_HEIGHT - HMI_BOTTOM_NAV_HEIGHT - 70);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(content, 14, 0);
    lv_obj_set_style_pad_row(content, 12, 0);
    lv_obj_set_style_pad_column(content, 12, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *left_panel = create_panel(content, "MACHINE STATUS");
    lv_obj_set_size(left_panel, 444, 258);
    lv_obj_set_style_pad_row(left_panel, 12, 0);
    lv_obj_set_flex_align(left_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    s_home.status_label = lv_label_create(left_panel);
    lv_obj_add_style(s_home.status_label, &styles->status_big, 0);
    lv_label_set_long_mode(s_home.status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_home.status_label, LV_PCT(100));

    s_home.explanation_label = lv_label_create(left_panel);
    lv_obj_add_style(s_home.explanation_label, &styles->status_text, 0);
    lv_label_set_long_mode(s_home.explanation_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_home.explanation_label, LV_PCT(100));

    lv_obj_t *right_panel = create_panel(content, "SYSTEM STATUS");
    lv_obj_set_size(right_panel, 304, 258);
    lv_obj_set_style_pad_row(right_panel, 0, 0);

    widget_stat_row_create(right_panel, &s_home.mode_row, "Selected mode");
    widget_stat_row_create(right_panel, &s_home.homing_row, "Homing status");
    widget_stat_row_create(right_panel, &s_home.job_row, "Job status");
    widget_stat_row_create(right_panel, &s_home.unwound_row, "Unwound fiber");
    widget_stat_row_create(right_panel, &s_home.error_row, "Last error");
    widget_stat_row_create(right_panel, &s_home.safety_row, "Safety circuit");

    lv_obj_t *action_row = lv_obj_create(root);
    lv_obj_remove_style_all(action_row);
    lv_obj_set_size(action_row, LV_PCT(100), 70);
    lv_obj_set_flex_flow(action_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(action_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(action_row, LV_OBJ_FLAG_SCROLLABLE);

    s_home.primary_button = lv_btn_create(action_row);
    lv_obj_remove_style_all(s_home.primary_button);
    lv_obj_add_style(s_home.primary_button, &styles->primary_button, 0);
    lv_obj_set_size(s_home.primary_button, 300, 58);
    lv_obj_set_style_bg_color(s_home.primary_button, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);

    s_home.primary_label = lv_label_create(s_home.primary_button);
    lv_label_set_text(s_home.primary_label, "CONFIGURE JOB");
    lv_obj_center(s_home.primary_label);
    lv_obj_add_event_cb(s_home.primary_button, primary_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *nav = lv_obj_create(root);
    lv_obj_remove_style_all(nav);
    lv_obj_set_size(nav, LV_PCT(100), HMI_BOTTOM_NAV_HEIGHT);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_color(nav, hmi_palette_get()->device, 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(nav, hmi_palette_get()->border, 0);
    lv_obj_set_style_border_width(nav, 2, 0);
    lv_obj_set_style_border_side(nav, LV_BORDER_SIDE_TOP, 0);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);

    create_nav_button(nav, "Jobs", true, HOME_NAV_JOBS);
    create_nav_button(nav, "Machine", true, HOME_NAV_MACHINE);
    create_nav_button(nav, "Manual", false, HOME_NAV_MACHINE);
    create_nav_button(nav, "Diagnostics", true, HOME_NAV_DIAGNOSTICS);
    create_nav_button(nav, "Settings", true, HOME_NAV_SETTINGS);
}

void screen_home_update(const hmi_state_t *state)
{
    if (state == NULL || s_home.root == NULL) {
        return;
    }

    char value_buf[32];

    widget_topbar_update(&s_home.topbar, state);

    lv_label_set_text(s_home.status_label, machine_big_text(state->machine_state));
    lv_obj_set_style_text_color(s_home.status_label, hmi_color_for_role(machine_color_for_state(state->machine_state)), 0);
    lv_label_set_text(s_home.explanation_label, machine_explanation_text(state));

    widget_stat_row_set_value(&s_home.mode_row, state->selected_mode != NULL ? state->selected_mode : "None", HMI_COLOR_BLUE);
    widget_stat_row_set_value(&s_home.homing_row, homing_text(state->homing_state), homing_color(state->homing_state));
    widget_stat_row_set_value(&s_home.job_row, job_text(state->job_state), job_color(state->job_state));

    snprintf(value_buf, sizeof(value_buf), "%.1f m", (double)state->unwound_length_m);
    widget_stat_row_set_value(&s_home.unwound_row, value_buf, HMI_COLOR_NEUTRAL);
    widget_stat_row_set_value(&s_home.error_row, state->last_error != NULL ? state->last_error : "None", state->last_error != NULL ? HMI_COLOR_RED : HMI_COLOR_DIM);
    widget_stat_row_set_value(&s_home.safety_row, state->safety_ok ? "OK" : "Fault", state->safety_ok ? HMI_COLOR_GREEN : HMI_COLOR_RED);

    lv_label_set_text(s_home.primary_label, primary_text_for_state(state));
    lv_obj_center(s_home.primary_label);
    lv_obj_set_style_bg_color(s_home.primary_button, hmi_color_for_role(machine_color_for_state(state->machine_state)), 0);
}
