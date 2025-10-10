/**
 * @file motion_detect.h
 * @brief Simple motion detection for ESP-NOW camera
 *
 * Detects motion by comparing consecutive frames using pixel differences.
 */

#pragma once

#include <Arduino.h>
#include <stdint.h>

// Motion detection configuration
#define MOTION_SAMPLE_SIZE 32       // Downsample to 32x32 for fast processing
#define MOTION_THRESHOLD 20         // Pixel difference threshold (0-255)
#define MOTION_MIN_CHANGES 50       // Minimum changed pixels to trigger motion
#define MOTION_SENSITIVITY 5        // Sensitivity level (1-10, higher = more sensitive)

// Motion detection statistics
typedef struct {
  bool motion_detected;           // Current motion state
  uint32_t changed_pixels;        // Number of changed pixels
  uint32_t motion_events;         // Total motion events detected
  uint32_t last_motion_time;      // Last time motion was detected
  uint8_t motion_level;           // Motion intensity (0-100%)
} motion_stats_t;

/**
 * @brief Initialize motion detection
 * @return true if successful
 */
bool motion_detect_init(void);

/**
 * @brief Check frame for motion
 * @param frame_data Pointer to RGB565 frame data
 * @param width Frame width
 * @param height Frame height
 * @return true if motion detected
 */
bool motion_detect_check(uint16_t* frame_data, uint16_t width, uint16_t height);

/**
 * @brief Get motion detection statistics
 * @return Motion statistics structure
 */
motion_stats_t motion_detect_get_stats(void);

/**
 * @brief Reset motion detection
 */
void motion_detect_reset(void);

/**
 * @brief Set motion sensitivity
 * @param sensitivity Sensitivity level (1-10)
 */
void motion_detect_set_sensitivity(uint8_t sensitivity);

/**
 * @brief Shutdown motion detection and free resources
 */
void motion_detect_shutdown(void);
