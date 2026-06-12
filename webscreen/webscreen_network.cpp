/**
 * @file webscreen_network.cpp
 * @brief WiFi connect + NTP helpers for the live boot path
 *
 * The former webscreen_network_init()/loop() layer (with its boot-resident
 * HTTPClient/WiFiClientSecure/PubSubClient globals) was dead code duplicating
 * the live g_wifiClient/g_mqttClient pair in lvgl_elk.h and has been removed.
 */

#include "webscreen_network.h"
#include "webscreen_main.h"
#include <time.h>
#include <sys/time.h>
static bool g_ntp_initialized = false;
bool webscreen_network_connect_wifi(const char* ssid, const char* password, uint32_t timeout_ms) {
  if (!ssid || strlen(ssid) == 0) {
    WEBSCREEN_DEBUG_PRINTLN("No WiFi SSID provided");
    return false;
  }

  WEBSCREEN_DEBUG_PRINTF("Connecting to WiFi: %s\n", ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long startMs = WEBSCREEN_MILLIS();
  while (WiFi.status() != WL_CONNECTED && (WEBSCREEN_MILLIS() - startMs) < timeout_ms) {
    vTaskDelay(pdMS_TO_TICKS(250));
    Serial.print(".");
  }
  WEBSCREEN_DEBUG_PRINTLN();

  if (WiFi.status() != WL_CONNECTED) {
    WEBSCREEN_DEBUG_PRINTLN("WiFi connection failed or timed out");
    return false;
  }

  WEBSCREEN_DEBUG_PRINTF("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
  return true;
}
bool webscreen_wifi_is_connected(void) {
  return (WiFi.status() == WL_CONNECTED);
}

// ============================================================================
// NTP TIME SYNCHRONIZATION
// ============================================================================

bool webscreen_ntp_init(const char* ntp_server, long utc_offset_sec, int daylight_offset_sec) {
  if (!webscreen_wifi_is_connected()) {
    WEBSCREEN_DEBUG_PRINTLN("NTP init failed: WiFi not connected");
    return false;
  }

  const char* server = (ntp_server && strlen(ntp_server) > 0) ? ntp_server : WEBSCREEN_NTP_SERVER_DEFAULT;

  WEBSCREEN_DEBUG_PRINTF("NTP: Configuring with server=%s, offset=%ld sec\n", server, utc_offset_sec);
  configTime(utc_offset_sec, daylight_offset_sec, server);

  // Wait for initial sync with timeout
  struct tm timeinfo;
  uint32_t start = WEBSCREEN_MILLIS();
  while (!getLocalTime(&timeinfo, 1000) && (WEBSCREEN_MILLIS() - start) < WEBSCREEN_NTP_SYNC_TIMEOUT_MS) {
    WEBSCREEN_DEBUG_PRINT(".");
    WEBSCREEN_DELAY(500);
  }
  WEBSCREEN_DEBUG_PRINTLN();

  if (getLocalTime(&timeinfo, 100)) {
    g_ntp_initialized = true;
    WEBSCREEN_DEBUG_PRINTF("NTP: Time synchronized: %04d-%02d-%02d %02d:%02d:%02d\n",
                           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return true;
  }

  WEBSCREEN_DEBUG_PRINTLN("NTP: Initial sync timeout, will retry in background");
  g_ntp_initialized = true;  // SNTP daemon is running, will sync eventually
  return false;
}

bool webscreen_ntp_init_tz(const char* ntp_server, const char* posix_tz) {
  if (!webscreen_wifi_is_connected()) {
    WEBSCREEN_DEBUG_PRINTLN("NTP init failed: WiFi not connected");
    return false;
  }

  const char* server = (ntp_server && strlen(ntp_server) > 0) ? ntp_server : WEBSCREEN_NTP_SERVER_DEFAULT;

  WEBSCREEN_DEBUG_PRINTF("NTP: Configuring with server=%s, tz=%s\n", server, posix_tz);
  configTzTime(posix_tz, server);

  // Wait for initial sync with timeout
  struct tm timeinfo;
  uint32_t start = WEBSCREEN_MILLIS();
  while (!getLocalTime(&timeinfo, 1000) && (WEBSCREEN_MILLIS() - start) < WEBSCREEN_NTP_SYNC_TIMEOUT_MS) {
    WEBSCREEN_DEBUG_PRINT(".");
    WEBSCREEN_DELAY(500);
  }
  WEBSCREEN_DEBUG_PRINTLN();

  if (getLocalTime(&timeinfo, 100)) {
    g_ntp_initialized = true;
    WEBSCREEN_DEBUG_PRINTF("NTP: Time synchronized: %04d-%02d-%02d %02d:%02d:%02d\n",
                           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return true;
  }

  g_ntp_initialized = true;
  return false;
}

bool webscreen_ntp_is_synced(void) {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    if (timeinfo.tm_year > (2020 - 1900)) {  // Sanity check: year > 2020
      return true;
    }
  }
  return false;
}

void webscreen_ntp_setup_from_config(void) {
  const char* tz_str = g_webscreen_config.system.timezone;
  const char* ntp_srv = g_webscreen_config.system.ntp_server;

  // Timezone format detection:
  // 1. "UTC" or empty -> offset 0
  // 2. Numeric ("-3", "5.5") -> simple UTC offset in hours (no DST)
  // 3. POSIX TZ string (e.g. "<-03>3", "EST5EDT,M3.2.0,M11.1.0") -> full timezone with DST
  //
  // POSIX TZ format: STDoffset[DST[offset],start,end]
  //   - Offset is hours WEST of UTC (positive=west, negative=east)
  //   - Examples: "<-03>3" (Buenos Aires), "JST-9" (Tokyo), "CET-1CEST,M3.5.0,M10.5.0/3" (Europe)
  //   - Full list: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

  if (strcmp(tz_str, "UTC") == 0 || strlen(tz_str) == 0) {
    WEBSCREEN_DEBUG_PRINTLN("NTP: Using UTC (offset 0)");
    webscreen_ntp_init(ntp_srv, 0, 0);
    return;
  }

  // Check if it's a simple numeric offset (starts with digit, '-', or '+')
  char first = tz_str[0];
  if (first == '-' || first == '+' || (first >= '0' && first <= '9')) {
    float offset_hours = atof(tz_str);
    long offset_sec = (long)(offset_hours * 3600);
    WEBSCREEN_DEBUG_PRINTF("NTP: Using UTC offset: %ld sec (%.1f hours)\n", offset_sec, offset_hours);
    webscreen_ntp_init(ntp_srv, offset_sec, 0);
    return;
  }

  // Everything else is treated as a POSIX TZ string
  WEBSCREEN_DEBUG_PRINTF("NTP: Using POSIX timezone: %s\n", tz_str);
  webscreen_ntp_init_tz(ntp_srv, tz_str);
}
