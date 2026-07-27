#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "hmi_events.h"
#include "winder_hmi.h"

/* Debug/notification hook for external observers (logging, diagnostics, tests).
 * Called synchronously inside hmi_command_bus_emit, before any internal listeners.
 * Must not execute machine behavior — command execution is done by registered listeners only. */
void hmi_command_bus_set_callback(winder_hmi_command_cb_t callback, void *user_ctx);

/* Internal listeners: receive every emitted command for execution and return
 * true only when the command was accepted for delivery. Fixed array — no
 * dynamic allocation. */
typedef bool (*hmi_command_listener_t)(hmi_command_t command,
                                       const hmi_command_payload_t *payload,
                                       void *user_ctx);

bool hmi_command_bus_add_listener(hmi_command_listener_t listener, void *user_ctx);
bool hmi_command_bus_remove_listener(hmi_command_listener_t listener, void *user_ctx);
void hmi_command_bus_clear_listeners(void);

/* Returns true when at least one internal listener accepted the command. */
bool hmi_command_bus_emit(hmi_command_t command, const hmi_command_payload_t *payload);
