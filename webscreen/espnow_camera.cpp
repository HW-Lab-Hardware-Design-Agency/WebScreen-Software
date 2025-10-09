/**
 * @file espnow_camera.cpp
 * @brief ESP-NOW camera receiver implementation for WebScreen
 */

#include "espnow_camera.h"
#include <esp_wifi.h>
#include <string.h>

// Double buffering for frame reception
static uint16_t* frame_buffers[2] = {nullptr, nullptr};
static uint8_t current_buffer = 0;
static uint8_t display_buffer = 1;

// Frame reception state
static uint32_t current_frame_id = 0;
static bool* chunks_received = nullptr;
static uint16_t chunks_received_count = 0;
static uint16_t expected_total_chunks = 0;
static bool new_frame_available = false;

// Statistics
static espnow_camera_stats_t stats = {0};
static uint32_t last_frame_start_time = 0;

// Expected camera MAC (optional filtering)
static uint8_t camera_mac[6] = {0};
static bool filter_by_mac = false;

// ESP-NOW receive callback (new API)
static void espnow_recv_callback(const esp_now_recv_info* recv_info, const uint8_t* data, int len) {
  if (len != sizeof(espnow_camera_chunk_t)) {
    return; // Invalid packet size
  }

  // Filter by MAC if configured
  if (filter_by_mac && memcmp(recv_info->src_addr, camera_mac, 6) != 0) {
    return;
  }

  espnow_camera_chunk_t* chunk = (espnow_camera_chunk_t*)data;

  // New frame started
  if (chunk->frame_id != current_frame_id) {
    if (chunks_received_count > 0 && chunks_received_count < expected_total_chunks) {
      stats.chunks_lost += (expected_total_chunks - chunks_received_count);
      Serial.printf("ESP-NOW: Previous frame incomplete (%d/%d chunks)\n",
                    chunks_received_count, expected_total_chunks);
    }

    // Reset for new frame
    current_frame_id = chunk->frame_id;
    expected_total_chunks = chunk->total_chunks;
    chunks_received_count = 0;
    last_frame_start_time = millis();

    if (chunks_received != nullptr) {
      memset(chunks_received, 0, expected_total_chunks);
    }
  }

  // Validate chunk index
  if (chunk->chunk_index >= expected_total_chunks) {
    return;
  }

  // Skip duplicate chunks
  if (chunks_received != nullptr && chunks_received[chunk->chunk_index]) {
    return;
  }

  // Copy chunk data to frame buffer
  uint32_t offset = chunk->chunk_index * ESPNOW_MAX_CHUNK_SIZE;
  uint32_t copy_size = chunk->chunk_size;

  if (offset + copy_size <= ESPNOW_CAMERA_BUFFER_SIZE) {
    uint8_t* buffer_ptr = (uint8_t*)frame_buffers[current_buffer];
    memcpy(buffer_ptr + offset, chunk->data, copy_size);

    if (chunks_received != nullptr) {
      chunks_received[chunk->chunk_index] = true;
    }
    chunks_received_count++;
    stats.chunks_received++;
  }

  // Check if frame complete
  if (chunks_received_count >= expected_total_chunks) {
    // Swap buffers
    uint8_t temp = current_buffer;
    current_buffer = display_buffer;
    display_buffer = temp;

    new_frame_available = true;
    stats.frames_received++;

    uint32_t frame_time = millis() - last_frame_start_time;
    stats.last_frame_time = frame_time;

    // Calculate FPS (exponential moving average)
    if (frame_time > 0) {
      float instant_fps = 1000.0f / frame_time;
      if (stats.fps == 0) {
        stats.fps = instant_fps;
      } else {
        stats.fps = stats.fps * 0.9f + instant_fps * 0.1f;
      }
    }
  }
}

