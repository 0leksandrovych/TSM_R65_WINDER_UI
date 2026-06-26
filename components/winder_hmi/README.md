# Winder HMI Vertical Slice

This component implements a small interactive LVGL HMI slice for the winding machine display panel.

## Implemented Screens

- Home
- Jobs / Mode Select
- Conical Setup
- Start Confirmation
- Homing
- Run
- Diagnostics
- Settings

Home remains the entry screen. The primary Home action routes to Homing when homing is required, starts/opens Run when a valid job exists, and otherwise opens Jobs. The Jobs screen currently enables only Conical Winding; Straight Winding and Test Mode are visible placeholders.

The conical flow is:

```c
Home -> Jobs -> Conical Setup -> Start Confirmation -> Run
```

## Mock/Demo Only

Demo mode is optional and disabled by default. Enable it only for bench testing:

```c
winder_hmi_init(lv_scr_act());
winder_hmi_enable_demo_mode(true);
winder_hmi_set_state(hmi_demo_state_ready());
```

When demo mode is enabled, `src/controller/hmi_mock_controller.c` is registered as the active controller transport. It receives the same logical controller messages that a UART transport will later send to the real controller, then posts state, validation, or rejection events back through the HMI event queue.

- Homing advances through demo steps: ready, moving to left limit, backoff, slow approach, complete.
- Conical Setup stores a job draft and validates it through the mock controller transport.
- Start Confirmation emits the same start command used by production integration.
- Run advances wound length, unwound length, progress, layer count, encoder count, and carriage position.
- Pause, resume, stop, and reset counter update only mock state.
- No real homing, motor control, UART, CRC, framing, or persistence is implemented.

## Command Flow

LVGL button callbacks call `hmi_actions_*()` functions. Actions emit commands through the internal command bus. The command bus now maps real machine commands into logical controller messages and sends them through `hmi_controller_transport` when a transport is configured. `winder_hmi_set_command_callback()` remains available for compatibility and logging.

```c
static void hmi_command_cb(hmi_command_t command,
                           const hmi_command_payload_t *payload,
                           void *user_ctx)
{
    (void)payload;
    (void)user_ctx;
    /* Debug/logging compatibility path. Production should use controller transport. */
}

winder_hmi_set_command_callback(hmi_command_cb, NULL);
```

When demo mode is enabled with `winder_hmi_enable_demo_mode(true)`, the component installs the mock controller transport so the UI can update without app-level machine logic. Demo responses are posted through the same internal HMI event queue used by future controller transports. When demo mode is disabled, the mock transport is unregistered.

Production integration should receive state from the external transport/application layer and post it with `winder_hmi_post_state()`, `winder_hmi_post_job_validation_result()`, or `winder_hmi_post_command_rejected()`. Those functions copy payloads into the internal FreeRTOS queue; `winder_hmi_tick()` processes queued events in the HMI/LVGL context and refreshes only the active screen. `winder_hmi_set_state()` remains available for same-context direct updates such as initialization.

## Intentionally Not Implemented

- Finished screen
- Alarm screen
- UART/protocol integration
- Real counters, validation, homing, motion, alarms, or safety logic
