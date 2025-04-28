// fallback.cpp

#include "fallback.h" // Own header first

#include <Arduino.h> // For Serial, String? (check usage)
#include <lvgl.h>     // Uses LVGL objects, styles, animations directly

#include "pins_config.h" // Needs pin definitions
#include "rm67162.h"     // Needs lcd_PushColors etc.
#include "notification.h"// Needs the image data `notification`
#include "globals.h"     // Needs LOG, EXAMPLE_LCD_*, LVGL_LCD_BUF_SIZE, ps_malloc?
#include "tick.h"        // Needs start_lvgl_tick()

namespace
{
  // how long the label scrolls (in ms)
  static constexpr uint32_t SCROLL_DURATION = 10000;

  // LVGL objects & buffers
  static lv_obj_t *fb_label = nullptr;
  static lv_obj_t *fb_gif = nullptr;
  static lv_disp_draw_buf_t fb_draw_buf;
  static lv_color_t *fb_buf = nullptr;

  // our fallback display flush callback
  static void disp_flush(lv_disp_drv_t *disp,
                         const lv_area_t *area,
                         lv_color_t *color_p)
  {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    // We know color_p->full is a uint16_t, so take its address as uint16_t*
    lcd_PushColors(area->x1,
                   area->y1,
                   w,
                   h,
                   reinterpret_cast<uint16_t *>(&color_p->full));
    lv_disp_flush_ready(disp);
  }

  // animation callback: move the label vertically
  static void scroll_anim_cb(void *var, int32_t v)
  {
    lv_obj_set_y(static_cast<lv_obj_t *>(var), v);
  }

  // kick off (or re-kick) the scrolling
  static void start_scroll_anim(lv_obj_t *obj)
  {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    // from just off the bottom to just above the top
    lv_anim_set_values(&a,
                       static_cast<int32_t>(EXAMPLE_LCD_V_RES),
                       -lv_obj_get_height(obj));
    lv_anim_set_time(&a, SCROLL_DURATION);
    lv_anim_set_exec_cb(&a, scroll_anim_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, 2);
    // when done, hide the label & show the GIF
    lv_anim_set_ready_cb(&a, [](lv_anim_t *anim)
                         {
      auto* lbl = static_cast<lv_obj_t*>(anim->var);
      lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(fb_gif, LV_OBJ_FLAG_HIDDEN); });
    lv_anim_start(&a);
  }
}

void fallback_setup()
{
  LOG("FALLBACK: setting up");

  // 1) LVGL init + tick
  lv_init();
  start_lvgl_tick();

  // 2) power & display HW
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  rm67162_init();
  lcd_setRotation(1);

  // 3) draw‐buffer
  fb_buf = static_cast<lv_color_t *>(
      ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE));
  if (!fb_buf)
  {
    LOG("FALLBACK: buffer alloc failed");
    return;
  }
  lv_disp_draw_buf_init(&fb_draw_buf, fb_buf, nullptr, LVGL_LCD_BUF_SIZE);

  // 4) register a dedicated display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.draw_buf = &fb_draw_buf;
  disp_drv.flush_cb = disp_flush;
  lv_disp_drv_register(&disp_drv);

  // 5) style for the label
  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_text_font(&style, &lv_font_montserrat_40);
  lv_style_set_text_color(&style, lv_color_white());
  lv_style_set_bg_color(&style, lv_color_black());
  lv_style_set_pad_all(&style, 5);
  lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);

  // 6) create & configure the label
  fb_label = lv_label_create(lv_scr_act());
  lv_obj_add_style(fb_label, &style, 0);
  lv_label_set_text(fb_label,
                    "/\\_/\\\n"
                    "= ( • . • ) =\n"
                    " /       \\ \n"
                    "Welcome to Webscreen! This is the Notification App,\n"
                    "you can also run apps from the SD card.\n");
  lv_label_set_long_mode(fb_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(fb_label, EXAMPLE_LCD_H_RES - 20);
  lv_obj_align(fb_label, LV_ALIGN_CENTER, 0, 0);

  // 7) start scrolling
  start_scroll_anim(fb_label);

  // 8) create the GIF (hidden until scroll finishes)
  fb_gif = lv_gif_create(lv_scr_act());
  lv_gif_set_src(fb_gif, &notification);
  lv_obj_align(fb_gif, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(fb_gif, LV_OBJ_FLAG_HIDDEN);
}

void fallback_loop()
{
  // let LVGL run
  lv_timer_handler();

  // if there’s serial input, replace the label & re-scroll
  if (Serial.available())
  {
    String line = Serial.readStringUntil('\n');
    lv_label_set_text(fb_label, line.c_str());
    lv_obj_align(fb_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(fb_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(fb_gif, LV_OBJ_FLAG_HIDDEN);
    start_scroll_anim(fb_label);
  }
}
