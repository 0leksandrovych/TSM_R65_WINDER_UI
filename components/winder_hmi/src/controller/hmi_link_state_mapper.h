#pragma once

#include <stdbool.h>

#include "hmi_state.h"
#include "winder_link_contract.h"

bool hmi_link_state_mapper_machine_state(
    link_machine_state_t link_state,
    hmi_machine_state_t *out_state
);

bool hmi_link_state_mapper_carriage_position(
    link_carriage_reference_position_t link_position,
    hmi_carriage_reference_position_t *out_position
);
