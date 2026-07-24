#include "screen_diagnostics.h"

#include <stdio.h>

#include "hmi_actions.h"
#include "hmi_model.h"
#include "hmi_styles.h"
#include "hmi_types.h"
#include "widget_stat_row.h"
#include "widget_status_badge.h"

typedef struct {
    lv_obj_t *root;
    hmi_status_badge_t badge;
    lv_obj_t *banner;
    hmi_stat_row_t connection_row;
    hmi_stat_row_t machine_state_row;
    hmi_stat_row_t travel_range_row;
    hmi_stat_row_t actual_speed_row;
    hmi_stat_row_t wound_length_row;
    hmi_stat_row_t completed_layers_row;
    hmi_stat_row_t last_error_row;
    hmi_stat_row_t homing_alarm_row;
    hmi_stat_row_t carriage_position_row;
    hmi_stat_row_t left_samples_row;
    hmi_stat_row_t right_samples_row;
    hmi_stat_row_t sample_target_row;
} diagnostics_screen_t;

static diagnostics_screen_t s_screen;

static void back_event_cb(lv_event_t *event)
{
    (void)event;
    hmi_actions_go_home();
}

static const char *connection_text(hmi_connection_state_t connection)
{
    switch (connection) {
    case HMI_CONNECTION_CONNECTING:
        return "Connecting";
    case HMI_CONNECTION_CONNECTED:
        return "Connected";
    case HMI_CONNECTION_LOST:
        return "Lost";
    case HMI_CONNECTION_DISCONNECTED:
    default:
        return "Disconnected";
    }
}

static hmi_color_role_t connection_color(hmi_connection_state_t connection)
{
    switch (connection) {
    case HMI_CONNECTION_CONNECTED:
        return HMI_COLOR_GREEN;
    case HMI_CONNECTION_CONNECTING:
        return HMI_COLOR_AMBER;
    case HMI_CONNECTION_LOST:
        return HMI_COLOR_RED;
    case HMI_CONNECTION_DISCONNECTED:
    default:
        return HMI_COLOR_DIM;
    }
}

static const char *machine_state_text(const hmi_state_t *state)
{
    if (state == NULL || !state->machine_state_known) {
        return "Unknown";
    }

    switch (state->machine_state) {
    case HMI_MACHINE_HOMING_REQUIRED:
        return "Homing required";
    case HMI_MACHINE_HOMING_SEARCHING_LEFT:
        return "Searching left edge";
    case HMI_MACHINE_HOMING_LEFT_BACKOFF:
        return "Left reference backoff";
    case HMI_MACHINE_HOMING_LEFT_MEASUREMENT:
        return "Measuring left edge";
    case HMI_MACHINE_HOMING_MASTER_POSITIONING:
        return "Positioning master";
    case HMI_MACHINE_HOMING_SEARCHING_RIGHT:
        return "Searching right edge";
    case HMI_MACHINE_HOMING_RIGHT_BACKOFF:
        return "Right reference backoff";
    case HMI_MACHINE_HOMING_RIGHT_MEASUREMENT:
        return "Measuring right edge";
    case HMI_MACHINE_HOMING_WAITING_NEXT_MEASUREMENT:
        return "Waiting next measurement";
    case HMI_MACHINE_HOMING_WAITING_ZERO_COMMAND:
        return "Waiting move to zero";
    case HMI_MACHINE_HOMING_MOVING_TO_ZERO:
        return "Moving to calibrated zero";
    case HMI_MACHINE_READY:
        return "Ready";
    case HMI_MACHINE_ACCELERATING:
        return "Accelerating";
    case HMI_MACHINE_RUNNING:
        return "Running";
    case HMI_MACHINE_PAUSED:
        return "Paused";
    case HMI_MACHINE_STOPPING:
        return "Stopping";
    case HMI_MACHINE_FINISHED:
        return "Finished";
    case HMI_MACHINE_ALARM:
        return "Alarm";
    case HMI_MACHINE_POSITIONING:
        return "Positioning carriage";
    default:
        return "Unknown";
    }
}

