#include "dynamic_js.h"
#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>

#include "pins_config.h"
#include "rm67162.h"
#include "lvgl_elk.h"  // Contains init_lvgl_display(), init_lv_fs(), etc.

void dynamic_js_setup() {
  Serial.println("DYNAMIC_JS: Setting up Elk + script.js scenario...");

  // If needed, mount the SD again (or confirm it’s already mounted):
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if(!SD_MMC.begin("/sdcard", true, false, 1000000)) {
    Serial.println("Card Mount Failed => can't run dynamic JS code properly");
    return;
  }

  // 1) Wi-Fi (station mode) - or skip if already connected
  WiFi.mode(WIFI_STA);

  // 2) Initialize LVGL display
  init_lvgl_display();

  // 3) 'S' driver for normal SD usage
  init_lv_fs();

  // 4) 'M' driver for memory usage (GIF)
  init_mem_fs();

  // 5) Init memory storage
  init_ram_images();

  // 6) Spawn Elk task
  xTaskCreatePinnedToCore(
      elk_task,          
      "ElkTask",         
      16384,             
      NULL,              
      1,                 
      NULL,              
      1                  
  );

  Serial.println("DYNAMIC_JS: setup done!");
}

void dynamic_js_loop() {
  // The Elk code runs in the FreeRTOS task (elk_task),
  // so we typically only do minimal work here
  delay(500);
}
