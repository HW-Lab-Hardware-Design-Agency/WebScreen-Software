#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>

#include "pins_config.h"     // For PIN_SD_CMD, etc.
#include "fallback.h"        // Fallback header
#include "dynamic_js.h"      // Dynamic (Elk + JS) header

#include <ArduinoJson.h>

// Global flag to decide fallback vs dynamic
static bool useFallback = false;

static bool readWiFiConfigJSON(const char* path, String &outSSID, String &outPASS) {
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

  // Extract SSID/PASS from doc
  outSSID = doc["settings"]["wifi"]["ssid"] | "";
  outPASS = doc["settings"]["wifi"]["pass"] | "";
  if (outSSID.isEmpty() || outPASS.isEmpty()) {
    Serial.println("SSID or PASS empty in JSON");
    return false;
  }

  // Optionally update 'last_read' with current time
  time_t now;
  time(&now);
  doc["last_read"] = (unsigned long)now;

  // Re-serialize and write back to the same file (if you want to save changes)
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
  String s, p;
  if(!readWiFiConfigJSON("/webscreen.json", s, p)) {
    Serial.println("Failed to read /webscreen.json => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }

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

  // Check if /script.js exists
  {
    File checkF = SD_MMC.open("/script.js");
    if(!checkF) {
      Serial.println("No script.js => fallback");
      useFallback = true;
      fallback_setup();
      return;
    }
    checkF.close();
  }

  // We have script.js => run dynamic
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