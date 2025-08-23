/**
 * @file WebScreen.h
 * @brief Main include file for WebScreen Arduino Library
 * 
 * This is the primary header file for the WebScreen library. Include this
 * file in your Arduino sketch to access all WebScreen functionality.
 * 
 * @author WebScreen Project
 * @version 2.0.0
 * @date 2024
 */

#pragma once

// Arduino framework compatibility check
#ifndef ARDUINO
#error "WebScreen library requires Arduino framework"
#endif

// ESP32 platform check
#if !defined(ESP32)
#error "WebScreen library requires ESP32 platform"
#endif

// Library version information
#define WEBSCREEN_LIBRARY_VERSION "2.0.0"
#define WEBSCREEN_LIBRARY_VERSION_MAJOR 2
#define WEBSCREEN_LIBRARY_VERSION_MINOR 0
#define WEBSCREEN_LIBRARY_VERSION_PATCH 0

// Mark as library mode to prevent certain sketch-specific includes
#define WEBSCREEN_LIBRARY_MODE 1

// Core Arduino includes
#include <Arduino.h>
#include <WiFi.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>

// Core WebScreen components
#include "WebScreen_Config.h"
#include "WebScreen_Hardware.h"
#include "WebScreen_Display.h"
#include "WebScreen_Network.h"
#include "WebScreen_Runtime.h"
#include "WebScreen_Utils.h"

/**
 * @brief Main WebScreen class
 * 
 * This class provides the primary interface for WebScreen functionality.
 * It follows Arduino library conventions with begin(), loop(), and end() methods.
 */
class WebScreen {
public:
    /**
     * @brief Constructor
     */
    WebScreen();
    
    /**
     * @brief Destructor
     */
    ~WebScreen();
    
    /**
     * @brief Initialize WebScreen
     * 
     * @param config_file Path to configuration file on SD card (default: "/webscreen.json")
     * @return true if initialization successful, false otherwise
     */
    bool begin(const char* config_file = "/webscreen.json");
    
    /**
     * @brief Main processing loop
     * 
     * Call this function repeatedly from your Arduino loop() function.
     */
    void loop();
    
    /**
     * @brief Shutdown WebScreen
     * 
     * Cleanly shuts down all subsystems and frees resources.
     */
    void end();
    
    // ========================================================================
    // CONFIGURATION
    // ========================================================================
    
    /**
     * @brief Set WiFi credentials
     * @param ssid WiFi network name
     * @param password WiFi password
     */
    void setWiFi(const char* ssid, const char* password);
    
    /**
     * @brief Set MQTT configuration
     * @param broker MQTT broker hostname
     * @param port MQTT broker port
     * @param username MQTT username (optional)
     * @param password MQTT password (optional)
     */
    void setMQTT(const char* broker, uint16_t port = 1883, 
                 const char* username = nullptr, const char* password = nullptr);
    
    /**
     * @brief Set JavaScript file to execute
     * @param script_file Path to JavaScript file on SD card
     */
    void setScript(const char* script_file);
    
    /**
     * @brief Set display configuration
     * @param brightness Display brightness (0-255)
     * @param rotation Display rotation (0-3)
     * @param bg_color Background color (0xRRGGBB)
     * @param fg_color Foreground color (0xRRGGBB)
     */
    void setDisplay(uint8_t brightness = 200, uint8_t rotation = 1,
                    uint32_t bg_color = 0x000000, uint32_t fg_color = 0xFFFFFF);
    
    // ========================================================================
    // STATUS AND MONITORING
    // ========================================================================
    
    /**
     * @brief Check if WebScreen is running
     * @return true if system is active, false otherwise
     */
    bool isRunning() const;
    
    /**
     * @brief Check if JavaScript runtime is active
     * @return true if JavaScript is running, false if in fallback mode
     */
    bool isJavaScriptMode() const;
    
    /**
     * @brief Check if WiFi is connected
     * @return true if connected, false otherwise
     */
    bool isWiFiConnected() const;
    
    /**
     * @brief Check if MQTT is connected
     * @return true if connected, false otherwise
     */
    bool isMQTTConnected() const;
    
    /**
     * @brief Get system status string
     * @return String describing current system status
     */
    String getStatus() const;
    
    /**
     * @brief Get uptime in milliseconds
     * @return System uptime in milliseconds
     */
    uint32_t getUptime() const;
    
    /**
     * @brief Get free memory
     * @return Free heap memory in bytes
     */
    uint32_t getFreeMemory() const;
    
    // ========================================================================
    // NETWORK FUNCTIONS
    // ========================================================================
    
