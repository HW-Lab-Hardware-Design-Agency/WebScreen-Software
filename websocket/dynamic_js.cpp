// dynamic_js.cpp

#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include "pins_config.h"
#include "rm67162.h"
#include "lvgl_elk.h"

// A flag or state variable to indicate setup success or not.
static bool dynamicJsSetupDone = false;
static unsigned long lastRetryAttempt = 0; // If you want a timed retry

void dynamic_js_setup() {
  Serial.println("DYNAMIC_JS: Setting up Elk + script.js scenario...");

  // 0) Remount SD card
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if(!SD_MMC.begin("/sdcard", true, false, 20)) {
    Serial.println("Card Mount Failed => dynamic setup fails");
    dynamicJsSetupDone = false;  // Mark as failed
    return;
  }

  // 1) Wi-Fi (station mode)
  WiFi.mode(WIFI_STA);

  // 2) LVGL display
  init_lvgl_display();

  // 3) 'S' driver for normal SD usage
  init_lv_fs();

  // 4) 'M' driver for memory usage (GIF)
  init_mem_fs();

  // 5) Init memory storage for RamImage
  for (int i = 0; i < MAX_RAM_IMAGES; i++) {
    g_ram_images[i].used   = false;
    g_ram_images[i].buffer = NULL;
    g_ram_images[i].size   = 0;
  }

  // 6) Create the Elk task
  xTaskCreatePinnedToCore(
      elk_task,          // function pointer
      "ElkTask",         // name of the task
      16384,             // stack size in bytes (16KB)
      NULL,              // parameter
      1,                 // priority
      NULL,              // task handle
      1                  // pin to core 1 (or 0 if you want)
  );

  // If we reach here, assume success
  dynamicJsSetupDone = true;
  Serial.println("DYNAMIC_JS: setup done!");
}

void dynamic_js_loop() {
  // If setup not done, try to do it (or re-do it)
  if(!dynamicJsSetupDone) {
    // Optionally limit how often we retry
    unsigned long now = millis();
    if(now - lastRetryAttempt > 5000) {
      lastRetryAttempt = now;
      Serial.println("DYNAMIC_JS: Trying dynamic_js_setup() again...");
      dynamic_js_setup();
    }
  }

  // Otherwise, if setup *did* succeed, just wait or do minimal tasks
  delay(1000);
}
