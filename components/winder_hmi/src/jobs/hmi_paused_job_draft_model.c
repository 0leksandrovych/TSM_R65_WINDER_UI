#include "hmi_paused_job_draft_model.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hmi_capability_model.h"
#include "hmi_events.h"

typedef struct {
    const hmi_param_descriptor_t *descriptor;
    hmi_param_value_t value;
} paused_value_t;

typedef struct {
    int32_t master_speed_centi_rps;
    int32_t winding_pitch_centi_mm;
    int32_t shift_every_layers;
    int32_t right_edge_shift_centi_mm;
    int32_t target_length_mm;
} paused_update_confirmation_t;

static const char *const s_param_keys[HMI_PAUSED_JOB_EDIT_PARAM_COUNT] = {
    "master_speed",
    "winding_pitch",
    "shift_every",
    "right_edge_shift",
};

static paused_value_t s_values[HMI_PAUSED_JOB_EDIT_PARAM_COUNT];
static bool s_initialized;
static bool s_dirty;
static bool s_additional_length_present;
static float s_additional_length_m;
static paused_update_confirmation_t s_candidate;
static paused_update_confirmation_t s_pending;
static bool s_candidate_valid;
static bool s_pending_valid;
static uint32_t s_confirmation_revision;

static hmi_job_mode_id_t active_mode(void)
{
    const hmi_mode_capability_t *mode =
        hmi_capability_model_get_mode_by_id(HMI_JOB_MODE_CONICAL);
    return mode != NULL && mode->enabled ? mode->id : HMI_JOB_MODE_NONE;
}

static bool scale_to_i32(double value, double scale, int32_t *out_value)
{
    if (out_value == NULL || !isfinite(value) || !isfinite(scale) || !(scale > 0.0)) {
        return false;
    }

    const double scaled = value * scale;
    if (scaled < (double)INT32_MIN || scaled > (double)INT32_MAX) {
        return false;
    }

    *out_value = (int32_t)llround(scaled);
    return true;
}

static bool is_whole_meter_value(float value_m)
{
    return isfinite(value_m) && truncf(value_m) == value_m;
}

static paused_value_t *find_value(uint16_t param_id)
{
    for (size_t i = 0; i < HMI_PAUSED_JOB_EDIT_PARAM_COUNT; i++) {
        if (s_values[i].descriptor != NULL && s_values[i].descriptor->id == param_id) {
            return &s_values[i];
        }
    }
    return NULL;
}

static bool confirmed_parameters_known(const hmi_state_t *state)
{
    return state != NULL &&
           state->job_master_speed_known &&
           state->winding_pitch_known &&
           state->shift_every_layers_known &&
           state->job_right_edge_shift_known;
}

static bool initialize_from_confirmed(const hmi_state_t *state)
{
    const hmi_job_mode_id_t mode_id = active_mode();
    if (!confirmed_parameters_known(state) || mode_id == HMI_JOB_MODE_NONE) {
        return false;
    }

    for (size_t i = 0; i < HMI_PAUSED_JOB_EDIT_PARAM_COUNT; i++) {
        s_values[i].descriptor =
            hmi_capability_model_get_param_by_key(mode_id, s_param_keys[i]);
        if (s_values[i].descriptor == NULL) {
            hmi_paused_job_draft_model_reset();
            return false;
        }
    }

    s_values[0].value.f32 = state->job_master_speed_rps;
    s_values[1].value.f32 = state->winding_pitch_mm;
    s_values[2].value.u32 = state->shift_every_layers;
    s_values[3].value.f32 = state->job_right_edge_shift_mm;
    s_initialized = true;
    s_dirty = false;
    return true;
}

