#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include <stdio.h>
#include "pins_config.h"

// Include fallback & dynamic
extern void fallback_setup();
extern void fallback_loop();
extern void dynamic_js_setup();
extern void dynamic_js_loop();

// We'll share these with fallback.cpp
lv_disp_draw_buf_t draw_buf;
lv_color_t *buf = nullptr;
bool fallbackActive = false;

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Try mounting SD
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  bool cardMounted = SD_MMC.begin("/sdcard", true, false, 1000000);

  if (!cardMounted) {
    // Fallback
    fallbackActive = true;
    fallback_setup();
    return;
  }

  // Check if /script.js exists
  File f = SD_MMC.open("/script.js");
  if (!f) {
    // Fallback
    fallbackActive = true;
    f.close();
    fallback_setup();
  } else {
    // Use dynamic JS
    fallbackActive = false;
    f.close();
    dynamic_js_setup();
  }
}

void loop() {
  if (fallbackActive) {
    fallback_loop();
  } else {
    dynamic_js_loop();
  }
}
