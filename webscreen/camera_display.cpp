/**
 * @file camera_display.cpp
 * @brief LVGL-based camera display implementation
 */

#include "camera_display.h"
#include "motion_detect.h"
#include <string.h>

// LVGL objects
static lv_obj_t* camera_container = nullptr;
static lv_obj_t* camera_canvas = nullptr;
static lv_obj_t* info_panel = nullptr;
static lv_obj_t* title_label = nullptr;
static lv_obj_t* fps_label = nullptr;
static lv_obj_t* frames_label = nullptr;
static lv_obj_t* quality_label = nullptr;
static lv_obj_t* uptime_label = nullptr;
static lv_obj_t* status_label = nullptr;
static lv_obj_t* motion_label = nullptr;
static lv_obj_t* motion_indicator = nullptr;

// Canvas buffer (stored globally since lv_canvas_get_buf was removed in LVGL 8.3+)
static lv_color_t* canvas_buf = nullptr;

// Motion detection
static bool motion_detection_enabled = true;

// Display settings
#define CANVAS_WIDTH ESPNOW_CAMERA_WIDTH
#define CANVAS_HEIGHT ESPNOW_CAMERA_HEIGHT
#define INFO_PANEL_WIDTH 200

// Statistics update interval
static uint32_t last_stats_update = 0;
#define STATS_UPDATE_INTERVAL 200  // Faster updates for smoother FPS display

