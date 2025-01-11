#pragma once

#include <Arduino.h>
#include "lvgl.h"              // For LVGL types
#include "notification.h"      // For the GIF data
// extern const lv_img_dsc_t notification; // Typically declared in notification.h

// Public API
void fallback_setup();
void fallback_loop();
