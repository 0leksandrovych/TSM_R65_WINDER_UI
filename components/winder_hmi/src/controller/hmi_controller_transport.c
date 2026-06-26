#include "hmi_controller_transport.h"

static hmi_controller_transport_t s_transport;

void hmi_controller_transport_set(const hmi_controller_transport_t *transport)
{
    if (transport == NULL) {
        s_transport = (hmi_controller_transport_t){0};
        return;
    }

    s_transport = *transport;
}

bool hmi_controller_transport_send(const hmi_controller_message_t *message)
{
    if (message == NULL || s_transport.send == NULL) {
        return false;
    }

    return s_transport.send(message, s_transport.user_ctx);
}

bool hmi_controller_transport_is_available(void)
{
    return s_transport.send != NULL;
}
