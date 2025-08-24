/**
 * @file webscreen_main.cpp
 * @brief Main application implementation for WebScreen
 * 
 * This file contains the core application logic following Arduino best practices.
 * All complex functionality is organized into supporting files within the sketch.
 */

#include "webscreen_main.h"
#include "webscreen_hardware.h"
#include "webscreen_runtime.h"
#include "webscreen_network.h"
#include <SD_MMC.h>
#include <ArduinoJson.h>
typedef enum { WEBSCREEN_STATE_INITIALIZING,
               WEBSCREEN_STATE_RUNNING_JS,
               WEBSCREEN_STATE_RUNNING_FALLBACK,
               WEBSCREEN_STATE_ERROR,
               WEBSCREEN_STATE_SHUTDOWN
} webscreen_app_state_t;
static webscreen_app_state_t g_app_state = WEBSCREEN_STATE_INITIALIZING;
static bool g_use_fallback = false;
static bool g_system_healthy = true;
static uint32_t g_last_health_check = 0;
static uint32_t g_last_stats_print = 0;
webscreen_config_t g_webscreen_config = { .wifi = {
                                            .ssid = "",
                                            .password = "",
                                            .enabled = true,
                                            .connection_timeout = WEBSCREEN_WIFI_CONNECTION_TIMEOUT_MS,
                                            .auto_reconnect = true },
                                          .mqtt = { .broker = "", .port = 1883, .username = "", .password = "", .client_id = "webscreen_001", .enabled = false, .keepalive = WEBSCREEN_MQTT_KEEPALIVE_SEC },
                                          .display = { .brightness = 200, .rotation = WEBSCREEN_DISPLAY_ROTATION, .background_color = 0x000000, .foreground_color = 0xFFFFFF, .auto_brightness = false, .screen_timeout = 0 },
                                          .system = { .device_name = "WebScreen", .timezone = "UTC", .log_level = 2, .performance_mode = false, .watchdog_timeout = WEBSCREEN_WATCHDOG_TIMEOUT_SEC * 1000 },
                                          .script_file = "/app.js",
                                          .config_version = 2,
                                          .last_modified = 0 };
static bool initialize_hardware(void);
static bool initialize_storage(void);
static bool load_configuration(void);
static bool initialize_network(void);
static bool start_runtime(void);
static void run_main_loop(void);
static void handle_system_health(void);
bool webscreen_setup(void) {
  WEBSCREEN_DEBUG_PRINTLN("WebScreen v" WEBSCREEN_VERSION_STRING " initializing...");
  if (!initialize_hardware()) {
    WEBSCREEN_DEBUG_PRINTLN("Hardware initialization failed");
    return false;
  }

  if (!initialize_storage()) {
    WEBSCREEN_DEBUG_PRINTLN("Warning: Storage initialization failed, using fallback mode");
    g_use_fallback = true;
  }

  if (!g_use_fallback && !load_configuration()) {
    WEBSCREEN_DEBUG_PRINTLN("Warning: Configuration load failed, using defaults");
  }

  if (!g_use_fallback && g_webscreen_config.wifi.enabled) {
    if (!initialize_network()) {
      WEBSCREEN_DEBUG_PRINTLN("Warning: Network initialization failed");
    }
  }

  if (!start_runtime()) {
    WEBSCREEN_DEBUG_PRINTLN("Runtime initialization failed - using fallback");
    g_use_fallback = true;
    if (!webscreen_runtime_start_fallback()) {
      WEBSCREEN_DEBUG_PRINTLN("Fallback startup failed");
      return false;
    }
  }

  WEBSCREEN_DEBUG_PRINTF("WebScreen initialization complete - Mode: %s\n",
                         g_use_fallback ? "Fallback" : "JavaScript");

  return true;
}
void webscreen_loop(void) {
  run_main_loop();
}

const char *webscreen_get_state(void) {
  switch (g_app_state) {
    case WEBSCREEN_STATE_INITIALIZING: return "Initializing";
    case WEBSCREEN_STATE_RUNNING_JS: return "Running JavaScript";
    case WEBSCREEN_STATE_RUNNING_FALLBACK: return "Running Fallback";
    case WEBSCREEN_STATE_ERROR: return "Error";
    case WEBSCREEN_STATE_SHUTDOWN: return "Shutdown";
    default: return "Unknown";
  }
}
bool webscreen_is_healthy(void) {
  return g_system_healthy;
}
void webscreen_shutdown(void) {
  WEBSCREEN_DEBUG_PRINTLN("Initiating graceful shutdown...");
  webscreen_runtime_shutdown();
  webscreen_network_shutdown();
  webscreen_hardware_shutdown();

  g_app_state = WEBSCREEN_STATE_SHUTDOWN;
  WEBSCREEN_DEBUG_PRINTLN("Shutdown complete");
}

