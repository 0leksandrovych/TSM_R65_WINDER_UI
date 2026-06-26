#include "hmi_param.h"

#include <stdio.h>
#include <string.h>

static const char *unit_text(const hmi_param_descriptor_t *descriptor)
{
    return descriptor != NULL && descriptor->unit != NULL ? descriptor->unit : "";
}

static void set_message(char *message, size_t message_size, const char *text)
{
    if (message == NULL || message_size == 0) {
        return;
    }
    snprintf(message, message_size, "%s", text != NULL ? text : "");
}

float hmi_param_value_as_float(const hmi_param_descriptor_t *descriptor, hmi_param_value_t value)
{
    if (descriptor == NULL) {
        return 0.0f;
    }

    switch (descriptor->type) {
    case HMI_PARAM_TYPE_FLOAT:
        return value.f32;
    case HMI_PARAM_TYPE_UINT32:
        return (float)value.u32;
    case HMI_PARAM_TYPE_BOOL:
        return value.boolean ? 1.0f : 0.0f;
    case HMI_PARAM_TYPE_ENUM:
        return (float)value.enum_value;
    default:
        return 0.0f;
    }
}

bool hmi_param_type_uses_integer_keypad(hmi_param_type_t type)
{
    return type == HMI_PARAM_TYPE_UINT32 || type == HMI_PARAM_TYPE_BOOL || type == HMI_PARAM_TYPE_ENUM;
}

bool hmi_param_format_value(const hmi_param_descriptor_t *descriptor,
                            hmi_param_value_t value,
                            char *buffer,
                            size_t buffer_size,
                            bool include_unit)
{
    if (descriptor == NULL || buffer == NULL || buffer_size == 0) {
        return false;
    }

    const char *unit = include_unit ? unit_text(descriptor) : "";
    const bool has_unit = include_unit && unit[0] != '\0';

    switch (descriptor->type) {
    case HMI_PARAM_TYPE_FLOAT:
        snprintf(buffer, buffer_size, has_unit ? "%.*f %s" : "%.*f",
                 (int)descriptor->decimals, (double)value.f32, unit);
        return true;
    case HMI_PARAM_TYPE_UINT32:
        snprintf(buffer, buffer_size, has_unit ? "%lu %s" : "%lu",
                 (unsigned long)value.u32, unit);
        return true;
    case HMI_PARAM_TYPE_BOOL:
        snprintf(buffer, buffer_size, "%s", value.boolean ? "On" : "Off");
        return true;
    case HMI_PARAM_TYPE_ENUM:
        snprintf(buffer, buffer_size, "%ld", (long)value.enum_value);
        return true;
    default:
        snprintf(buffer, buffer_size, "--");
        return false;
    }
}

bool hmi_param_validate_value(const hmi_param_descriptor_t *descriptor,
                              hmi_param_value_t value,
                              char *message,
                              size_t message_size)
{
    if (descriptor == NULL) {
        set_message(message, message_size, "Parameter descriptor is missing.");
        return false;
    }

    switch (descriptor->type) {
    case HMI_PARAM_TYPE_FLOAT:
        if (value.f32 < descriptor->min_value.f32 || value.f32 > descriptor->max_value.f32) {
            snprintf(message, message_size, "%s must be in range %.*f-%.*f %s.",
                     descriptor->label != NULL ? descriptor->label : "Value",
                     (int)descriptor->decimals, (double)descriptor->min_value.f32,
                     (int)descriptor->decimals, (double)descriptor->max_value.f32,
                     unit_text(descriptor));
            return false;
        }
        break;
    case HMI_PARAM_TYPE_UINT32:
        if (value.u32 < descriptor->min_value.u32 || value.u32 > descriptor->max_value.u32) {
            snprintf(message, message_size, "%s must be in range %lu-%lu %s.",
                     descriptor->label != NULL ? descriptor->label : "Value",
                     (unsigned long)descriptor->min_value.u32,
                     (unsigned long)descriptor->max_value.u32,
                     unit_text(descriptor));
            return false;
        }
        break;
    case HMI_PARAM_TYPE_BOOL:
        break;
    case HMI_PARAM_TYPE_ENUM:
        if (value.enum_value < descriptor->min_value.enum_value ||
            value.enum_value > descriptor->max_value.enum_value) {
            snprintf(message, message_size, "%s is outside the supported options.",
                     descriptor->label != NULL ? descriptor->label : "Value");
            return false;
        }
        break;
    default:
        set_message(message, message_size, "Unsupported parameter type.");
        return false;
    }

    set_message(message, message_size, "");
    return true;
}

bool hmi_param_step_value(const hmi_param_descriptor_t *descriptor,
                          hmi_param_value_t current,
                          int direction,
                          hmi_param_value_t *out)
{
    if (descriptor == NULL || out == NULL || direction == 0) {
        return false;
    }

    hmi_param_value_t next = current;
    const int dir = direction > 0 ? 1 : -1;

    switch (descriptor->type) {
    case HMI_PARAM_TYPE_FLOAT:
        next.f32 = current.f32 + ((float)dir * descriptor->step.f32);
        if (next.f32 < descriptor->min_value.f32) {
            next.f32 = descriptor->min_value.f32;
        }
        if (next.f32 > descriptor->max_value.f32) {
            next.f32 = descriptor->max_value.f32;
        }
        break;
    case HMI_PARAM_TYPE_UINT32:
        if (dir > 0) {
            uint32_t step = descriptor->step.u32 > 0 ? descriptor->step.u32 : 1U;
            next.u32 = current.u32 > descriptor->max_value.u32 - step ? descriptor->max_value.u32 : current.u32 + step;
        } else {
            uint32_t step = descriptor->step.u32 > 0 ? descriptor->step.u32 : 1U;
            next.u32 = current.u32 < descriptor->min_value.u32 + step ? descriptor->min_value.u32 : current.u32 - step;
        }
        break;
    case HMI_PARAM_TYPE_BOOL:
        next.boolean = !current.boolean;
        break;
    case HMI_PARAM_TYPE_ENUM: {
        int32_t step = descriptor->step.enum_value != 0 ? descriptor->step.enum_value : 1;
        next.enum_value = current.enum_value + ((int32_t)dir * step);
        if (next.enum_value < descriptor->min_value.enum_value) {
            next.enum_value = descriptor->min_value.enum_value;
        }
        if (next.enum_value > descriptor->max_value.enum_value) {
            next.enum_value = descriptor->max_value.enum_value;
        }
        break;
    }
    default:
        return false;
    }

    *out = next;
    return true;
}

bool hmi_param_from_keypad_float(const hmi_param_descriptor_t *descriptor,
                                 float value,
                                 uint32_t u32_value,
                                 hmi_param_value_t *out)
{
    if (descriptor == NULL || out == NULL) {
        return false;
    }

    hmi_param_value_t next = {0};
    switch (descriptor->type) {
    case HMI_PARAM_TYPE_FLOAT:
        next.f32 = value;
        break;
    case HMI_PARAM_TYPE_UINT32:
        next.u32 = u32_value;
        break;
    case HMI_PARAM_TYPE_BOOL:
        next.boolean = u32_value != 0U;
        break;
    case HMI_PARAM_TYPE_ENUM:
        next.enum_value = (int32_t)u32_value;
        break;
    default:
        return false;
    }

    *out = next;
    return true;
}
