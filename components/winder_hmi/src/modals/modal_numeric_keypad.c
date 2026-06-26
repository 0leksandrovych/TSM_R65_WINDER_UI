#include "modal_numeric_keypad.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "hmi_styles.h"

#define INPUT_BUFFER_LEN 16

static const char *TAG = "numkeypad";

typedef struct {
    lv_obj_t *overlay;
    lv_obj_t *input_label;
    lv_obj_t *error_label;
    lv_obj_t *decimal_button;
    modal_numeric_keypad_config_t config;
    char input[INPUT_BUFFER_LEN];
} numeric_keypad_t;

static numeric_keypad_t s_keypad;

static void reset_keypad_state(void)
{
    s_keypad = (numeric_keypad_t){0};
}

void modal_numeric_keypad_close(void)
{
    lv_obj_t *overlay = s_keypad.overlay;

    reset_keypad_state();

    if (overlay != NULL && lv_obj_is_valid(overlay)) {
        lv_obj_del_async(overlay);
    }
}

static void overlay_delete_event_cb(lv_event_t *event)
{
    lv_obj_t *target = lv_event_get_target(event);

    if (target == s_keypad.overlay) {
        reset_keypad_state();
    }
}

static void update_input_label(void)
{
    if (s_keypad.input_label != NULL) {
        lv_label_set_text(s_keypad.input_label, s_keypad.input[0] != '\0' ? s_keypad.input : "--");
    }
    if (s_keypad.error_label != NULL) {
        lv_label_set_text(s_keypad.error_label, "");
    }
}

static void set_error(const char *message)
{
    if (s_keypad.error_label != NULL) {
        lv_label_set_text(s_keypad.error_label, message != NULL ? message : "");
    }
}

static void append_char(char ch)
{
    size_t len = strlen(s_keypad.input);
    if (len + 1 >= sizeof(s_keypad.input)) {
        set_error("Input is too long.");
        return;
    }
    if (ch == '.' && s_keypad.config.integer_only) {
        return;
    }
    if (ch == '.' && strchr(s_keypad.input, '.') != NULL) {
        return;
    }
    if (ch == '.' && len == 0) {
        s_keypad.input[len++] = '0';
    }
    s_keypad.input[len] = ch;
    s_keypad.input[len + 1] = '\0';
    update_input_label();
}

static void backspace_input(void)
{
    size_t len = strlen(s_keypad.input);
    if (len > 0) {
        s_keypad.input[len - 1] = '\0';
    }
    update_input_label();
}

static bool parse_input(float *value, uint32_t *u32_value)
{
    if (s_keypad.input[0] == '\0') {
        set_error("Enter a value.");
        return false;
    }
    if (strcmp(s_keypad.input, ".") == 0 || strcmp(s_keypad.input, "0.") == 0) {
        set_error("Enter a complete value.");
        return false;
    }

    errno = 0;
    char *end = NULL;
    float parsed = strtof(s_keypad.input, &end);
    if (errno != 0 || end == s_keypad.input || *end != '\0') {
        set_error("Invalid numeric value.");
        return false;
    }

    if (parsed < s_keypad.config.min_value || parsed > s_keypad.config.max_value) {
        char message[80];
        snprintf(message, sizeof(message), "Range %.2f to %.2f %s.",
                 (double)s_keypad.config.min_value,
                 (double)s_keypad.config.max_value,
                 s_keypad.config.unit != NULL ? s_keypad.config.unit : "");
        set_error(message);
        return false;
    }

    if (s_keypad.config.integer_only) {
        if (strchr(s_keypad.input, '.') != NULL) {
            set_error("Whole number required.");
            return false;
        }
        *u32_value = (uint32_t)(parsed + 0.5f);
        *value = (float)*u32_value;
    } else {
        *u32_value = 0;
        *value = parsed;
    }

    return true;
}

static void key_event_cb(lv_event_t *event)
{
    const char *text = (const char *)lv_event_get_user_data(event);
    if (text == NULL) {
        return;
    }

    if (strcmp(text, "back") == 0) {
        backspace_input();
        return;
    }

    append_char(text[0]);
}

static void cancel_event_cb(lv_event_t *event)
{
    (void)event;
    modal_numeric_keypad_close();
}

static void ok_event_cb(lv_event_t *event)
{
    (void)event;

    float value = 0.0f;
    uint32_t u32_value = 0;
    if (!parse_input(&value, &u32_value)) {
        return;
    }

    modal_numeric_keypad_apply_cb_t apply_cb = s_keypad.config.apply_cb;
    void *user_ctx = s_keypad.config.user_ctx;
    modal_numeric_keypad_close();

    if (apply_cb != NULL) {
        apply_cb(value, u32_value, user_ctx);
    }
}

static lv_obj_t *create_button(lv_obj_t *parent, const char *text, int32_t width, int32_t height,
                               lv_event_cb_t cb, const void *user_data)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->nav_button, 0);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->border_strong, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, hmi_palette_get()->text, 0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, (void *)user_data);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return button;
}