static hmi_color_role_t machine_state_color(const hmi_state_t *state)
{
    if (state == NULL || !state->machine_state_known) {
        return HMI_COLOR_DIM;
    }

    if (hmi_machine_state_is_homing(state->machine_state)) {
        return HMI_COLOR_BLUE;
    }
    if (hmi_machine_state_is_positioning(state->machine_state)) {
        return HMI_COLOR_AMBER;
    }

    switch (state->machine_state) {
    case HMI_MACHINE_ALARM:
        return HMI_COLOR_RED;
    case HMI_MACHINE_PAUSED:
    case HMI_MACHINE_STOPPING:
    case HMI_MACHINE_HOMING_REQUIRED:
        return HMI_COLOR_AMBER;
    case HMI_MACHINE_READY:
    case HMI_MACHINE_ACCELERATING:
    case HMI_MACHINE_RUNNING:
    case HMI_MACHINE_FINISHED:
        return HMI_COLOR_GREEN;
    default:
        return HMI_COLOR_DIM;
    }
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
    lv_label_set_text(title, "DIAGNOSTICS");

    lv_obj_t *spacer = lv_obj_create(topbar);
    lv_obj_remove_style_all(spacer);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_size(spacer, 0, 1);

    widget_status_badge_create(topbar, &s_screen.badge);
}

