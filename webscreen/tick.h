#pragma once
#include <lvgl.h>
#include "esp_timer.h"

static void start_lvgl_tick() {
  // Read monotonic time on demand: no 1 kHz timer task or duplicate tick sources.
  lv_tick_set_cb([]() -> uint32_t {
    return (uint32_t)(esp_timer_get_time() / 1000);
  });
}
