#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include <stdio.h>

// 1) Pin definitions + config
#include "pins_config.h"

// 2) LVGL + display driver
#include "rm67162.h"  // Your custom display driver
#include "lvgl_elk.h"


/******************************************************************************
 * K) Arduino setup() & loop()
 ******************************************************************************/
void setup() {
  Serial.begin(115200);
  delay(2000);

  // 1) Mount SD card
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if(!SD_MMC.begin("/sdcard", true, false, 1000000)) {
    Serial.println("Card Mount Failed");
    return;
  }

  // 2) Wi-Fi (station mode)
  WiFi.mode(WIFI_STA);

  // 3) Initialize LVGL display
  init_lvgl_display();

  // 4) 'S' driver for normal SD usage
  init_lv_fs();

  // 4b) 'M' driver for memory usage (GIF)
  init_mem_fs();

  // 4c) Init Memory storage
  for (int i = 0; i < MAX_RAM_IMAGES; i++) {
    g_ram_images[i].used   = false;
    g_ram_images[i].buffer = NULL;
    g_ram_images[i].size   = 0;
  }

  // ***** Instead of calling Elk creation + script here,
  // ***** we spawn a new FreeRTOS task with a larger stack:
  xTaskCreatePinnedToCore(
      elk_task,          // function pointer
      "ElkTask",         // name of the task
      16384,             // stack size in bytes (16KB)
      NULL,              // parameter
      1,                 // priority
      NULL,              // task handle
      1                  // pin to core 1 (or 0 if you want)
  );
}

void loop() {
  delay(1000);
}
