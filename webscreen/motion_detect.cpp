/**
 * @file motion_detect.cpp
 * @brief Simple motion detection implementation
 *
 * Uses pixel difference comparison on downsampled frames for fast processing.
 */

#include "motion_detect.h"
#include <string.h>
#include <stdlib.h>

// Motion detection state
static uint8_t* prev_frame = nullptr;
static uint8_t* curr_frame = nullptr;
static motion_stats_t stats = {0};
static uint8_t sensitivity = MOTION_SENSITIVITY;
static bool initialized = false;

// Convert RGB565 to grayscale (Y = 0.299R + 0.587G + 0.114B)
static inline uint8_t rgb565_to_gray(uint16_t rgb565) {
  uint8_t r = (rgb565 >> 11) & 0x1F;  // 5 bits
  uint8_t g = (rgb565 >> 5) & 0x3F;   // 6 bits
  uint8_t b = rgb565 & 0x1F;          // 5 bits

  // Scale to 8-bit
  r = (r * 255) / 31;
  g = (g * 255) / 63;
  b = (b * 255) / 31;

  // Convert to grayscale
  return (uint8_t)((299 * r + 587 * g + 114 * b) / 1000);
}

// Downsample frame from source to MOTION_SAMPLE_SIZE x MOTION_SAMPLE_SIZE
static void downsample_frame(uint16_t* src, uint16_t src_width, uint16_t src_height,
                             uint8_t* dst, uint16_t dst_size) {
  float x_ratio = (float)src_width / (float)dst_size;
  float y_ratio = (float)src_height / (float)dst_size;

  for (int y = 0; y < dst_size; y++) {
    for (int x = 0; x < dst_size; x++) {
      int src_x = (int)(x * x_ratio);
      int src_y = (int)(y * y_ratio);
      int src_idx = src_y * src_width + src_x;

      uint16_t pixel = src[src_idx];
      dst[y * dst_size + x] = rgb565_to_gray(pixel);
    }
  }
}

bool motion_detect_init(void) {
  if (initialized) {
    return true;
  }

  Serial.println("Motion Detection: Initializing...");

  // Allocate buffers for downsampled frames
  size_t buffer_size = MOTION_SAMPLE_SIZE * MOTION_SAMPLE_SIZE;
  prev_frame = (uint8_t*)malloc(buffer_size);
  curr_frame = (uint8_t*)malloc(buffer_size);

  if (prev_frame == nullptr || curr_frame == nullptr) {
    Serial.println("Motion Detection: Failed to allocate buffers");
    return false;
  }

  // Initialize to zero
  memset(prev_frame, 0, buffer_size);
  memset(curr_frame, 0, buffer_size);
  memset(&stats, 0, sizeof(stats));

  initialized = true;
  Serial.printf("Motion Detection: Initialized with %dx%d sampling\n",
                MOTION_SAMPLE_SIZE, MOTION_SAMPLE_SIZE);
  return true;
}

bool motion_detect_check(uint16_t* frame_data, uint16_t width, uint16_t height) {
  if (!initialized || frame_data == nullptr) {
    return false;
  }

  // Downsample current frame
  downsample_frame(frame_data, width, height, curr_frame, MOTION_SAMPLE_SIZE);

  // Compare with previous frame
  uint32_t changed_pixels = 0;
  uint32_t total_diff = 0;
  size_t pixel_count = MOTION_SAMPLE_SIZE * MOTION_SAMPLE_SIZE;

  // Calculate motion threshold based on sensitivity (1-10)
  uint8_t threshold = MOTION_THRESHOLD - (sensitivity * 2);
  uint32_t min_changes = MOTION_MIN_CHANGES - (sensitivity * 5);

  for (size_t i = 0; i < pixel_count; i++) {
    int diff = abs((int)curr_frame[i] - (int)prev_frame[i]);

    if (diff > threshold) {
      changed_pixels++;
      total_diff += diff;
    }
  }

  // Update statistics
  stats.changed_pixels = changed_pixels;

  // Calculate motion level percentage
  if (changed_pixels > 0) {
    uint32_t avg_diff = total_diff / changed_pixels;
    stats.motion_level = (uint8_t)((avg_diff * 100) / 255);
    if (stats.motion_level > 100) stats.motion_level = 100;
  } else {
    stats.motion_level = 0;
  }

  // Detect motion
  bool motion_now = (changed_pixels > min_changes);

  if (motion_now && !stats.motion_detected) {
    // Motion started
    stats.motion_detected = true;
    stats.motion_events++;
    stats.last_motion_time = millis();
    Serial.printf("Motion Detection: MOTION STARTED (changed: %lu, level: %d%%)\n",
                  changed_pixels, stats.motion_level);
  } else if (!motion_now && stats.motion_detected) {
    // Motion stopped
    stats.motion_detected = false;
    Serial.println("Motion Detection: Motion stopped");
  }

  // Copy current frame to previous for next comparison
  memcpy(prev_frame, curr_frame, pixel_count);

  return stats.motion_detected;
}

motion_stats_t motion_detect_get_stats(void) {
  return stats;
}

void motion_detect_reset(void) {
  if (initialized) {
    size_t buffer_size = MOTION_SAMPLE_SIZE * MOTION_SAMPLE_SIZE;
    memset(prev_frame, 0, buffer_size);
    memset(curr_frame, 0, buffer_size);
  }

  stats.motion_detected = false;
  stats.changed_pixels = 0;
  stats.motion_level = 0;
  // Keep motion_events and last_motion_time
}

void motion_detect_set_sensitivity(uint8_t new_sensitivity) {
  if (new_sensitivity >= 1 && new_sensitivity <= 10) {
    sensitivity = new_sensitivity;
    Serial.printf("Motion Detection: Sensitivity set to %d\n", sensitivity);
  }
}

void motion_detect_shutdown(void) {
  if (prev_frame != nullptr) {
    free(prev_frame);
    prev_frame = nullptr;
  }

  if (curr_frame != nullptr) {
    free(curr_frame);
    curr_frame = nullptr;
  }

  initialized = false;
  Serial.println("Motion Detection: Shutdown complete");
}
