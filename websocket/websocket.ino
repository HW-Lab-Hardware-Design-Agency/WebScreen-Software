#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>

#include "pins_config.h"     // For PIN_SD_CMD, etc.
#include "fallback.h"        // Fallback header
#include "dynamic_js.h"      // Dynamic (Elk + JS) header

// Optionally, if you want to read webscreen.yml for Wi-Fi info
#include <ArduinoJson.h>
#include <YAMLDuino.h>
#include <time.h>

#include <esp_heap_caps.h>
#include <esp_system.h>
#include "esp_task_wdt.h"

// Global flag to decide fallback vs dynamic
static bool useFallback = false;

// Latching pin definitions
#define LATCH_INPUT_PIN 33
#define LATCH_OUTPUT_PIN 1

// Power control variables
static unsigned long buttonPressStart = 0;
static const unsigned long holdTime = 1000; // 1-second hold for power-off
static const unsigned long debounceDelay = 50; // 50ms debounce time
static unsigned long lastButtonCheck = 0;
static bool buttonState = HIGH; // Assume unpressed (HIGH due to INPUT_PULLUP)

// Example function to read /webscreen.yml (like your prior code):
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

  // Write updated
  String updated;
  if(serializeYml(doc.as<JsonVariant>(), updated)==0) {
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

// Function to enter deep sleep
void enterDeepSleep() {
  Serial.println("Entering deep sleep...");
  // Configure the latch button as the wake-up source
  esp_sleep_enable_ext0_wakeup((gpio_num_t)LATCH_INPUT_PIN, LOW);
  // Set latch output low (optional visual indication of power off)
  digitalWrite(LATCH_OUTPUT_PIN, LOW);
  // Enter deep sleep
  esp_deep_sleep_start();
}

// Function to initialize power state
void initializePowerState() {
  pinMode(LATCH_INPUT_PIN, INPUT_PULLUP); // Button with pull-up
  pinMode(LATCH_OUTPUT_PIN, OUTPUT);     // Latching output
  digitalWrite(LATCH_OUTPUT_PIN, HIGH);  // Start with power on
}

// Function to handle button press logic
void handlePowerButton() {
  unsigned long currentMillis = millis();

  // Debounce check
  if ((currentMillis - lastButtonCheck) < debounceDelay) {
    return;
  }
  lastButtonCheck = currentMillis;

  // Read button state
  bool currentButtonState = digitalRead(LATCH_INPUT_PIN);

  if (currentButtonState == LOW) { // Button is pressed
    if (buttonPressStart == 0) {
      buttonPressStart = currentMillis; // Start timing
    } else if ((currentMillis - buttonPressStart) >= holdTime) {
      Serial.println("Button held long enough. Powering off...");
      enterDeepSleep();
    }
  } else {
    buttonPressStart = 0; // Reset timing if button is released
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Print last reset reason for debugging
  esp_reset_reason_t reset_reason = esp_reset_reason();
  Serial.printf("Last reset reason: %d\n", reset_reason);

  // Disable watchdog for debugging
  esp_task_wdt_delete(NULL);

  // SD card initialization
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if (!SD_MMC.begin("/sdcard", true, false, 1000000)) {
    Serial.println("SD card mount fail => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }

  // Wi-Fi configuration and setup
  String s, p;
  if (!readWiFiConfigYAML("/webscreen.yml", s, p)) {
    Serial.println("Failed to read /webscreen.yml => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(s.c_str(), p.c_str());
  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 15000) {
    delay(250);
    Serial.print(".");
    esp_task_wdt_reset(); // Feed watchdog during connection attempts
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi fail => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }
  Serial.println("Wi-Fi connected => " + WiFi.localIP().toString());

  // Check for /script.js
  File checkF = SD_MMC.open("/script.js");
  if (!checkF) {
    Serial.println("No script.js => fallback");
    useFallback = true;
    fallback_setup();
    return;
  }
  checkF.close();

  // Dynamic JS setup
  useFallback = false;
  dynamic_js_setup();
}


void loop() {
  // Handle power button logic
  handlePowerButton();

  // Run appropriate logic
  if(useFallback) {
    fallback_loop();
  } else {
    dynamic_js_loop();
  }
}
