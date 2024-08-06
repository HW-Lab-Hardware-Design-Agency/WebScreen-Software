#include <Arduino.h>
#include "lvgl.h"          // Include LVGL library
#include "rm67162.h"       // Display driver
#include "notification.h"  // Include the header for the animated GIF

// Declare the GIF image data from the generated file
extern const lv_img_dsc_t notification;

#if ARDUINO_USB_CDC_ON_BOOT != 1
#warning "If you need to monitor printed data, be sure to set USB CDC On boot to ENABLE, otherwise you will not see any data in the serial monitor"
#endif

#ifndef BOARD_HAS_PSRAM
#error "Detected that PSRAM is not turned on. Please set PSRAM to OPI PSRAM in ArduinoIDE"
#endif

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;
lv_obj_t *label;  // Global label object
lv_obj_t *gif;    // GIF object

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

static void scroll_anim_cb(void *var, int32_t v) {
  lv_obj_set_y((lv_obj_t *)var, v);
}

void create_scroll_animation(lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, start, end);
  lv_anim_set_time(&a, duration);
  lv_anim_set_exec_cb(&a, scroll_anim_cb);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out); // Smooth animation
  lv_anim_set_repeat_count(&a, 2); // Repeat twice

  lv_anim_set_ready_cb(&a, [](lv_anim_t *anim) {
    lv_obj_t *obj = static_cast<lv_obj_t *>(anim->var);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN); // Hide the text label
    lv_obj_clear_flag(gif, LV_OBJ_FLAG_HIDDEN); // Show the GIF
  });

  lv_anim_start(&a);
}

void setup() {
  Serial.begin(115200);
  /*
    * Compatible with touch version
    * Touch version, IO38 is the screen power enable
    * Non-touch version, IO38 is an onboard LED light
    * * */
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  rm67162_init();  // Initialize the AMOLED display
  lcd_setRotation(3);

  lv_init();  // Initialize the LVGL library

  buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE);
  assert(buf);

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_LCD_BUF_SIZE);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 536;  // Update to your display resolution
  disp_drv.ver_res = 240;  // Update to your display resolution
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Style settings for the label
  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_text_font(&style, &lv_font_montserrat_40);  // Use Montserrat font size 40
  lv_style_set_text_color(&style, lv_color_white());       // White text color
  lv_style_set_bg_color(&style, lv_color_black());         // Black background color
  lv_style_set_pad_all(&style, 5);                         // Set padding
  lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);   // Center align the text

  label = lv_label_create(lv_scr_act());
  lv_obj_add_style(label, &style, 0);  // Apply style to label
  lv_label_set_text(label, "This is a long text that will be scrolled up and down automatically. Add more text here to test scrolling functionality. This is a very long text indeed.");
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);  // Break the long lines
  lv_obj_set_width(label, 525);                       // Adjust the label width to the display width
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);         // Align the label to the bottom middle

  // Create scroll animation
  create_scroll_animation(label, EXAMPLE_LCD_V_RES, -lv_obj_get_height(label), 10000);  // 15 seconds for a full scroll

  // Create a GIF object
  gif = lv_gif_create(lv_scr_act());
  lv_gif_set_src(gif, &notification);
  lv_obj_align(gif, LV_ALIGN_CENTER, 0, 0);  // Correctly align the GIF on the screen

  // Initially hide the label and show the GIF
  lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(gif, LV_OBJ_FLAG_HIDDEN);
}

void loop() {
  lv_timer_handler();

  if (Serial.available()) {
    String received = Serial.readStringUntil('\n');
    lv_label_set_text(label, received.c_str());
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0); // Re-align the label
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN); // Show the label
    lv_obj_add_flag(gif, LV_OBJ_FLAG_HIDDEN); // Hide the GIF
    create_scroll_animation(label, EXAMPLE_LCD_V_RES, -lv_obj_get_height(label), 10000); // Restart scroll animation
  }
  delay(5);
}
