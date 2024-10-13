#include <Arduino.h>
#include "pins_config.h"   // Include your pin configuration file
#include <WiFi.h>          // Include the Wi-Fi library for ESP32
#include <ESPping.h>       // Include the ESPping library
#include <FS.h>
#include <SD_MMC.h>        // Include SD_MMC library for SD card access
#include <ArduinoJson.h>   // Include ArduinoJson library
#include <YAMLDuino.h>     // Include YAMLDuino library for YAML parsing
#include <time.h>          // Include time library for timestamp

// Define SD card pins
#define PIN_SD_CMD 13  // CMD
#define PIN_SD_CLK 11  // CLK
#define PIN_SD_D0  12  // Data0

// Wi-Fi credentials and config parameters (to be read from SD card)
String ssid = "";
String password = "";
String version = "";
String last_read = "";

// Ping settings
IPAddress pingAddress(8, 8, 8, 8);  // IP address to ping
int pingCount = 4;                  // Number of ping attempts
int pingInterval = 1000;            // Interval between pings in milliseconds

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

bool readWiFiConfig(fs::FS &fs, const char * path) {
  Serial.printf("Reading Wi-Fi config from: %s\n", path);

  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open Wi-Fi config file");
    return false;
  }

  // Read the entire file into a String
  String yamlContent = file.readString();
  file.close();

  // Parse YAML content into ArduinoJson document
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeYml(doc, yamlContent.c_str());

  if (error) {
    Serial.print("Failed to parse YAML file: ");
    Serial.println(error.c_str());
    return false;
  }

  // Extract values
  version = doc["version"] | "";
  last_read = doc["last_read"] | "";
  ssid = doc["settings"]["wifi"]["ssid"] | "";
  password = doc["settings"]["wifi"]["pass"] | "";

  // Print the configuration values
  Serial.print("Version: ");
  Serial.println(version);
  Serial.print("Last Read: ");
  Serial.println(last_read);
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("Password: ");
  Serial.println(password);

  if (ssid.length() == 0 || password.length() == 0) {
    Serial.println("SSID or password not found in config file");
    return false;
  }

  // Update last_read with current timestamp
  time_t now;
  time(&now);
  last_read = String((unsigned long)now);
  doc["last_read"] = last_read;

  // Serialize the updated document back to YAML
  String updatedYamlContent;
  size_t bytesWritten = serializeYml(doc.as<JsonVariant>(), updatedYamlContent);

  if (bytesWritten == 0) {
    Serial.println("Failed to serialize updated YAML content");
    return false;
  }

  // Write the updated YAML content back to the file
  file = fs.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  if (file.print(updatedYamlContent)) {
    Serial.println("Config file updated");
  } else {
    Serial.println("Failed to write to config file");
    file.close();
    return false;
  }
  file.close();

  return true;
}

void initTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for time synchronization");
  while (time(nullptr) < 100000) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" done!");
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for Serial Monitor to initialize

  // Initialize SD_MMC with custom pins
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);

  if (!SD_MMC.begin("/sdcard", true)) {  // Mount point "/sdcard", 1-bit mode
    Serial.println("Card Mount Failed");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD_MMC card attached");
    return;
  }

  Serial.print("SD_MMC Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);

  // List files in the root directory
  listDir(SD_MMC, "/", 0);

  // Read Wi-Fi credentials from SD card
  if (!readWiFiConfig(SD_MMC, "/webscreen.yml")) {
    Serial.println("Failed to read Wi-Fi configuration");
    return;
  }

  // Initialize LED pin
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);  // Turn off LED (assuming HIGH is off)

  // Initialize Wi-Fi connection
  Serial.println("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);  // Set Wi-Fi to station mode
  WiFi.begin(ssid.c_str(), password.c_str());

  // Blink LED while connecting
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(PIN_LED, LOW);   // Turn LED on
    delay(250);
    digitalWrite(PIN_LED, HIGH);  // Turn LED off
    delay(250);
    Serial.print(".");
  }

  // Wi-Fi connected
  Serial.println("\nWi-Fi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Display Wi-Fi signal strength
  Serial.print("Signal Strength (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  // Turn LED on solid to indicate successful connection
  digitalWrite(PIN_LED, LOW);  // Turn LED on

  // Initialize Ping
  Serial.println("Initializing ping test...");
}

void loop() {
  // Check Wi-Fi connection status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected!");
    digitalWrite(PIN_LED, HIGH);  // Turn LED off

    // Attempt to reconnect
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());

    // Blink LED while reconnecting
    while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(PIN_LED, LOW);   // Turn LED on
      delay(250);
      digitalWrite(PIN_LED, HIGH);  // Turn LED off
      delay(250);
      Serial.print(".");
    }

    Serial.println("\nReconnected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Display Wi-Fi signal strength
    Serial.print("Signal Strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    // Turn LED on solid
    digitalWrite(PIN_LED, LOW);  // Turn LED on
  }

  // Perform ping test
  Serial.println("Pinging 8.8.8.8...");

  int successfulPings = 0;
  int minTime = INT_MAX;
  int maxTime = 0;
  float totalTime = 0;

  for (int i = 0; i < pingCount; i++) {
    // Send one ping
    int result = Ping.ping(pingAddress, 1);
    if (result > 0) {
      float time = Ping.averageTime();
      successfulPings++;
      totalTime += time;
      if (time < minTime) minTime = time;
      if (time > maxTime) maxTime = time;
      Serial.printf("Reply from %s: time=%.2f ms\n", pingAddress.toString().c_str(), time);
    } else {
      Serial.println("Request timed out.");
    }
    delay(pingInterval);
  }

  // Display statistics
  Serial.println("Ping statistics for 8.8.8.8:");
  Serial.printf("    Packets: Sent = %d, Received = %d, Lost = %d (%d%% loss)\n",
                pingCount, successfulPings, pingCount - successfulPings,
                (100 * (pingCount - successfulPings) / pingCount));

  if (successfulPings > 0) {
    Serial.println("Approximate round trip times in milli-seconds:");
    Serial.printf("    Minimum = %d ms, Maximum = %d ms, Average = %.2f ms\n",
                  minTime, maxTime, totalTime / successfulPings);
  } else {
    Serial.println("No responses received.");
  }

  // Wait before next ping test
  delay(10000);  // Wait for 10 seconds before next test
}
