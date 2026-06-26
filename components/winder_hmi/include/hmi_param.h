#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    HMI_PARAM_TYPE_FLOAT,
    HMI_PARAM_TYPE_UINT32,
    HMI_PARAM_TYPE_BOOL,
    HMI_PARAM_TYPE_ENUM
} hmi_param_type_t;

typedef union {
    float f32;
    uint32_t u32;
    bool boolean;
    int32_t enum_value;
} hmi_param_value_t;

typedef struct {
    uint16_t id;
    const char *key;
    const char *label;
    const char *unit;

    hmi_param_type_t type;

    hmi_param_value_t default_value;
    hmi_param_value_t min_value;
    hmi_param_value_t max_value;
    hmi_param_value_t step;

    uint8_t decimals;
    bool editable;
    bool visible;
} hmi_param_descriptor_t;

bool hmi_param_format_value(const hmi_param_descriptor_t *descriptor,
                            hmi_param_value_t value,
                            char *buffer,
                            size_t buffer_size,
                            bool include_unit);
bool hmi_param_validate_value(const hmi_param_descriptor_t *descriptor,
                              hmi_param_value_t value,
                              char *message,
                              size_t message_size);
bool hmi_param_step_value(const hmi_param_descriptor_t *descriptor,
                          hmi_param_value_t current,
                          int direction,
                          hmi_param_value_t *out);
bool hmi_param_from_keypad_float(const hmi_param_descriptor_t *descriptor,
                                 float value,
                                 uint32_t u32_value,
                                 hmi_param_value_t *out);
float hmi_param_value_as_float(const hmi_param_descriptor_t *descriptor, hmi_param_value_t value);
bool hmi_param_type_uses_integer_keypad(hmi_param_type_t type);
