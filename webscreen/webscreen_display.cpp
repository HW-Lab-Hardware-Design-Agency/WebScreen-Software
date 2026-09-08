#include "webscreen_display.h"
#include <Arduino.h>
#include <lvgl.h>
#include "globals.h"
#include "pins_config.h"
#include "rm67162.h"
#include "tick.h"
#include "webscreen_hardware.h"
#include "webscreen_main.h"

static void display_flush(lv_disp_drv_t *display, const lv_area_t *area, lv_color_t *pixels) {
  lcd_PushColors(area->x1, area->y1, lv_area_get_width(area), lv_area_get_height(area),
                 (uint16_t *)pixels);
  lv_disp_flush_ready(display);
}

bool init_lvgl_display() {
  static lv_disp_t *display = nullptr;
  if (display) return true;

  if (!lv_is_initialized()) lv_init();
  start_lvgl_tick();
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  rm67162_init();
  lcd_setRotation(WEBSCREEN_DISPLAY_ROTATION);
  webscreen_display_set_brightness(g_webscreen_config.display.brightness);

  // Flushes are synchronous; both runtimes share this 40-line internal-RAM buffer.
  static_assert(LV_COLOR_DEPTH == 16 && LV_COLOR_16_SWAP == 1, "Panel requires swapped RGB565");
  alignas(4) static lv_color_t pixels[EXAMPLE_LCD_H_RES * 40];
  static lv_disp_draw_buf_t draw_buffer;
  static lv_disp_drv_t driver;
  lv_disp_draw_buf_init(&draw_buffer, pixels, nullptr, EXAMPLE_LCD_H_RES * 40);
  lv_disp_drv_init(&driver);
  driver.hor_res = EXAMPLE_LCD_H_RES;
  driver.ver_res = EXAMPLE_LCD_V_RES;
  driver.flush_cb = display_flush;
  driver.draw_buf = &draw_buffer;
  lv_disp_t *candidate = lv_disp_drv_register(&driver);
  if (!candidate) {
    LOG("Failed to create LVGL display");
    return false;
  }
  lv_obj_t *screen = lv_disp_get_scr_act(candidate);
  lv_obj_set_style_bg_color(screen, lv_color_hex(g_bg_color), 0);
  lv_obj_set_style_text_color(screen, lv_color_hex(g_fg_color), 0);
  display = candidate;
  return true;
}
