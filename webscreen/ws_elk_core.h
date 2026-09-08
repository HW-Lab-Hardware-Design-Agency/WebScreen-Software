// ws_elk_core.h — fragment of the WebScreen Elk/LVGL bridge; included once, in order, by lvgl_elk.h (not standalone).

#pragma once

#include <lvgl.h>
#include <lvgl_private.h>  // lv_timer_t internals (timer bridge walks timer_cb/user_data)
#include <HTTPClient.h>

#include <NimBLEDevice.h>

#include <WiFi.h>  // WiFi library that also provides WiFiClient
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>  // For MQTT
#include <time.h>
#include <esp_task_wdt.h>
#include <esp_random.h>

#include <vector>
#include <utility>  // for std::pair

#include "globals.h"
#include "rm67162.h"
#include "webscreen_hardware.h"
#include "webscreen_main.h"

WiFiClient g_wifiClient;
PubSubClient g_mqttClient(g_wifiClient);

static char *g_httpCAcert = nullptr;  // Will hold entire PEM cert from SD
static std::vector<std::pair<String, String>> g_http_headers;

static NimBLEServer *g_bleServer = nullptr;
static NimBLECharacteristic *g_bleChar = nullptr;
static bool g_bleConnected = false;

#define JS_GC_THRESHOLD 0.90

extern "C" {
#include "elk.h"
}

static char g_mqttCallbackName[32];  // Big enough for a function name
static char g_mqttBrokerCopy[128];   // Persistent copy of broker hostname for PubSubClient

// MQTT message queue — onMqttMessage stores here, JS polls via mqtt_has_message()
static bool g_mqttMsgPending = false;
static char g_mqttMsgTopic[128];
static char g_mqttMsgPayload[1024];

static bool g_mqttMsgReady = false;
static unsigned long lastMqttReconnectAttempt = 0;
static unsigned long lastWiFiReconnectAttempt = 0;

/******************************************************************************
 * A) Elk Memory + Global Instances
 ******************************************************************************/
// Default arena size; override via "js_heap_kb" in /webscreen.json (64..1024 KB) before the first init_elk_memory().
#define ELK_HEAP_BYTES_DEFAULT (256 * 1024)
static size_t g_elk_heap_bytes = ELK_HEAP_BYTES_DEFAULT;
static uint8_t *elk_memory = NULL;
static size_t elk_memory_size = 0;
struct js *js = NULL;  // Global Elk instance

static void set_elk_heap_kb(int kb) {
  if (elk_memory != NULL) return;  // Arena already allocated — too late
  if (kb < 64) kb = 64;
  if (kb > 1024) kb = 1024;
  g_elk_heap_bytes = (size_t)kb * 1024;
}

// Initialize Elk memory from PSRAM (must be called before js_create)
static bool init_elk_memory() {
  if (elk_memory != NULL) {
    return true;  // Already initialized
  }

  elk_memory = (uint8_t*)ps_malloc(g_elk_heap_bytes);
  if (elk_memory != NULL) {
    elk_memory_size = g_elk_heap_bytes;
    LOGF("Elk heap allocated in PSRAM: %u KB\n", (unsigned)(elk_memory_size / 1024));
    return true;
  }

  size_t fallback_size = 96 * 1024;
  elk_memory = (uint8_t*)malloc(fallback_size);
  if (elk_memory != NULL) {
    elk_memory_size = fallback_size;
    LOGF("Elk heap allocated in RAM (fallback): %u KB\n", fallback_size / 1024);
    return true;
  }

  LOG("ERROR: Failed to allocate Elk heap!");
  return false;
}
#define MAX_RAM_IMAGES 16

struct RamImage {
  bool used;         // Is this slot in use?
  uint8_t *buffer;   // Raw image data allocated from ps_malloc()
  size_t size;       // Byte size of that buffer
  lv_img_dsc_t dsc;  // The descriptor we pass to lv_img_set_src()
};

static RamImage g_ram_images[MAX_RAM_IMAGES];
void init_ram_images() {
  for (int i = 0; i < MAX_RAM_IMAGES; i++) {
    g_ram_images[i].used = false;
    g_ram_images[i].buffer = NULL;
    g_ram_images[i].size = 0;
  }
}

