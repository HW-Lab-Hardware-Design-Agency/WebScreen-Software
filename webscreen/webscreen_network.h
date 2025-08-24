/**
 * @file webscreen_network.h
 * @brief Network connectivity management for WebScreen
 * 
 * Provides Arduino-style interface for WiFi, MQTT, BLE, and HTTP
 * functionality with automatic connection management.
 */

#pragma once

#include "webscreen_config.h"
#include "webscreen_main.h"

#ifdef __cplusplus
extern "C" {
#endif

  // ============================================================================
  // NETWORK INITIALIZATION
  // ============================================================================

  /**
 * @brief Initialize network subsystems
 * 
 * Initializes WiFi, MQTT, and BLE based on configuration.
 * 
 * @param config Pointer to WebScreen configuration
 * @return true if initialization successful, false otherwise
 */

  bool webscreen_network_init(const webscreen_config_t* config);

  /**
 * @brief Run network maintenance loop
 * 
 * Handles connection monitoring, reconnection, and message processing.
 * Should be called regularly from main loop.
 */

  void webscreen_network_loop(void);

  /**
 * @brief Shutdown all network connections
 */

  void webscreen_network_shutdown(void);

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

  // ============================================================================
  // WIFI MANAGEMENT
  // ============================================================================

  /**
 * @brief Connect to WiFi network
 * @param ssid Network SSID
 * @param password Network password
 * @param timeout_ms Connection timeout in milliseconds
 * @return true if connected successfully, false otherwise
 */

  bool webscreen_wifi_connect(const char* ssid, const char* password, uint32_t timeout_ms);

  /**
 * @brief Disconnect from WiFi
 */

  void webscreen_wifi_disconnect(void);

  /**
 * @brief Check if WiFi is connected
 * @return true if connected, false otherwise
 */

  bool webscreen_wifi_is_connected(void);

  /**
 * @brief Get WiFi connection status
 * @return WiFi status code (WL_CONNECTED, WL_DISCONNECTED, etc.)
 */

  int webscreen_wifi_get_status(void);

  /**
 * @brief Get local IP address
 * @param ip_str Buffer to store IP address string (min 16 bytes)
 * @return true if IP address retrieved, false otherwise
 */

  bool webscreen_wifi_get_ip_address(char* ip_str);

  /**
 * @brief Get WiFi signal strength
 * @return RSSI in dBm, or 0 if not connected
 */

  int32_t webscreen_wifi_get_rssi(void);

  /**
 * @brief Enable or disable WiFi auto-reconnect
 * @param enable true to enable auto-reconnect, false to disable
 */

  void webscreen_wifi_set_auto_reconnect(bool enable);

  // ============================================================================
  // HTTP CLIENT
  // ============================================================================

  /**
 * @brief Perform HTTP GET request
 * @param url Target URL
 * @param response_buffer Buffer to store response
 * @param buffer_size Size of response buffer
 * @return HTTP status code, or negative value on error
 */

  int webscreen_http_get(const char* url, char* response_buffer, size_t buffer_size);

  /**
 * @brief Perform HTTP POST request
 * @param url Target URL
 * @param data POST data
 * @param content_type Content-Type header value
 * @param response_buffer Buffer to store response
 * @param buffer_size Size of response buffer
 * @return HTTP status code, or negative value on error
 */

  int webscreen_http_post(const char* url, const char* data, const char* content_type,
                          char* response_buffer, size_t buffer_size);

  /**
 * @brief Set HTTP timeout
 * @param timeout_ms Timeout in milliseconds
 */

  void webscreen_http_set_timeout(uint32_t timeout_ms);

  /**
 * @brief Load SSL certificate from SD card
 * @param cert_file Path to certificate file on SD card
 * @return true if certificate loaded successfully, false otherwise
 */

  bool webscreen_http_set_ca_cert_from_sd(const char* cert_file);

  /**
 * @brief Add HTTP header for next request
 * @param name Header name
 * @param value Header value
 */

  void webscreen_http_add_header(const char* name, const char* value);

  /**
 * @brief Clear all custom HTTP headers
 */

  void webscreen_http_clear_headers(void);

  // ============================================================================
  // MQTT CLIENT
  // ============================================================================

  /**
 * @brief Initialize MQTT client
 * @param broker MQTT broker hostname or IP
 * @param port MQTT broker port
 * @param client_id MQTT client ID
 * @return true if initialization successful, false otherwise
 */

  bool webscreen_mqtt_init(const char* broker, uint16_t port, const char* client_id);

  /**
 * @brief Connect to MQTT broker
 * @param username MQTT username (can be NULL)
 * @param password MQTT password (can be NULL)
 * @return true if connected successfully, false otherwise
 */

  bool webscreen_mqtt_connect(const char* username, const char* password);

  /**
 * @brief Disconnect from MQTT broker
 */

  void webscreen_mqtt_disconnect(void);

  /**
 * @brief Check if MQTT is connected
 * @return true if connected, false otherwise
 */

  bool webscreen_mqtt_is_connected(void);

  /**
 * @brief Publish MQTT message
 * @param topic Topic to publish to
 * @param payload Message payload
 * @param retain Retain message flag
 * @return true if message published successfully, false otherwise
 */

  bool webscreen_mqtt_publish(const char* topic, const char* payload, bool retain);

  /**
 * @brief Subscribe to MQTT topic
 * @param topic Topic to subscribe to
 * @param qos Quality of Service level (0, 1, or 2)
 * @return true if subscription successful, false otherwise
 */

  bool webscreen_mqtt_subscribe(const char* topic, uint8_t qos);

  /**
 * @brief Unsubscribe from MQTT topic
 * @param topic Topic to unsubscribe from
 * @return true if unsubscription successful, false otherwise
 */

  bool webscreen_mqtt_unsubscribe(const char* topic);

  /**
 * @brief Set MQTT message callback
 * @param callback Function to call when message is received
 */

  void webscreen_mqtt_set_callback(void (*callback)(const char* topic, const char* payload));

  /**
 * @brief Process MQTT messages
 * 
 * Should be called regularly to process incoming messages.
 */

  void webscreen_mqtt_loop(void);

  // ============================================================================
  // BLUETOOTH LE
  // ============================================================================

#if WEBSCREEN_ENABLE_BLE

  /**
 * @brief Initialize BLE
 * @param device_name BLE device name
 * @return true if initialization successful, false otherwise
 */

  bool webscreen_ble_init(const char* device_name);

  /**
 * @brief Start BLE advertising
 * @return true if advertising started, false otherwise
 */

  bool webscreen_ble_start_advertising(void);

  /**
 * @brief Stop BLE advertising
 */

  void webscreen_ble_stop_advertising(void);

  /**
 * @brief Check if BLE client is connected
 * @return true if client connected, false otherwise
 */

  bool webscreen_ble_is_connected(void);

  /**
 * @brief Send data via BLE
 * @param data Data to send
 * @param length Data length
 * @return true if data sent successfully, false otherwise
 */

  bool webscreen_ble_send_data(const uint8_t* data, size_t length);

  /**
 * @brief Set BLE data received callback
 * @param callback Function to call when data is received
 */

  void webscreen_ble_set_data_callback(void (*callback)(const uint8_t* data, size_t length));

  /**
 * @brief Shutdown BLE
 */

  void webscreen_ble_shutdown(void);

#endif  // WEBSCREEN_ENABLE_BLE

  // ============================================================================
  // NETWORK UTILITIES
  // ============================================================================

  /**
 * @brief Check if any network interface is connected
 * @return true if any network is available, false otherwise
 */

  bool webscreen_network_is_available(void);

  /**
 * @brief Get network status string
 * @return String describing current network status
 */
  const char* webscreen_network_get_status(void);

  /**
 * @brief Print network information to serial
 */

  void webscreen_network_print_status(void);

  /**
 * @brief Run network connectivity test
 * @param test_url URL to test connectivity (can be NULL for default)
 * @return true if connectivity test passes, false otherwise
 */

  bool webscreen_network_test_connectivity(const char* test_url);

  /**
 * @brief Get network statistics
 * @param bytes_sent Pointer to store bytes sent
 * @param bytes_received Pointer to store bytes received
 * @param connection_uptime Pointer to store connection uptime in ms
 */

  void webscreen_network_get_stats(uint32_t* bytes_sent,
                                   uint32_t* bytes_received,
                                   uint32_t* connection_uptime);

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

// ============================================================================
// LEGACY COMPATIBILITY
// ============================================================================

// Compatibility with existing JavaScript API
#ifdef WEBSCREEN_ENABLE_DEPRECATED_API
#define wifi_connect(ssid, pass) webscreen_wifi_connect(ssid, pass, WEBSCREEN_WIFI_CONNECTION_TIMEOUT_MS)
#define wifi_status() webscreen_wifi_get_status()
#define wifi_get_ip() webscreen_wifi_get_ip_address
#define http_get(url) webscreen_http_get
#define http_post(url, data) webscreen_http_post
#define mqtt_connect(broker, port, client_id) webscreen_mqtt_connect
#define mqtt_publish(topic, payload) webscreen_mqtt_publish(topic, payload, false)
#define mqtt_subscribe(topic) webscreen_mqtt_subscribe(topic, 0)
#endif