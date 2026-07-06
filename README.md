[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT) ![Issues](https://img.shields.io/github/issues/HW-Lab-Hardware-Design-Agency/WebScreen-Software) [![image](https://img.shields.io/badge/website-WebScreen.cc-D31027)](https://webscreen.cc) [![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/HW-Lab-Hardware-Design-Agency/WebScreen-Software) [![image](https://img.shields.io/badge/view_on-CrowdSupply-099)](https://www.crowdsupply.com/hw-media-lab/webscreen)

# WebScreen Software

![til](./docs/WebScreen_Notification.gif)

WebScreen is a hackable, open-source gadget for gamers, makers, and creators! Get the notifications you want, build custom JavaScript apps, and stay in the zone—no distractions. Powered by ESP32-S3 with an AMOLED screen, fully open hardware and software.

## Core Features

### Runtime Environment
- **JavaScript Engine**: Elk JavaScript runtime with comprehensive API bindings
- **Graphics Library**: LVGL integration with RM67162 AMOLED display support
- **Storage Management**: Robust SD card handling with multiple filesystem drivers
- **Fallback System**: Built-in notification app with scrolling text and GIF animation

### Hardware Integration
- **ESP32-S3 Platform**: Full PSRAM support and optimized memory allocation
- **RM67162 Display**: 536x240 AMOLED with QSPI interface and brightness control
- **Power Management**: Smart power button handling on GPIO 33
- **Storage Interface**: SD_MMC card support with robust initialization

### Networking & Connectivity  
- **WiFi Management**: Connection handling with timeout and status monitoring
- **Secure HTTPS**: Certificate chain validation with SD card certificate storage
- **MQTT Integration**: Client support with publish/subscribe functionality
- **BLE Support**: Bluetooth Low Energy stack integration

### Development Features
- **Modular Architecture**: Separated concerns across hardware, network, and runtime modules
- **Serial Commands**: Interactive development console with comprehensive command system
- **Screen Capture**: `/screenshot` streams the live display as base64 RGB565 over serial — no camera needed
- **Configuration System**: JSON-based configuration with comprehensive validation
- **Error Handling**: Robust error reporting and recovery mechanisms
- **Debug Support**: Serial logging and development utilities

## Quick Start

### Prerequisites

- **Hardware**: WebScreen PCB
- **Storage**: microSD card (formatted as FAT32)
- **Cable**: USB-C for serial communication and power
- **Software** (for compilation): Arduino IDE 2.0+

### Installation

#### Option 1: Web Flasher (Recommended for beginners)

For users who don't want to set up Arduino IDE and compile from source:

1. **Visit the Web Flasher**
   Navigate to: https://flash.webscreen.cc/

2. **Connect WebScreen**
   Connect your WebScreen device via USB-C cable

3. **Flash Firmware**
   Select the latest firmware version and click "Flash"

4. **Setup SD Card**
   Create your `webscreen.json` configuration file and JavaScript app on SD card

This is the easiest way to get started with WebScreen without any development setup.

#### Option 2: Arduino IDE (For developers)

1. **Install ESP32 Support**
   ```
   File → Preferences → Additional Board Manager URLs
   Add: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```

2. **Install ESP32 Boards**
   ```
   Tools → Board Manager → Search "ESP32" → Install v2.0.3+
   ```

3. **Install Required Libraries**
   ```
   Library Manager → Install:
   - ArduinoJson (by Benoit Blanchon) - v6.x or later
   - LVGL (by kisvegabor) - v9.5.x
   - PubSubClient (by Nick O'Leary) - v2.8 or later
   ```

4. **Configure LVGL**
   Copy the provided `lv_conf.h` file to your Arduino libraries folder:
   ```
   cp WebScreen-Software/lv_conf.h ~/Arduino/libraries/
   ```

   Key LVGL settings configured for WebScreen:
   - Color depth: 16-bit (RGB565), display runs in byte-swapped mode for the QSPI panel
   - stdlib malloc/free for LVGL allocations
   - Display refresh: 30ms for stability
   - **Enabled fonts**: Montserrat 14, 20, 28, 34, 40, 44, 48
   - **Image formats**: PNG, GIF, JPG (BMP disabled)
   - **Widgets**: Label, Image, Arc, Line, Button, Bar, Chart, Scale, Span
   - **Layouts**: Flexbox and Grid enabled

   Note: this branch requires LVGL 9.5. The `lv_conf.h` in the repo is in the
   v9 format and will not work with LVGL 8. If you were building an older
   branch before, replace the LVGL library and copy the new `lv_conf.h` again.
   - Complex drawing features enabled (shadows, gradients, etc.)

5. **Open WebScreen Sketch**
   ```
   File → Open → WebScreen-Software/webscreen/webscreen.ino
   ```

6. **Board Configuration**
   - **Board**: ESP32S3 Dev Module  
   - **CPU Frequency**: 240MHz
   - **Flash Size**: 16MB (or your board's flash size)
   - **PSRAM**: OPI PSRAM  
   - **USB CDC On Boot**: Enabled
   - **Upload Speed**: 921600

   ![Board Settings](docs/arduino_tools_settings.png)

#### Option 3: Direct Compilation

For advanced users who want to modify the source code, you can compile directly from the Arduino IDE following the steps above.

### Hardware Setup

#### Upload Mode (if USB not detected)
1. Power off device
2. Hold **BOOT** button (behind RST button)  
3. Connect USB-C cable
4. Hold **BOOT**, press **RESET**, release **BOOT**
5. Upload firmware
6. Press **RESET** to run

#### Power Button (GPIO 33)
- **Short Press** (< 3s): Toggle display on/off (device keeps running)
- **Long Press** (>= 3s): Power off via latch circuit (drives OUTPUT_PIN 1 LOW to cut power, falls back to deep sleep if latch is not present)
- **Pin**: GPIO 33 (INPUT_PULLUP, active LOW)
- **Debounce**: 50ms
- **Power-on**: Press the button to supply momentary power; firmware latches power by driving OUTPUT_PIN HIGH in `setup()`

> **Note**: For the power-on button press to work after a latch power-off, the hardware circuit must wire the button in parallel with the latch MOSFET so it can momentarily supply power independently of the GPIO latch output. If your circuit doesn't have this, the device will enter deep sleep instead and wake on the next button press.

## Configuration

WebScreen uses a JSON configuration file stored on the SD card as `/webscreen.json`. This file controls WiFi settings, display colors, MQTT configuration, and which JavaScript app to run.

### Configuration Format

**IMPORTANT:** The current firmware uses the following format with nested "settings" structure:

```json
{
  "settings": {
    "wifi": {
      "ssid": "your_wifi_network",
      "pass": "your_wifi_password"
    },
    "mqtt": {
      "enabled": false
    }
  },
  "screen": {
    "background": "#2980b9",
    "foreground": "#00fff1"
  },
  "display": {
    "brightness": 200
  },
  "script": "app.js"
}
```

### Configuration Fields

| Section | Field | Description | Default |
|---------|-------|-------------|---------|
| **settings.wifi** | `ssid` | WiFi network name | `""` |
| | `pass` | WiFi password | `""` |
| **settings.mqtt** | `enabled` | Enable MQTT functionality | `false` |
| **screen** | `background` | Background color (hex format) | `"#000000"` |
| | `foreground` | Text/foreground color (hex) | `"#FFFFFF"` |
| **display** | `brightness` | Display brightness (0-255) | `200` |
| **Root** | `script` | JavaScript file to execute | `"app.js"` |

### Example Configurations

#### Basic WiFi Setup
```json
{
  "settings": {
    "wifi": {
      "ssid": "MyNetwork",
      "pass": "MyPassword"
    }
  },
  "script": "app.js"
}
```

#### Custom Colors
```json
{
  "settings": {
    "wifi": {
      "ssid": "MyNetwork",
      "pass": "MyPassword"
    }
  },
  "screen": {
    "background": "#1a1a2e",
    "foreground": "#eee"
  },
  "script": "weather.js"
}
```

#### With MQTT Enabled
```json
{
  "settings": {
    "wifi": {
      "ssid": "MyNetwork",
      "pass": "MyPassword"
    },
    "mqtt": {
      "enabled": true
    }
  },
  "script": "mqtt_dashboard.js"
}
```

#### Minimal Configuration (WiFi Only)
```json
{
  "settings": {
    "wifi": {
      "ssid": "MyNetwork",
      "pass": "MyPassword"
    }
  }
}
```

**Note:** WebScreen will start in fallback mode (displaying the notification screen) if:
- No `/webscreen.json` configuration file is found on the SD card
- The JavaScript file specified in the `script` field doesn't exist on the SD card
- The SD card cannot be mounted

**Important:** If WiFi configuration is missing or WiFi connection fails, WebScreen will still execute the JavaScript application (if the script file exists). This allows offline applications to run without network connectivity.

## Architecture & Building

### System Architecture

WebScreen features a modular architecture with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                        │
│  ┌─────────────────┐    ┌─────────────────┐                │
│  │  JavaScript     │    │   Fallback      │                │
│  │   Runtime       │    │     App         │                │
│  └─────────────────┘    └─────────────────┘                │
├─────────────────────────────────────────────────────────────┤
│                      Runtime Management                    │
│  ┌─────────────────────────────────────────────────────────┐│
│  │           webscreen_runtime.cpp/.h                     ││
│  │  • LVGL initialization and management                   ││
│  │  • Elk JavaScript engine integration                   ││
│  │  • Task management and execution                       ││
│  │  • Memory filesystem drivers                           ││
│  └─────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│                    Core Application                        │
│  ┌─────────────────────────────────────────────────────────┐│
│  │             webscreen_main.cpp/.h                      ││
│  │  • Configuration loading and management                ││
│  │  • Application state management                        ││
│  │  • Main setup and loop coordination                    ││
│  └─────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│                  Hardware Abstraction                      │
│  ┌─────────────────────────────────────────────────────────┐│
│  │           webscreen_hardware.cpp/.h                    ││
│  │  • SD card initialization with retry logic             ││
│  │  • Power button handling                               ││
│  │  • Display management                                  ││
│  │  • Pin configuration and GPIO control                  ││
│  └─────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│                     Network Layer                          │
│  ┌─────────────────────────────────────────────────────────┐│
│  │            webscreen_network.cpp/.h                    ││
│  │  • WiFi connection with timeout handling               ││
│  │  • HTTPS client with certificate validation            ││
│  │  • MQTT client integration                             ││
│  │  • BLE stack management                                ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

### Build Process

#### Arduino IDE Build
```
1. Open Arduino IDE
2. File → Open → webscreen/webscreen.ino  
3. Select ESP32S3 Dev Module board
4. Configure board settings (see installation guide)
5. Click Upload button
```

#### LVGL Configuration
WebScreen includes a custom `lv_conf.h` file (LVGL 9.5 format) optimized for ESP32-S3 with AMOLED display:

**Display Settings:**
- **Color Format**: 16-bit RGB565, byte-swapped at the display level (`LV_COLOR_FORMAT_RGB565_SWAPPED`)
- **Resolution**: 536x240 pixels
- **DPI**: 130 for optimal widget sizing
- **Refresh Rate**: 30ms for stable display output

**Available Fonts (Montserrat):**
| Size | Usage |
|------|-------|
| 14 | Default, small text |
| 20 | Body text |
| 28 | Subheadings |
| 34 | Medium headings |
| 40 | Large headings |
| 44 | Extra large |
| 48 | Display text |

**Note:** Other font sizes (8, 10, 12, 16, 18, 22, 24, etc.) are NOT available.

**Enabled Widgets:**
- **Core**: Label, Image, Arc, Line, Button, Button Matrix, Canvas
- **Extra**: Chart, Meter, Message Box, Span (rich text)
- **Layouts**: Flexbox and Grid

**Supported Image Formats:**
- PNG ✅, GIF ✅, SJPG ✅, BMP ❌

**Performance Optimizations:**
- Image caching enabled (decoded PNG/SJPG/GIF images from SD are not re-decoded on every redraw)
- Gradient caching disabled to reduce memory usage
- Shadow caching disabled for predictable memory consumption
- Memory management uses ESP32 heap allocator

#### Debug Build
To enable debug mode, uncomment this line in `webscreen/webscreen_config.h`:
```cpp
#define WEBSCREEN_DEBUG 1
```
This enables verbose logging and memory debugging features.

### Runtime Modes

| Mode | Trigger | Description |
|------|---------|-------------|
| **JavaScript** | Valid `script_file` found | Full JavaScript runtime with all APIs |
| **Fallback** | No script or WiFi failure | Built-in notification app with GIF animation |
| **Recovery** | System errors detected | Minimal mode with error reporting |
| **Update** | Special SD card structure | Firmware update mode |

### Development & Debugging

#### Serial Monitor Output
```
[1234.567] INFO: [Main] WebScreen v2.0 initializing...
[1234.678] INFO: [Memory] PSRAM: 8388608 bytes available
[1234.789] INFO: [Display] RM67162 initialized (536x240)
[1234.890] INFO: [WiFi] Connected to MyNetwork (192.168.1.100)
[1234.991] INFO: [JavaScript] Loaded /apps/weather.js (2.4KB)
```

#### Serial Commands

WebScreen includes a comprehensive serial command system for interactive development. Commands work in both fallback and dynamic JavaScript modes:

**Core Commands:**
```
/help                    - Show all available commands
/stats                   - Display system statistics (memory, storage, WiFi)
/info                    - Show device information and version
/write <filename>        - Interactive JavaScript editor
/load <script.js> [save] - Switch to different JS application (save = persist to config)
/eval <js-code>          - Evaluate JS in the running app (REPL)
/errors                  - Show last JS error and restart-ladder state
/screenshot              - Capture the screen as base64 RGB565 (alias /ss)
/brightness <0-255>      - Set display brightness (no args to query current)
/factory_reset confirm   - Delete webscreen.json and reboot to fallback
/reboot                  - Restart the device
```

**Network & Monitoring:**
```
/wget <url> [file]       - Download file from URL to SD card (alias /fetch)
/ping <host>             - Test network connectivity
/monitor [cpu|mem|net]   - Live system monitoring (press any key to stop)
```

**Configuration Management:**
```
/config get <key>        - Get configuration value
/config set <key> <val>  - Set configuration value
/backup [save|restore]   - Backup or restore configuration
```

**File Operations:**
```
/ls [path] [json]        - List files/directories (json = machine-readable)
/cat <file>              - Display file contents
/rm <file|empty-dir>     - Delete file or empty directory
/mkdir <path>            - Create directory
/download <file>         - Dump file as base64 for host-side download (alias /dl)
```

**Example Development Workflow:**
```
WebScreen> /write hello.js
Enter JavaScript code. End with a line containing only 'END':
---
+ create_label_with_text('Hello WebScreen!');
+ END
[OK] Script saved: /hello.js (45 bytes)

WebScreen> /load hello.js
[OK] Loading script: /hello.js
```

JavaScript errors are reported with a 1-based source line number, e.g. `ERROR: 'foo' not found (line 12)` (inside a function body the line is relative to that function's first line). Use `/errors` to review the most recent error, and `/eval <js-code>` to poke at the running app interactively.

For detailed command reference, see [docs/SerialCommands.md](docs/SerialCommands.md).

#### Debug Commands
```cpp
// Memory usage report
memory_print_report();

// Display statistics  
display_print_status();

// System health check
webscreen_error_print_report();

// Network status
wifi_manager_print_status();
```

#### Performance Monitoring
```cpp
// Enable performance profiling
display_set_performance_monitoring(true);

// Monitor frame rate and memory usage
display_stats_t stats;
display_get_stats(&stats);
Serial.printf("FPS: %d, Memory: %d KB\n", stats.last_fps, stats.memory_used/1024);
```

## JavaScript API

The firmware exposes numerous functions to your JavaScript applications. Some highlights include:
- **Basic:** `print()`, `delay()`, `get_millis()`, `toNumber()`, `numberToString()`, `random()`
- **Strings:** `str_length()`, `str_substring()`, `str_index_of()`, `str_split()`, `str_split_count()`, `format_number()`, `pad_number()`
- **Wi-Fi:** `wifi_connect()`, `wifi_status()`, `wifi_get_ip()`
- **HTTP:** `http_get()`, `http_post()`, `http_delete()` (all support custom ports like `http://host:port/path`), `http_set_ca_cert_from_sd()`, `http_set_header()`, `http_clear_headers()`, `parse_json_value()`
- **SD Card:** `sd_read_file()`, `sd_write_file()`, `sd_list_dir()`, `sd_delete_file()`
- **BLE:** `ble_init()`, `ble_is_connected()`, `ble_write()`
- **Display:** `set_brightness()`, `get_brightness()`
- **Button:** `on_button()`, `get_button_event()`, `button_set_toggle()` (power button short press)
- **Time:** `get_hours()`, `get_minutes()`, `get_seconds()`, `get_year()`, `get_month()`, `get_day()`, `get_weekday()`, `get_epoch()`, `ntp_synced()`, `format_time()`
- **UI Drawing:** `draw_label()`, `draw_rect()`, `show_image()`, `create_label()`, `label_set_text()`
- **Image Handling:** `create_image()`, `create_image_from_ram()`, `rotate_obj()`, `move_obj()`, `animate_obj()`
- **Styles & Layout:** `create_style()`, `obj_add_style()`, `style_set_*()`, `obj_align()`
- **Advanced Widgets:** Meter, Chart, Span (rich text), Line
- **MQTT:** `mqtt_init()`, `mqtt_connect()`, `mqtt_publish()`, `mqtt_subscribe()`, `mqtt_loop()`, `mqtt_on_message()`, `mqtt_has_message()`, `mqtt_get_payload()`, `mqtt_msg_clear()`

For a full list and examples of usage, see the [JavaScript API Reference](docs/API.md).

## Secure HTTPS Connections

To call secure APIs (e.g., using `http_get()`), load a full chain certificate stored on the SD card using:
```js
http_set_ca_cert_from_sd("/timeapi.pem");
```
### Creating a Full Chain Certificate
1. **Obtain Certificates:**  
   Collect your server certificate and the intermediate certificate(s). Optionally, include the root certificate.
2. **Concatenate Certificates:**  
   Use a text editor or command-line tool:
   ```bash
   cat server.crt intermediate.crt root.crt > fullchain.pem
   ```
   Ensure each certificate block starts with `-----BEGIN CERTIFICATE-----` and ends with `-----END CERTIFICATE-----`.
3. **Deploy:**  
   Copy the resulting `fullchain.pem` file to the SD card.
4. **Usage:**  
   Your JavaScript app should load it with `http_set_ca_cert_from_sd()` to enable secure HTTPS requests.

## Contributing & Support

### For Developers

WebScreen is designed to be contributor-friendly with comprehensive documentation and testing frameworks.

#### Getting Started
1. **Read the Docs**: Check out [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) for detailed guidelines
2. **Set Up Environment**: Follow the development setup instructions
3. **Pick an Issue**: Look for "good first issue" labels on GitHub
4. **Submit PR**: Follow our contribution workflow

#### Key Areas for Contribution
- **Performance**: Memory optimization, rendering improvements
- **Hardware Support**: New display drivers, sensor integration  
- **Network**: Protocol implementations, connectivity features
- **Documentation**: API docs, tutorials, examples
- **Testing**: Unit tests, integration tests, hardware testing

### Getting Help

| Type | Resource | Description |
|------|----------|-------------|
| 🐛 **Bug Reports** | [GitHub Issues](https://github.com/HW-Lab-Hardware-Design-Agency/WebScreen-Software/issues) | Report bugs and request features |
| 💬 **Discussions** | [GitHub Discussions](https://github.com/HW-Lab-Hardware-Design-Agency/WebScreen-Software/discussions) | Ask questions and share ideas |
| 📖 **Documentation** | [docs/](docs/) | API reference and guides |
| 🌐 **Website** | [WebScreen.cc](https://webscreen.cc) | Official project website |
| 🛒 **Hardware** | [CrowdSupply](https://www.crowdsupply.com/hw-media-lab/webscreen) | Purchase WebScreen hardware |

### Support the Project

If WebScreen has been useful for your projects:

- ⭐ **Star the repo** to show your support
- 🍴 **Fork and contribute** to make it better  
- 🐛 **Report issues** to help us improve
- 📖 **Improve documentation** for other users
- 💰 **Sponsor development** to fund new features

## License

This project is open source. See the [LICENSE](LICENSE) file for details.
