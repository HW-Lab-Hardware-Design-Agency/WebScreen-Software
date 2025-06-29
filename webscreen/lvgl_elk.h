#pragma once

#include <lvgl.h>
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <HTTPClient.h>
#include "tick.h"
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <vector>
#include <utility>
#include "pins_config.h"
#include "rm67162.h"
#include "globals.h"
#include <SPIFFS.h>

extern "C" {
#include "elk.h"
}

// --- Type Definitions ---
struct RamImage {
  bool used;
  uint8_t *buffer;
  size_t size;
  lv_image_dsc_t dsc;
};


// --- Global Variable DECLARATIONS (using extern) ---
extern struct js *js;
extern uint8_t *g_gifBuffer;
extern size_t g_gifSize;


// --- Function DECLARATIONS ---
void init_flash_fs();
void init_ram_images();
void init_lv_fs();
void init_mem_fs();
void init_lvgl_display();
void lvgl_loop();
bool load_gif_into_ram(const char *path);
bool load_image_file_into_ram(const char *path, RamImage *outImg);
bool load_and_execute_js_script(const char *path);
void register_js_functions();
void wifiMqttMaintainLoop();
bool doMqttConnect();
bool doWiFiReconnect();
void elk_task(void *pvParam);