#include "dynamic_js.h"
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <WiFi.h>

#include "pins_config.h"
#include "rm67162.h"
#include "webscreen_runtime.h"
#include "globals.h"

String g_script_filename = "/app.js";


void dynamic_js_setup() {
  LOG("DYNAMIC_JS: Setting up Elk + script scenario...");

  WiFi.mode(WIFI_STA);

  if (!webscreen_runtime_start_javascript(g_script_filename.c_str())) {
    LOG("Failed to start JavaScript runtime");
    return;
  }

  LOG("DYNAMIC_JS: setup done!");
}


void dynamic_js_loop() {
  webscreen_runtime_loop_javascript();
}
