#pragma once
#include <lvgl.h>
#include "esp_timer.h"

static void start_lvgl_tick()
{
    static bool started = false; // guard: run only once
    if (started)
        return;

#if !defined(LV_TICK_CUSTOM) || LV_TICK_CUSTOM == 0
    static esp_timer_handle_t h;
    const esp_timer_create_args_t cbs = {
        .callback = [](void *)
        { lv_tick_inc(1); },
        .name = "lvgl_1ms"};
    esp_timer_create(&cbs, &h);
    esp_timer_start_periodic(h, 1000); // 1 ms
#endif
    started = true;
}
