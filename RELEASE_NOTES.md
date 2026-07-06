# Release Notes

## Unreleased - LVGL 9.5 Migration

- **LVGL 8.3.11 → 9.5.0** (`feature/lvgl-9.5-migration`). New v9-format `lv_conf.h` (copy to `~/Arduino/libraries/lv_conf.h`; the Arduino `lvgl` library folder must contain LVGL 9.5.0).
- Display driver ported to `lv_display_create`/`lv_display_set_buffers`; the panel's byte-swapped RGB565 is now expressed as the display color format `LV_COLOR_FORMAT_RGB565_SWAPPED` (LVGL 8's `LV_COLOR_16_SWAP` is gone).
- **JS `lv_meter_*` API reimplemented on `lv_scale`** (lv_meter was removed upstream): needles (line/image), arcs and scale-line sections keep their JS signatures; tick-gradient colors and `label_gap` have no v9 equivalent and are ignored.
- `lv_chart_set_zoom_x/y` and `lv_chart_set_axis_tick` JS bindings are kept as no-ops (APIs removed upstream).
- `/screenshot` now streams plain RGB565 (`=== SCREENSHOT WxH RGB565 ===`); host tools already accept both markers.
- Image assets converted to v9 formats: the boot logo is planar `RGB565A8`, the fallback GIF descriptor is `LV_COLOR_FORMAT_RAW`.
- Flash: 3,032,515 bytes (96%) — +82KB vs LVGL 8; static DRAM 124KB (37%).

## Unreleased (2.2.0-dev) - JS APIs, Live REPL, Serial Tooling & Rendering Performance

### New JS APIs

- **String/number/random helpers**: `random()` / `random(max)` / `random(min, max)` (hardware RNG), `str_split(str, sep, idx)` and `str_split_count(str, sep)` (multi-char separators, empty fields as `""`), `format_number(value, decimals)` and `pad_number(value, width)`. These replace the hand-rolled LCGs, CSV parsers, and `padZero` helpers that every example app used to carry.
- **`format_time(fmt)` / `format_time(fmt, epoch)`**: `strftime` of the current local time or of a given epoch, e.g. `format_time("%H:%M:%S")` → `"14:05:09"`.
- **Power-button events**: `on_button("fn")` delivers each short press to `fn(1)` (and suppresses the default display toggle while registered; `on_button("")` releases it); `get_button_event()` is a poll-style alternative; `button_set_toggle(bool)` keeps or suppresses the default toggle. The long press remains firmware power-off and never reaches JS. Button state is reset on app restart/switch.
- **Chart bindings now reachable from JS**: the full `lv_chart_*` family (`lv_chart_create`, `lv_chart_set_type`, `lv_chart_set_div_line_count`, `lv_chart_set_update_mode`, `lv_chart_set_range`, `lv_chart_set_point_count`, `lv_chart_refresh`, `lv_chart_add_series`, `lv_chart_set_next_value`, `lv_chart_set_next_value2`, `lv_chart_set_axis_tick`, `lv_chart_set_zoom_x`, `lv_chart_set_zoom_y`) was implemented but never registered; it is now exposed, with bounds-checked series handles (-1 on failure). (`lv_chart_get_y_array` stays unregistered — it returns a raw pointer JS cannot use.)

### Developer experience

- **`/eval <js-code>` REPL**: evaluate a one-liner (max 255 chars) inside the running app at the JS task's next safe point; the result or error is printed with an `[EVAL]` prefix. Refused in fallback or safe mode, or while a previous snippet is still in flight.
- **`/errors` report**: prints the last JS error with its age (captured from script-eval failures, timer-callback errors, button-callback errors, and `/eval` errors), the startup error if any, the restart-failure and auto-restart-cycle counters, the safe-mode flag, and the current script.
- **`/load <script.js> [save]`**: the new optional `save` argument also persists the script path into `webscreen.json` (the `script` key) so it survives a reboot; without it, `/load` stays session-only.
- **JS errors carry a source line number**: errors now end with a 1-based `(line N)` suffix, e.g. `ERROR: 'foo' not found (line 12)`. Inside a function body the line is relative to that function's first line (Elk re-parses function bodies as separate snippets).

### Serial console: screenshots, file download & filesystem tools

- **`/screenshot` (alias `/ss`) screen capture**: queues a capture that the JS task executes at its next safe point (LVGL objects must never be touched from another task), then streams the snapshot as base64 raw RGB565 between `=== SCREENSHOT <w>x<h> RGB565_SWAP ===` and `=== SCREENSHOT END ===` markers (`_SWAP` = byte-swapped pixels, per `LV_COLOR_16_SWAP`). Requires the JS runtime to be active. Implemented with LVGL `lv_snapshot`; **`lv_conf.h` changed** (`LV_USE_SNAPSHOT 1`) — remember to copy the updated file to the Arduino libraries folder.
- **`/download <file>` (alias `/dl`) base64 file dump**: streams any SD-card file, text or binary, between `=== DOWNLOAD <path> SIZE <n> ===` and `=== DOWNLOAD END ===` markers for binary-safe host-side download — the reverse of `/upload <file> base64`.
- **`/mkdir <path>`** creates directories on the SD card, and **`/rm` now also removes empty directories** (via `rmdir`; non-empty directories are refused, never deleted recursively).
- **`/factory_reset confirm`**: deletes `/webscreen.json` and reboots into fallback mode; the literal `confirm` argument is required, otherwise the command only prints a warning.
- **Machine-readable `/ls`**: a trailing `json` token (`/ls /apps json`) emits a single-line `{"path":...,"entries":[{"name","dir","size"}]}` listing for host tools, and the plain-text listing now ends with a `Total: N files, M directories` end-marker line so hosts can detect completion.
- **`/wget` alias changed from `download` to `fetch`**: the old alias was undocumented; `/download` now means the base64 file dump above.
- Shared base64 encoder extracted to `webscreen_base64.h` (used by `/download` and `/screenshot`). Flash cost of the batch: 2,950,655 bytes (93%), up from 2,943,811.

### Performance

- **Single internal-DRAM LVGL draw buffer**: the second PSRAM draw buffer was removed. LVGL 8 alternates buffers, so half of all rendering ran against slow OPI PSRAM for no benefit under a synchronous flush.
- **LVGL image cache enabled** (`LV_IMG_CACHE_DEF_SIZE 2`): PNG/SJPG/GIF images loaded from SD are no longer re-decoded on every redraw. **Note:** `lv_conf.h` changed — remember to copy the updated file to the Arduino libraries folder.
- **JS/LVGL task moved from core 0 to core 1** (same priority 1 as loopTask): it no longer competes with the WiFi/lwip stack, and FreeRTOS time-slicing keeps the power button responsive.
- **Script loading is now a single sized read straight into PSRAM** (was byte-at-a-time `readString()`).
- **Serial commands no longer call `SD_MMC.begin()` on every invocation** — the card is only (re)mounted when it is actually absent.

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
