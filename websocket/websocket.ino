#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>

#include "pins_config.h"     // For PIN_SD_CMD, etc.
#include "fallback.h"        // Fallback header
#include "dynamic_js.h"      // Dynamic (Elk + JS) header

#include <ArduinoJson.h>     // For reading YAML -> JSON
#include <YAMLDuino.h>       // For deserializeYml()
#include <time.h>            // For time() if we want to update last_read in the webscreen.yml file

static bool latchState = false;
static unsigned long buttonPressStart = 0;
static const unsigned long holdTime = 1000; // 1 second hold required

// Flag to decide fallback vs. dynamic
static bool useFallback = false;

// Example function to read /webscreen.yml
static bool readWiFiConfigYAML(const char* path, String &outSSID, String &outPASS) {
  File f = SD_MMC.open(path);
  if(!f) {
    Serial.println("No YAML file");
    return false;
  }
  String yamlStr = f.readString();
  f.close();

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeYml(doc, yamlStr.c_str());
  if(error) {
    Serial.printf("Failed to parse YAML: %s\n", error.c_str());
    return false;
  }

  outSSID = doc["settings"]["wifi"]["ssid"] | "";
  outPASS = doc["settings"]["wifi"]["pass"] | "";
  if(outSSID.isEmpty() || outPASS.isEmpty()) {
    Serial.println("SSID or PASS empty in YAML");
    return false;
  }

  // Optionally update 'last_read'
  time_t now;
  time(&now);
  doc["last_read"] = String((unsigned long)now);

  // Write updated YAML
  String updated;
  if(serializeYml(doc.as<JsonVariant>(), updated) == 0) {
    Serial.println("Failed to serialize updated YAML");
    return false;
  }
  f = SD_MMC.open(path, FILE_WRITE);
  if(!f) {
    Serial.println("Failed to open YAML for writing");
    return false;
  }
  f.print(updated);
  f.close();

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // ---------------------
  // 1) Latching pin setup
  // ---------------------
  pinMode(LATCH_INPUT_PIN, INPUT_PULLUP);
  pinMode(LATCH_OUTPUT_PIN, OUTPUT);
  digitalWrite(LATCH_OUTPUT_PIN, LOW); // Start latched off

  // ---------------------
  // 2) SD + Wi-Fi + fallback/dynamic logic
  // ---------------------
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if(!SD_MMC.begin("/sdcard", true, false, 1000000)) {
    Serial.println("SD card mount fail => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }

  // Optionally read /webscreen.yml for Wi-Fi
  String s, p;
  if(!readWiFiConfigYAML("/webscreen.yml", s, p)) {
    Serial.println("Failed to read /webscreen.yml => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }

  // Connect Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(s.c_str(), p.c_str());
  unsigned long startMs = millis();
  while(WiFi.status() != WL_CONNECTED && (millis() - startMs) < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if(WiFi.status() != WL_CONNECTED) {
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

  // We have /script.js => run dynamic
  useFallback = false;
  dynamic_js_setup();
}

void loop() {
  // ------------------------------------------------------
  // 3) Latching logic: hold button for 1s => toggle OUTPUT
  // ------------------------------------------------------
  bool buttonState = digitalRead(LATCH_INPUT_PIN);
  if (buttonState == LOW) { // Button pressed
    if (buttonPressStart == 0) {
      buttonPressStart = millis();
    } else if (millis() - buttonPressStart >= holdTime) {
      latchState = !latchState;
      digitalWrite(LATCH_OUTPUT_PIN, latchState ? HIGH : LOW);
      buttonPressStart = 0; // Reset so next hold can occur
    }
  } else {
    // Button not pressed; reset timer
    buttonPressStart = 0;
  }

  // ----------------------------
  // 4) Fallback or Dynamic logic
  // ----------------------------
  if(useFallback) {
    fallback_loop();
  } else {
    dynamic_js_loop();
  }
}
