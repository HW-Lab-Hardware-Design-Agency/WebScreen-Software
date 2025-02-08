#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>

#include "pins_config.h"     // For PIN_SD_CMD, etc.
#include "fallback.h"        // Fallback header
#include "dynamic_js.h"      // Dynamic (Elk + JS) header
#include "globals.h"

#include <ArduinoJson.h>

// Global flag to decide fallback vs dynamic
static bool useFallback = false;

static bool readConfigJSON(const char* path, String &outSSID, String &outPASS, String &outScript) {
  File f = SD_MMC.open(path);
  if (!f) {
    Serial.println("No JSON file");
    return false;
  }
  String jsonStr = f.readString();
  f.close();

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) {
    Serial.printf("Failed to parse JSON: %s\n", error.c_str());
    return false;
  }

  // Extract Wi-Fi settings
  outSSID = doc["settings"]["wifi"]["ssid"] | "";
  outPASS = doc["settings"]["wifi"]["pass"] | "";
  if (outSSID.isEmpty() || outPASS.isEmpty()) {
    Serial.println("SSID or PASS empty in JSON");
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
    Serial.println("Failed to open JSON for writing");
    return false;
  }
  f.print(updated);
  f.close();

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Attempt to mount SD
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if(!SD_MMC.begin("/sdcard", true, false, 1000000)) {
    Serial.println("SD card mount fail => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }

  // Optionally read /webscreen.json for Wi-Fi
  String s, p, scriptFile;
  if(!readConfigJSON("/webscreen.json", s, p, scriptFile)) {
    Serial.println("Failed to read /webscreen.json => fallback");
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
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if(WiFi.status()!=WL_CONNECTED) {
    Serial.println("Wi-Fi fail => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }
  Serial.println("Wi-Fi connected => " + WiFi.localIP().toString());

  // Use the filename specified in the config; ensure it has a leading '/'
  String scriptPath = scriptFile;
  if (!scriptPath.startsWith("/")) {
    scriptPath = "/" + scriptPath;
  }
  // Check if the script file exists on the SD card:
  File checkF = SD_MMC.open(g_script_filename);
  if(!checkF) {
    Serial.printf("No %s found => fallback\n", g_script_filename.c_str());
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