#include "hmi_state.h"

#include <stdio.h>

bool hmi_machine_state_is_homing(hmi_machine_state_t state)
{
    switch (state) {
    case HMI_MACHINE_HOMING_SEARCHING_LEFT:
    case HMI_MACHINE_HOMING_LEFT_BACKOFF:
    case HMI_MACHINE_HOMING_LEFT_MEASUREMENT:
    case HMI_MACHINE_HOMING_MASTER_POSITIONING:
    case HMI_MACHINE_HOMING_SEARCHING_RIGHT:
    case HMI_MACHINE_HOMING_RIGHT_BACKOFF:
    case HMI_MACHINE_HOMING_RIGHT_MEASUREMENT:
    case HMI_MACHINE_HOMING_WAITING_NEXT_MEASUREMENT:
    case HMI_MACHINE_HOMING_WAITING_ZERO_COMMAND:
    case HMI_MACHINE_HOMING_MOVING_TO_ZERO:
        return true;
    default:
        return false;
    }
}

bool hmi_machine_state_is_positioning(hmi_machine_state_t state)
{
    return state == HMI_MACHINE_POSITIONING ||
           state == HMI_MACHINE_HOMING_MOVING_TO_ZERO;
}

bool hmi_state_can_start_job(const hmi_state_t *state)
{
    return state != NULL &&
           state->machine_state_known &&
           state->machine_state == HMI_MACHINE_READY &&
           state->carriage_reference_position_known &&
           state->carriage_reference_position == HMI_CARRIAGE_POSITION_ZERO &&
           state->safety_ok;
}

const char *hmi_carriage_reference_position_text(
    hmi_carriage_reference_position_t position)
{
    switch (position) {
    case HMI_CARRIAGE_POSITION_ZERO:
        return "Zero";
    case HMI_CARRIAGE_POSITION_LEFT_EDGE:
        return "Left edge";
    case HMI_CARRIAGE_POSITION_MOVING:
        return "Moving";
    case HMI_CARRIAGE_POSITION_UNKNOWN:
    default:
        return "Unknown";
    }
}

const char *hmi_homing_alarm_text(uint32_t code, char *buffer, size_t buffer_size)
{
    switch (code) {
    case 0U:
        return "None";
    case 1U:
        return "Homing failed: sensor timeout";
    case 2U:
        return "Homing failed: invalid measured edge order";
    default:
        if (buffer == NULL || buffer_size == 0U) {
            return "Unknown homing error";
        }
        snprintf(buffer, buffer_size, "Unknown homing error, code %lu",
                 (unsigned long)code);
        return buffer;
    }
}
