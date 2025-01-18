#include "dynamic_js.h"
#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>

#include "pins_config.h"
#include "rm67162.h"
#include "lvgl_elk.h"  // Contains init_lvgl_display(), init_lv_fs(), etc.

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_task_wdt.h"

void dynamic_js_setup() {
  Serial.println("DYNAMIC_JS: Setting up Elk + script.js scenario...");

  // Ensure SD card is mounted
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if(!SD_MMC.begin("/sdcard", true, false, 1000000)) {
    Serial.println("Card Mount Failed => can't run dynamic JS code properly");
    return;
  }

  // Initialize LVGL display
  init_lvgl_display();

  // Register file system drivers
  init_lv_fs(); // SD-based file system
  init_mem_fs(); // Memory-based file system (GIF support)

  // Initialize RAM images for dynamic operations
  init_ram_images();

  // Spawn the Elk FreeRTOS task
  xTaskCreatePinnedToCore(
      elk_task,          // Elk task function
      "ElkTask",         // Name of the task
      16384,             // Stack size in bytes
      NULL,              // Task input parameter
      1,                 // Task priority
      NULL,              // Task handle
      1                  // Run on core 1
  );

  Serial.println("DYNAMIC_JS: setup complete!");
}

void dynamic_js_loop() {
  esp_task_wdt_reset(); // Reset the watchdog periodically
  lv_timer_handler(); // Ensure LVGL tasks are handled
  vTaskDelay(500 / portTICK_PERIOD_MS); // 500ms delay
}
