#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>

#include "pins_config.h"
#include "fallback.h"
#include "dynamic_js.h"
#include "globals.h"
#include "webscreen_main.h"
#include "webscreen_hardware.h"
#include "webscreen_network.h"
#include "espnow_camera.h"
#include "camera_display.h"
#include "rm67162.h"
#include "tick.h"
#include <lvgl.h>

#include <ArduinoJson.h>

bool g_mqtt_enabled = false;
static bool useFallback = false;
static bool useCameraMode = false;
uint32_t g_bg_color = 0x000000;
uint32_t g_fg_color = 0xFFFFFF;

// Camera mode display resources
static lv_disp_draw_buf_t camera_draw_buf;
static lv_color_t *camera_buf = nullptr;

static void camera_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

void camera_setup() {
  LOG("Starting ESP-NOW camera mode...");

  // Initialize LVGL
  lv_init();
  start_lvgl_tick();
  LOG("LVGL initialized");

  // Initialize LED
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  // Initialize display driver
  rm67162_init();
  lcd_setRotation(1);
  LOG("Display driver initialized");

  // Allocate LVGL frame buffer
  camera_buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE);
  if (!camera_buf) {
    LOG("Failed to allocate LVGL buffer");
    return;
  }
  LOG("LVGL buffer allocated");

  // Initialize LVGL display driver
  lv_disp_draw_buf_init(&camera_draw_buf, camera_buf, nullptr, LVGL_LCD_BUF_SIZE);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 536;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = camera_disp_flush;
  disp_drv.draw_buf = &camera_draw_buf;
  lv_disp_drv_register(&disp_drv);
  LOG("LVGL display driver registered");

  // Initialize ESP-NOW camera receiver
  if (!espnow_camera_init(nullptr)) {
    LOG("ESP-NOW camera initialization failed!");
    return;
  }

  // Initialize camera display
  if (!camera_display_init()) {
    LOG("Camera display initialization failed!");
    espnow_camera_shutdown();
    return;
  }

  LOG("ESP-NOW camera mode started successfully!");
  LOG("Waiting for camera frames...");
}

void camera_loop() {
  // Update camera display with any new frames
  camera_display_update();

  // Handle LVGL tasks
  lv_timer_handler();

  delay(10);
}

void setup() {
  Serial.begin(115200);

  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, HIGH);

  // Always start in camera mode
  LOG("Starting in camera mode...");
  useCameraMode = true;
  camera_setup();
}

void loop() {
  if (useCameraMode) {
    camera_loop();
  } else if (useFallback) {
    fallback_loop();
  } else {
    dynamic_js_loop();
  }
}