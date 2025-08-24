/**
 * @file webscreen_config.h
 * @brief Configuration constants and compile-time settings for WebScreen
 * 
 * This file contains all compile-time configuration options for WebScreen.
 * Following Arduino best practices, this is included in the main sketch directory.
 */

#pragma once

#include <Arduino.h>

// ============================================================================
// FIRMWARE VERSION
// ============================================================================
#define WEBSCREEN_VERSION_MAJOR 2
#define WEBSCREEN_VERSION_MINOR 0
#define WEBSCREEN_VERSION_PATCH 0
#define WEBSCREEN_VERSION_STRING "2.0.0"

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================

// Display Configuration
#define WEBSCREEN_DISPLAY_WIDTH 536
#define WEBSCREEN_DISPLAY_HEIGHT 240
#define WEBSCREEN_DISPLAY_ROTATION 1  // 90 degrees

// Pin Definitions (ESP32-S3 WebScreen Hardware)
#define WEBSCREEN_PIN_LED 38
#define WEBSCREEN_PIN_BUTTON 33
#define WEBSCREEN_PIN_OUTPUT 1

// Display Pins (RM67162 AMOLED)
#define WEBSCREEN_TFT_CS 6
#define WEBSCREEN_TFT_DC 7
#define WEBSCREEN_TFT_RST 17
#define WEBSCREEN_TFT_SCK 47
#define WEBSCREEN_TFT_MOSI 18
#define WEBSCREEN_TFT_D1 7
#define WEBSCREEN_TFT_D2 48
#define WEBSCREEN_TFT_D3 5

// SD Card Pins
#define WEBSCREEN_SD_CMD 13
#define WEBSCREEN_SD_CLK 11
#define WEBSCREEN_SD_D0 12

// ============================================================================
// MEMORY CONFIGURATION
// ============================================================================

// Buffer Sizes
#define WEBSCREEN_DISPLAY_BUFFER_SIZE (WEBSCREEN_DISPLAY_WIDTH * WEBSCREEN_DISPLAY_HEIGHT)
#define WEBSCREEN_CONFIG_BUFFER_SIZE 2048
#define WEBSCREEN_LOG_BUFFER_SIZE 512
#define WEBSCREEN_SERIAL_BUFFER_SIZE 256

// Memory Allocation Thresholds
#define WEBSCREEN_PSRAM_THRESHOLD_KB 32          // Use PSRAM for allocations > 32KB
#define WEBSCREEN_MEMORY_WARNING_THRESHOLD 0.85  // Warn when 85% memory used

// JavaScript Engine
#define WEBSCREEN_JS_HEAP_SIZE_KB 512           // JavaScript heap size (KB)
#define WEBSCREEN_JS_MAX_EXECUTION_TIME_MS 100  // Max script execution time

// ============================================================================
// NETWORK CONFIGURATION
// ============================================================================

// WiFi Settings
#define WEBSCREEN_WIFI_CONNECTION_TIMEOUT_MS 15000
#define WEBSCREEN_WIFI_RETRY_ATTEMPTS 3
#define WEBSCREEN_WIFI_RETRY_DELAY_MS 1000

// HTTP Client
#define WEBSCREEN_HTTP_TIMEOUT_MS 10000
#define WEBSCREEN_HTTP_MAX_RESPONSE_SIZE 32768

// MQTT Client
#define WEBSCREEN_MQTT_KEEPALIVE_SEC 60
#define WEBSCREEN_MQTT_MAX_PACKET_SIZE 1024

// ============================================================================
// SYSTEM CONFIGURATION
// ============================================================================

// Timing
#define WEBSCREEN_LOOP_DELAY_MS 1
#define WEBSCREEN_BUTTON_DEBOUNCE_MS 50
#define WEBSCREEN_STATS_REPORT_INTERVAL_MS 300000  // 5 minutes

// Watchdog
#define WEBSCREEN_WATCHDOG_TIMEOUT_SEC 30

