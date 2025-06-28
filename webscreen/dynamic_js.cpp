#include "dynamic_js.h"
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

#include "pins_config.h"
#include "rm67162.h"
#include "lvgl_elk.h"
#include "globals.h"

String g_script_filename = "/app.js";

void dynamic_js_setup() {
  LOG("DYNAMIC_JS: Setting up Elk + script scenario...");

  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if(!SD_MMC.begin("/sdcard", true, false, 1000000)) {
    LOG("Card Mount Failed => can't run dynamic JS code properly");
    return;
  }

  WiFi.mode(WIFI_STA);

  init_lvgl_display();

  init_lv_fs();

  init_mem_fs();

  init_ram_images();

  xTaskCreatePinnedToCore(
      elk_task,
      "ElkTask",
      16384,
      NULL,
      1,
      NULL,
      0
  );

  LOG("DYNAMIC_JS: setup done!");
}

void dynamic_js_loop() {
  vTaskDelay(pdMS_TO_TICKS(50));
}
