/**
 * @file webscreen_main.h
 * @brief Main application interface for WebScreen
 * 
 * This header defines the main application functions following Arduino
 * best practices for sketch organization.
 */

#pragma once

#include "webscreen_config.h"
#include <Arduino.h>

// Forward declarations for Arduino compatibility
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the WebScreen application
 * 
 * This function initializes all WebScreen subsystems including:
 * - Memory management
 * - Hardware abstraction layer
 * - Display system
 * - Storage (SD card)
 * - Network connectivity
 * - JavaScript runtime or fallback app
 * 
 * Called from Arduino setup() function.
 * 
 * @return true if initialization successful, false on critical failure
 */

bool webscreen_setup(void);

/**
 * @brief Main application loop
 * 
 * This function runs the main WebScreen application logic:
 * - Power button handling
 * - Runtime execution (JavaScript or fallback)
 * - Network maintenance
 * - System health monitoring
 * 
 * Called repeatedly from Arduino loop() function.
 */

void webscreen_loop(void);

/**
 * @brief Get current application state
 * 
 * @return String describing current state (for debugging)
 */
const char* webscreen_get_state(void);

/**
 * @brief Check if system is healthy
 * 
 * @return true if system is operating normally
 */

bool webscreen_is_healthy(void);

/**
 * @brief Request graceful shutdown
 * 
 * Initiates a controlled shutdown of all subsystems.
 * Useful for power management or error recovery.
 */

void webscreen_shutdown(void);

/**
 * @brief Load configuration from SD card JSON file
 * 
 * Reads and parses the webscreen.json configuration file from SD card.
 * Extracts WiFi credentials, MQTT settings, display colors, and script filename.
 * 
 * @param path Path to JSON config file (e.g., "/webscreen.json")
 * @param outSSID Reference to store WiFi SSID
 * @param outPASS Reference to store WiFi password
 * @param outScript Reference to store script filename
 * @param outMqttEnabled Reference to store MQTT enabled flag
 * @param outBgColor Reference to store background color
 * @param outFgColor Reference to store foreground color
 * @return true if config loaded successfully, false otherwise
 */

bool webscreen_load_config(const char* path, 
                          String &outSSID, 
                          String &outPASS, 
                          String &outScript, 
                          bool &outMqttEnabled,
                          uint32_t &outBgColor,
                          uint32_t &outFgColor);

#ifdef __cplusplus
}
#endif

// ============================================================================
// CONFIGURATION STRUCTURES (Arduino compatible)
// ============================================================================

/**
 * @brief Main WebScreen configuration structure
 * 
 * This structure holds all runtime configuration options.
 * Loaded from SD card JSON file at startup.
 */
typedef struct {    // WiFi Configuration
    struct {        char ssid[64];              ///< WiFi SSID
        char password[64];          ///< WiFi password
        bool enabled;               ///< WiFi enabled flag
        uint32_t connection_timeout; ///< Connection timeout in ms
        bool auto_reconnect;        ///< Auto-reconnect on disconnect
    } wifi;
    
    // MQTT Configuration
    struct {        char broker[128];           ///< MQTT broker URL
        uint16_t port;              ///< MQTT broker port
        char username[64];          ///< MQTT username
        char password[64];          ///< MQTT password
        char client_id[32];         ///< MQTT client ID
        bool enabled;               ///< MQTT enabled flag
        uint16_t keepalive;         ///< Keep-alive interval in seconds
    } mqtt;
    
    // Display Configuration
    struct {        uint8_t brightness;         ///< Display brightness (0-255)
        uint8_t rotation;           ///< Display rotation (0-3)
        uint32_t background_color;  ///< Background color (RGB)
        uint32_t foreground_color;  ///< Foreground color (RGB)
        bool auto_brightness;       ///< Auto-brightness enabled
        uint32_t screen_timeout;    ///< Screen timeout in ms (0 = never)
    } display;
    
    // System Configuration
    struct {        char device_name[32];       ///< Device name
        char timezone[32];          ///< Timezone string
        uint8_t log_level;          ///< Logging level (0-4)
        bool performance_mode;      ///< Performance mode enabled
        uint32_t watchdog_timeout;  ///< Watchdog timeout in ms
    } system;
    
    char script_file[128];          ///< JavaScript file to execute
    uint32_t config_version;        ///< Configuration version
    uint32_t last_modified;         ///< Last modification timestamp
} webscreen_config_t;