static bool confirmation_matches(const hmi_state_t *state,
                                 const paused_update_confirmation_t *expected)
{
    int32_t master = 0;
    int32_t pitch = 0;
    int32_t right_shift = 0;
    int32_t target = 0;
    if (!confirmed_parameters_known(state) ||
        !state->target_length_known ||
        !scale_to_i32(state->job_master_speed_rps, 100.0, &master) ||
        !scale_to_i32(state->winding_pitch_mm, 100.0, &pitch) ||
        !scale_to_i32(state->job_right_edge_shift_mm, 100.0, &right_shift) ||
        !scale_to_i32(state->target_length_m, 1000.0, &target)) {
        return false;
    }

    return master == expected->master_speed_centi_rps &&
           pitch == expected->winding_pitch_centi_mm &&
           state->shift_every_layers == (uint32_t)expected->shift_every_layers &&
           right_shift == expected->right_edge_shift_centi_mm &&
           llabs((long long)target - (long long)expected->target_length_mm) <= 1LL;
}

void hmi_paused_job_draft_model_init(void)
{
    s_confirmation_revision = 0U;
    hmi_paused_job_draft_model_reset();
}

void hmi_paused_job_draft_model_reset(void)
{
    memset(s_values, 0, sizeof(s_values));
    s_initialized = false;
    s_dirty = false;
    s_additional_length_present = false;
    s_additional_length_m = 0.0f;
    s_candidate = (paused_update_confirmation_t){0};
    s_pending = (paused_update_confirmation_t){0};
    s_candidate_valid = false;
    s_pending_valid = false;
}

void hmi_paused_job_draft_model_sync(const hmi_state_t *confirmed_state)
{
    if (confirmed_state == NULL ||
        !confirmed_state->machine_state_known ||
        confirmed_state->machine_state != HMI_MACHINE_PAUSED) {
        hmi_paused_job_draft_model_reset();
        return;
    }

    if (s_pending_valid && confirmation_matches(confirmed_state, &s_pending)) {
        s_pending_valid = false;
        s_additional_length_present = false;
        s_additional_length_m = 0.0f;
        s_initialized = false;
        (void)initialize_from_confirmed(confirmed_state);
        s_confirmation_revision++;
        return;
    }

    if (!s_initialized) {
        (void)initialize_from_confirmed(confirmed_state);
    }
}

bool hmi_paused_job_draft_model_begin_edit(const hmi_state_t *confirmed_state)
{
    if (s_candidate_valid || s_pending_valid) {
        return false;
    }

    hmi_paused_job_draft_model_reset();
    return initialize_from_confirmed(confirmed_state);
}

bool hmi_paused_job_draft_model_is_initialized(void)
{
    return s_initialized;
}

bool hmi_paused_job_draft_model_is_dirty(void)
{
    return s_dirty;
}

const hmi_param_descriptor_t *hmi_paused_job_draft_model_get_descriptor(size_t index)
{
    return index < HMI_PAUSED_JOB_EDIT_PARAM_COUNT ? s_values[index].descriptor : NULL;
}

bool hmi_paused_job_draft_model_get_value(uint16_t param_id, hmi_param_value_t *out_value)
{
    paused_value_t *slot = find_value(param_id);
    if (slot == NULL || out_value == NULL) {
        return false;
    }
    *out_value = slot->value;
    return true;
}

bool hmi_paused_job_draft_model_set_value(uint16_t param_id, hmi_param_value_t value)
{
    paused_value_t *slot = find_value(param_id);
    char message[HMI_TEXT_MESSAGE_MAX];
    if (slot == NULL ||
        !hmi_param_validate_value(slot->descriptor, value, message, sizeof(message))) {
        return false;
    }

    slot->value = value;
    s_dirty = true;
    return true;
}

bool hmi_paused_job_draft_model_has_additional_length(void)
{
    return s_additional_length_present;
}

float hmi_paused_job_draft_model_get_additional_length_m(void)
{
    return s_additional_length_m;
}

bool hmi_paused_job_draft_model_set_additional_length_m(float value_m)
{
    int32_t wire_value = 0;
    if (!(value_m >= 1.0f) || !is_whole_meter_value(value_m) ||
        !scale_to_i32(value_m, 1000.0, &wire_value)) {
        return false;
    }

    s_additional_length_present = true;
    s_additional_length_m = value_m;
    s_dirty = true;
    return true;
}

void hmi_paused_job_draft_model_clear_additional_length(void)
{
    if (s_additional_length_present) {
        s_additional_length_present = false;
        s_additional_length_m = 0.0f;
        s_dirty = true;
    }
}

