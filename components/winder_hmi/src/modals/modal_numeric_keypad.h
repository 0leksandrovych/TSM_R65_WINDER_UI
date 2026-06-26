#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

typedef void (*modal_numeric_keypad_apply_cb_t)(float value, uint32_t u32_value, void *user_ctx);

typedef struct {
    const char *title;
    const char *unit;
    float initial_value;
    float min_value;
    float max_value;
    uint8_t decimals;
    bool integer_only;
    modal_numeric_keypad_apply_cb_t apply_cb;
    void *user_ctx;
} modal_numeric_keypad_config_t;

void modal_numeric_keypad_open(lv_obj_t *parent, const modal_numeric_keypad_config_t *config);
void modal_numeric_keypad_close(void);
