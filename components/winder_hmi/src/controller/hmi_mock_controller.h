#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hmi_controller_transport.h"

void hmi_mock_controller_init(void);
void hmi_mock_controller_reset(void);
void hmi_mock_controller_set_enabled(bool enabled);
const hmi_controller_transport_t *hmi_mock_controller_get_transport(void);
void hmi_mock_controller_tick(uint32_t elapsed_ms);
