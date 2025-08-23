# WebScreen Arduino Sketch

This is the main WebScreen firmware designed for Arduino IDE. The project follows Arduino best practices and is optimized for the custom WebScreen PCB with ESP32-S3.

## 📁 Project Structure

```
webscreen/
├── webscreen.ino              # Main Arduino sketch file
├── webscreen_config.h         # Configuration constants
├── webscreen_main.h/.cpp      # Main application logic
├── webscreen_hardware.h/.cpp  # Hardware abstraction layer
├── webscreen_runtime.h/.cpp   # JavaScript and fallback runtime
├── webscreen_network.h/.cpp   # Network connectivity
└── README.md                  # This file
```

## 🚀 Getting Started

### 1. Hardware Requirements

- **WebScreen Custom PCB** with ESP32-S3 and PSRAM
- **RM67162 AMOLED Display** (536x240 pixels) - integrated on PCB
- **SD Card slot** - integrated on PCB
- **Power button** - integrated on PCB (GPIO 33)
- **Status LED** - integrated on PCB (GPIO 38)

### 2. Arduino IDE Setup

1. **Install ESP32 Support**:
   ```
   File → Preferences → Additional Board Manager URLs
   Add: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

2. **Install ESP32 Boards**:
   ```
   Tools → Board Manager → Search "ESP32" → Install v2.0.3+
   ```

3. **Install Required Libraries**:
   - ArduinoJson (by Benoit Blanchon)
   - LVGL (by kisvegabor) 
   - PubSubClient (by Nick O'Leary) - for MQTT

4. **Board Configuration**:
   - **Board**: ESP32S3 Dev Module
   - **CPU Frequency**: 240MHz
   - **Flash Size**: 16MB (adjust to your board)
   - **PSRAM**: OPI PSRAM
   - **USB CDC On Boot**: Enabled
   - **Upload Speed**: 921600

### 3. SD Card Setup

Create these files on your SD card:

#### `webscreen.json` (Configuration):
```json
{
  "wifi": {
    "ssid": "YourNetworkName",
    "password": "YourPassword",
    "enabled": true
  },
  "display": {
    "brightness": 200,
    "rotation": 1,
    "background_color": "#000080",
    "foreground_color": "#00FFFF"
  },
  "script_file": "/app.js"
}
```

#### `app.js` (Sample JavaScript):
```javascript
// Simple WebScreen JavaScript application
print("Hello from WebScreen!");

// Create a simple UI
let label = create_label("WebScreen v2.0\nJavaScript Mode");
style_set_text_color(label, 0x00FFFF);
obj_align(label, 0, 0, 0); // Center

// Handle button events
setInterval(() => {
    let status = wifi_status();
    if (status === 3) { // Connected
        label_set_text(label, "WiFi Connected\n" + wifi_get_ip());
    }
}, 5000);
```

## 🔧 Configuration Options

### webscreen_config.h

Key configuration constants you can modify:

```cpp
// Display settings
#define WEBSCREEN_DISPLAY_WIDTH 536
#define WEBSCREEN_DISPLAY_HEIGHT 240
#define WEBSCREEN_DISPLAY_ROTATION 1

// Memory settings
#define WEBSCREEN_JS_HEAP_SIZE_KB 512
#define WEBSCREEN_DISPLAY_BUFFER_SIZE (536 * 240)

// Network timeouts
#define WEBSCREEN_WIFI_CONNECTION_TIMEOUT_MS 15000
#define WEBSCREEN_HTTP_TIMEOUT_MS 10000

// Feature enables (set to 0 to disable)
#define WEBSCREEN_ENABLE_WIFI 1
#define WEBSCREEN_ENABLE_MQTT 1
#define WEBSCREEN_ENABLE_JAVASCRIPT 1
```

### Build Configurations

Add these defines before including headers for different builds:

```cpp
// Debug build
#define WEBSCREEN_DEBUG 1

// Performance build  
#define WEBSCREEN_PERFORMANCE 1

// Memory debugging
#define WEBSCREEN_MEMORY_DEBUG 1
```

## 🖥️ Serial Commands

When connected via Serial Monitor (115200 baud), you can use these commands:

- `status` - Show system status
- `memory` - Show memory usage
- `restart` - Restart system
- `help` - Show available commands

## 📊 System Monitoring

The firmware provides comprehensive monitoring:

```
=== SYSTEM STATUS ===
Uptime: 12345 ms
Free Heap: 245760 bytes
Free PSRAM: 8123456 bytes
WiFi: Connected (192.168.1.100)
SD Card: Mounted (15.9 GB)
====================
```

## 🐛 Troubleshooting

### Common Issues

1. **Compilation Errors**:
   - Ensure all required libraries are installed
   - Check ESP32 board package version (≥2.0.3)
   - Verify board settings match your hardware

2. **Upload Issues**:
   - Hold BOOT button while connecting USB if port not detected
   - Check USB cable and driver installation
   - Try lower upload speed (460800 or 115200)

3. **SD Card Not Detected**:
   - Format SD card as FAT32
   - Check SD card pin connections
   - Try different SD card (Class 10 recommended)

4. **WiFi Connection Failed**:
   - Verify SSID/password in webscreen.json
   - Check WiFi signal strength
   - Ensure 2.4GHz network (ESP32 doesn't support 5GHz)

5. **Display Issues**:
   - Check display pin connections
   - Verify PSRAM is enabled in board settings
   - Try reducing display buffer size if memory issues

### Debug Output

Enable debug output by adding to the top of webscreen.ino:

```cpp
#define WEBSCREEN_DEBUG 1
```

This will show detailed initialization and runtime information.

## 🔧 Customization

### Adding Custom Hardware

1. **Modify Pin Definitions** in `webscreen_config.h`:
   ```cpp
   #define WEBSCREEN_PIN_CUSTOM_SENSOR 12
   ```

2. **Add Hardware Functions** in `webscreen_hardware.cpp`:
   ```cpp
   int webscreen_hardware_read_sensor() {
       return analogRead(WEBSCREEN_PIN_CUSTOM_SENSOR);
   }
   ```

3. **Expose to JavaScript** (in full implementation):
   ```cpp
   // Add to JavaScript API bindings
   js_add_function("readSensor", webscreen_hardware_read_sensor);
   ```

### Custom Network Protocols

Add custom network functionality in `webscreen_network.cpp`:

```cpp
bool webscreen_custom_protocol_send(const char* data) {
    // Your custom protocol implementation
    return true;
}
```

## 📚 Arduino Library Usage

WebScreen can also be used as an Arduino library. See the `WebScreenLib/` directory for:

- Library structure with `library.properties`
- Example sketches demonstrating features
- C++ class interface for easier integration

### Using as Library

```cpp
#include <WebScreen.h>

WebScreen webscreen;

void setup() {
    webscreen.setWiFi("SSID", "Password");
    webscreen.begin();
}

void loop() {
    webscreen.loop();
}
```

## 🔗 Related Files

- [../WebScreenLib/](../WebScreenLib/) - Arduino library version
- [../docs/API.md](../docs/API.md) - JavaScript API reference
- [../CONTRIBUTING.md](../CONTRIBUTING.md) - Development guidelines
- [../MIGRATION.md](../MIGRATION.md) - Upgrading from v1.x

## 📄 License

This project is open source under the MIT License. See [LICENSE](../LICENSE) for details.