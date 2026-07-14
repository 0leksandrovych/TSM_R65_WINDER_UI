#include "hmi_link_state_mapper.h"

#include <stddef.h>

bool hmi_link_state_mapper_machine_state(
    link_machine_state_t link_state,
    hmi_machine_state_t *out_state)
{
    if (out_state == NULL) {
        return false;
    }

    switch (link_state) {
    case LINK_MACHINE_STATE_BOOTING:
        *out_state = HMI_MACHINE_BOOTING;
        return true;
    case LINK_MACHINE_STATE_HOMING_REQUIRED:
    case LINK_MACHINE_STATE_HOMING:
        /* Active homing is represented by the homing sub-state in the current UI. */
        *out_state = HMI_MACHINE_HOMING_REQUIRED;
        return true;
    case LINK_MACHINE_STATE_READY:
        *out_state = HMI_MACHINE_READY;
        return true;
    case LINK_MACHINE_STATE_RUNNING:
        *out_state = HMI_MACHINE_RUNNING;
        return true;
    case LINK_MACHINE_STATE_STOPPING:
        *out_state = HMI_MACHINE_STOPPING;
        return true;
    case LINK_MACHINE_STATE_FINISHED:
        *out_state = HMI_MACHINE_FINISHED;
        return true;
    case LINK_MACHINE_STATE_ALARM:
        *out_state = HMI_MACHINE_ALARM;
        return true;
    default:
        return false;
    }
}

bool hmi_link_state_mapper_homing_state(
    link_homing_state_t link_state,
    hmi_homing_state_t *out_state)
{
    if (out_state == NULL) {
        return false;
    }

    switch (link_state) {
    case LINK_HOMING_STATE_REQUIRED:
        *out_state = HMI_HOMING_REQUIRED;
        return true;
    case LINK_HOMING_STATE_IN_PROGRESS:
        *out_state = HMI_HOMING_IN_PROGRESS;
        return true;
    case LINK_HOMING_STATE_COMPLETE:
        *out_state = HMI_HOMING_OK;
        return true;
    case LINK_HOMING_STATE_FAILED:
        *out_state = HMI_HOMING_FAILED;
        return true;
    default:
        return false;
    }
}
