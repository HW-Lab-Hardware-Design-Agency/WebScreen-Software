/**
 * @file camera_display.cpp
 * @brief LVGL-based camera display implementation
 */

#include "camera_display.h"
#include <string.h>

// LVGL objects
static lv_obj_t* camera_container = nullptr;
static lv_obj_t* camera_canvas = nullptr;
static lv_obj_t* stats_label = nullptr;

// Canvas buffer (stored globally since lv_canvas_get_buf was removed in LVGL 8.3+)
static lv_color_t* canvas_buf = nullptr;

// Display settings
#define CANVAS_WIDTH ESPNOW_CAMERA_WIDTH
#define CANVAS_HEIGHT ESPNOW_CAMERA_HEIGHT

// Statistics update interval
static uint32_t last_stats_update = 0;
#define STATS_UPDATE_INTERVAL 500

bool camera_display_init(void) {
  Serial.println("Camera Display: Initializing...");

  // Create container for camera display
  camera_container = lv_obj_create(lv_scr_act());
  if (camera_container == nullptr) {
    Serial.println("Camera Display: Failed to create container");
    return false;
  }

  lv_obj_set_size(camera_container, LV_HOR_RES, LV_VER_RES);
  lv_obj_set_pos(camera_container, 0, 0);
  lv_obj_set_style_bg_color(camera_container, lv_color_black(), 0);
  lv_obj_set_style_border_width(camera_container, 0, 0);
  lv_obj_set_style_pad_all(camera_container, 0, 0);

  // Allocate canvas buffer in PSRAM
  canvas_buf = (lv_color_t*)ps_malloc(CANVAS_WIDTH * CANVAS_HEIGHT * sizeof(lv_color_t));
  if (canvas_buf == nullptr) {
    Serial.println("Camera Display: Failed to allocate canvas buffer");
    return false;
  }

  // Clear buffer
  memset(canvas_buf, 0, CANVAS_WIDTH * CANVAS_HEIGHT * sizeof(lv_color_t));

  // Create canvas for camera frame
  camera_canvas = lv_canvas_create(camera_container);
  if (camera_canvas == nullptr) {
    Serial.println("Camera Display: Failed to create canvas");
    return false;
  }

  lv_canvas_set_buffer(camera_canvas, canvas_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);

  // Center the canvas on screen (536x240 screen, 320x240 camera)
  int16_t x_offset = (LV_HOR_RES - CANVAS_WIDTH) / 2;
  int16_t y_offset = (LV_VER_RES - CANVAS_HEIGHT) / 2;
  lv_obj_set_pos(camera_canvas, x_offset, y_offset);

  // Create statistics label (top-left overlay)
  stats_label = lv_label_create(camera_container);
  if (stats_label != nullptr) {
    lv_obj_set_pos(stats_label, 10, 10);
    lv_obj_set_style_text_color(stats_label, lv_color_white(), 0);
    lv_obj_set_style_bg_color(stats_label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(stats_label, LV_OPA_70, 0);
    lv_obj_set_style_pad_all(stats_label, 5, 0);
    lv_label_set_text(stats_label, "Waiting for camera...");
  }

  Serial.println("Camera Display: Initialized successfully");
  Serial.printf("  - Canvas size: %dx%d\n", CANVAS_WIDTH, CANVAS_HEIGHT);
  Serial.printf("  - Screen size: %dx%d\n", LV_HOR_RES, LV_VER_RES);
  Serial.printf("  - Canvas offset: (%d, %d)\n", x_offset, y_offset);

  return true;
}

void camera_display_update(void) {
  if (camera_canvas == nullptr || canvas_buf == nullptr) {
    return;
  }

  // Check for new frame
  if (espnow_camera_has_new_frame()) {
    uint16_t* frame_data = espnow_camera_get_frame();

    if (frame_data != nullptr) {
      // Copy RGB565 frame data to LVGL canvas buffer
      // LVGL uses lv_color_t which is RGB565 on ESP32
      memcpy(canvas_buf, frame_data, CANVAS_WIDTH * CANVAS_HEIGHT * sizeof(lv_color_t));

      // Invalidate canvas to trigger redraw
      lv_obj_invalidate(camera_canvas);

      espnow_camera_frame_processed();
    }
  }

  // Update statistics periodically
  uint32_t now = millis();
  if (stats_label != nullptr && (now - last_stats_update > STATS_UPDATE_INTERVAL)) {
    last_stats_update = now;

    espnow_camera_stats_t stats = espnow_camera_get_stats();

    if (stats.frames_received > 0) {
      char stats_text[128];
      snprintf(stats_text, sizeof(stats_text),
               "Frames: %lu\nFPS: %.1f\nChunks lost: %lu",
               stats.frames_received,
               stats.fps,
               stats.chunks_lost);
      lv_label_set_text(stats_label, stats_text);
    }
  }
}

void camera_display_shutdown(void) {
  Serial.println("Camera Display: Shutting down...");

  // Delete LVGL objects
  if (stats_label != nullptr) {
    lv_obj_del(stats_label);
    stats_label = nullptr;
  }

  if (camera_canvas != nullptr) {
    lv_obj_del(camera_canvas);
    camera_canvas = nullptr;
  }

  if (camera_container != nullptr) {
    lv_obj_del(camera_container);
    camera_container = nullptr;
  }

  // Free canvas buffer
  if (canvas_buf != nullptr) {
    free(canvas_buf);
    canvas_buf = nullptr;
  }

  Serial.println("Camera Display: Shutdown complete");
}

lv_obj_t* camera_display_get_object(void) {
  return camera_container;
}
