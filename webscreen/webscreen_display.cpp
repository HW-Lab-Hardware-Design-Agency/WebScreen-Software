#include "webscreen_display.h"
#include <Arduino.h>
#include <lvgl.h>
#include "globals.h"
#include "pins_config.h"
#include "rm67162.h"
#include "tick.h"
#include "webscreen_hardware.h"
#include "webscreen_main.h"

static void display_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixels) {
  lcd_PushColors(area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area),
                 (uint16_t *)pixels);
  lv_display_flush_ready(display);
}

bool init_lvgl_display() {
  static lv_display_t *display = nullptr;
  if (display) return true;

  if (!lv_is_initialized()) lv_init();
  start_lvgl_tick();
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  rm67162_init();
  lcd_setRotation(WEBSCREEN_DISPLAY_ROTATION);
  webscreen_display_set_brightness(g_webscreen_config.display.brightness);

  // Flushes are synchronous; both runtimes share this 40-line internal-RAM buffer.
  alignas(LV_DRAW_BUF_ALIGN) static uint8_t pixels[EXAMPLE_LCD_H_RES * 40 * 2];
  lv_display_t *candidate = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
  if (!candidate) {
    LOG("Failed to create LVGL display");
    return false;
  }
  lv_display_set_color_format(candidate, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_flush_cb(candidate, display_flush);
  lv_display_set_buffers(candidate, pixels, nullptr, sizeof(pixels), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_obj_t *screen = lv_display_get_screen_active(candidate);
  lv_obj_set_style_bg_color(screen, lv_color_hex(g_bg_color), 0);
  lv_obj_set_style_text_color(screen, lv_color_hex(g_fg_color), 0);
  display = candidate;
  return true;
}
