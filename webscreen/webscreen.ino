#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>           // Needed for File type
#include <SD_MMC.h>       // Needed for SD_MMC object/methods
#include <ArduinoJson.h>  // Needed for JSON parsing

#include "pins_config.h" // Needed for SD pins
#include "globals.h"     // For LOG, g_script_filename
#include "fallback.h"    // For fallback mode functions
#include "dynamic_js.h"  // For dynamic mode functions

// Global flag to decide fallback vs dynamic
static bool useFallback = false;

static bool readConfigJSON(const char* path, String &outSSID, String &outPASS, String &outScript) {
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

  // Extract Wi-Fi settings
  outSSID = doc["settings"]["wifi"]["ssid"] | "";
  outPASS = doc["settings"]["wifi"]["pass"] | "";
  if (outSSID.isEmpty() || outPASS.isEmpty()) {
    LOG("SSID or PASS empty in JSON");
    return false;
  }

  // Extract the script filename (default to "app.js" if not provided)
  outScript = doc["script"] | "app.js";

  // Update 'last_read' if desired
  time_t now;
  time(&now);
  doc["last_read"] = (unsigned long)now;

  // Optionally write back the updated JSON
  String updated;
  serializeJson(doc, updated);
  f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    LOG("Failed to open JSON for writing");
    return false;
  }
  f.print(updated);
  f.close();

  return true;
}

void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(2000));

  // force the display power/backlight pin always ON:
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, HIGH);

  // Attempt to mount SD
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if(!SD_MMC.begin("/sdcard", true, false, 1000000)) {
    LOG("SD card mount fail => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }

  // Optionally read /webscreen.json for Wi-Fi
  String s, p, scriptFile;
  if(!readConfigJSON("/webscreen.json", s, p, scriptFile)) {
    LOG("Failed to read /webscreen.json => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }

  // Ensure the script filename starts with a '/'
  if (!scriptFile.startsWith("/")) {
    scriptFile = "/" + scriptFile;
  }
  g_script_filename = scriptFile;  // update global variable

  // Connect Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(s.c_str(), p.c_str());
  unsigned long startMs = millis();
  while(WiFi.status()!=WL_CONNECTED && (millis()-startMs)<15000) {
    vTaskDelay(pdMS_TO_TICKS(250));
    Serial.print(".");
  }
  LOG();
  if(WiFi.status()!=WL_CONNECTED) {
    LOG("Wi-Fi fail => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }
  LOG("Wi-Fi connected => " + WiFi.localIP().toString());

  // Use the filename specified in the config; ensure it has a leading '/'
  String scriptPath = scriptFile;
  if (!scriptPath.startsWith("/")) {
    scriptPath = "/" + scriptPath;
  }
  // Check if the script file exists on the SD card:
  File checkF = SD_MMC.open(g_script_filename);
  if(!checkF) {
    LOGF("No %s found => fallback\n", g_script_filename.c_str());
    useFallback = true;
    fallback_setup();
    return;
  }
  checkF.close();

  // If everything is okay, run the dynamic JS functionality:
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