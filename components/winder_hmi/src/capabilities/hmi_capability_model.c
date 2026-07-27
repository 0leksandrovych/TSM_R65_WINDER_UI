#include "hmi_capability_model.h"

#include <string.h>

#include "winder_link_contract.h"

enum {
    HMI_DEMO_PARAM_MASTER_SPEED = 1,
    HMI_DEMO_PARAM_WINDING_PITCH,
    HMI_DEMO_PARAM_TARGET_LENGTH,
    HMI_DEMO_PARAM_SHIFT_EVERY,
    HMI_DEMO_PARAM_RIGHT_EDGE_SHIFT,
};

static bool s_demo_initialized;

static const hmi_param_descriptor_t s_demo_winding_params[] = {
    {
        .id = HMI_DEMO_PARAM_MASTER_SPEED,
        .key = "master_speed",
        .label = "Master speed",
        .unit = "rps",
        .type = HMI_PARAM_TYPE_FLOAT,
        .wire = {
            .param_id = LINK_PARAM_JOB_MASTER_SPEED,
            .scale = 100.0,
        },
        .default_value.f32 = 2.50f,
        .min_value.f32 = 0.10f,
        .max_value.f32 = 30.00f,
        .step.f32 = 0.10f,
        .decimals = 1,
        .editable = true,
        .visible = true,
    },
    {
        .id = HMI_DEMO_PARAM_WINDING_PITCH,
        .key = "winding_pitch",
        .label = "Winding pitch",
        .unit = "mm/rev",
        .type = HMI_PARAM_TYPE_FLOAT,
        .wire = {
            .param_id = LINK_PARAM_JOB_WINDING_PITCH,
            .scale = 100.0,
        },
        .default_value.f32 = 0.80f,
        .min_value.f32 = 0.10f,
        .max_value.f32 = 30.00f,
        .step.f32 = 0.01f,
        .decimals = 2,
        .editable = true,
        .visible = true,
    },
    {
        .id = HMI_DEMO_PARAM_TARGET_LENGTH,
        .key = "target_length",
        .label = "Target length",
        .unit = "m",
        .type = HMI_PARAM_TYPE_UINT32,
        .wire = {
            .param_id = LINK_PARAM_JOB_TARGET_LENGTH,
            .scale = 1.0,
        },
        .default_value.u32 = 120U,
        .min_value.u32 = 1U,
        .max_value.u32 = 100000U,
        .step.u32 = 1U,
        .decimals = 0,
        .editable = true,
        .visible = true,
    },
    {
        .id = HMI_DEMO_PARAM_SHIFT_EVERY,
        .key = "shift_every",
        .label = "Shift every",
        .unit = "layers",
        .type = HMI_PARAM_TYPE_UINT32,
        .wire = {
            .param_id = LINK_PARAM_JOB_SHIFT_EVERY,
            .scale = 1.0,
        },
        .default_value.u32 = 3U,
        .min_value.u32 = 2U,
        .max_value.u32 = 100U,
        .step.u32 = 1U,
        .decimals = 0,
        .editable = true,
        .visible = true,
    },
    {
        .id = HMI_DEMO_PARAM_RIGHT_EDGE_SHIFT,
        .key = "right_edge_shift",
        .label = "Right edge shift",
        .unit = "mm",
        .type = HMI_PARAM_TYPE_FLOAT,
        .wire = {
            .param_id = LINK_PARAM_JOB_RIGHT_EDGE_SHIFT,
            .scale = 100.0,
        },
        .default_value.f32 = 1.00f,
        .min_value.f32 = 0.06f,
        .max_value.f32 = 50.00f,
        .step.f32 = 0.01f,
        .decimals = 2,
        .editable = true,
        .visible = true,
    },
};

