#include "hmi_event_queue.h"
#include "hmi_config.h"

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[HMI_EVENT_QUEUE_LENGTH * sizeof(hmi_internal_event_t)];
static QueueHandle_t s_queue;

bool hmi_event_queue_init(void)
{
    if (s_queue != NULL) {
        return true;
    }

    s_queue = xQueueCreateStatic(
        HMI_EVENT_QUEUE_LENGTH,
        sizeof(hmi_internal_event_t),
        s_queue_storage,
        &s_queue_control
    );

    return s_queue != NULL;
}

bool hmi_event_queue_post(const hmi_internal_event_t *event)
{
    if (event == NULL || event->type == HMI_INTERNAL_EVENT_NONE) {
        return false;
    }

    if (s_queue == NULL && !hmi_event_queue_init()) {
        return false;
    }

    return xQueueSend(s_queue, event, 0) == pdTRUE;
}

bool hmi_event_queue_poll(hmi_internal_event_t *out_event)
{
    if (out_event == NULL || s_queue == NULL) {
        return false;
    }

    return xQueueReceive(s_queue, out_event, 0) == pdTRUE;
}

void hmi_event_queue_clear(void)
{
    if (s_queue != NULL) {
        xQueueReset(s_queue);
    }
}
