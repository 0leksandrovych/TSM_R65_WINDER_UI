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
    case LINK_MACHINE_STATE_HOMING_REQUIRED:
        *out_state = HMI_MACHINE_HOMING_REQUIRED;
        return true;
    case LINK_MACHINE_STATE_HOMING_SEARCHING_RIGHT_REFERENCE:
        *out_state = HMI_MACHINE_HOMING_SEARCHING_RIGHT_REFERENCE;
        return true;
    case LINK_MACHINE_STATE_HOMING_BACKING_OFF_RIGHT_REFERENCE:
        *out_state = HMI_MACHINE_HOMING_BACKING_OFF_RIGHT_REFERENCE;
        return true;
    case LINK_MACHINE_STATE_HOMING_SEARCHING_LEFT_REFERENCE:
        *out_state = HMI_MACHINE_HOMING_SEARCHING_LEFT_REFERENCE;
        return true;
    case LINK_MACHINE_STATE_HOMING_BACKING_OFF_LEFT_REFERENCE:
        *out_state = HMI_MACHINE_HOMING_BACKING_OFF_LEFT_REFERENCE;
        return true;
    case LINK_MACHINE_STATE_HOMING_MEASURING_TRAVEL:
        *out_state = HMI_MACHINE_HOMING_MEASURING_TRAVEL;
        return true;
    case LINK_MACHINE_STATE_HOMING_APPLYING_OFFSET:
        *out_state = HMI_MACHINE_HOMING_APPLYING_OFFSET;
        return true;
    case LINK_MACHINE_STATE_HOMING_COMPLETING:
        *out_state = HMI_MACHINE_HOMING_COMPLETING;
        return true;
    case LINK_MACHINE_STATE_READY:
        *out_state = HMI_MACHINE_READY;
        return true;
    case LINK_MACHINE_STATE_ACCELERATING:
        *out_state = HMI_MACHINE_ACCELERATING;
        return true;
    case LINK_MACHINE_STATE_RUNNING:
        *out_state = HMI_MACHINE_RUNNING;
        return true;
    case LINK_MACHINE_STATE_PAUSED:
        *out_state = HMI_MACHINE_PAUSED;
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