bool hmi_paused_job_draft_model_preview_target_m(const hmi_state_t *confirmed_state,
                                                  float *out_target_m)
{
    if (confirmed_state == NULL || out_target_m == NULL ||
        !confirmed_state->wound_length_known || !s_additional_length_present) {
        return false;
    }
    *out_target_m = confirmed_state->wound_length_m + s_additional_length_m;
    return isfinite(*out_target_m);
}

bool hmi_paused_job_draft_model_validate(char *message, size_t message_size)
{
    if (!s_initialized) {
        snprintf(message, message_size, "Waiting for active job parameters");
        return false;
    }

    for (size_t i = 0; i < HMI_PAUSED_JOB_EDIT_PARAM_COUNT; i++) {
        if (!hmi_param_validate_value(s_values[i].descriptor,
                                      s_values[i].value,
                                      message,
                                      message_size)) {
            return false;
        }
    }

    int32_t additional_mm = 0;
    if (s_additional_length_present &&
        (!(s_additional_length_m >= 1.0f) ||
         !is_whole_meter_value(s_additional_length_m) ||
         !scale_to_i32(s_additional_length_m, 1000.0, &additional_mm))) {
        snprintf(message,
                 message_size,
                 "Additional length must be whole metres (minimum 1 m)");
        return false;
    }

    if (message != NULL && message_size > 0U) {
        message[0] = '\0';
    }
    return true;
}

bool hmi_paused_job_draft_model_get_update(hmi_paused_job_update_t *out_update)
{
    char message[HMI_TEXT_MESSAGE_MAX];
    if (out_update == NULL || !hmi_paused_job_draft_model_validate(message, sizeof(message))) {
        return false;
    }

    *out_update = (hmi_paused_job_update_t){
        .master_speed_rps = s_values[0].value.f32,
        .winding_pitch_mm = s_values[1].value.f32,
        .shift_every_layers = s_values[2].value.u32,
        .right_edge_shift_mm = s_values[3].value.f32,
        .additional_length_present = s_additional_length_present,
        .additional_length_m = s_additional_length_present ? s_additional_length_m : 0.0f,
    };
    return true;
}

bool hmi_paused_job_draft_model_stage_update(const hmi_state_t *confirmed_state)
{
    hmi_paused_job_update_t update;
    double expected_target = 0.0;
    if (!hmi_paused_job_draft_model_get_update(&update) || confirmed_state == NULL) {
        return false;
    }

    if (update.additional_length_present) {
        if (!confirmed_state->wound_length_known) {
            return false;
        }
        expected_target = (double)confirmed_state->wound_length_m +
                          (double)update.additional_length_m;
    } else {
        if (!confirmed_state->target_length_known) {
            return false;
        }
        expected_target = confirmed_state->target_length_m;
    }

    paused_update_confirmation_t candidate = {0};
    if (!scale_to_i32(update.master_speed_rps, 100.0, &candidate.master_speed_centi_rps) ||
        !scale_to_i32(update.winding_pitch_mm, 100.0, &candidate.winding_pitch_centi_mm) ||
        update.shift_every_layers > (uint32_t)INT32_MAX ||
        !scale_to_i32(update.right_edge_shift_mm, 100.0, &candidate.right_edge_shift_centi_mm) ||
        !scale_to_i32(expected_target, 1000.0, &candidate.target_length_mm)) {
        return false;
    }
    candidate.shift_every_layers = (int32_t)update.shift_every_layers;

    s_candidate = candidate;
    s_candidate_valid = true;
    return true;
}

void hmi_paused_job_draft_model_accept_update(void)
{
    if (!s_candidate_valid) {
        return;
    }
    s_pending = s_candidate;
    s_pending_valid = true;
    s_candidate_valid = false;
}

void hmi_paused_job_draft_model_reject_update(void)
{
    s_candidate = (paused_update_confirmation_t){0};
    s_candidate_valid = false;
}

bool hmi_paused_job_draft_model_update_awaiting_confirmation(void)
{
    return s_candidate_valid || s_pending_valid;
}

uint32_t hmi_paused_job_draft_model_confirmation_revision(void)
{
    return s_confirmation_revision;
}
