/**
 * @file camera_display.h
 * @brief LVGL-based camera display for WebScreen
 */

#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include "espnow_camera.h"

/**
 * @brief Initialize camera display
 * @return true if successful, false otherwise
 */
bool camera_display_init(void);

/**
 * @brief Update camera display with new frame
 * Should be called regularly in loop when new frames are available
 */
void camera_display_update(void);

/**
 * @brief Shutdown camera display
 */
void camera_display_shutdown(void);

/**
 * @brief Get camera display object
 * @return LVGL object containing camera display
 */
lv_obj_t* camera_display_get_object(void);
