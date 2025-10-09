/**
 * @file espnow_camera.h
 * @brief ESP-NOW camera receiver for WebScreen
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// Camera frame configuration (matches camera_sender)
#define ESPNOW_CAMERA_WIDTH 320
#define ESPNOW_CAMERA_HEIGHT 240
#define ESPNOW_CAMERA_BUFFER_SIZE (ESPNOW_CAMERA_WIDTH * ESPNOW_CAMERA_HEIGHT * 2) // RGB565
#define ESPNOW_MAX_CHUNK_SIZE 250
#define ESPNOW_CAMERA_CHANNEL 1

// Frame chunk structure (must match camera_sender)
typedef struct {
  uint32_t frame_id;
  uint16_t chunk_index;
  uint16_t total_chunks;
  uint16_t chunk_size;
  uint8_t data[ESPNOW_MAX_CHUNK_SIZE];
} espnow_camera_chunk_t;

// Statistics
typedef struct {
  uint32_t frames_received;
  uint32_t chunks_received;
  uint32_t chunks_lost;
  uint32_t last_frame_time;
  float fps;
} espnow_camera_stats_t;

// Initialize ESP-NOW camera receiver
bool espnow_camera_init(const uint8_t* camera_mac = nullptr);

// Get pointer to current frame buffer (RGB565 format)
uint16_t* espnow_camera_get_frame();

// Check if new frame available
bool espnow_camera_has_new_frame();

// Mark frame as processed
void espnow_camera_frame_processed();

// Get statistics
espnow_camera_stats_t espnow_camera_get_stats();

// Shutdown
void espnow_camera_shutdown();

// Get dimensions
uint16_t espnow_camera_get_width();
uint16_t espnow_camera_get_height();