static bool initialize_hardware(void) {
  WEBSCREEN_DEBUG_PRINTLN("Initializing hardware...");
  WEBSCREEN_PIN_MODE(WEBSCREEN_PIN_LED, OUTPUT);
  WEBSCREEN_PIN_MODE(WEBSCREEN_PIN_BUTTON, INPUT_PULLUP);
  WEBSCREEN_PIN_MODE(WEBSCREEN_PIN_OUTPUT, OUTPUT);
  WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_LED);
  WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_OUTPUT);
  if (!webscreen_hardware_init()) {
    WEBSCREEN_DEBUG_PRINTLN("Error: Display initialization failed");
    return false;
  }

  WEBSCREEN_DEBUG_PRINTLN("Hardware initialization complete");
  return true;
}

static bool initialize_storage(void) {
  WEBSCREEN_DEBUG_PRINTLN("Initializing SD Card...");
  SD_MMC.setPins(WEBSCREEN_SD_CLK, WEBSCREEN_SD_CMD, WEBSCREEN_SD_D0);
  for (int i = 0; i < 3; i++) {
    WEBSCREEN_DEBUG_PRINTF("Attempt %d: Mounting SD card at a safe, low frequency...\n", i + 1);
    if (SD_MMC.begin("/sdcard", true, false, 400000)) {
      WEBSCREEN_DEBUG_PRINTLN("SD Card mounted successfully at low frequency.");
      SD_MMC.end();
      WEBSCREEN_DEBUG_PRINTLN("Re-mounting SD card at high frequency...");
      if (SD_MMC.begin("/sdcard", true, false, 10000000)) {
        WEBSCREEN_DEBUG_PRINTLN("SD Card re-mounted successfully at high frequency.");
        return true;
      } else {
        WEBSCREEN_DEBUG_PRINTLN("Failed to re-mount at high frequency. Falling back to low speed mount.");

        if (SD_MMC.begin("/sdcard", true, false, 400000)) {
          WEBSCREEN_DEBUG_PRINTLN("Continuing at safe, low frequency.");
          return true;
        }
      }
    }

    WEBSCREEN_DEBUG_PRINTF("Attempt %d failed. Retrying in 200ms...\n", i + 1);
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  WEBSCREEN_DEBUG_PRINTLN("All attempts to mount SD card failed.");
  return false;
}

static bool load_configuration(void) {
  WEBSCREEN_DEBUG_PRINTLN("Loading configuration...");
  if (!SD_MMC.exists(WEBSCREEN_CONFIG_FILENAME)) {
    WEBSCREEN_DEBUG_PRINTLN("Config file not found, using defaults");
    return false;
  }
  File configFile = SD_MMC.open(WEBSCREEN_CONFIG_FILENAME, FILE_READ);
  if (!configFile) {
    WEBSCREEN_DEBUG_PRINTLN("Failed to open config file");
    return false;
  }
  String configStr = configFile.readString();
  configFile.close();

  StaticJsonDocument<WEBSCREEN_CONFIG_BUFFER_SIZE> doc;
  DeserializationError error = deserializeJson(doc, configStr);

  if (error) {
    WEBSCREEN_DEBUG_PRINTF("Config parse error: %s\n", error.c_str());
    return false;
  }
  if (doc["wifi"]["ssid"]) {
    WEBSCREEN_STR_COPY(g_webscreen_config.wifi.ssid,
                       doc["wifi"]["ssid"], sizeof(g_webscreen_config.wifi.ssid));
  }
  if (doc["wifi"]["password"]) {
    WEBSCREEN_STR_COPY(g_webscreen_config.wifi.password,
                       doc["wifi"]["password"], sizeof(g_webscreen_config.wifi.password));
  }

  g_webscreen_config.wifi.enabled = doc["wifi"]["enabled"] | g_webscreen_config.wifi.enabled;
  g_webscreen_config.display.brightness = doc["display"]["brightness"] | g_webscreen_config.display.brightness;
  g_webscreen_config.system.log_level = doc["system"]["log_level"] | g_webscreen_config.system.log_level;

  if (doc["script_file"]) {
    WEBSCREEN_STR_COPY(g_webscreen_config.script_file,
                       doc["script_file"], sizeof(g_webscreen_config.script_file));
  }

  WEBSCREEN_DEBUG_PRINTLN("Configuration loaded successfully");
  return true;
}

static bool initialize_network(void) {
  WEBSCREEN_DEBUG_PRINTLN("Initializing network...");

  if (strlen(g_webscreen_config.wifi.ssid) == 0) {
    WEBSCREEN_DEBUG_PRINTLN("No WiFi SSID configured");
    return false;
  }

  return webscreen_network_init(&g_webscreen_config);
}

static bool start_runtime(void) {
  WEBSCREEN_DEBUG_PRINTLN("Starting runtime...");
  if (g_use_fallback) {
    WEBSCREEN_DEBUG_PRINTLN("Starting fallback application");
    g_app_state = WEBSCREEN_STATE_RUNNING_FALLBACK;
    return webscreen_runtime_start_fallback();
  }
  if (!SD_MMC.exists(g_webscreen_config.script_file)) {
    WEBSCREEN_DEBUG_PRINTF("Script file not found: %s\n", g_webscreen_config.script_file);
    WEBSCREEN_DEBUG_PRINTLN("Falling back to fallback application");
    g_use_fallback = true;
    g_app_state = WEBSCREEN_STATE_RUNNING_FALLBACK;
    return webscreen_runtime_start_fallback();
  }
  WEBSCREEN_DEBUG_PRINTF("Starting JavaScript runtime with: %s\n", g_webscreen_config.script_file);

  if (webscreen_runtime_start_javascript(g_webscreen_config.script_file)) {
    g_app_state = WEBSCREEN_STATE_RUNNING_JS;
    return true;
  } else {
    WEBSCREEN_DEBUG_PRINTLN("JavaScript runtime failed, using fallback");
    g_use_fallback = true;
    g_app_state = WEBSCREEN_STATE_RUNNING_FALLBACK;
    return webscreen_runtime_start_fallback();
  }
}

static void run_main_loop(void) {

  webscreen_hardware_handle_button();
  switch (g_app_state) {
    case WEBSCREEN_STATE_RUNNING_JS:
      webscreen_runtime_loop_javascript();
      break;

    case WEBSCREEN_STATE_RUNNING_FALLBACK:
      webscreen_runtime_loop_fallback();
      break;

    case WEBSCREEN_STATE_ERROR:

      WEBSCREEN_DELAY(1000);
      break;

    case WEBSCREEN_STATE_SHUTDOWN:

      return;

    default:
      WEBSCREEN_DEBUG_PRINTF("Invalid app state: %d\n", g_app_state);
      g_app_state = WEBSCREEN_STATE_ERROR;
      break;
  }
  if (!g_use_fallback && g_webscreen_config.wifi.enabled) {
    webscreen_network_loop();
  }
  handle_system_health();
  WEBSCREEN_DELAY(WEBSCREEN_LOOP_DELAY_MS);
}

static void handle_system_health(void) {
  uint32_t now = WEBSCREEN_MILLIS();
  if (now - g_last_health_check > 30000) {
    g_last_health_check = now;
    uint32_t free_heap = ESP.getFreeHeap();
    uint32_t total_heap = ESP.getHeapSize();
    float memory_usage = 1.0f - ((float)free_heap / (float)total_heap);

    if (memory_usage > WEBSCREEN_MEMORY_WARNING_THRESHOLD) {
      WEBSCREEN_DEBUG_PRINTF("Warning: High memory usage (%.1f%%)\n", memory_usage * 100);
      g_system_healthy = false;
    } else {
      g_system_healthy = true;
    }
    if (now - g_last_stats_print > WEBSCREEN_STATS_REPORT_INTERVAL_MS) {
      g_last_stats_print = now;
      WEBSCREEN_DEBUG_PRINTF("System Health: %s, Free Heap: %d bytes, Uptime: %lu ms\n",
                             g_system_healthy ? "Good" : "Degraded",
                             free_heap, now);
    }
  }
}
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
  String jsonStr = f.readString();
  f.close();

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonStr);
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

  outBgColor = strtol(bgColorStr + 1, NULL, 16);
  outFgColor = strtol(fgColorStr + 1, NULL, 16);

  WEBSCREEN_DEBUG_PRINTF("Config loaded - SSID: %s, Script: %s, MQTT: %s\n",
                         outSSID.c_str(), outScript.c_str(), outMqttEnabled ? "enabled" : "disabled");

  return true;
}
