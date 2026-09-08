/**
 * @file webscreen_main.cpp
 * @brief Global configuration storage and loader for WebScreen
 *
 * The live boot path is webscreen.ino setup() -> webscreen_load_config().
 * The former webscreen_setup()/webscreen_loop() state machine was an unused
 * second architecture and has been removed.
 */

#include "webscreen_main.h"
#include "webscreen_runtime.h"
#include "webscreen_config_parse.h"
#include <SD_MMC.h>
#include <ArduinoJson.h>
webscreen_config_t g_webscreen_config = { .wifi = {
                                            .ssid = "",
                                            .password = "",
                                            .enabled = true,
                                            .connection_timeout = WEBSCREEN_WIFI_CONNECTION_TIMEOUT_MS,
                                            .auto_reconnect = true },
                                          .mqtt = { .broker = "", .port = 1883, .username = "", .password = "", .client_id = "webscreen_001", .enabled = false, .keepalive = WEBSCREEN_MQTT_KEEPALIVE_SEC },
                                          .display = { .brightness = 200, .rotation = WEBSCREEN_DISPLAY_ROTATION, .background_color = 0x000000, .foreground_color = 0xFFFFFF, .auto_brightness = false, .screen_timeout = 0 },
                                          .system = { .device_name = "WebScreen", .timezone = "UTC", .ntp_server = "pool.ntp.org", .log_level = 2, .performance_mode = false, .watchdog_timeout = WEBSCREEN_WATCHDOG_TIMEOUT_SEC * 1000 },
                                          .script_file = "/app.js",
                                          .config_version = 2,
                                          .last_modified = 0 };
bool webscreen_load_config(const char *path,
                           String &outSSID,
                           String &outPASS,
                           String &outScript,
                           bool &outMqttEnabled,
                           uint32_t &outBgColor,
                           uint32_t &outFgColor) {
  WEBSCREEN_DEBUG_PRINTF("Loading configuration from: %s\n", path);

  File f = SD_MMC.open(path);
  if (!f) {
    WEBSCREEN_DEBUG_PRINTLN("No JSON config file found");
    return false;
  }
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, f);
  f.close();
  if (error) {
    WEBSCREEN_DEBUG_PRINTF("Failed to parse JSON: %s\n", error.c_str());
    return false;
  }

  outSSID = doc["settings"]["wifi"]["ssid"] | "";
  outPASS = doc["settings"]["wifi"]["pass"] | "";
  outMqttEnabled = doc["settings"]["mqtt"]["enabled"] | false;
  outScript = doc["script"] | "app.js";

  const char *bgColorStr = doc["screen"]["background"] | "#000000";
  const char *fgColorStr = doc["screen"]["foreground"] | "#FFFFFF";

  outBgColor = webscreen_parse_color(bgColorStr, 0x000000);
  outFgColor = webscreen_parse_color(fgColorStr, 0xFFFFFF);

  // Load display brightness into global config so init_lvgl_display() can apply it
  int brightness = doc["display"]["brightness"] | (int)g_webscreen_config.display.brightness;
  g_webscreen_config.display.brightness = brightness < 0 ? 0 : brightness > 255 ? 255 : brightness;

  // Load timezone (check top-level first, then system.timezone)
  const char* tz = doc["timezone"] | (const char*)nullptr;
  if (!tz) tz = doc["system"]["timezone"] | (const char*)nullptr;
  if (tz) {
    WEBSCREEN_STR_COPY(g_webscreen_config.system.timezone, tz, sizeof(g_webscreen_config.system.timezone));
  }

  // Load NTP server
  const char* ntp = doc["system"]["ntp_server"] | (const char*)nullptr;
  if (ntp) {
    WEBSCREEN_STR_COPY(g_webscreen_config.system.ntp_server, ntp, sizeof(g_webscreen_config.system.ntp_server));
  }

  // Optional JS engine arena size in KB (flat top-level key, clamped by the
  // runtime to 64..1024). Must be applied before the engine starts.
  int jsHeapKb = doc["js_heap_kb"] | 0;
  if (jsHeapKb > 0) {
    webscreen_runtime_set_js_heap_kb(jsHeapKb);
    WEBSCREEN_DEBUG_PRINTF("JS heap size configured: %d KB\n", jsHeapKb);
  }

  WEBSCREEN_DEBUG_PRINTF("Config loaded - SSID: %s, Script: %s, MQTT: %s, Brightness: %d, TZ: %s\n",
                         outSSID.c_str(), outScript.c_str(), outMqttEnabled ? "enabled" : "disabled",
                         g_webscreen_config.display.brightness, g_webscreen_config.system.timezone);

  return true;
}