bool camera_display_init(void) {
  Serial.println("Camera Display: Initializing...");

  // Create main container
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

  // Create left info panel
  info_panel = lv_obj_create(camera_container);
  lv_obj_set_size(info_panel, INFO_PANEL_WIDTH, LV_VER_RES);
  lv_obj_set_pos(info_panel, 0, 0);
  lv_obj_set_style_bg_color(info_panel, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_style_border_width(info_panel, 0, 0);
  lv_obj_set_style_pad_all(info_panel, 10, 0);
  lv_obj_set_style_radius(info_panel, 0, 0);

  // Create info labels with styling
  static lv_style_t title_style;
  lv_style_init(&title_style);
  lv_style_set_text_color(&title_style, lv_color_hex(0x00ff00));
  lv_style_set_text_font(&title_style, &lv_font_montserrat_20);

  static lv_style_t label_style;
  lv_style_init(&label_style);
  lv_style_set_text_color(&label_style, lv_color_white());
  lv_style_set_text_font(&label_style, &lv_font_montserrat_14);

  // Title
  title_label = lv_label_create(info_panel);
  lv_obj_add_style(title_label, &title_style, 0);
  lv_label_set_text(title_label, "ESP-NOW\nCAMERA");
  lv_obj_set_pos(title_label, 0, 5);

  // FPS
  fps_label = lv_label_create(info_panel);
  lv_obj_add_style(fps_label, &label_style, 0);
  lv_label_set_text(fps_label, "FPS: --");
  lv_obj_set_pos(fps_label, 0, 60);

  // Frames count
  frames_label = lv_label_create(info_panel);
  lv_obj_add_style(frames_label, &label_style, 0);
  lv_label_set_text(frames_label, "Frames: 0");
  lv_obj_set_pos(frames_label, 0, 85);

  // Quality/Loss
  quality_label = lv_label_create(info_panel);
  lv_obj_add_style(quality_label, &label_style, 0);
  lv_label_set_text(quality_label, "Loss: 0");
  lv_obj_set_pos(quality_label, 0, 110);

  // Uptime
  uptime_label = lv_label_create(info_panel);
  lv_obj_add_style(uptime_label, &label_style, 0);
  lv_label_set_text(uptime_label, "Up: 0s");
  lv_obj_set_pos(uptime_label, 0, 135);

  // Status
  status_label = lv_label_create(info_panel);
  lv_obj_add_style(status_label, &label_style, 0);
  lv_label_set_text(status_label, "Waiting...");
  lv_obj_set_pos(status_label, 0, 165);
  lv_obj_set_width(status_label, INFO_PANEL_WIDTH - 20);
  lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);

  // Motion detection label
  motion_label = lv_label_create(info_panel);
  lv_obj_add_style(motion_label, &label_style, 0);
  lv_label_set_text(motion_label, "Motion: --");
  lv_obj_set_pos(motion_label, 0, 200);

  // Motion indicator (visual alert)
  motion_indicator = lv_obj_create(info_panel);
  lv_obj_set_size(motion_indicator, 15, 15);
  lv_obj_set_pos(motion_indicator, INFO_PANEL_WIDTH - 25, 200);
  lv_obj_set_style_radius(motion_indicator, 8, 0);
  lv_obj_set_style_bg_color(motion_indicator, lv_color_hex(0x555555), 0); // Gray when inactive
  lv_obj_set_style_border_width(motion_indicator, 0, 0);

  // Allocate canvas buffer in PSRAM
  canvas_buf = (lv_color_t*)ps_malloc(CANVAS_WIDTH * CANVAS_HEIGHT * sizeof(lv_color_t));
  if (canvas_buf == nullptr) {
    Serial.println("Camera Display: Failed to allocate canvas buffer");
    return false;
  }
  memset(canvas_buf, 0, CANVAS_WIDTH * CANVAS_HEIGHT * sizeof(lv_color_t));

  // Create canvas for camera frame (positioned on the right)
  camera_canvas = lv_canvas_create(camera_container);
  if (camera_canvas == nullptr) {
    Serial.println("Camera Display: Failed to create canvas");
    return false;
  }

  lv_canvas_set_buffer(camera_canvas, canvas_buf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);

  // Position camera on the right side
  int16_t x_offset = LV_HOR_RES - CANVAS_WIDTH;  // Right align
  int16_t y_offset = 0;  // Top align
  lv_obj_set_pos(camera_canvas, x_offset, y_offset);

  // Initialize motion detection
  if (motion_detection_enabled) {
    if (!motion_detect_init()) {
      Serial.println("Camera Display: Motion detection init failed (non-critical)");
      motion_detection_enabled = false;
    }
  }

  Serial.println("Camera Display: Initialized successfully");
  Serial.printf("  - Canvas size: %dx%d\n", CANVAS_WIDTH, CANVAS_HEIGHT);
  Serial.printf("  - Screen size: %dx%d\n", LV_HOR_RES, LV_VER_RES);
  Serial.printf("  - Info panel: %dx%d\n", INFO_PANEL_WIDTH, LV_VER_RES);
  Serial.printf("  - Canvas position: (%d, %d)\n", x_offset, y_offset);
  Serial.printf("  - Motion detection: %s\n", motion_detection_enabled ? "enabled" : "disabled");

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
      // Check for motion if enabled
      if (motion_detection_enabled) {
        motion_detect_check(frame_data, CANVAS_WIDTH, CANVAS_HEIGHT);
      }

      // Copy RGB565 frame data to LVGL canvas buffer
      // LVGL uses lv_color_t which is RGB565 on ESP32
      memcpy(canvas_buf, frame_data, CANVAS_WIDTH * CANVAS_HEIGHT * sizeof(lv_color_t));

      // Invalidate canvas to trigger redraw
      lv_obj_invalidate(camera_canvas);

      espnow_camera_frame_processed();
    }
  }

  // Update info labels periodically
  uint32_t now = millis();
  if (now - last_stats_update > STATS_UPDATE_INTERVAL) {
    last_stats_update = now;

    espnow_camera_stats_t stats = espnow_camera_get_stats();

    // Update FPS
    if (fps_label != nullptr) {
      char fps_text[32];
      snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", stats.fps);
      lv_label_set_text(fps_label, fps_text);
    }

    // Update frame count
    if (frames_label != nullptr) {
      char frames_text[32];
      snprintf(frames_text, sizeof(frames_text), "Frames: %lu", stats.frames_received);
      lv_label_set_text(frames_label, frames_text);
    }

    // Update quality/loss info
    if (quality_label != nullptr) {
      char quality_text[32];
      if (stats.chunks_received > 0) {
        float loss_percent = (stats.chunks_lost * 100.0f) / (stats.chunks_received + stats.chunks_lost);
        snprintf(quality_text, sizeof(quality_text), "Loss: %.1f%%", loss_percent);
      } else {
        snprintf(quality_text, sizeof(quality_text), "Loss: 0%%");
      }
      lv_label_set_text(quality_label, quality_text);
    }

    // Update uptime
    if (uptime_label != nullptr) {
      char uptime_text[32];
      uint32_t seconds = now / 1000;
      if (seconds < 60) {
        snprintf(uptime_text, sizeof(uptime_text), "Up: %lus", seconds);
      } else {
        snprintf(uptime_text, sizeof(uptime_text), "Up: %lum %lus", seconds / 60, seconds % 60);
      }
      lv_label_set_text(uptime_label, uptime_text);
    }

    // Update status
    if (status_label != nullptr) {
      if (stats.frames_received == 0) {
        lv_label_set_text(status_label, "Waiting for\ncamera...");
      } else if (stats.fps > 15) {
        lv_label_set_text(status_label, "Streaming\nGOOD");
      } else if (stats.fps > 8) {
        lv_label_set_text(status_label, "Streaming\nFAIR");
      } else {
        lv_label_set_text(status_label, "Streaming\nSLOW");
      }
    }

    // Update motion detection info
    if (motion_detection_enabled && motion_label != nullptr && motion_indicator != nullptr) {
      motion_stats_t motion_stats = motion_detect_get_stats();

      // Update motion label
      if (motion_stats.motion_events == 0) {
        lv_label_set_text(motion_label, "Motion: --");
      } else {
        char motion_text[32];
        snprintf(motion_text, sizeof(motion_text), "Motion: %lu", motion_stats.motion_events);
        lv_label_set_text(motion_label, motion_text);
      }

      // Update motion indicator color
      if (motion_stats.motion_detected) {
        // Red when motion detected
        lv_obj_set_style_bg_color(motion_indicator, lv_color_hex(0xFF0000), 0);
      } else {
        // Gray when no motion
        lv_obj_set_style_bg_color(motion_indicator, lv_color_hex(0x555555), 0);
      }
    }
  }
}

void camera_display_shutdown(void) {
  Serial.println("Camera Display: Shutting down...");

  // Shutdown motion detection
  if (motion_detection_enabled) {
    motion_detect_shutdown();
  }

  // Delete LVGL objects (child objects deleted with parent)
  if (camera_container != nullptr) {
    lv_obj_del(camera_container);
    camera_container = nullptr;
  }

  // Nullify all pointers
  info_panel = nullptr;
  title_label = nullptr;
  fps_label = nullptr;
  frames_label = nullptr;
  quality_label = nullptr;
  uptime_label = nullptr;
  status_label = nullptr;
  motion_label = nullptr;
  motion_indicator = nullptr;
  camera_canvas = nullptr;

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

void camera_display_set_motion_detection(bool enable) {
  motion_detection_enabled = enable;

  if (enable && motion_label != nullptr) {
    if (!motion_detect_init()) {
      Serial.println("Camera Display: Failed to enable motion detection");
      motion_detection_enabled = false;
    }
  } else if (!enable) {
    motion_detect_shutdown();
  }

  Serial.printf("Camera Display: Motion detection %s\n",
                motion_detection_enabled ? "enabled" : "disabled");
}
