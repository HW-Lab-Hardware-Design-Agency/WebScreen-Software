# Release Notes

## Unreleased - JS Runtime Stability & Memory Overhaul

### Fixed

- **Device no longer self-reboots on low memory.** The JS timer bridge used to call `ESP.restart()` when internal heap ran low; it now skips JS ticks under pressure and warns over serial. The scheduled reboot after 36,000 combined timer ticks (roughly hourly with a single 100ms timer) is gone entirely.
- **Elk `setprop` out-of-memory no longer corrupts the JS arena** (backported upstream cesanta/elk fix `a128ee2`).
- **`++`/`--` on a non-lvalue no longer performs a wild arena write** — it raises a JS error instead (backported upstream fix `a07410e`).
- **`/load` now actually loads the requested script.** It previously rebooted the device and lost the selection; it now restarts the JS app in place with the new script, no reboot.
- **WiFi and MQTT actually reconnect after a drop.** The maintain loop previously only logged; it now calls `WiFi.reconnect()` (10s interval) and retries MQTT with the last working credentials (5s backoff), re-subscribing on success.
- **Base64 `/upload` can no longer overflow its stack decode buffer.** Decoding is bounded: a line decoding to more than 512 bytes aborts the upload with an `[ERROR]` line, and 30 seconds of inactivity aborts the transfer (partial file removed in both cases).
- **SPI display race fixed.** `lcd_PushColors`, `lcd_brightness`, and `lcd_send_cmd` are serialized by a recursive panel mutex — a brightness change during a JS-task flush used to corrupt the QSPI command stream.
- **GIF reload leak fixed.** Loading a new GIF frees the previous PSRAM buffer instead of abandoning it.
- **Span text use-after-GC fixed.** `lv_span_set_text_static` now copies the text; Elk's compacting GC moves arena strings, so the old non-copying reference rendered garbage after a collection.
- **Wild pointer dereferences eliminated in chart/meter/span bindings.** Raw LVGL pointers were handed to JS as doubles and cast straight back; they are now bounds-checked slot-index handles (-1 on failure), and an invalid handle produces a JS error instead of a LoadProhibited crash.

### Added

- **In-place JS app restart + safe mode.** A misbehaving script (10 consecutive timer-callback errors, `/restart_app`, or `/load`) is torn down and restarted by the JS task itself over the same arena — no device reboot. After 2 failed restarts the device shows the script error on screen and stays alive for serial commands; boot-time script failures are shown on screen too instead of a black screen.
- **`js_heap_kb` config key**: flat key in `webscreen.json` to size the Elk arena (clamped 64-1024 KB, default 256).
- **New JavaScript functions**: `mem_info()`, `gc()`, `timer_delete(fname)`, `obj_delete(handle)`, `gif_free()`, `ram_image_free(slot)`, `mqtt_dropped()`.
- **New serial commands**: `/restart_app` (in-place app restart) and `/gc` (run JS garbage collection and report arena usage).
- **`/stats` and `/info` telemetry**: Min Free Heap, Largest Free Block, and JS Arena Used/Total.
- **Runaway-script guards**: a per-eval statement budget (2,000,000 statements — exceeded loops get a `step limit` JS error) and a C-stack guard (deep recursion gets a `C stack` JS error) replace device crashes.

### Changed

- **LVGL heap allocations now prefer PSRAM** (`heap_caps_*_prefer` wrappers in `lv_conf.h`), keeping internal DRAM free for TLS, lwIP, and DMA, which cannot spill to PSRAM. Remember to copy the updated `lv_conf.h` to the Arduino libraries folder.
- **~214KB of PSRAM reclaimed** from the LVGL second draw buffer: it was allocated full-screen (257,280 bytes) but only the 40-line draw area (42,880 bytes) was ever used.
- **`lvgl_elk.h` split into `ws_*.h` modules.** The 3,662-line monolith is now an aggregator including 14 order-dependent fragment headers (`ws_elk_core.h`, `ws_lvgl_fs.h`, `ws_lvgl_display.h`, `ws_elk_basics.h`, `ws_elk_media.h`, `ws_lvgl_widgets.h`, `ws_lvgl_styles.h`, `ws_lvgl_charts.h`, `ws_elk_http.h`, `ws_elk_sd_ext.h`, `ws_elk_ble.h`, `ws_elk_mqtt.h`, `ws_elk_time.h`, `ws_elk_runtime.h`), included once by `webscreen_runtime.cpp`.
- **Dead code removed**: duplicate boot-resident network clients and the unused second runtime architecture are gone; the simulated memory-usage stub now reports real JS arena and heap numbers.

## v2.1.0 - Display Brightness Control & Admin UI Improvements

### New Features

#### Display Brightness Control
- **Hardware brightness support**: Direct AMOLED brightness control via RM67162 command (0x51), with a range of 0 (off) to 255 (maximum).
- **JavaScript API**: New `set_brightness(value)` and `get_brightness()` functions available in JavaScript applications.
- **Serial command**: New `/brightness <0-255>` serial command to set or query display brightness interactively.
- **Configuration persistence**: Brightness is stored in `webscreen.json` under `display.brightness` and applied automatically on boot.
- **Default value**: 200 (approximately 78% brightness).

#### Admin Tool UI Improvements
- **Unified Settings page**: WiFi, brightness, display colors, and all device settings are now organized into four dynamic sections: General, Device, Time & Location, and Advanced.
- **Real-time brightness slider**: Adjusting the brightness slider in the Admin tool sends the change to the device immediately.
- **EVA theme fixes**: Fixed white backgrounds appearing on upload areas and file browser in EVA theme. Brightness slider and quick-action buttons now use the EVA green accent color.

### Configuration

Add brightness to your `webscreen.json`:

```json
{
  "settings": {
    "wifi": {
      "ssid": "MyNetwork",
      "pass": "MyPassword"
    }
  },
  "display": {
    "brightness": 200
  },
  "script": "app.js"
}
```

### JavaScript API Usage

```javascript
// Set brightness
set_brightness(200);

// Read current brightness
let level = get_brightness();
print("Brightness: " + numberToString(level));
```

### Serial Command Usage

```
WebScreen> /brightness
Current brightness: 200

WebScreen> /brightness 150
[OK] Brightness set to 150
```

### Files Changed

**Firmware (WebScreen-Software):**
- `rm67162.h/cpp` - Added `lcd_brightness()` hardware function
- `webscreen_hardware.cpp` - Connected brightness stub to display driver
- `lvgl_elk.h` - Added JS API bindings for `set_brightness`/`get_brightness`
- `fallback.cpp` - Applied config brightness in fallback mode
- `serial_commands.h/cpp` - Added `/brightness` serial command
- `webscreen_main.cpp` - Added brightness reading in `webscreen_load_config()`

**Admin Tool (WebScreen-Admin):**
- `index.html` - Reorganized settings into dynamic sections
- `app.js` - Dynamic config rendering with brightness slider, WiFi in General section
- `serial.js` - Added `setBrightness()` method
- `styles.css` - Fixed EVA theme backgrounds, slider accent color, quick-action button contrast
