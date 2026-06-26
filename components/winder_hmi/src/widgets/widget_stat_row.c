#include "widget_stat_row.h"

#include "hmi_styles.h"

void widget_stat_row_create(lv_obj_t *parent, hmi_stat_row_t *row, const char *label_text)
{
    if (row == NULL) {
        return;
    }

    hmi_styles_t *styles = hmi_styles_get();

    row->root = lv_obj_create(parent);
    lv_obj_remove_style_all(row->root);
    lv_obj_set_size(row->root, LV_PCT(100), 32);
    lv_obj_set_flex_flow(row->root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row->root, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_side(row->root, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row->root, 1, 0);
    lv_obj_set_style_border_color(row->root, hmi_palette_get()->border, 0);
    lv_obj_set_style_pad_left(row->root, 0, 0);
    lv_obj_set_style_pad_right(row->root, 0, 0);

    row->label = lv_label_create(row->root);
    lv_obj_add_style(row->label, &styles->stat_label, 0);
    lv_label_set_text(row->label, label_text != NULL ? label_text : "");

    row->value = lv_label_create(row->root);
    lv_obj_add_style(row->value, &styles->stat_value, 0);
    lv_label_set_text(row->value, "--");
    lv_label_set_long_mode(row->value, LV_LABEL_LONG_DOT);
    lv_obj_set_width(row->value, 170);
    lv_obj_set_style_text_align(row->value, LV_TEXT_ALIGN_RIGHT, 0);
}

void widget_stat_row_set_value(hmi_stat_row_t *row, const char *value_text, hmi_color_role_t color)
{
    if (row == NULL || row->value == NULL) {
        return;
    }

    lv_label_set_text(row->value, value_text != NULL ? value_text : "");
    lv_obj_set_style_text_color(row->value, hmi_color_for_role(color), 0);
}