    /**
     * @brief Perform HTTP GET request
     * @param url Target URL
     * @return Response string (empty on error)
     */
    String httpGet(const char* url);
    
    /**
     * @brief Perform HTTP POST request
     * @param url Target URL
     * @param data POST data
     * @param content_type Content type (default: "application/json")
     * @return Response string (empty on error)
     */
    String httpPost(const char* url, const char* data, const char* content_type = "application/json");
    
    /**
     * @brief Publish MQTT message
     * @param topic MQTT topic
     * @param payload Message payload
     * @param retain Retain flag (default: false)
     * @return true if published successfully, false otherwise
     */
    bool mqttPublish(const char* topic, const char* payload, bool retain = false);
    
    /**
     * @brief Subscribe to MQTT topic
     * @param topic MQTT topic
     * @param callback Function to call when message received
     * @return true if subscribed successfully, false otherwise
     */
    bool mqttSubscribe(const char* topic, void (*callback)(const char* topic, const char* payload));
    
    // ========================================================================
    // DISPLAY FUNCTIONS
    // ========================================================================
    
    /**
     * @brief Set display brightness
     * @param brightness Brightness level (0-255)
     */
    void setBrightness(uint8_t brightness);
    
    /**
     * @brief Get current brightness
     * @return Current brightness level (0-255)
     */
    uint8_t getBrightness() const;
    
    /**
     * @brief Turn display on or off
     * @param on true to turn on, false to turn off
     */
    void setDisplayPower(bool on);
    
    /**
     * @brief Check if display is on
     * @return true if display is on, false otherwise
     */
    bool isDisplayOn() const;
    
    /**
     * @brief Set fallback display text
     * @param text Text to display when in fallback mode
     */
    void setFallbackText(const char* text);
    
    // ========================================================================
    // UTILITIES
    // ========================================================================
    
    /**
     * @brief Print system information to Serial
     */
    void printSystemInfo() const;
    
    /**
     * @brief Run system self-test
     * @return true if all tests pass, false otherwise
     */
    bool selfTest();
    
    /**
     * @brief Save current configuration to SD card
     * @return true if saved successfully, false otherwise
     */
    bool saveConfig();
    
    /**
     * @brief Load configuration from SD card
     * @param config_file Path to configuration file
     * @return true if loaded successfully, false otherwise
     */
    bool loadConfig(const char* config_file = "/webscreen.json");
    
    /**
     * @brief Restart the system
     */
    void restart();

private:
    bool m_initialized;
    bool m_running;
    bool m_javascript_mode;
    uint32_t m_start_time;
    String m_status;
    
    // Internal helper functions
    bool initializeHardware();
    bool initializeStorage();
    bool initializeNetwork();
    bool startRuntime();
    void updateStatus();
};

// ============================================================================
// GLOBAL CONVENIENCE FUNCTIONS
// ============================================================================

/**
 * @brief Get WebScreen library version
 * @return Version string (e.g., "2.0.0")
 */
const char* getWebScreenVersion();

/**
 * @brief Check if WebScreen hardware is detected
 * @return true if WebScreen hardware detected, false otherwise
 */
bool isWebScreenHardware();

/**
 * @brief Quick setup function for basic WebScreen operation
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @param script_file JavaScript file to run (optional)
 * @return true if setup successful, false otherwise
 */
bool webScreenQuickSetup(const char* ssid, const char* password, const char* script_file = "/app.js");

// ============================================================================
// ARDUINO LIBRARY UTILITIES
// ============================================================================

// Ensure proper linking with C++ name mangling
#ifdef __cplusplus
extern "C" {
#endif

// C-style functions for compatibility with existing code
bool webscreen_arduino_init(void);
void webscreen_arduino_loop(void);
void webscreen_arduino_shutdown(void);

#ifdef __cplusplus
}
#endif

// ============================================================================
// EXAMPLES AND DOCUMENTATION LINKS
// ============================================================================

/*
Example Usage:

#include <WebScreen.h>

WebScreen webscreen;

void setup() {
  Serial.begin(115200);
  
  // Basic setup
  webscreen.setWiFi("MyNetwork", "MyPassword");
  webscreen.setScript("/apps/weather.js");
  
  if (!webscreen.begin()) {
    Serial.println("WebScreen initialization failed!");
    return;
  }
  
  Serial.println("WebScreen ready!");
}

void loop() {
  webscreen.loop();
  
  // Your custom code here
  delay(10);
}

For more examples, see:
- examples/Basic/
- examples/WiFiDemo/
- examples/MQTTDemo/
- examples/CustomDisplay/

Documentation:
- https://github.com/HW-Lab-Hardware-Design-Agency/WebScreen-Software
- https://webscreen.cc/docs/
*/