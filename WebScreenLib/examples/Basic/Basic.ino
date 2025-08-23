/**
 * @file Basic.ino
 * @brief Basic WebScreen example
 * 
 * This example demonstrates the simplest way to use the WebScreen library.
 * It connects to WiFi and runs a JavaScript application from the SD card.
 * 
 * Hardware Required:
 * - WebScreen device (ESP32-S3 with display)
 * - SD card with webscreen.json and app.js
 * 
 * Circuit:
 * - No additional wiring required for WebScreen hardware
 * 
 * Created by WebScreen Project
 * This example code is in the public domain.
 */

#include <WebScreen.h>

// Create WebScreen instance
WebScreen webscreen;

// WiFi credentials - change these to match your network
const char* ssid = "YourWiFiNetwork";
const char* password = "YourWiFiPassword";

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial && millis() < 5000) {
    delay(10); // Wait up to 5 seconds for serial
  }
  
  Serial.println("WebScreen Basic Example");
  Serial.println("======================");
  
  // Configure WiFi
  webscreen.setWiFi(ssid, password);
  
  // Optional: Set display preferences
  webscreen.setDisplay(200, 1, 0x000080, 0x00FFFF); // brightness, rotation, bg_color, fg_color
  
  // Optional: Set JavaScript file to run
  webscreen.setScript("/app.js");
  
  // Initialize WebScreen
  Serial.println("Initializing WebScreen...");
  if (!webscreen.begin()) {
    Serial.println("ERROR: WebScreen initialization failed!");
    Serial.println("Check SD card and configuration.");
    
    // Blink LED to indicate error
    pinMode(38, OUTPUT); // LED pin
    while (true) {
      digitalWrite(38, HIGH);
      delay(500);
      digitalWrite(38, LOW);
      delay(500);
    }
  }
  
  Serial.println("WebScreen initialized successfully!");
  Serial.println();
  
  // Print system information
  webscreen.printSystemInfo();
  
  Serial.println();
  Serial.println("System is running. Monitor for status updates...");
  Serial.println("Send 'status' to see current system status");
  Serial.println("Send 'test' to run system self-test");
}

void loop() {
  // Run WebScreen main loop - this handles everything automatically
  webscreen.loop();
  
  // Handle serial commands for debugging
  handleSerialCommands();
  
  // Optional: Add your custom code here
  // Note: Keep processing light to avoid interfering with WebScreen operation
  
  delay(10); // Small delay to prevent overwhelming the system
}

/**
 * @brief Handle simple serial commands for debugging
 */
void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "status") {
      printDetailedStatus();
    } else if (command == "test") {
      runSystemTest();
    } else if (command == "restart") {
      Serial.println("Restarting system...");
      webscreen.restart();
    } else if (command == "brightness") {
      // Cycle through brightness levels
      static uint8_t brightness = 100;
      brightness += 50;
      if (brightness > 255) brightness = 50;
      webscreen.setBrightness(brightness);
      Serial.printf("Brightness set to %d\n", brightness);
    } else if (command == "help") {
      Serial.println("Available commands:");
      Serial.println("  status     - Show detailed system status");
      Serial.println("  test       - Run system self-test");
      Serial.println("  restart    - Restart the system");
      Serial.println("  brightness - Cycle display brightness");
      Serial.println("  help       - Show this help");
    } else if (command.length() > 0) {
      Serial.printf("Unknown command: %s\n", command.c_str());
      Serial.println("Type 'help' for available commands.");
    }
  }
}

/**
 * @brief Print detailed system status
 */
void printDetailedStatus() {
  Serial.println("\n=== DETAILED SYSTEM STATUS ===");
  
  Serial.printf("WebScreen Library: v%s\n", getWebScreenVersion());
  Serial.printf("System Status: %s\n", webscreen.getStatus().c_str());
  Serial.printf("Running: %s\n", webscreen.isRunning() ? "Yes" : "No");
  Serial.printf("Mode: %s\n", webscreen.isJavaScriptMode() ? "JavaScript" : "Fallback");
  Serial.printf("Uptime: %lu ms (%.1f minutes)\n", 
                webscreen.getUptime(), webscreen.getUptime() / 60000.0);
  Serial.printf("Free Memory: %lu bytes (%.1f KB)\n", 
                webscreen.getFreeMemory(), webscreen.getFreeMemory() / 1024.0);
  
  Serial.println("\n--- Network Status ---");
  Serial.printf("WiFi Connected: %s\n", webscreen.isWiFiConnected() ? "Yes" : "No");
  if (webscreen.isWiFiConnected()) {
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
  }
  Serial.printf("MQTT Connected: %s\n", webscreen.isMQTTConnected() ? "Yes" : "No");
  
  Serial.println("\n--- Display Status ---");
  Serial.printf("Display On: %s\n", webscreen.isDisplayOn() ? "Yes" : "No");
  Serial.printf("Brightness: %d/255\n", webscreen.getBrightness());
  
  Serial.println("\n--- Hardware Status ---");
  Serial.printf("Chip: %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash Size: %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("PSRAM Size: %d MB\n", ESP.getPsramSize() / (1024 * 1024));
  
  Serial.println("==============================\n");
}

/**
 * @brief Run comprehensive system test
 */
void runSystemTest() {
  Serial.println("\n=== SYSTEM SELF-TEST ===");
  
  Serial.print("Running hardware self-test... ");
  bool test_result = webscreen.selfTest();
  Serial.println(test_result ? "PASS" : "FAIL");
  
  if (test_result) {
    Serial.println("All systems operational!");
  } else {
    Serial.println("Some systems may have issues. Check hardware connections.");
  }
  
  Serial.println("========================\n");
}

/**
 * @brief This function is called when MQTT message is received
 * You can customize this to handle incoming MQTT messages
 */
void onMqttMessage(const char* topic, const char* payload) {
  Serial.printf("MQTT Message: [%s] %s\n", topic, payload);
  
  // Example: Handle simple commands via MQTT
  if (strcmp(topic, "webscreen/command") == 0) {
    if (strcmp(payload, "brightness_up") == 0) {
      uint8_t current = webscreen.getBrightness();
      webscreen.setBrightness(min(255, current + 25));
    } else if (strcmp(payload, "brightness_down") == 0) {
      uint8_t current = webscreen.getBrightness();
      webscreen.setBrightness(max(25, current - 25));
    } else if (strcmp(payload, "display_off") == 0) {
      webscreen.setDisplayPower(false);
    } else if (strcmp(payload, "display_on") == 0) {
      webscreen.setDisplayPower(true);
    }
  }
}