#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>

#include "pins_config.h"
#include "fallback.h"
#include "dynamic_js.h"
#include "globals.h"

#include <ArduinoJson.h>

// Define and initialize the new global flag
bool g_mqtt_enabled = false;

// Global flag to decide fallback vs dynamic
static bool useFallback = false;

// Define global color variables with default values
uint32_t g_bg_color = 0x000000; // Default: black
uint32_t g_fg_color = 0xFFFFFF; // Default: white

bool initialize_sd_card() {
    LOG("Initializing SD Card...");
    SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);

    // Attempt to mount with retries and different speeds
    for (int i = 0; i < 3; i++) {
        // First, try with a slower, more compatible frequency for initialization
        LOGF("Attempt %d: Mounting SD card at a safe, low frequency...\n", i + 1);
        if (SD_MMC.begin("/sdcard", true, false, 400000)) { // 400 kHz is very safe
            LOG("SD Card mounted successfully at low frequency.");
            
            // Now, try to re-initialize at a higher speed for performance
            SD_MMC.end(); // Unmount first
            LOG("Re-mounting SD card at high frequency...");
            if (SD_MMC.begin("/sdcard", true, false, 10000000)) { // 10 MHz is a good target
                LOG("SD Card re-mounted successfully at high frequency.");
                return true;
            } else {
                LOG("Failed to re-mount at high frequency. Falling back to low speed mount.");
                // If high speed fails, mount again at low speed and continue
                if(SD_MMC.begin("/sdcard", true, false, 400000)) {
                    LOG("Continuing at safe, low frequency.");
                    return true;
                }
            }
        }
        
        LOGF("Attempt %d failed. Retrying in 200ms...\n", i + 1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    LOG("All attempts to mount SD card failed.");
    return false;
}

static bool readConfigJSON(
    const char* path, 
    String &outSSID, 
    String &outPASS, 
    String &outScript, 
    bool &outMqttEnabled,
    uint32_t &outBgColor,
    uint32_t &outFgColor
) {
  File f = SD_MMC.open(path);
  if (!f) {
    LOG("No JSON file");
    return false;
  }
  String jsonStr = f.readString();
  f.close();

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) {
    LOGF("Failed to parse JSON: %s\n", error.c_str());
    return false;
  }

  outSSID = doc["settings"]["wifi"]["ssid"] | "";
  outPASS = doc["settings"]["wifi"]["pass"] | "";
  outMqttEnabled = doc["settings"]["mqtt"]["enabled"] | false;
  outScript = doc["script"] | "app.js";

  const char* bgColorStr = doc["screen"]["background"] | "#000000";
  const char* fgColorStr = doc["screen"]["foreground"] | "#FFFFFF";

  outBgColor = strtol(bgColorStr + 1, NULL, 16);
  outFgColor = strtol(fgColorStr + 1, NULL, 16);

  return true;
}

void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(2000));

  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, HIGH);

  // Use the new robust initialization function
  if (!initialize_sd_card()) {
    LOG("SD card mount fail => starting fallback mode.");
    useFallback = true;
    fallback_setup();
    return;
  }

  String s, p, scriptFile;
  if(!readConfigJSON("/webscreen.json", s, p, scriptFile, g_mqtt_enabled, g_bg_color, g_fg_color)) {
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

  WiFi.mode(WIFI_STA);
  WiFi.begin(s.c_str(), p.c_str());
  unsigned long startMs = millis();
  while(WiFi.status()!=WL_CONNECTED && (millis()-startMs)<15000) {
    vTaskDelay(pdMS_TO_TICKS(250));
    Serial.print(".");
  }
  LOG();
  if(WiFi.status()!=WL_CONNECTED) {
    LOG("Wi-Fi fail => starting fallback mode.");
    useFallback = true;
    fallback_setup();
    return;
  }
  LOG("Wi-Fi connected => " + WiFi.localIP().toString());

  String scriptPath = scriptFile;
  if (!scriptPath.startsWith("/")) {
    scriptPath = "/" + scriptPath;
  }

  File checkF = SD_MMC.open(g_script_filename);
  if(!checkF) {
    LOGF("No %s found => starting fallback mode.\n", g_script_filename.c_str());
    useFallback = true;
    fallback_setup();
    return;
  }
  checkF.close();

  useFallback = false;
  dynamic_js_setup();
}

void loop() {
  if(useFallback) {
    fallback_loop();
  } else {
    dynamic_js_loop();
  }
}