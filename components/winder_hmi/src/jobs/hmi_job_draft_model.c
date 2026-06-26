#include "hmi_job_draft_model.h"

#include <stdio.h>

#define HMI_JOB_DRAFT_MAX_PARAMS 16

typedef struct {
    uint16_t param_id;
    hmi_param_type_t type;
    hmi_param_value_t value;
    bool active;
} hmi_job_draft_value_t;

static hmi_job_mode_id_t s_selected_mode;
static hmi_job_draft_value_t s_values[HMI_JOB_DRAFT_MAX_PARAMS];
static bool s_dirty;
static hmi_job_validation_t s_validation;

static void set_validation(bool valid, const char *message)
{
    s_validation.valid = valid;
    snprintf(s_validation.message, sizeof(s_validation.message), "%s", message != NULL ? message : "");
}

static void clear_estimates(void)
{
    s_validation.estimated_layers = 0;
    s_validation.estimated_time_min = 0.0f;
    s_validation.estimated_offset_mm = 0.0f;
}

static hmi_job_draft_value_t *find_value(uint16_t param_id)
{
    for (size_t i = 0; i < sizeof(s_values) / sizeof(s_values[0]); i++) {
        if (s_values[i].active && s_values[i].param_id == param_id) {
            return &s_values[i];
        }
    }
    return NULL;
}

void hmi_job_draft_model_init(void)
{
    s_selected_mode = HMI_JOB_MODE_NONE;
    for (size_t i = 0; i < sizeof(s_values) / sizeof(s_values[0]); i++) {
        s_values[i] = (hmi_job_draft_value_t){0};
    }
    s_dirty = false;
    clear_estimates();
    set_validation(false, "Select a job mode.");
}

bool hmi_job_draft_model_select_mode(hmi_job_mode_id_t mode_id)
{
    const hmi_mode_capability_t *mode = hmi_capability_model_get_mode_by_id(mode_id);
    if (mode == NULL || !mode->enabled || mode->param_count > HMI_JOB_DRAFT_MAX_PARAMS) {
        return false;
    }

    for (size_t i = 0; i < sizeof(s_values) / sizeof(s_values[0]); i++) {
        s_values[i] = (hmi_job_draft_value_t){0};
    }

    s_selected_mode = mode_id;
    for (size_t i = 0; i < mode->param_count; i++) {
        s_values[i].param_id = mode->params[i].id;
        s_values[i].type = mode->params[i].type;
        s_values[i].value = mode->params[i].default_value;
        s_values[i].active = true;
    }

    s_dirty = true;
    clear_estimates();
    set_validation(false, "Parameters are ready. Press Validate to check the job.");
    return true;
}

hmi_job_mode_id_t hmi_job_draft_model_get_mode(void)
{
    return s_selected_mode;
}

bool hmi_job_draft_model_get_value(uint16_t param_id, hmi_param_value_t *out)
{
    if (out == NULL) {
        return false;
    }

    hmi_job_draft_value_t *value = find_value(param_id);
    if (value == NULL) {
        return false;
    }

    *out = value->value;
    return true;
}

bool hmi_job_draft_model_set_value(uint16_t param_id, hmi_param_value_t value)
{
    hmi_job_draft_value_t *slot = find_value(param_id);
    const hmi_param_descriptor_t *descriptor =
        hmi_capability_model_get_param_by_id(s_selected_mode, param_id);
    if (slot == NULL || descriptor == NULL || slot->type != descriptor->type) {
        return false;
    }

    char message[96];
    if (!hmi_param_validate_value(descriptor, value, message, sizeof(message))) {
        clear_estimates();
        set_validation(false, message);
        return false;
    }

    slot->value = value;
    hmi_job_draft_model_mark_dirty("Parameters changed. Press Validate to re-check the job.");
    return true;
}

bool hmi_job_draft_model_step_value(uint16_t param_id, int direction)
{
    hmi_param_value_t current;
    hmi_param_value_t next;
    const hmi_param_descriptor_t *descriptor =
        hmi_capability_model_get_param_by_id(s_selected_mode, param_id);

    if (descriptor == NULL || !hmi_job_draft_model_get_value(param_id, &current)) {
        return false;
    }
    if (!hmi_param_step_value(descriptor, current, direction, &next)) {
        return false;
    }

    return hmi_job_draft_model_set_value(param_id, next);
}

bool hmi_job_draft_model_is_dirty(void)
{
    return s_dirty;
}

void hmi_job_draft_model_mark_dirty(const char *message)
{
    s_dirty = true;
    clear_estimates();
    set_validation(false, message != NULL ? message : "Press Validate to check the job.");
}

bool hmi_job_draft_model_validate_local(void)
{
    const hmi_mode_capability_t *mode = hmi_capability_model_get_mode_by_id(s_selected_mode);
    if (mode == NULL || !mode->enabled) {
        clear_estimates();
        set_validation(false, "Select an enabled job mode.");
        return false;
    }

    for (size_t i = 0; i < mode->param_count; i++) {
        hmi_param_value_t value;
        char message[96];
        if (!hmi_job_draft_model_get_value(mode->params[i].id, &value)) {
            clear_estimates();
            set_validation(false, "Draft parameter value is missing.");
            return false;
        }
        if (!hmi_param_validate_value(&mode->params[i], value, message, sizeof(message))) {
            clear_estimates();
            set_validation(false, message);
            return false;
        }
    }

    s_dirty = false;
    clear_estimates();
    set_validation(true, "Local draft is valid. Controller validation is still required in production.");
    return true;
}

const hmi_job_validation_t *hmi_job_draft_model_get_validation(void)
{
    return &s_validation;
}

void hmi_job_draft_model_apply_validation_result(const hmi_job_validation_t *validation)
{
    if (validation == NULL) {
        return;
    }

    s_validation = *validation;
    s_dirty = !validation->valid;
}

void hmi_job_draft_model_set_estimates(uint32_t layers, float time_min, float offset_mm)
{
    s_validation.estimated_layers = layers;
    s_validation.estimated_time_min = time_min;
    s_validation.estimated_offset_mm = offset_mm;
}