// File System
#define WEBSCREEN_CONFIG_FILENAME "/webscreen.json"
#define WEBSCREEN_LOG_FILENAME "/webscreen.log"
#define WEBSCREEN_MAX_LOG_FILE_SIZE_KB 512

// ============================================================================
// FEATURE ENABLES
// ============================================================================

// Enable/disable major features (set to 0 to disable)
#define WEBSCREEN_ENABLE_WIFI 1
#define WEBSCREEN_ENABLE_MQTT 1
#define WEBSCREEN_ENABLE_BLE 0  // Disabled to avoid conflicts with NimBLE
#define WEBSCREEN_ENABLE_JAVASCRIPT 1
#define WEBSCREEN_ENABLE_FALLBACK_APP 1
#define WEBSCREEN_ENABLE_SD_LOGGING 1
#define WEBSCREEN_ENABLE_PERFORMANCE_MONITORING 1

// Debug Features
#define WEBSCREEN_ENABLE_SERIAL_COMMANDS 1
#define WEBSCREEN_ENABLE_MEMORY_DEBUGGING 1
#define WEBSCREEN_ENABLE_ERROR_RECOVERY 1

// ============================================================================
// LVGL CONFIGURATION
// ============================================================================

// LVGL Display
#define WEBSCREEN_LVGL_BUFFER_SIZE WEBSCREEN_DISPLAY_BUFFER_SIZE
#define WEBSCREEN_LVGL_REFRESH_PERIOD_MS 30
#define WEBSCREEN_LVGL_INDEV_READ_PERIOD_MS 30

// LVGL Features (aligned with lv_conf.h)
#define WEBSCREEN_LVGL_USE_PSRAM 1      // Use PSRAM for LVGL buffers
#define WEBSCREEN_LVGL_DOUBLE_BUFFER 1  // Enable double buffering if PSRAM available
#define WEBSCREEN_LVGL_COLOR_DEPTH 16   // 16-bit color depth

// ============================================================================
// ARDUINO COMPATIBILITY
// ============================================================================

// Ensure compatibility with Arduino IDE
#ifndef ARDUINO
#error "This code is designed for Arduino framework"
#endif

// ESP32 specific checks
#if !defined(ESP32)
#error "This code requires ESP32 platform"
#endif

// Check for ESP32-S3
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#warning "This code is optimized for ESP32-S3, other variants may have issues"
#endif

// ============================================================================
// BUILD CONFIGURATION
// ============================================================================

// Debug vs Release builds
#ifdef WEBSCREEN_DEBUG
#define WEBSCREEN_LOG_LEVEL 0  // Debug level
#define WEBSCREEN_ENABLE_ASSERTIONS 1
#else
#define WEBSCREEN_LOG_LEVEL 2  // Info level
#define WEBSCREEN_ENABLE_ASSERTIONS 0
#endif

// Performance builds
#ifdef WEBSCREEN_PERFORMANCE
#define WEBSCREEN_DISABLE_DEBUG_OUTPUT 1
#define WEBSCREEN_OPTIMIZE_MEMORY 1
#define WEBSCREEN_LOOP_DELAY_MS 0  // No loop delay
#endif

// ============================================================================
// VALIDATION
// ============================================================================

// Compile-time validation of configuration
#if WEBSCREEN_DISPLAY_WIDTH * WEBSCREEN_DISPLAY_HEIGHT > 200000
#error "Display buffer too large for available memory"
#endif

#if WEBSCREEN_JS_HEAP_SIZE_KB > 1024
#warning "JavaScript heap size may be too large"
#endif

// ============================================================================
// ARDUINO LIBRARY DEPENDENCIES
// ============================================================================

/*
Required Arduino Libraries:
- WiFi (ESP32 Core)
- SD_MMC (ESP32 Core) 
- ArduinoJson (by Benoit Blanchon)
- LVGL (by kisvegabor)
- PubSubClient (by Nick O'Leary) - if MQTT enabled
- ESP32-BLE-Arduino (ESP32 Core) - if BLE enabled

Install via Arduino Library Manager or copy to libraries folder.
*/