#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>

#include "pins_config.h"
#include "fallback.h"
#include "dynamic_js.h"
#include "globals.h"
#include "webscreen_main.h"
#include "webscreen_hardware.h"
#include "webscreen_network.h"

#include <ArduinoJson.h>

bool g_mqtt_enabled = false;
static bool useFallback = false;
uint32_t g_bg_color = 0x000000;
uint32_t g_fg_color = 0xFFFFFF;

void setup() {
  Serial.begin(115200);

  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, HIGH);

  if (!webscreen_hardware_init_sd_card()) {
    LOG("SD card mount fail => starting fallback mode.");
    useFallback = true;
    fallback_setup();
    return;
  }

  String s, p, scriptFile;
  if (!webscreen_load_config("/webscreen.json", s, p, scriptFile, g_mqtt_enabled, g_bg_color, g_fg_color)) {
    LOG("Failed to read /webscreen.json => starting fallback mode.");
    useFallback = true;
    fallback_setup();
    return;
  }

  if (g_mqtt_enabled) {
    LOG("MQTT feature is enabled via config.");
  } else {
    LOG("MQTT feature is disabled.");
  }

  if (!scriptFile.startsWith("/")) {
    scriptFile = "/" + scriptFile;
  }
  g_script_filename = scriptFile;

  // Try to connect to WiFi if configured, but don't fail if it doesn't work
  if (s.length() > 0) {
    if (!webscreen_network_connect_wifi(s.c_str(), p.c_str(), 15000)) {
      LOG("Wi-Fi connection failed, but continuing to check for script...");
      // Don't return here - continue to check if script exists
    } else {
      LOG("Wi-Fi connected successfully.");
    }
  } else {
    LOG("No WiFi configured, running in offline mode.");
  }

  // Check if the script file exists
  String scriptPath = scriptFile;
  if (!scriptPath.startsWith("/")) {
    scriptPath = "/" + scriptPath;
  }

  File checkF = SD_MMC.open(g_script_filename);
  if (!checkF) {
    LOGF("No %s found => starting fallback mode.\n", g_script_filename.c_str());
    useFallback = true;
    fallback_setup();
    return;
  }
  checkF.close();

  // Script exists, run it (with or without WiFi)
  LOG("Script file found, starting JavaScript runtime...");
  useFallback = false;
  dynamic_js_setup();
}

void loop() {
  if (useFallback) {
    fallback_loop();
  } else {
    dynamic_js_loop();
  }
}