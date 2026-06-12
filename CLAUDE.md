# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

WebScreen is an ESP32-based platform that runs dynamic JavaScript applications using the Elk engine and LVGL for UI rendering. Applications are stored on SD card with configurable selection via JSON files. The system supports HTTPS (with full chain certificates), BLE, MQTT, and provides a fallback notification app when no JavaScript app is found.

## Development Environment

### Platform: Arduino IDE for ESP32
- Uses ESP32-Arduino SDK (version 2.0.3 or above)
- Target board: ESP32-S3 with specific pin configuration
- USB upload via JTAG port
- Serial debugging output at 115200 baud

### Build Process
The project is compiled using Arduino IDE:
1. Install ESP32 boards package via Board Manager (v2.0.3 or higher)
2. Install required libraries: ArduinoJson, LVGL v8.3.0-dev, PubSubClient
3. Copy the provided `lv_conf.h` configuration file to Arduino libraries folder
4. Select ESP32-S3 board with specific settings (refer to docs/arduino_tools_settings.png)
5. Compile and upload via USB (CDC_ON_BOOT must be enabled)

### Upload Mode
If USB port not detected:
1. Power off, hold BOOT button
2. Connect USB
3. Hold BOOT, press RESET, release BOOT

## Code Architecture

### Core Components

#### Main Application (`webscreen.ino`)
- **Configuration Loading**: Reads `/webscreen.json` from SD card for WiFi, MQTT, colors, and script selection
- **SD Card Initialization**: Robust mounting with retry logic and speed fallback (400kHz to 10MHz)
- **Mode Selection**: Chooses between dynamic JavaScript execution or fallback notification app
- **Power Button**: Uses INPUT_PIN (pin 33) for screen on/off toggle with brightness control
- **Global State**: Manages MQTT enablement, color scheme, script filename, and screen power state

#### Dual Runtime System
- **Dynamic Mode** (`dynamic_js.h/cpp`): Executes user-provided JavaScript using Elk engine
- **Fallback Mode** (`fallback.h/cpp`): Built-in notification app when JS execution unavailable

#### Hardware Abstraction
- **Pin Configuration** (`pins_config.h`): ESP32-S3 pin mappings for LCD, SD card, buttons, etc.
- **Display**: RM67162 LCD controller (536x240 resolution) via QSPI
- **Storage**: SD card interface via SD_MMC pins

### JavaScript Runtime (Elk Engine)
Exposes comprehensive API to JavaScript applications:
- **Network**: WiFi management, HTTP/HTTPS requests with custom ports
- **Storage**: SD card file operations
- **UI**: LVGL widget creation, styling, animation
- **Communication**: BLE and MQTT protocols
- **Hardware**: GPIO, display control (brightness via `set_brightness()`/`get_brightness()`)

**Memory**: The Elk arena defaults to 256KB allocated in PSRAM, configurable via the flat `webscreen.json` key `"js_heap_kb"` (clamped to 64-1024 KB; must be set before the engine starts). LVGL allocations prefer PSRAM (`heap_caps_*_prefer` wrappers in `lv_conf.h`), keeping internal DRAM free for TLS/lwip/DMA. The firmware never self-reboots on memory pressure: low internal heap sheds JS timer ticks, arena pressure triggers GC at safe points, and repeated script errors trigger an in-place JS app restart — escalating to safe mode with an on-screen error (device stays alive for serial commands) rather than a reboot.

**Bridge structure**: `lvgl_elk.h` is an aggregator that includes 14 `ws_*.h` fragment headers in a fixed order (the concatenation matches the former ~3,700-line monolith, so fragments are order-dependent and not individually includable). It is included exactly once, by `webscreen_runtime.cpp`.

### LVGL Configuration (lv_conf.h)

**Available Fonts** (Montserrat only):
- Sizes: 14 (default), 20, 28, 34, 40, 44, 48
- Other sizes (8, 10, 12, 16, 18, 22, 24, etc.) are NOT enabled

**Enabled Widgets**:
- Core: Label, Image, Arc, Line, Button, Button Matrix, Canvas
- Extra: Chart, Meter, Message Box, Span (rich text)