bool espnow_camera_init(const uint8_t* camera_mac_addr) {
  Serial.println("ESP-NOW Camera: Initializing...");

  // Allocate frame buffers in PSRAM
  frame_buffers[0] = (uint16_t*)ps_malloc(ESPNOW_CAMERA_BUFFER_SIZE);
  frame_buffers[1] = (uint16_t*)ps_malloc(ESPNOW_CAMERA_BUFFER_SIZE);

  if (frame_buffers[0] == nullptr || frame_buffers[1] == nullptr) {
    Serial.println("ESP-NOW Camera: Failed to allocate frame buffers");
    return false;
  }

  memset(frame_buffers[0], 0, ESPNOW_CAMERA_BUFFER_SIZE);
  memset(frame_buffers[1], 0, ESPNOW_CAMERA_BUFFER_SIZE);

  // Allocate chunk tracking buffer (max 650 chunks for 320x240 RGB565)
  chunks_received = (bool*)malloc(650);
  if (chunks_received == nullptr) {
    Serial.println("ESP-NOW Camera: Failed to allocate chunk tracking");
    return false;
  }
  memset(chunks_received, 0, 650);

  // Configure MAC filtering if provided
  if (camera_mac_addr != nullptr) {
    memcpy(camera_mac, camera_mac_addr, 6);
    filter_by_mac = true;
    Serial.printf("ESP-NOW Camera: Filtering for MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  camera_mac[0], camera_mac[1], camera_mac[2],
                  camera_mac[3], camera_mac[4], camera_mac[5]);
  }

  // Initialize WiFi in STA mode (required for ESP-NOW)
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // CRITICAL: Set WiFi channel to match camera
  WiFi.setChannel(ESPNOW_CAMERA_CHANNEL);
  Serial.printf("ESP-NOW Camera: Set WiFi channel to %d\n", ESPNOW_CAMERA_CHANNEL);

  // Wait for WiFi to be ready
  uint32_t timeout = millis() + 5000;
  while (!WiFi.STA.started() && millis() < timeout) {
    delay(10);
  }

  if (!WiFi.STA.started()) {
    Serial.println("ESP-NOW Camera: WiFi STA failed to start");
    return false;
  }

  // Print MAC address for camera configuration (after WiFi is started)
  Serial.println("\n====================================");
  Serial.print("WEBSCREEN MAC ADDRESS: ");
  Serial.println(WiFi.macAddress());
  Serial.println("====================================\n");
  Serial.println("Copy this MAC to camera_sender.ino line 40!");

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Camera: Init failed");
    return false;
  }

  // Register receive callback
  if (esp_now_register_recv_cb(espnow_recv_callback) != ESP_OK) {
    Serial.println("ESP-NOW Camera: Failed to register callback");
    esp_now_deinit();
    return false;
  }

  // Optionally add camera as peer (not strictly necessary for receiving)
  if (camera_mac_addr != nullptr) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, camera_mac_addr, 6);
    peerInfo.channel = ESPNOW_CAMERA_CHANNEL;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("ESP-NOW Camera: Warning - Failed to add peer (non-critical)");
    }
  }

  Serial.println("ESP-NOW Camera: Initialized successfully");
  Serial.printf("  - Frame size: %dx%d\n", ESPNOW_CAMERA_WIDTH, ESPNOW_CAMERA_HEIGHT);
  Serial.printf("  - Buffer size: %d bytes\n", ESPNOW_CAMERA_BUFFER_SIZE);
  Serial.printf("  - WiFi Channel: %d\n", ESPNOW_CAMERA_CHANNEL);

  return true;
}

uint16_t* espnow_camera_get_frame() {
  return frame_buffers[display_buffer];
}

bool espnow_camera_has_new_frame() {
  return new_frame_available;
}

void espnow_camera_frame_processed() {
  new_frame_available = false;
}

espnow_camera_stats_t espnow_camera_get_stats() {
  return stats;
}

void espnow_camera_shutdown() {
  Serial.println("ESP-NOW Camera: Shutting down...");

  esp_now_unregister_recv_cb();
  esp_now_deinit();

  if (frame_buffers[0] != nullptr) {
    free(frame_buffers[0]);
    frame_buffers[0] = nullptr;
  }

  if (frame_buffers[1] != nullptr) {
    free(frame_buffers[1]);
    frame_buffers[1] = nullptr;
  }

  if (chunks_received != nullptr) {
    free(chunks_received);
    chunks_received = nullptr;
  }

  Serial.println("ESP-NOW Camera: Shutdown complete");
}

uint16_t espnow_camera_get_width() {
  return ESPNOW_CAMERA_WIDTH;
}

uint16_t espnow_camera_get_height() {
  return ESPNOW_CAMERA_HEIGHT;
}