static lv_obj_t *create_action_button(lv_obj_t *parent, const char *text, hmi_color_role_t role, lv_event_cb_t cb)
{
    hmi_styles_t *styles = hmi_styles_get();

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_add_style(button, &styles->primary_button, 0);
    lv_obj_set_size(button, 154, 46);
    lv_obj_set_style_bg_color(button, hmi_color_for_role(role), 0);
    lv_obj_set_style_bg_color(button, hmi_palette_get()->panel_secondary, LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, role == HMI_COLOR_AMBER ? hmi_palette_get()->device : lv_color_white(), 0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return button;
}

void modal_numeric_keypad_open(lv_obj_t *parent, const modal_numeric_keypad_config_t *config)
{
    (void)parent;

    if (config == NULL) {
        return;
    }

    modal_numeric_keypad_close();
    s_keypad.config = *config;
    if (s_keypad.config.integer_only) {
        snprintf(s_keypad.input, sizeof(s_keypad.input), "%lu", (unsigned long)(config->initial_value + 0.5f));
    } else {
        snprintf(s_keypad.input, sizeof(s_keypad.input), "%.*f", (int)config->decimals, (double)config->initial_value);
    }

    hmi_styles_t *styles = hmi_styles_get();

    s_keypad.overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_keypad.overlay);
    lv_obj_set_size(s_keypad.overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_keypad.overlay, hmi_palette_get()->bg, 0);
    lv_obj_set_style_bg_opa(s_keypad.overlay, LV_OPA_70, 0);
    lv_obj_clear_flag(s_keypad.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(s_keypad.overlay);
    lv_obj_add_event_cb(s_keypad.overlay, overlay_delete_event_cb, LV_EVENT_DELETE, NULL);

    lv_obj_t *modal = lv_obj_create(s_keypad.overlay);
    lv_obj_remove_style_all(modal);
    lv_obj_add_style(modal, &styles->panel, 0);
    lv_obj_set_size(modal, 420, 390);
    lv_obj_center(modal);
    lv_obj_set_flex_flow(modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(modal, 8, 0);
    lv_obj_clear_flag(modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(modal);
    lv_obj_add_style(title, &styles->topbar_title, 0);
    lv_label_set_text(title, config->title != NULL ? config->title : "EDIT VALUE");

    s_keypad.input_label = lv_label_create(modal);
    lv_obj_add_style(s_keypad.input_label, &styles->status_text, 0);
    lv_obj_set_width(s_keypad.input_label, LV_PCT(100));
    lv_obj_set_style_text_align(s_keypad.input_label, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *hint = lv_label_create(modal);
    lv_obj_add_style(hint, &styles->topbar_text, 0);
    lv_obj_set_width(hint, LV_PCT(100));
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    char hint_text[96];
    snprintf(hint_text, sizeof(hint_text), "Min %.2f   Max %.2f %s",
             (double)config->min_value,
             (double)config->max_value,
             config->unit != NULL ? config->unit : "");
    lv_label_set_text(hint, hint_text);

    lv_obj_t *grid = lv_obj_create(modal);
    lv_obj_remove_style_all(grid);
    lv_obj_set_size(grid, 330, 202);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(grid, 8, 0);
    lv_obj_set_style_pad_column(grid, 8, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_align(grid, LV_ALIGN_CENTER, 0);

    static const char *keys[] = {"7", "8", "9", "4", "5", "6", "1", "2", "3", "0", ".", "back"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        const char *button_text = strcmp(keys[i], "back") == 0 ? "<" : keys[i];
        lv_obj_t *button = create_button(grid, button_text, 100, 44, key_event_cb, keys[i]);
        if (strcmp(keys[i], ".") == 0) {
            s_keypad.decimal_button = button;
            if (config->integer_only) {
                lv_obj_add_state(button, LV_STATE_DISABLED);
                lv_obj_set_style_opa(button, LV_OPA_40, 0);
            }
        }
    }

    s_keypad.error_label = lv_label_create(modal);
    lv_obj_add_style(s_keypad.error_label, &styles->topbar_text, 0);
    lv_obj_set_width(s_keypad.error_label, LV_PCT(100));
    lv_obj_set_style_text_align(s_keypad.error_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_keypad.error_label, hmi_palette_get()->red, 0);
    lv_label_set_text(s_keypad.error_label, "");

    lv_obj_t *actions = lv_obj_create(modal);
    lv_obj_remove_style_all(actions);
    lv_obj_set_size(actions, LV_PCT(100), 48);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(actions, 12, 0);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);

    create_action_button(actions, "CANCEL", HMI_COLOR_DIM, cancel_event_cb);
    create_action_button(actions, "OK", HMI_COLOR_GREEN, ok_event_cb);

    update_input_label();
    ESP_LOGI(TAG, "Numeric keypad opened");
}
