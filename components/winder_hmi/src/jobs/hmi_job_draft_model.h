#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hmi_capability_model.h"
#include "hmi_param.h"

void hmi_job_draft_model_init(void);

bool hmi_job_draft_model_select_mode(hmi_job_mode_id_t mode_id);
hmi_job_mode_id_t hmi_job_draft_model_get_mode(void);

bool hmi_job_draft_model_get_value(uint16_t param_id, hmi_param_value_t *out);
bool hmi_job_draft_model_set_value(uint16_t param_id, hmi_param_value_t value);
bool hmi_job_draft_model_step_value(uint16_t param_id, int direction);

bool hmi_job_draft_model_is_dirty(void);
void hmi_job_draft_model_mark_dirty(const char *message);

bool hmi_job_draft_model_validate_local(void);
const hmi_job_validation_t *hmi_job_draft_model_get_validation(void);
void hmi_job_draft_model_apply_validation_result(const hmi_job_validation_t *validation);
void hmi_job_draft_model_set_estimates(uint32_t layers, float time_min, float offset_mm);
