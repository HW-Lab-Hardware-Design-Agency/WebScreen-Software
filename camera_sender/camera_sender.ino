/**
 * XIAO ESP32S3 Camera Sender for WebScreen
 *
 * This sketch captures camera frames and sends them via ESP-NOW to WebScreen
 * for real-time display on the AMOLED screen.
 *
 * Hardware: Seeed Studio XIAO ESP32S3 Sense
 * Target: WebScreen with AMOLED display
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_camera.h>

// Camera model selection
#define CAMERA_MODEL_XIAO_ESP32S3

// Camera pins for XIAO ESP32S3
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13
#define LED_GPIO_NUM      21

// WebScreen receiver MAC address
// MAC Address: CC:BA:97:26:09:F0
uint8_t webscreen_mac[] = {0xCC, 0xBA, 0x97, 0x26, 0x09, 0xF0};

// Camera configuration
const int CAMERA_WIDTH = 320;
const int CAMERA_HEIGHT = 240;
const int TARGET_FPS = 10;  // Reduced for reliability
const int FRAME_DELAY = 1000 / TARGET_FPS; // milliseconds

// ESP-NOW frame structure (must match WebScreen receiver)
typedef struct {
  uint32_t frame_id;
  uint16_t chunk_index;
  uint16_t total_chunks;
  uint16_t chunk_size;
  uint8_t data[250];
} espnow_camera_chunk_t;

// Statistics
uint32_t frames_sent = 0;
uint32_t chunks_sent = 0;
uint32_t send_errors = 0;
uint32_t last_stats_time = 0;
uint32_t last_frame_time = 0;
float current_fps = 0;

// Send callback tracking
volatile bool send_complete = false;
volatile bool send_success = false;

// ESP-NOW callbacks
// Note: Using new ESP-NOW API with wifi_tx_info_t structure
void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  send_success = (status == ESP_NOW_SEND_SUCCESS);
  send_complete = true;

  if (status != ESP_NOW_SEND_SUCCESS) {
    send_errors++;
  }
}

// Initialize camera
bool init_camera() {
  Serial.println("Initializing camera...");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565; // RGB565 for direct display
  config.frame_size = FRAMESIZE_QVGA;     // 320x240
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 2; // Double buffering

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return false;
  }

  // Get camera sensor for settings adjustment
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect, ...)
    s->set_whitebal(s, 1);       // 0 = disable , 1 = enable
    s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
    s->set_wb_mode(s, 0);        // 0 to 4 - if awb_gain enabled
    s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
    s->set_aec2(s, 0);           // 0 = disable , 1 = enable
    s->set_ae_level(s, 0);       // -2 to 2
    s->set_aec_value(s, 300);    // 0 to 1200
    s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
    s->set_agc_gain(s, 0);       // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0); // 0 to 6
    s->set_bpc(s, 0);            // 0 = disable , 1 = enable
    s->set_wpc(s, 1);            // 0 = disable , 1 = enable
    s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
    s->set_lenc(s, 1);           // 0 = disable , 1 = enable
    s->set_hmirror(s, 0);        // 0 = disable , 1 = enable
    s->set_vflip(s, 0);          // 0 = disable , 1 = enable
    s->set_dcw(s, 1);            // 0 = disable , 1 = enable
    s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
  }

  Serial.println("Camera initialized successfully");
  Serial.printf("Resolution: %dx%d\n", CAMERA_WIDTH, CAMERA_HEIGHT);
  Serial.printf("Format: RGB565\n");
  Serial.printf("Target FPS: %d\n", TARGET_FPS);

  return true;
}

// Initialize ESP-NOW
bool init_espnow() {
  Serial.println("Initializing ESP-NOW...");

  // Set WiFi to station mode and channel
  WiFi.mode(WIFI_STA);
  WiFi.setChannel(1);  // Must match receiver channel

  // Wait for WiFi STA to start
  while (!WiFi.STA.started()) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  Serial.print("Camera MAC Address: ");
  Serial.println(WiFi.macAddress());

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return false;
  }

  // Register send callback
  esp_now_register_send_cb(on_data_sent);

  // Add WebScreen as peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, webscreen_mac, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return false;
  }

  Serial.print("WebScreen peer added: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", webscreen_mac[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  return true;
}

// Send frame via ESP-NOW
void send_frame(camera_fb_t *fb) {
  if (!fb || !fb->buf || fb->len == 0) {
    Serial.println("Invalid frame buffer");
    return;
  }

  uint32_t frame_id = millis();
  uint32_t total_size = fb->len;
  uint16_t total_chunks = (total_size + 249) / 250; // 250 bytes per chunk

  // Send all chunks
  for (uint16_t i = 0; i < total_chunks; i++) {
    espnow_camera_chunk_t chunk;
    chunk.frame_id = frame_id;
    chunk.chunk_index = i;
    chunk.total_chunks = total_chunks;

    uint32_t offset = i * 250;
    chunk.chunk_size = min(250, (int)(total_size - offset));

    memcpy(chunk.data, fb->buf + offset, chunk.chunk_size);

    // Wait for previous send to complete
    send_complete = false;
    send_success = false;

    // Send chunk
    esp_err_t result = esp_now_send(webscreen_mac, (uint8_t*)&chunk, sizeof(chunk));

    if (result == ESP_OK) {
      // Wait for send callback (with timeout)
      uint32_t timeout = millis() + 100;
      while (!send_complete && millis() < timeout) {
        delay(1);
      }

      if (send_success) {
        chunks_sent++;
      }
    } else {
      send_errors++;
    }
  }

  frames_sent++;

  // Calculate FPS
  uint32_t now = millis();
  if (last_frame_time > 0) {
    float delta = (now - last_frame_time) / 1000.0;
    if (delta > 0) {
      current_fps = 1.0 / delta;
    }
  }
  last_frame_time = now;
}

// Print statistics
void print_stats() {
  uint32_t now = millis();
  if (now - last_stats_time >= 2000) { // Every 2 seconds
    Serial.println("=== Statistics ===");
    Serial.printf("Frames sent: %lu\n", frames_sent);
    Serial.printf("Chunks sent: %lu\n", chunks_sent);
    Serial.printf("Send errors: %lu\n", send_errors);
    Serial.printf("Current FPS: %.1f\n", current_fps);
    Serial.printf("Uptime: %lu seconds\n", millis() / 1000);
    Serial.println();

    last_stats_time = now;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== XIAO ESP32S3 Camera Sender for WebScreen ===");
  Serial.println("Initializing...");

  // Initialize camera
  if (!init_camera()) {
    Serial.println("Camera initialization failed!");
    Serial.println("System halted.");
    while (1) {
      digitalWrite(LED_GPIO_NUM, !digitalRead(LED_GPIO_NUM));
      delay(200);
    }
  }

  // Initialize ESP-NOW
  if (!init_espnow()) {
    Serial.println("ESP-NOW initialization failed!");
    Serial.println("System halted.");
    while (1) {
      digitalWrite(LED_GPIO_NUM, !digitalRead(LED_GPIO_NUM));
      delay(500);
    }
  }

  // LED indicator
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);

  Serial.println("\n=== Setup Complete ===");
  Serial.println("Streaming camera to WebScreen...");
  Serial.println("Serial commands:");
  Serial.println("  's' - Show statistics");
  Serial.println();
}

void loop() {
  // Capture frame
  camera_fb_t *fb = esp_camera_fb_get();

  if (fb) {
    // Toggle LED during transmission
    digitalWrite(LED_GPIO_NUM, HIGH);

    // Send frame via ESP-NOW
    send_frame(fb);

    digitalWrite(LED_GPIO_NUM, LOW);

    // Return frame buffer
    esp_camera_fb_return(fb);

    // Print stats periodically
    print_stats();

    // No frame delay needed - blocking send provides natural pacing
  } else {
    Serial.println("Camera capture failed!");
    delay(1000);
  }

  // Handle serial commands
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 's' || cmd == 'S') {
      print_stats();
    }
  }
}
