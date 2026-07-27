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
        /* The synchronized controller contract currently uses this one wire
         * value for both right fast search and right sample measurement. */
        *out_state = HMI_MACHINE_HOMING_SEARCHING_RIGHT;
        return true;
    case LINK_MACHINE_STATE_HOMING_BACKING_OFF_RIGHT_REFERENCE:
        *out_state = HMI_MACHINE_HOMING_RIGHT_BACKOFF;
        return true;
    case LINK_MACHINE_STATE_HOMING_SEARCHING_LEFT_REFERENCE:
        *out_state = HMI_MACHINE_HOMING_SEARCHING_LEFT;
        return true;
    case LINK_MACHINE_STATE_HOMING_BACKING_OFF_LEFT_REFERENCE:
        *out_state = HMI_MACHINE_HOMING_LEFT_BACKOFF;
        return true;
    case LINK_MACHINE_STATE_HOMING_MEASURING_TRAVEL:
        *out_state = HMI_MACHINE_HOMING_LEFT_MEASUREMENT;
        return true;
    case LINK_MACHINE_STATE_HOMING_APPLYING_OFFSET:
        *out_state = HMI_MACHINE_HOMING_WAITING_ZERO_COMMAND;
        return true;
    case LINK_MACHINE_STATE_HOMING_COMPLETING:
        *out_state = HMI_MACHINE_HOMING_MOVING_TO_ZERO;
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
    case LINK_MACHINE_STATE_HOMING_WAITING_NEXT_MEASUREMENT:
        *out_state = HMI_MACHINE_HOMING_WAITING_NEXT_MEASUREMENT;
        return true;
    case LINK_MACHINE_STATE_HOMING_WAITING_ZERO_COMMAND:
        *out_state = HMI_MACHINE_HOMING_WAITING_ZERO_COMMAND;
        return true;
    case LINK_MACHINE_STATE_HOMING_MOVING_TO_ZERO:
        *out_state = HMI_MACHINE_HOMING_MOVING_TO_ZERO;
        return true;
    case LINK_MACHINE_STATE_READY_AT_ZERO:
    case LINK_MACHINE_STATE_READY_AT_LEFT_EDGE:
        *out_state = HMI_MACHINE_READY;
        return true;
    case LINK_MACHINE_STATE_POSITIONING:
        *out_state = HMI_MACHINE_POSITIONING;
        return true;
    case LINK_MACHINE_STATE_HOMING_MASTER_POSITIONING:
        *out_state = HMI_MACHINE_HOMING_MASTER_POSITIONING;
        return true;
    default:
        return false;
    }
}

bool hmi_link_state_mapper_carriage_position(
    link_carriage_reference_position_t link_position,
    hmi_carriage_reference_position_t *out_position)
{
    if (out_position == NULL) {
        return false;
    }

    switch (link_position) {
    case LINK_CARRIAGE_REFERENCE_POSITION_UNKNOWN:
        *out_position = HMI_CARRIAGE_POSITION_UNKNOWN;
        return true;
    case LINK_CARRIAGE_REFERENCE_POSITION_ZERO:
        *out_position = HMI_CARRIAGE_POSITION_ZERO;
        return true;
    case LINK_CARRIAGE_REFERENCE_POSITION_LEFT_EDGE:
        *out_position = HMI_CARRIAGE_POSITION_LEFT_EDGE;
        return true;
    case LINK_CARRIAGE_REFERENCE_POSITION_MOVING:
        *out_position = HMI_CARRIAGE_POSITION_MOVING;
        return true;
    default:
        return false;
    }
}
