#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "hmi_events.h"
#include "hmi_param.h"

typedef struct {
    hmi_job_mode_id_t id;
    const char *key;
    const char *title;
    const char *description;
    bool enabled;

    const hmi_param_descriptor_t *params;
    size_t param_count;
} hmi_mode_capability_t;

typedef enum {
    HMI_EDGE_TRIM_LEFT = 0,
    HMI_EDGE_TRIM_RIGHT,
    HMI_EDGE_TRIM_COUNT,
} hmi_edge_trim_side_t;

void hmi_capability_model_init_demo(void);

size_t hmi_capability_model_get_mode_count(void);
const hmi_mode_capability_t *hmi_capability_model_get_mode_by_index(size_t index);
const hmi_mode_capability_t *hmi_capability_model_get_mode_by_id(hmi_job_mode_id_t mode_id);

size_t hmi_capability_model_get_param_count(hmi_job_mode_id_t mode_id);
const hmi_param_descriptor_t *hmi_capability_model_get_param_by_index(hmi_job_mode_id_t mode_id, size_t index);
const hmi_param_descriptor_t *hmi_capability_model_get_param_by_id(hmi_job_mode_id_t mode_id, uint16_t param_id);
const hmi_param_descriptor_t *hmi_capability_model_get_param_by_key(hmi_job_mode_id_t mode_id, const char *key);

const hmi_param_descriptor_t *hmi_capability_model_get_edge_trim(
    hmi_edge_trim_side_t side);
