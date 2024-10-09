#include <Arduino.h>
#include "pins_config.h"  // Include the pin configuration file
#include <WiFi.h>         // Include the Wi-Fi library for ESP32
#include <ESPping.h>      // Include the ESPping library

// Replace with your network credentials
const char* ssid = "Personal-0F8-2.4GHz";
const char* password = "A756F350F8";

// Ping settings
IPAddress pingAddress(8, 8, 8, 8);  // IP address to ping
int pingCount = 4;                  // Number of ping attempts
int pingInterval = 1000;            // Interval between pings in milliseconds

void setup() {
  Serial.begin(115200);

  // Initialize LED pin
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);  // Turn off LED (assuming HIGH is off)

  // Initialize Wi-Fi connection
  Serial.println("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);  // Set Wi-Fi to station mode
  WiFi.begin(ssid, password);

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
    WiFi.begin(ssid, password);

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
