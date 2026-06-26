#pragma once

#include "lvgl.h"

#include "hmi_events.h"
#include "hmi_state.h"
#include "hmi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*winder_hmi_command_cb_t)(
    hmi_command_t command,
    const hmi_command_payload_t *payload,
    void *user_ctx
);

void winder_hmi_init(lv_obj_t *root);
void winder_hmi_enable_demo_mode(bool enabled);
void winder_hmi_set_command_callback(winder_hmi_command_cb_t callback, void *user_ctx);
void winder_hmi_set_state(const hmi_state_t *state);
bool winder_hmi_post_state(const hmi_state_t *state);
bool winder_hmi_post_job_validation_result(const hmi_job_validation_t *result);
bool winder_hmi_post_command_accepted(hmi_command_t command);
bool winder_hmi_post_command_rejected(hmi_command_t command, const char *reason);
void winder_hmi_tick(void);

#ifdef __cplusplus
}
#endif
