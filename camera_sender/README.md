# XIAO ESP32S3 Camera Sender

Arduino sketch for Seeed Studio XIAO ESP32S3 Sense to stream camera feed to WebScreen via ESP-NOW.

## Quick Start

### 1. Get WebScreen's MAC Address
Upload WebScreen firmware and check serial monitor for MAC address.

### 2. Configure This Sketch
Edit `camera_sender.ino` line 29:
```cpp
uint8_t webscreen_mac[] = {0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF}; // Your WebScreen MAC
```

### 3. Board Settings
- **Board:** XIAO_ESP32S3
- **PSRAM:** OPI PSRAM
- **USB Mode:** Hardware CDC and JTAG
- **Upload Speed:** 921600

### 4. Upload & Run
Connect XIAO ESP32S3, select port, and upload.

## Serial Monitor Output

```
=== XIAO ESP32S3 Camera Sender for WebScreen ===
Camera initialized successfully
Resolution: 320x240
Format: RGB565
Target FPS: 15
Camera MAC Address: 12:34:56:78:9A:BC
WebScreen peer added: 24:6F:28:AB:CD:EF
Streaming camera to WebScreen...

=== Statistics ===
Frames sent: 156
Chunks sent: 95784
Send errors: 0
Current FPS: 14.8
```

## Commands
- Press `s` in serial monitor to show statistics

## Troubleshooting

### No image on WebScreen?
1. Verify MAC address matches
2. Try broadcast mode: `{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}`
3. Check both devices are powered on

### Low FPS?
Reduce `TARGET_FPS` (line 24) to 10

### Camera init failed?
1. Check camera ribbon cable
2. Press RESET button
3. Verify PSRAM enabled

## Configuration

### Frame Rate
```cpp
const int TARGET_FPS = 15;  // Line 24
```

### Chunk Delay
```cpp
delayMicroseconds(300);  // Line 213 (increase for reliability)
```

### Camera Settings
See lines 110-135 for brightness, contrast, flip, etc.

## See Also
- [Complete Setup Guide](../CAMERA_DEMO_SETUP.md)
- [ESP-NOW Camera Documentation](../ESPNOW_CAMERA.md)

## Hardware
- **Board:** Seeed Studio XIAO ESP32S3 Sense
- **Camera:** OV2640 (built-in)
- **LED:** GPIO 21 (blinks during transmission)
