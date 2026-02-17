/**
 * @file webscreen_network.cpp
 * @brief Network connectivity implementation for Arduino
 * 
 * Simplified network implementation following Arduino best practices.
 */

#include "webscreen_network.h"
#include "webscreen_main.h"
#include <time.h>
#include <sys/time.h>
static bool g_network_initialized = false;
static bool g_ntp_initialized = false;
static bool g_wifi_auto_reconnect = true;
static String g_wifi_ssid = "";
static String g_wifi_password = "";
static uint32_t g_wifi_connection_time = 0;
static uint32_t g_bytes_sent = 0;
static uint32_t g_bytes_received = 0;
static HTTPClient g_http_client;
static WiFiClientSecure g_wifi_client_secure;
#if WEBSCREEN_ENABLE_MQTT
static PubSubClient g_mqtt_client;
static String g_mqtt_broker = "";
static uint16_t g_mqtt_port = 1883;
static String g_mqtt_client_id = "";
static void (*g_mqtt_callback)(const char*, const char*) = nullptr;
#endif
bool webscreen_network_init(const webscreen_config_t* config) {
  if (!config) {
    return false;
  }

  WEBSCREEN_DEBUG_PRINTLN("Initializing network...");
  g_wifi_auto_reconnect = config->wifi.auto_reconnect;

  if (!config->wifi.enabled) {
    WEBSCREEN_DEBUG_PRINTLN("WiFi disabled");
    return false;
  }

  if (strlen(config->wifi.ssid) == 0) {
    WEBSCREEN_DEBUG_PRINTLN("No WiFi SSID configured");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(g_wifi_auto_reconnect);

  WEBSCREEN_DEBUG_PRINTF("Connecting to: %s\n", config->wifi.ssid);
  g_wifi_ssid = config->wifi.ssid;
  g_wifi_password = config->wifi.password;

  if (!webscreen_wifi_connect(g_wifi_ssid.c_str(), g_wifi_password.c_str(),
                              config->wifi.connection_timeout)) {
    WEBSCREEN_DEBUG_PRINTLN("WiFi connection failed");
    return false;
  }

  WEBSCREEN_DEBUG_PRINTF("Successfully connected to: %s\n", g_wifi_ssid.c_str());
  g_http_client.setTimeout(WEBSCREEN_HTTP_TIMEOUT_MS);

  // Initialize NTP time synchronization
  webscreen_ntp_setup_from_config();

#if WEBSCREEN_ENABLE_MQTT

  if (config->mqtt.enabled && strlen(config->mqtt.broker) > 0) {
    g_mqtt_broker = config->mqtt.broker;
    g_mqtt_port = config->mqtt.port;
    g_mqtt_client_id = config->mqtt.client_id;

    if (!webscreen_mqtt_init(g_mqtt_broker.c_str(), g_mqtt_port, g_mqtt_client_id.c_str())) {
      WEBSCREEN_DEBUG_PRINTLN("MQTT initialization failed");
    } else if (!webscreen_mqtt_connect(config->mqtt.username, config->mqtt.password)) {
      WEBSCREEN_DEBUG_PRINTLN("MQTT connection failed");
    }
  }
#endif

  g_network_initialized = true;
  WEBSCREEN_DEBUG_PRINTLN("Network initialization complete");
  return true;
}
void webscreen_network_loop(void) {
  if (!g_network_initialized) {
    return;
  }
  if (WiFi.status() != WL_CONNECTED && g_wifi_auto_reconnect) {
    static uint32_t last_reconnect_attempt = 0;
    if (WEBSCREEN_MILLIS() - last_reconnect_attempt > 10000) {
      last_reconnect_attempt = WEBSCREEN_MILLIS();
      WEBSCREEN_DEBUG_PRINTLN("WiFi disconnected, attempting reconnection...");
      if (webscreen_wifi_connect(g_wifi_ssid.c_str(), g_wifi_password.c_str(), 5000)) {
        // WiFi reconnected - initialize NTP if not already synced
        if (!g_ntp_initialized) {
          webscreen_ntp_setup_from_config();
        }
      }
    }
  }

#if WEBSCREEN_ENABLE_MQTT

  if (g_mqtt_client.connected()) {
    webscreen_mqtt_loop();
  } else if (g_mqtt_broker.length() > 0) {
    static uint32_t last_mqtt_reconnect = 0;
    if (WEBSCREEN_MILLIS() - last_mqtt_reconnect > 30000) {
      last_mqtt_reconnect = WEBSCREEN_MILLIS();
      WEBSCREEN_DEBUG_PRINTLN("MQTT disconnected, attempting reconnection...");
      webscreen_mqtt_connect(nullptr, nullptr);
    }
  }
#endif
}
void webscreen_network_shutdown(void) {
  if (!g_network_initialized) {
    return;
  }

  WEBSCREEN_DEBUG_PRINTLN("Shutting down network...");

#if WEBSCREEN_ENABLE_MQTT
  webscreen_mqtt_disconnect();
#endif

  webscreen_wifi_disconnect();

  g_network_initialized = false;
  WEBSCREEN_DEBUG_PRINTLN("Network shutdown complete");
}
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
bool webscreen_wifi_connect(const char* ssid, const char* password, uint32_t timeout_ms) {
  if (!ssid) {
    return false;
  }

  WEBSCREEN_DEBUG_PRINTF("Connecting to WiFi: %s\n", ssid);

  WiFi.begin(ssid, password);

  uint32_t start_time = WEBSCREEN_MILLIS();
  while (WiFi.status() != WL_CONNECTED && (WEBSCREEN_MILLIS() - start_time) < timeout_ms) {
    WEBSCREEN_DELAY(250);
    WEBSCREEN_DEBUG_PRINT(".");
  }
  WEBSCREEN_DEBUG_PRINTLN();

  if (WiFi.status() == WL_CONNECTED) {
    g_wifi_connection_time = WEBSCREEN_MILLIS();
    WEBSCREEN_DEBUG_PRINTF("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    WEBSCREEN_DEBUG_PRINTF("Signal strength: %d dBm\n", WiFi.RSSI());
    return true;
  } else {
    WEBSCREEN_DEBUG_PRINTF("WiFi connection failed. Status: %d\n", WiFi.status());
    return false;
  }
}
void webscreen_wifi_disconnect(void) {
  WiFi.disconnect();
  WEBSCREEN_DEBUG_PRINTLN("WiFi disconnected");
}
bool webscreen_wifi_is_connected(void) {
  return (WiFi.status() == WL_CONNECTED);
}
int webscreen_wifi_get_status(void) {
  return WiFi.status();
}
bool webscreen_wifi_get_ip_address(char* ip_str) {
  if (!ip_str || !webscreen_wifi_is_connected()) {
    return false;
  }

  strcpy(ip_str, WiFi.localIP().toString().c_str());
  return true;
}
int32_t webscreen_wifi_get_rssi(void) {
  if (!webscreen_wifi_is_connected()) {
    return 0;
  }
  return WiFi.RSSI();
}
void webscreen_wifi_set_auto_reconnect(bool enable) {
  g_wifi_auto_reconnect = enable;
  WiFi.setAutoReconnect(enable);
}
int webscreen_http_get(const char* url, char* response_buffer, size_t buffer_size) {
  if (!url || !response_buffer || buffer_size == 0) {
    return -1;
  }

  if (!webscreen_wifi_is_connected()) {
    WEBSCREEN_DEBUG_PRINTLN("HTTP GET failed: WiFi not connected");
    return -2;
  }

  WEBSCREEN_DEBUG_PRINTF("HTTP GET: %s\n", url);

  g_http_client.begin(url);
  int http_code = g_http_client.GET();

  if (http_code > 0) {
    String payload = g_http_client.getString();
    size_t copy_length = min(payload.length(), buffer_size - 1);
    strncpy(response_buffer, payload.c_str(), copy_length);
    response_buffer[copy_length] = '\0';

    g_bytes_received += payload.length();

    WEBSCREEN_DEBUG_PRINTF("HTTP GET response: %d (%d bytes)\n", http_code, payload.length());
  } else {
    WEBSCREEN_DEBUG_PRINTF("HTTP GET failed: %s\n", g_http_client.errorToString(http_code).c_str());
    response_buffer[0] = '\0';
  }

  g_http_client.end();
  return http_code;
}
int webscreen_http_post(const char* url, const char* data, const char* content_type,
                        char* response_buffer, size_t buffer_size) {
  if (!url || !data || !response_buffer || buffer_size == 0) {
    return -1;
  }

  if (!webscreen_wifi_is_connected()) {
    WEBSCREEN_DEBUG_PRINTLN("HTTP POST failed: WiFi not connected");
    return -2;
  }

  WEBSCREEN_DEBUG_PRINTF("HTTP POST: %s\n", url);

  g_http_client.begin(url);
  g_http_client.addHeader("Content-Type", content_type ? content_type : "application/json");

  int http_code = g_http_client.POST(data);

  if (http_code > 0) {
    String payload = g_http_client.getString();
    size_t copy_length = min(payload.length(), buffer_size - 1);
    strncpy(response_buffer, payload.c_str(), copy_length);
    response_buffer[copy_length] = '\0';

    g_bytes_sent += strlen(data);
    g_bytes_received += payload.length();

    WEBSCREEN_DEBUG_PRINTF("HTTP POST response: %d (%d bytes)\n", http_code, payload.length());
  } else {
    WEBSCREEN_DEBUG_PRINTF("HTTP POST failed: %s\n", g_http_client.errorToString(http_code).c_str());
    response_buffer[0] = '\0';
  }

  g_http_client.end();
  return http_code;
}
void webscreen_http_set_timeout(uint32_t timeout_ms) {
  g_http_client.setTimeout(timeout_ms);
}
bool webscreen_http_set_ca_cert_from_sd(const char* cert_file) {
  if (!cert_file || !SD_MMC.exists(cert_file)) {
    WEBSCREEN_DEBUG_PRINTF("Certificate file not found: %s\n", cert_file);
    return false;
  }

  File cert = SD_MMC.open(cert_file, FILE_READ);
  if (!cert) {
    WEBSCREEN_DEBUG_PRINTF("Failed to open certificate file: %s\n", cert_file);
    return false;
  }

  String cert_content = cert.readString();
  cert.close();

  g_wifi_client_secure.setCACert(cert_content.c_str());
  WEBSCREEN_DEBUG_PRINTF("SSL certificate loaded from: %s\n", cert_file);
  return true;
}
void webscreen_http_add_header(const char* name, const char* value) {
  if (name && value) {
    g_http_client.addHeader(name, value);
  }
}
void webscreen_http_clear_headers(void) {
  g_http_client.end();
}

#if WEBSCREEN_ENABLE_MQTT
bool webscreen_mqtt_init(const char* broker, uint16_t port, const char* client_id) {
  if (!broker || !client_id) {
    return false;
  }

  static WiFiClient wifi_client;
  g_mqtt_client.setClient(wifi_client);
  g_mqtt_client.setServer(broker, port);

  WEBSCREEN_DEBUG_PRINTF("MQTT initialized: %s:%d (client: %s)\n", broker, port, client_id);
  return true;
}
bool webscreen_mqtt_connect(const char* username, const char* password) {
  if (!webscreen_wifi_is_connected()) {
    WEBSCREEN_DEBUG_PRINTLN("MQTT connect failed: WiFi not connected");
    return false;
  }

  WEBSCREEN_DEBUG_PRINTF("Connecting to MQTT broker: %s\n", g_mqtt_broker.c_str());

  bool connected;
  if (username && password) {
    connected = g_mqtt_client.connect(g_mqtt_client_id.c_str(), username, password);
  } else {
    connected = g_mqtt_client.connect(g_mqtt_client_id.c_str());
  }

  if (connected) {
    WEBSCREEN_DEBUG_PRINTLN("MQTT connected");
    if (g_mqtt_callback) {
      g_mqtt_client.setCallback([](char* topic, byte* payload, unsigned int length) {
        payload[length] = '\0';
        if (g_mqtt_callback) {
          g_mqtt_callback(topic, (char*)payload);
        }
      });
    }
  } else {
    WEBSCREEN_DEBUG_PRINTF("MQTT connection failed, rc=%d\n", g_mqtt_client.state());
  }

  return connected;
}
void webscreen_mqtt_disconnect(void) {
  if (g_mqtt_client.connected()) {
    g_mqtt_client.disconnect();
    WEBSCREEN_DEBUG_PRINTLN("MQTT disconnected");
  }
}
bool webscreen_mqtt_is_connected(void) {
  return g_mqtt_client.connected();
}
bool webscreen_mqtt_publish(const char* topic, const char* payload, bool retain) {
  if (!topic || !payload || !g_mqtt_client.connected()) {
    return false;
  }

  bool result = g_mqtt_client.publish(topic, payload, retain);
  if (result) {
    g_bytes_sent += strlen(payload);
    WEBSCREEN_DEBUG_PRINTF("MQTT published: [%s] %s\n", topic, payload);
  } else {
    WEBSCREEN_DEBUG_PRINTF("MQTT publish failed: [%s]\n", topic);
  }

  return result;
}
bool webscreen_mqtt_subscribe(const char* topic, uint8_t qos) {
  if (!topic || !g_mqtt_client.connected()) {
    return false;
  }

  bool result = g_mqtt_client.subscribe(topic, qos);
  if (result) {
    WEBSCREEN_DEBUG_PRINTF("MQTT subscribed: %s (QoS %d)\n", topic, qos);
  } else {
    WEBSCREEN_DEBUG_PRINTF("MQTT subscribe failed: %s\n", topic);
  }

  return result;
}
bool webscreen_mqtt_unsubscribe(const char* topic) {
  if (!topic || !g_mqtt_client.connected()) {
    return false;
  }

  bool result = g_mqtt_client.unsubscribe(topic);
  if (result) {
    WEBSCREEN_DEBUG_PRINTF("MQTT unsubscribed: %s\n", topic);
  } else {
    WEBSCREEN_DEBUG_PRINTF("MQTT unsubscribe failed: %s\n", topic);
  }

  return result;
}
void webscreen_mqtt_set_callback(void (*callback)(const char* topic, const char* payload)) {
  g_mqtt_callback = callback;
}
void webscreen_mqtt_loop(void) {
  if (g_mqtt_client.connected()) {
    g_mqtt_client.loop();
  }
}

#endif
bool webscreen_network_is_available(void) {
  return webscreen_wifi_is_connected();
}

const char* webscreen_network_get_status(void) {
  static String status;

  if (!g_network_initialized) {
    return "Network not initialized";
  }

  status = "WiFi: ";
  if (webscreen_wifi_is_connected()) {
    status += "Connected (";
    status += WiFi.localIP().toString();
    status += ")";
  } else {
    status += "Disconnected";
  }

#if WEBSCREEN_ENABLE_MQTT
  status += " | MQTT: ";
  status += webscreen_mqtt_is_connected() ? "Connected" : "Disconnected";
#endif

  return status.c_str();
}
void webscreen_network_print_status(void) {
  WEBSCREEN_DEBUG_PRINTLN("\n=== NETWORK STATUS ===");
  WEBSCREEN_DEBUG_PRINTF("Initialized: %s\n", g_network_initialized ? "Yes" : "No");
  WEBSCREEN_DEBUG_PRINTF("WiFi Status: %s\n", webscreen_network_get_status());

  if (webscreen_wifi_is_connected()) {
    WEBSCREEN_DEBUG_PRINTF("SSID: %s\n", WiFi.SSID().c_str());
    WEBSCREEN_DEBUG_PRINTF("IP Address: %s\n", WiFi.localIP().toString().c_str());
    WEBSCREEN_DEBUG_PRINTF("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    WEBSCREEN_DEBUG_PRINTF("DNS: %s\n", WiFi.dnsIP().toString().c_str());
    WEBSCREEN_DEBUG_PRINTF("RSSI: %d dBm\n", WiFi.RSSI());
    WEBSCREEN_DEBUG_PRINTF("Connection uptime: %lu ms\n",
                           WEBSCREEN_MILLIS() - g_wifi_connection_time);
  }

  WEBSCREEN_DEBUG_PRINTF("Bytes sent: %lu\n", g_bytes_sent);
  WEBSCREEN_DEBUG_PRINTF("Bytes received: %lu\n", g_bytes_received);

  WEBSCREEN_DEBUG_PRINTLN("======================\n");
}
bool webscreen_network_test_connectivity(const char* test_url) {
  const char* url = test_url ? test_url : "http://httpbin.org/get";

  char response[256];
  int result = webscreen_http_get(url, response, sizeof(response));

  bool success = (result >= 200 && result < 300);
  WEBSCREEN_DEBUG_PRINTF("Connectivity test: %s (HTTP %d)\n",
                         success ? "PASS" : "FAIL", result);

  return success;
}
void webscreen_network_get_stats(uint32_t* bytes_sent,
                                 uint32_t* bytes_received,
                                 uint32_t* connection_uptime) {
  if (bytes_sent) *bytes_sent = g_bytes_sent;
  if (bytes_received) *bytes_received = g_bytes_received;
  if (connection_uptime) {
    *connection_uptime = webscreen_wifi_is_connected() ? (WEBSCREEN_MILLIS() - g_wifi_connection_time) : 0;
  }
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