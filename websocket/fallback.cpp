#include <Arduino.h>
#include "fallback.h"
#include "pins_config.h"
#include "rm67162.h"          // LCD driver
#include <lvgl.h>             // Ensure you have LVGL
#include "notification.h"      // For the GIF data

// We'll store references to the fallback label + gif
static lv_obj_t* fb_label = nullptr;
static lv_obj_t* fb_gif   = nullptr;

// A local display buffer/driver just for fallback, if you want separate from dynamic
static lv_disp_draw_buf_t fbDrawBuf;
static lv_color_t* fbBuf = nullptr;

// The flush callback
static void fallback_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

// Animation callback
static void scroll_anim_cb(void *var, int32_t v) {
  lv_obj_set_y((lv_obj_t *)var, v);
}

// Helper to create the scrolling animation
static void create_scroll_animation(lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, start, end);
  lv_anim_set_time(&a, duration);
  lv_anim_set_exec_cb(&a, scroll_anim_cb);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out); // Smooth
  lv_anim_set_repeat_count(&a, 2); // Repeat twice

  // On animation finish, hide label, show GIF
  lv_anim_set_ready_cb(&a, [](lv_anim_t *anim) {
    lv_obj_t *obj = (lv_obj_t *)anim->var;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(fb_gif, LV_OBJ_FLAG_HIDDEN);
  });

  lv_anim_start(&a);
}

void fallback_setup() {
  Serial.println("FALLBACK: Setting up scrolling label + GIF...");

  // 1) Minimal LVGL init (only if not already done).  
  //    If your main code calls lv_init() elsewhere, 
  //    you might skip or check if it’s safe to call again.
  lv_init();

  // 2) Power on the screen, set backlight
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  // 3) Init your display driver
  rm67162_init();
  lcd_setRotation(1);

  // 4) Allocate a buffer for fallback usage
  fbBuf = (lv_color_t*) ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE);
  if(!fbBuf) {
    Serial.println("FALLBACK: Failed to allocate buffer");
    return;
  }

  lv_disp_draw_buf_init(&fbDrawBuf, fbBuf, nullptr, LVGL_LCD_BUF_SIZE);

  // 5) Register fallback display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 536;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = fallback_disp_flush;
  disp_drv.draw_buf = &fbDrawBuf;
  lv_disp_drv_register(&disp_drv);

  // 6) Create a style for the label
  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_text_font(&style, &lv_font_montserrat_40);
  lv_style_set_text_color(&style, lv_color_white());
  lv_style_set_bg_color(&style, lv_color_black());
  lv_style_set_pad_all(&style, 5);
  lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);

  // 7) Create the label
  fb_label = lv_label_create(lv_scr_act());
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

  // 8) Create the scroll animation
  create_scroll_animation(fb_label, 240, -lv_obj_get_height(fb_label), 10000);

  // 9) Create the GIF
  fb_gif = lv_gif_create(lv_scr_act());
  lv_gif_set_src(fb_gif, &notification); // from notification.h
  lv_obj_align(fb_gif, LV_ALIGN_CENTER, 0, 0);

  // Show label first, hide GIF
  lv_obj_clear_flag(fb_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(fb_gif, LV_OBJ_FLAG_HIDDEN);
}

void fallback_loop() {
  // Let LVGL run
  lv_timer_handler();

  // If serial data arrives, treat that as an input to update label
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    lv_label_set_text(fb_label, line.c_str());
    lv_obj_align(fb_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(fb_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(fb_gif, LV_OBJ_FLAG_HIDDEN);

    // re-run animation
    create_scroll_animation(fb_label, 240, -lv_obj_get_height(fb_label), 10000);
  }
}
