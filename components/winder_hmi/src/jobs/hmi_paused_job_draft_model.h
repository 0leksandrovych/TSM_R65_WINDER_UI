#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hmi_param.h"
#include "hmi_state.h"

#define HMI_PAUSED_JOB_EDIT_PARAM_COUNT 4U

typedef struct {
    float master_speed_rps;
    float winding_pitch_mm;
    uint32_t shift_every_layers;
    float right_edge_shift_mm;
    bool additional_length_present;
    float additional_length_m;
} hmi_paused_job_update_t;

void hmi_paused_job_draft_model_init(void);
void hmi_paused_job_draft_model_reset(void);
void hmi_paused_job_draft_model_sync(const hmi_state_t *confirmed_state);
bool hmi_paused_job_draft_model_begin_edit(const hmi_state_t *confirmed_state);

bool hmi_paused_job_draft_model_is_initialized(void);
bool hmi_paused_job_draft_model_is_dirty(void);
const hmi_param_descriptor_t *hmi_paused_job_draft_model_get_descriptor(size_t index);
bool hmi_paused_job_draft_model_get_value(uint16_t param_id, hmi_param_value_t *out_value);
bool hmi_paused_job_draft_model_set_value(uint16_t param_id, hmi_param_value_t value);

bool hmi_paused_job_draft_model_has_additional_length(void);
float hmi_paused_job_draft_model_get_additional_length_m(void);
bool hmi_paused_job_draft_model_set_additional_length_m(float value_m);
void hmi_paused_job_draft_model_clear_additional_length(void);
bool hmi_paused_job_draft_model_preview_target_m(const hmi_state_t *confirmed_state,
                                                  float *out_target_m);

bool hmi_paused_job_draft_model_validate(char *message, size_t message_size);
bool hmi_paused_job_draft_model_get_update(hmi_paused_job_update_t *out_update);

bool hmi_paused_job_draft_model_stage_update(const hmi_state_t *confirmed_state);
void hmi_paused_job_draft_model_accept_update(void);
void hmi_paused_job_draft_model_reject_update(void);
bool hmi_paused_job_draft_model_update_awaiting_confirmation(void);
uint32_t hmi_paused_job_draft_model_confirmation_revision(void);