**Disabled Widgets** (to save memory):
- Bar, Slider, Switch, Checkbox, Dropdown, Roller
- Textarea, Table, Calendar, Colorwheel, Keyboard
- List, Menu, Spinbox, Spinner, Tabview, Tileview, Window

**Image Formats**:
- PNG: ✅ Enabled
- GIF: ✅ Enabled
- SJPG: ✅ Enabled (split JPG)
- BMP: ❌ Disabled

**Layouts**: Flexbox and Grid enabled

**Display**: 16-bit color (RGB565), 130 DPI, 30ms refresh

### Configuration System
Uses `/webscreen.json` on SD card:
```json
{
  "settings": {
    "wifi": {"ssid": "...", "pass": "..."},
    "mqtt": {"enabled": false}
  },
  "screen": {"background": "#2980b9", "foreground": "#00fff1"},
  "display": {"brightness": 200},
  "script": "app.js",
  "js_heap_kb": 256
}
```

## Development Guidelines

### Commit Messages
Follows Conventional Commits v1.0.0:
- `feat(scope): description` - New features
- `fix(scope): description` - Bug fixes  
- `docs: description` - Documentation changes
- `chore: description` - Maintenance tasks

### Code Structure
- Keep hardware-specific code in appropriate modules
- JavaScript API bindings should be comprehensive and well-documented
- Maintain robust error handling for SD card and network operations
- Use global variables sparingly (current globals: `g_script_filename`, `g_mqtt_enabled`, `g_bg_color`, `g_fg_color`, `g_screen_on`, `g_last_button_state`, `g_last_button_time`)

### Testing
- Test with various SD card speeds and types
- Verify JavaScript API functions work correctly
- Test fallback mode when SD card/network unavailable
- Validate secure HTTPS with full certificate chains

## Key Files

- `webscreen/webscreen.ino` - Main application entry point
- `webscreen/dynamic_js.cpp` - JavaScript runtime implementation  
- `webscreen/lvgl_elk.h` - Elk JS <-> LVGL bridge aggregator (includes the `ws_*.h` fragment headers in order; only `webscreen_runtime.cpp` includes it)
- `webscreen/serial_commands.cpp` - Interactive serial console (`/help`, `/stats`, `/upload`, `/load`, `/restart_app`, `/gc`, ...)
- `webscreen/fallback.cpp` - Fallback notification app
- `webscreen/pins_config.h` - Hardware pin definitions
- `webscreen/globals.h` - Global variable declarations
- `lv_conf.h` - LVGL library configuration optimized for ESP32-S3 and AMOLED display
- `docs/API.md` - Complete JavaScript API reference
- `docs/CONTRIBUTING.md` - Contribution guidelines

## Build Artifacts
Compiled binaries are stored in `webscreen/build/esp32.esp32.esp32s3/`:
- `webscreen.ino.bin` - Main application binary
- `webscreen.ino.bootloader.bin` - Bootloader
- `webscreen.ino.merged.bin` - Combined flash image

## Code Search

Use ken as the first attempt for codebase questions. Prefer ken MCP tools before
broad text search or reading many files:

- Start with `ken_rank` for the current task, or pass a query when the question
  needs a focused search.
- Use `ken_search_files` to find files by intent, feature, behavior, or concept.
- Use `ken_search_symbols` to find functions, classes, methods, APIs, and other
  named code objects.
- Use `ken_file_outline`, `ken_file_symbols`, and `ken_file_snippets` to inspect
  surfaced files precisely before opening larger chunks of code.
- Use `ken_file_neighbors`, `ken_module_graph`, and `ken_find_tests` to follow
  imports, related modules, and source/test pairs.
- Use `ken_changed_context` when working from an existing diff or local edits.
- Use `ken_project_overview` for a compact map of an unfamiliar project area.
- Use `ken_recall` and `ken_findings` for saved project knowledge, and
  `ken_remember` when a durable finding should help future sessions.
- Use `ken_explain_rank` when rankings look surprising or an expected file is
  missing.
- Use `ken_dismiss` when ken surfaces a file that is clearly not relevant, so
  future similar tasks get better results.

After ken narrows the search space, read the relevant files directly. Fall back
to `rg` when ken is insufficient, when an exact literal search is required, or
when verifying a specific string occurrence.
