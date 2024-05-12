#include <Arduino.h>
#include "lvgl.h" /* https://github.com/lvgl/lvgl.git */
#include "rm67162.h"

#if ARDUINO_USB_CDC_ON_BOOT != 1
#warning "If you need to monitor printed data, be sure to set USB CDC On boot to ENABLE, otherwise you will not see any data in the serial monitor"
#endif

#ifndef BOARD_HAS_PSRAM
#error "Detected that PSRAM is not turned on. Please set PSRAM to OPI PSRAM in ArduinoIDE"
#endif

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf;
lv_obj_t *label;  // Global label object

void my_disp_flush(lv_disp_drv_t *disp,
                   const lv_area_t *area,
                   lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
  lv_disp_flush_ready(disp);
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

  rm67162_init();  // amoled lcd initialization

  lcd_setRotation(1);

  lv_init();

  buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE);
  assert(buf);


  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_LCD_BUF_SIZE);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, "Waiting for serial input...");
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);  // Break the long lines
  lv_obj_set_width(label, 480);                       // Adjust the label width to the display width
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);         // Center the label on the screen
}

void loop() {
  lv_timer_handler();

  if (Serial.available()) {
    String received = Serial.readStringUntil('\n');
    lv_label_set_text(label, received.c_str());
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);  // Re-center the label
  }
  delay(5);
}