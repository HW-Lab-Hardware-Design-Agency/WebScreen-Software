#include <Arduino.h>
#include "fallback.h"
#include "pins_config.h"
#include "rm67162.h"
#include <lvgl.h>
#include "notification.h"
#include "globals.h"
#include "tick.h"

static lv_obj_t* fb_label = nullptr;
static lv_obj_t* fb_gif   = nullptr;

static lv_color_t* fbBuf = nullptr;

static void fallback_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)px_map);
  lv_display_flush_ready(disp);
}

static void scroll_anim_cb(void *var, int32_t v) {
  lv_obj_set_y((lv_obj_t *)var, v);
}

static void create_scroll_animation(lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, start, end);
  lv_anim_set_duration(&a, duration);
  lv_anim_set_exec_cb(&a, scroll_anim_cb);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_set_repeat_count(&a, 2);

  lv_anim_set_completed_cb(&a, [](lv_anim_t *anim) {
    lv_obj_t *obj = (lv_obj_t *)anim->var;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(fb_gif, LV_OBJ_FLAG_HIDDEN);
  });

  lv_anim_start(&a);
}

void fallback_setup() {
  LOG("FALLBACK: Setting up scrolling label + GIF...");

  lv_init();
  start_lvgl_tick();

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  rm67162_init();
  lcd_setRotation(1);

  fbBuf = (lv_color_t*) ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE);
  if(!fbBuf) {
    LOG("FALLBACK: Failed to allocate buffer");
    return;
  }

  lv_display_t * disp = lv_display_create(536, 240);
  lv_display_set_flush_cb(disp, fallback_disp_flush);
  lv_display_set_buffers(disp, fbBuf, nullptr, sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_text_font(&style, &lv_font_montserrat_40);
  lv_style_set_text_color(&style, lv_color_white());
  lv_style_set_bg_color(&style, lv_color_black());
  lv_style_set_pad_all(&style, 5);
  lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);

  fb_label = lv_label_create(lv_screen_active());
  lv_obj_add_style(fb_label, &style, 0);
  lv_label_set_text(fb_label,
    "/\\_/\\\n"
    "= ( • . • ) =\n"
    " /       \\ \n"
    "Welcome to Webscreen! This is the Notification App, you can also run apps from the SD card.\n"
    " \n"
    " \n"
  );
  lv_label_set_long_mode(fb_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(fb_label, 525);
  lv_obj_align(fb_label, LV_ALIGN_CENTER, 0, 0);

  create_scroll_animation(fb_label, 240, -lv_obj_get_height(fb_label), 10000);

  fb_gif = lv_image_create(lv_screen_active());
  lv_image_set_src(fb_gif, &notification);
  lv_obj_align(fb_gif, LV_ALIGN_CENTER, 0, 0);

  lv_obj_remove_flag(fb_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(fb_gif, LV_OBJ_FLAG_HIDDEN);
}

void fallback_loop() {
  lv_timer_handler();

  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    lv_label_set_text(fb_label, line.c_str());
    lv_obj_align(fb_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(fb_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(fb_gif, LV_OBJ_FLAG_HIDDEN);

    create_scroll_animation(fb_label, 240, -lv_obj_get_height(fb_label), 10000);
  }
}