// Global configuration instance (defined in webscreen.ino)
extern webscreen_config_t g_webscreen_config;

// ============================================================================
// UTILITY MACROS (Arduino Style)
// ============================================================================

/**
 * @brief Utility macros following Arduino conventions
 */

// Memory allocation helpers
#define WEBSCREEN_MALLOC(size) malloc(size)
#define WEBSCREEN_FREE(ptr) do { if(ptr) { free(ptr); ptr = NULL; } } while(0)

// String helpers
#define WEBSCREEN_STR_EQUAL(s1, s2) (strcmp(s1, s2) == 0)
#define WEBSCREEN_STR_COPY(dst, src, max_len) strncpy(dst, src, max_len - 1); dst[max_len - 1] = '\0'

// Time helpers
#define WEBSCREEN_MILLIS() millis()
#define WEBSCREEN_DELAY(ms) delay(ms)

// Digital I/O helpers
#define WEBSCREEN_PIN_HIGH(pin) digitalWrite(pin, HIGH)
#define WEBSCREEN_PIN_LOW(pin) digitalWrite(pin, LOW)
#define WEBSCREEN_PIN_READ(pin) digitalRead(pin)
#define WEBSCREEN_PIN_MODE(pin, mode) pinMode(pin, mode)

// Debug helpers
#if WEBSCREEN_ENABLE_SERIAL_COMMANDS
  #define WEBSCREEN_DEBUG_PRINT(msg) Serial.print(msg)
  #define WEBSCREEN_DEBUG_PRINTLN(msg) Serial.println(msg)
  #define WEBSCREEN_DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define WEBSCREEN_DEBUG_PRINT(msg)
  #define WEBSCREEN_DEBUG_PRINTLN(msg)
  #define WEBSCREEN_DEBUG_PRINTF(fmt, ...)
#endif

// ============================================================================
// ARDUINO COMPATIBILITY HELPERS
// ============================================================================

/**
 * @brief Arduino IDE compatibility helpers
 */

// Include guards for library compatibility
#ifndef WEBSCREEN_LIBRARY_MODE
  // In sketch mode, include common Arduino libraries
  #include <WiFi.h>
  #include <SD_MMC.h>
  #include <ArduinoJson.h>
  #include <HTTPClient.h>
  #include <WiFiClientSecure.h>
  
  #if WEBSCREEN_ENABLE_MQTT
    #include <PubSubClient.h>
  #endif
  
  // BLE support temporarily disabled to avoid conflicts with NimBLE
  //#if WEBSCREEN_ENABLE_BLE
  //  #include <BLEDevice.h>
  //#endif
#endif

// Ensure required core includes
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>

// Temperature sensor
#ifdef __cplusplus
extern "C" {
#endif

float temperatureRead();
#ifdef __cplusplus
}
#endif

// ============================================================================
// DEPRECATED COMPATIBILITY (for migration from v1.x)
// ============================================================================

/**
 * @brief Deprecated functions for backward compatibility
 * These will be removed in future versions
 */
#ifdef WEBSCREEN_ENABLE_DEPRECATED_API
  #define LOG(msg) WEBSCREEN_DEBUG_PRINTLN(msg)
  #define LOGF(fmt, ...) WEBSCREEN_DEBUG_PRINTF(fmt, ##__VA_ARGS__)
  
  // Legacy global variables (now access via config)
  #define g_script_filename g_webscreen_config.script_file
  #define g_mqtt_enabled g_webscreen_config.mqtt.enabled
  #define g_bg_color g_webscreen_config.display.background_color
  #define g_fg_color g_webscreen_config.display.foreground_color
#endif