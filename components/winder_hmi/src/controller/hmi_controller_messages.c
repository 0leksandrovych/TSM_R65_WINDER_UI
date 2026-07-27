#include "hmi_controller_messages.h"

bool hmi_controller_message_init(hmi_controller_message_t *out,
                                 hmi_controller_msg_type_t type)
{
    if (out == NULL) {
        return false;
    }

    *out = (hmi_controller_message_t){0};
    out->type = type;
    return true;
}

bool hmi_controller_message_init_job(hmi_controller_message_t *out,
                                     hmi_controller_msg_type_t type,
                                     const hmi_controller_job_payload_t *job_payload)
{
    if (out == NULL || job_payload == NULL) {
        return false;
    }

    *out = (hmi_controller_message_t){0};
    out->type = type;
    out->data.job = *job_payload;
    return true;
}

bool hmi_controller_message_init_edge_trim(
    hmi_controller_message_t *out,
    float left_trim_mm,
    float right_trim_mm)
{
    if (out == NULL) {
        return false;
    }

    *out = (hmi_controller_message_t){0};
    out->type = HMI_CONTROLLER_MSG_APPLY_EDGE_TRIM;
    out->data.edge_trim.left_trim_mm = left_trim_mm;
    out->data.edge_trim.right_trim_mm = right_trim_mm;
    return true;
}
