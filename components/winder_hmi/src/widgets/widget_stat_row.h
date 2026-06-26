#pragma once

#include "lvgl.h"
#include "hmi_types.h"

typedef struct {
    lv_obj_t *root;
    lv_obj_t *label;
    lv_obj_t *value;
} hmi_stat_row_t;

void widget_stat_row_create(lv_obj_t *parent, hmi_stat_row_t *row, const char *label_text);
void widget_stat_row_set_value(hmi_stat_row_t *row, const char *value_text, hmi_color_role_t color);