void screen_diagnostics_create(lv_obj_t *root)
{
    hmi_styles_t *styles = hmi_styles_get();
    s_screen = (diagnostics_screen_t){0};
    s_screen.root = root;

    create_topbar(root);

    lv_obj_t *content = lv_obj_create(root);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, LV_PCT(100), HMI_DISPLAY_HEIGHT - HMI_TOPBAR_HEIGHT);
    lv_obj_set_style_pad_all(content, 14, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(content, 12, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(content);
    lv_obj_remove_style_all(panel);
    lv_obj_add_style(panel, &styles->panel, 0);
    lv_obj_set_size(panel, 374, LV_PCT(100));
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_obj_add_style(title, &styles->panel_title, 0);
    lv_label_set_text(title, "CONTROLLER STATUS");

    s_screen.banner = lv_label_create(panel);
    lv_obj_add_style(s_screen.banner, &styles->status_text, 0);
    lv_obj_set_width(s_screen.banner, LV_PCT(100));
    lv_obj_set_style_pad_all(s_screen.banner, 9, 0);
    lv_obj_set_style_bg_opa(s_screen.banner, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_screen.banner, 0, 0);
    lv_label_set_text(s_screen.banner, "Live values reported by the main controller.");

    widget_stat_row_create(panel, &s_screen.connection_row, "Controller connection");
    widget_stat_row_create(panel, &s_screen.machine_state_row, "Machine state");
    widget_stat_row_create(panel, &s_screen.travel_range_row, "Measured travel range");
    widget_stat_row_create(panel, &s_screen.actual_speed_row, "Actual master speed");
    widget_stat_row_create(panel, &s_screen.wound_length_row, "Wound length");
    widget_stat_row_create(panel, &s_screen.completed_layers_row, "Completed layers");
    widget_stat_row_create(panel, &s_screen.last_error_row, "Last error");

    lv_obj_t *homing_panel = lv_obj_create(content);
    lv_obj_remove_style_all(homing_panel);
    lv_obj_add_style(homing_panel, &styles->panel, 0);
    lv_obj_set_size(homing_panel, 374, LV_PCT(100));
    lv_obj_set_flex_flow(homing_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(homing_panel, 8, 0);
    lv_obj_clear_flag(homing_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *homing_title = lv_label_create(homing_panel);
    lv_obj_add_style(homing_title, &styles->panel_title, 0);
    lv_label_set_text(homing_title, "HOMING READ MODEL");

    widget_stat_row_create(homing_panel, &s_screen.homing_alarm_row, "Homing alarm");
    widget_stat_row_create(homing_panel, &s_screen.carriage_position_row, "Carriage reference");
    widget_stat_row_create(homing_panel, &s_screen.left_samples_row, "Left edge samples");
    widget_stat_row_create(homing_panel, &s_screen.right_samples_row, "Right edge samples");
    widget_stat_row_create(homing_panel, &s_screen.sample_target_row, "Sample target");
    lv_obj_set_width(s_screen.homing_alarm_row.value, 220);
}

void screen_diagnostics_update(const hmi_state_t *state)
{
    if (state == NULL || s_screen.root == NULL) {
        return;
    }

    char value[96];
    char alarm_buffer[64];
    bool active_job = state->machine_state_known &&
                      (state->machine_state == HMI_MACHINE_ACCELERATING ||
                       state->machine_state == HMI_MACHINE_RUNNING ||
                       state->machine_state == HMI_MACHINE_PAUSED ||
                       state->machine_state == HMI_MACHINE_STOPPING);

    widget_status_badge_update(&s_screen.badge, state);
    lv_label_set_text(
        s_screen.banner,
        active_job ? "READ ONLY - machine is running" :
                     "Live values reported by the main controller.");
    lv_obj_set_style_text_color(
        s_screen.banner,
        active_job ? hmi_palette_get()->amber : hmi_palette_get()->text_dim,
        0);
    lv_obj_set_style_bg_opa(s_screen.banner, active_job ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(s_screen.banner, hmi_palette_get()->panel_secondary, 0);
    lv_obj_set_style_border_width(s_screen.banner, active_job ? 1 : 0, 0);
    lv_obj_set_style_border_color(s_screen.banner, hmi_palette_get()->amber, 0);

    hmi_connection_state_t connection = hmi_model_get_connection_state();
    widget_stat_row_set_value(
        &s_screen.connection_row,
        connection_text(connection),
        connection_color(connection));
    widget_stat_row_set_value(
        &s_screen.machine_state_row,
        machine_state_text(state),
        machine_state_color(state));

    if (state->travel_range_known) {
        snprintf(value, sizeof(value), "%.2f mm", state->travel_range_mm);
        widget_stat_row_set_value(&s_screen.travel_range_row, value, HMI_COLOR_NEUTRAL);
    } else {
        widget_stat_row_set_value(&s_screen.travel_range_row, "--", HMI_COLOR_DIM);
    }

    if (state->master_speed_known) {
        snprintf(value, sizeof(value), "%.2f rps", (double)state->master_speed_rps);
        widget_stat_row_set_value(&s_screen.actual_speed_row, value, HMI_COLOR_GREEN);
    } else {
        widget_stat_row_set_value(&s_screen.actual_speed_row, "--", HMI_COLOR_DIM);
    }

    snprintf(value, sizeof(value), "%.3f m", (double)state->wound_length_m);
    widget_stat_row_set_value(&s_screen.wound_length_row, value, HMI_COLOR_NEUTRAL);

    snprintf(value, sizeof(value), "%lu", (unsigned long)state->current_layer);
    widget_stat_row_set_value(&s_screen.completed_layers_row, value, HMI_COLOR_NEUTRAL);

    if (state->homing_alarm_code_known) {
        const char *alarm_text = hmi_homing_alarm_text(
            state->homing_alarm_code, alarm_buffer, sizeof(alarm_buffer));
        snprintf(value, sizeof(value), "%lu: %s",
                 (unsigned long)state->homing_alarm_code, alarm_text);
        widget_stat_row_set_value(
            &s_screen.homing_alarm_row, value,
            state->homing_alarm_code == 0U ? HMI_COLOR_GREEN : HMI_COLOR_RED);
    } else {
        widget_stat_row_set_value(&s_screen.homing_alarm_row, "--", HMI_COLOR_DIM);
    }

    widget_stat_row_set_value(
        &s_screen.carriage_position_row,
        state->carriage_reference_position_known ?
            hmi_carriage_reference_position_text(state->carriage_reference_position) : "--",
        state->carriage_reference_position_known ? HMI_COLOR_NEUTRAL : HMI_COLOR_DIM);

    if (state->left_edge_sample_count_known) {
        snprintf(value, sizeof(value), "%lu",
                 (unsigned long)state->left_edge_sample_count);
        widget_stat_row_set_value(&s_screen.left_samples_row, value, HMI_COLOR_NEUTRAL);
    } else {
        widget_stat_row_set_value(&s_screen.left_samples_row, "--", HMI_COLOR_DIM);
    }

    if (state->right_edge_sample_count_known) {
        snprintf(value, sizeof(value), "%lu",
                 (unsigned long)state->right_edge_sample_count);
        widget_stat_row_set_value(&s_screen.right_samples_row, value, HMI_COLOR_NEUTRAL);
    } else {
        widget_stat_row_set_value(&s_screen.right_samples_row, "--", HMI_COLOR_DIM);
    }

    if (state->homing_sample_target_count_known) {
        snprintf(value, sizeof(value), "%lu",
                 (unsigned long)state->homing_sample_target_count);
        widget_stat_row_set_value(&s_screen.sample_target_row, value, HMI_COLOR_NEUTRAL);
    } else {
        widget_stat_row_set_value(&s_screen.sample_target_row, "--", HMI_COLOR_DIM);
    }

    widget_stat_row_set_value(
        &s_screen.last_error_row,
        state->last_error != NULL && state->last_error[0] != '\0' ? state->last_error : "None",
        state->last_error != NULL && state->last_error[0] != '\0' ? HMI_COLOR_RED : HMI_COLOR_DIM);
}
