#include <Arduino.h>
#include "fallback.h"
#include "pins_config.h"
#include "rm67162.h"
#include <lvgl.h>
#include "notification.h"
#include "webscreen.h"
#include "globals.h"
#include "tick.h"
#include "serial_commands.h"
#include "webscreen_main.h"
#include "webscreen_hardware.h"
static lv_obj_t *fb_label = nullptr;
static lv_obj_t *fb_gif = nullptr;
static lv_obj_t *fb_container = nullptr;
static lv_obj_t *fb_image = nullptr;
static uint8_t *fbBuf = nullptr;

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
  lv_anim_set_time(&a, duration);
  lv_anim_set_exec_cb(&a, scroll_anim_cb);
  lv_anim_set_path_cb(&a, lv_anim_path_linear);
  lv_anim_set_repeat_count(&a, 2);
  lv_anim_set_repeat_delay(&a, 500);
  lv_anim_set_ready_cb(&a, [](lv_anim_t *anim) {
    lv_obj_t *obj = (lv_obj_t *)anim->var;
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(fb_gif, LV_OBJ_FLAG_HIDDEN);
  });

  lv_anim_start(&a);
}

void fallback_setup() {
  LOG("FALLBACK: Setting up scrolling label + GIF...");
  // If JS mode already brought the display up, re-initializing would double-init the SPI bus (abort).
  bool display_already_up = lv_is_initialized();
  SerialCommands::init();
  if (!display_already_up) {
    lv_init();
    start_lvgl_tick();
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);
    rm67162_init();
    lcd_setRotation(1);

    // Apply configured brightness
    if (g_webscreen_config.display.brightness > 0) {
      lcd_brightness(g_webscreen_config.display.brightness);
      // Keep the cached value in sync so the button's display off/on toggle restores it
      webscreen_hardware_sync_brightness(g_webscreen_config.display.brightness);
    }

    // 2 bytes/px: the display renders RGB565 (v9's lv_color_t would be 3)
  fbBuf = (uint8_t *)ps_malloc(LVGL_LCD_BUF_SIZE * 2);
    if (!fbBuf) {
      LOG("FALLBACK: Failed to allocate buffer");
      return;
    }

    lv_display_t *disp = lv_display_create(536, 240);
    // Panel expects byte-swapped RGB565 (was LV_COLOR_16_SWAP in LVGL 8)
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(disp, fallback_disp_flush);
    lv_display_set_buffers(disp, fbBuf, nullptr,
                           LVGL_LCD_BUF_SIZE * 2,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
  }

  // Create a container for the image and label
  fb_container = lv_obj_create(lv_scr_act());
  lv_obj_set_size(fb_container, 536, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(fb_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_opa(fb_container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_pad_all(fb_container, 0, 0);
  lv_obj_set_flex_flow(fb_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(fb_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Create the webscreen image with better quality settings
  fb_image = lv_img_create(fb_container);
  lv_img_set_src(fb_image, &webscreen);
  lv_img_set_antialias(fb_image, true);
  lv_obj_set_style_pad_bottom(fb_image, 15, 0);
  lv_obj_set_style_img_recolor(fb_image, lv_color_white(), 0);
  lv_obj_set_style_img_recolor_opa(fb_image, 0, 0);

  // Create the label with improved styling
  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_text_font(&style, &lv_font_montserrat_40);
  lv_style_set_text_color(&style, lv_color_white());
  lv_style_set_bg_color(&style, lv_color_black());
  lv_style_set_pad_all(&style, 10);
  lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);
  lv_style_set_text_line_space(&style, 8);

  fb_label = lv_label_create(fb_container);
  lv_obj_add_style(fb_label, &style, 0);
  lv_label_set_text(fb_label,
                    "Welcome! This is the Notification App, you can also run apps from the SD card.\n"
                    " \n"
                    " \n");
  lv_label_set_long_mode(fb_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(fb_label, 525);

  lv_obj_align(fb_container, LV_ALIGN_TOP_MID, 0, 240);
  create_scroll_animation(fb_container, 240, -lv_obj_get_height(fb_container) - 100, 8000);
  fb_gif = lv_gif_create(lv_scr_act());
  lv_gif_set_src(fb_gif, &notification);
  lv_obj_align(fb_gif, LV_ALIGN_CENTER, 0, 0);
  lv_obj_clear_flag(fb_container, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(fb_gif, LV_OBJ_FLAG_HIDDEN);
}

void fallback_loop() {
  // Handle power button (short press = display toggle, long press = power off)
  webscreen_hardware_handle_button();

  lv_timer_handler();
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    
    // Check if it's a command (starts with /)
    if (line.startsWith("/")) {
      SerialCommands::processCommand(line);
    } else {
      // Display text as before
      lv_label_set_text(fb_label, line.c_str());
      lv_obj_align(fb_container, LV_ALIGN_CENTER, 0, 0);
      lv_obj_clear_flag(fb_container, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(fb_gif, LV_OBJ_FLAG_HIDDEN);
      create_scroll_animation(fb_container, 240, -lv_obj_get_height(fb_container) - 100, 8000);
    }
  }
}