static const hmi_param_descriptor_t s_edge_trim_params[HMI_EDGE_TRIM_COUNT] = {
    [HMI_EDGE_TRIM_LEFT] = {
        .id = LINK_PARAM_LEFT_EDGE_TRIM_MM,
        .key = "left_edge_trim",
        .label = "Left edge trim",
        .unit = "mm",
        .type = HMI_PARAM_TYPE_FLOAT,
        .wire = {
            .param_id = LINK_PARAM_LEFT_EDGE_TRIM_MM,
            .scale = 100.0,
        },
        .default_value.f32 = 0.0f,
        .min_value.f32 = -50.0f,
        .max_value.f32 = 50.0f,
        .step.f32 = 0.1f,
        .decimals = 1,
        .editable = true,
        .visible = true,
    },
    [HMI_EDGE_TRIM_RIGHT] = {
        .id = LINK_PARAM_RIGHT_EDGE_TRIM_MM,
        .key = "right_edge_trim",
        .label = "Right edge trim",
        .unit = "mm",
        .type = HMI_PARAM_TYPE_FLOAT,
        .wire = {
            .param_id = LINK_PARAM_RIGHT_EDGE_TRIM_MM,
            .scale = 100.0,
        },
        .default_value.f32 = 0.0f,
        .min_value.f32 = -50.0f,
        .max_value.f32 = 50.0f,
        .step.f32 = 0.1f,
        .decimals = 1,
        .editable = true,
        .visible = true,
    },
};

static const hmi_mode_capability_t s_modes[] = {
    {
        .id = HMI_JOB_MODE_CONICAL,
        .key = "conical",
        .title = "Conical Winding",
        .description = "Gradual right edge shift after configured number of layers.",
        .enabled = true,
        .params = s_demo_winding_params,
        .param_count = sizeof(s_demo_winding_params) / sizeof(s_demo_winding_params[0]),
    },
    {
        .id = HMI_JOB_MODE_STRAIGHT,
        .key = "straight",
        .title = "Straight Winding",
        .description = "Constant edge winding profile.",
        .enabled = false,
        .params = NULL,
        .param_count = 0,
    },
    {
        .id = HMI_JOB_MODE_TEST,
        .key = "test",
        .title = "Test Mode",
        .description = "Service and commissioning motions.",
        .enabled = false,
        .params = NULL,
        .param_count = 0,
    },
};

void hmi_capability_model_init_demo(void)
{
    s_demo_initialized = true;
}

size_t hmi_capability_model_get_mode_count(void)
{
    (void)s_demo_initialized;
    return sizeof(s_modes) / sizeof(s_modes[0]);
}

const hmi_mode_capability_t *hmi_capability_model_get_mode_by_index(size_t index)
{
    if (index >= hmi_capability_model_get_mode_count()) {
        return NULL;
    }
    return &s_modes[index];
}

const hmi_mode_capability_t *hmi_capability_model_get_mode_by_id(hmi_job_mode_id_t mode_id)
{
    for (size_t i = 0; i < hmi_capability_model_get_mode_count(); i++) {
        if (s_modes[i].id == mode_id) {
            return &s_modes[i];
        }
    }
    return NULL;
}

size_t hmi_capability_model_get_param_count(hmi_job_mode_id_t mode_id)
{
    const hmi_mode_capability_t *mode = hmi_capability_model_get_mode_by_id(mode_id);
    return mode != NULL ? mode->param_count : 0;
}

const hmi_param_descriptor_t *hmi_capability_model_get_param_by_index(hmi_job_mode_id_t mode_id, size_t index)
{
    const hmi_mode_capability_t *mode = hmi_capability_model_get_mode_by_id(mode_id);
    if (mode == NULL || index >= mode->param_count) {
        return NULL;
    }
    return &mode->params[index];
}

const hmi_param_descriptor_t *hmi_capability_model_get_param_by_id(hmi_job_mode_id_t mode_id, uint16_t param_id)
{
    const hmi_mode_capability_t *mode = hmi_capability_model_get_mode_by_id(mode_id);
    if (mode == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < mode->param_count; i++) {
        if (mode->params[i].id == param_id) {
            return &mode->params[i];
        }
    }
    return NULL;
}

const hmi_param_descriptor_t *hmi_capability_model_get_param_by_key(hmi_job_mode_id_t mode_id, const char *key)
{
    const hmi_mode_capability_t *mode = hmi_capability_model_get_mode_by_id(mode_id);
    if (mode == NULL || key == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < mode->param_count; i++) {
        if (mode->params[i].key != NULL && strcmp(mode->params[i].key, key) == 0) {
            return &mode->params[i];
        }
    }
    return NULL;
}

const hmi_param_descriptor_t *hmi_capability_model_get_edge_trim(
    hmi_edge_trim_side_t side)
{
    if ((unsigned)side >= HMI_EDGE_TRIM_COUNT) {
        return NULL;
    }

    return &s_edge_trim_params[side];
}
