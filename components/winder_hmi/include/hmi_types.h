#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define HMI_DISPLAY_WIDTH  800
#define HMI_DISPLAY_HEIGHT 480

#define HMI_TOPBAR_HEIGHT 54
#define HMI_BOTTOM_NAV_HEIGHT 70

typedef enum {
    HMI_COLOR_NEUTRAL = 0,
    HMI_COLOR_BLUE,
    HMI_COLOR_GREEN,
    HMI_COLOR_AMBER,
    HMI_COLOR_RED,
    HMI_COLOR_DIM,
} hmi_color_role_t;
