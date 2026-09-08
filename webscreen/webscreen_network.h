/**
 * @file webscreen_network.h
 * @brief Network connectivity helpers for WebScreen
 *
 * WiFi connect (boot path) and NTP time synchronization. The JS runtime's
 * HTTP/MQTT clients live in lvgl_elk.h; this module no longer duplicates them.
 */

#pragma once

#include "webscreen_config.h"
#include "webscreen_main.h"

#ifdef __cplusplus
extern "C" {
#endif

  // ============================================================================
  // WIFI
  // ============================================================================

  /**
 * @brief Connect to WiFi with timeout and retry logic
 *
 * Attempts to connect to WiFi using the provided credentials.
 * Uses robust connection logic with timeout and status monitoring.
 *
 * @param ssid WiFi network name
 * @param password WiFi password
 * @param timeout_ms Connection timeout in milliseconds
 * @return true if connected successfully, false if failed or timeout
 */

  bool webscreen_network_connect_wifi(const char* ssid, const char* password, uint32_t timeout_ms);

  /**
 * @brief Check if WiFi is connected
 * @return true if connected, false otherwise
 */

  bool webscreen_wifi_is_connected(void);

  // ============================================================================
  // NTP TIME SYNCHRONIZATION
  // ============================================================================

  /**
   * @brief Initialize NTP time synchronization with UTC offset
   * @param ntp_server NTP server hostname (e.g., "pool.ntp.org")
   * @param utc_offset_sec UTC offset in seconds
   * @param daylight_offset_sec Daylight saving offset in seconds
   * @return true if NTP sync initiated successfully
   */
  bool webscreen_ntp_init(const char* ntp_server, long utc_offset_sec, int daylight_offset_sec);

  /**
   * @brief Initialize NTP with POSIX timezone string
   * @param ntp_server NTP server hostname
   * @param posix_tz POSIX timezone string (e.g., "EST5EDT,M3.2.0,M11.1.0")
   * @return true if NTP sync initiated successfully
   */
  bool webscreen_ntp_init_tz(const char* ntp_server, const char* posix_tz);

  // Boot-only grace period for certificate validation; never call from the LVGL task.
  bool webscreen_ntp_wait_for_sync(uint32_t timeout_ms);

  /**
   * @brief Check if NTP time has been synchronized
   * @return true if time is synchronized, false otherwise
   */
  bool webscreen_ntp_is_synced(void);

  /**
   * @brief Initialize NTP from global config (timezone + ntp_server)
   */
  void webscreen_ntp_setup_from_config(void);

#ifdef __cplusplus
}
#endif

// ============================================================================
// ARDUINO COMPATIBILITY
// ============================================================================

// Include necessary Arduino libraries based on enabled features
#ifndef WEBSCREEN_LIBRARY_MODE
#include <WiFi.h>

#if WEBSCREEN_ENABLE_MQTT
#include <PubSubClient.h>
#endif

// BLE support temporarily disabled to avoid conflicts with NimBLE
//#if WEBSCREEN_ENABLE_BLE
//  #include <BLEDevice.h>
//  #include <BLEServer.h>
//  #include <BLEUtils.h>
//  #include <BLE2902.h>
//#endif

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#endif
