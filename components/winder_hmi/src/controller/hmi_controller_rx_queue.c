#include "hmi_controller_rx_queue.h"

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[
    HMI_CONTROLLER_RX_QUEUE_CAPACITY * sizeof(hmi_controller_link_decoded_t)
];
static QueueHandle_t s_queue;

bool hmi_controller_rx_queue_init(void)
{
    if (s_queue != NULL) {
        return true;
    }

    s_queue = xQueueCreateStatic(
        HMI_CONTROLLER_RX_QUEUE_CAPACITY,
        sizeof(hmi_controller_link_decoded_t),
        s_queue_storage,
        &s_queue_control);

    return s_queue != NULL;
}

bool hmi_controller_rx_queue_push(const hmi_controller_link_decoded_t *decoded)
{
    if (decoded == NULL || decoded->type == HMI_CONTROLLER_LINK_DECODED_NONE) {
        return false;
    }
    if (s_queue == NULL && !hmi_controller_rx_queue_init()) {
        return false;
    }

    return xQueueSend(s_queue, decoded, 0) == pdTRUE;
}

bool hmi_controller_rx_queue_pop(hmi_controller_link_decoded_t *out_decoded)
{
    if (out_decoded == NULL || s_queue == NULL) {
        return false;
    }

    return xQueueReceive(s_queue, out_decoded, 0) == pdTRUE;
}

void hmi_controller_rx_queue_clear(void)
{
    if (s_queue != NULL) {
        xQueueReset(s_queue);
    }
}
