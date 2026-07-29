#pragma once

#include <stdbool.h>

#include "lvgl.h"
#include "hmi_types.h"

/* Minimal reusable confirmation dialog.
 *
 * Renders a modal overlay with a title, a body message, and two buttons:
 * a neutral CANCEL and a caller-styled confirm action. The confirm callback
 * runs only when the user taps the confirm button; CANCEL and any navigation
 * change dismiss the dialog without invoking it.
 *
 * There is at most one confirmation dialog on screen; opening a new one
 * replaces any existing dialog. */

typedef void (*modal_confirm_cb_t)(void *user_ctx);

typedef struct {
    const char *title;        /* Bold heading. */
    const char *body;         /* Explanatory text (wraps). */
    const char *cancel_text;  /* NULL -> "CANCEL". */
    const char *confirm_text; /* NULL -> "CONFIRM". */
    hmi_color_role_t confirm_role; /* Confirm button color role. */
    modal_confirm_cb_t confirm_cb; /* Invoked on confirm; may be NULL. */
    void *user_ctx;                /* Passed to confirm_cb. */
} modal_confirm_config_t;

void modal_confirm_open(const modal_confirm_config_t *config);
void modal_confirm_close(void);
bool modal_confirm_is_open(void);
