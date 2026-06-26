#pragma once

/* HMI UI tick period in milliseconds.
 * Drives the LVGL timer callback and the mock controller simulation step.
 *
 * IMPORTANT: UART RX/TX must NOT be tied to this period.
 * Future UART transport must be task/event-driven and must not be polled
 * by the HMI UI tick. */
#define HMI_TICK_PERIOD_MS              100U
#define HMI_MOCK_TICK_PERIOD_MS         HMI_TICK_PERIOD_MS

/* Internal event queue capacity.
 * Fixed static allocation via xQueueCreateStatic — never resized at runtime. */
#define HMI_EVENT_QUEUE_LENGTH          12U

/* Maximum internal events processed per LVGL tick.
 * Prevents a burst of queued events from monopolizing the UI context;
 * any remaining events are drained on the next tick. */
#define HMI_MAX_EVENTS_PER_TICK         8U

/* Command bus listener capacity.
 * Expected slots:
 *   [0] hmi_controller_client  — forwards commands to the transport layer
 *   [1] optional diagnostics/debug observer
 *   [2] optional test hook
 *   [3] spare
 * Fixed static array — no dynamic allocation. */
#define HMI_COMMAND_BUS_MAX_LISTENERS   4U

/* Pending command timeout constants (milliseconds).
 * Used to detect lost ACK/REJECT responses before UART is implemented.
 * Timeout clears the pending state and records a communication error;
 * it does not produce fake success. */
#define HMI_PENDING_DEFAULT_TIMEOUT_MS  1000U
#define HMI_PENDING_VALIDATE_TIMEOUT_MS 2000U
#define HMI_PENDING_START_TIMEOUT_MS    2000U
#define HMI_PENDING_STOP_TIMEOUT_MS     2000U
