#pragma once

#include <stdbool.h>

#include "hmi_events.h"
#include "hmi_state.h"

typedef enum {
    HMI_INTERNAL_EVENT_NONE = 0,

    HMI_INTERNAL_EVENT_STATE_UPDATE,
    HMI_INTERNAL_EVENT_CAPABILITIES_UPDATE,
    HMI_INTERNAL_EVENT_COMMAND_ACCEPTED,
    HMI_INTERNAL_EVENT_JOB_VALIDATION_RESULT,
    HMI_INTERNAL_EVENT_COMMAND_REJECTED,
    HMI_INTERNAL_EVENT_CONNECTION_STATE_CHANGED
} hmi_internal_event_type_t;

typedef struct {
    hmi_internal_event_type_t type;
    union {
        hmi_state_t state;
        hmi_command_t command_accepted;
        hmi_job_validation_t validation;
        hmi_command_rejected_t command_rejected;
        hmi_connection_state_t connection;
    } data;
} hmi_internal_event_t;

bool hmi_event_queue_init(void);
bool hmi_event_queue_post(const hmi_internal_event_t *event);
bool hmi_event_queue_poll(hmi_internal_event_t *out_event);
void hmi_event_queue_clear(void);
