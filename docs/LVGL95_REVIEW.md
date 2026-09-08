# LVGL 9.5 Firmware Review

## Comparison and scope

Reviewed migration head `d048560` against local `main` (`61da813`); no `master`
branch exists in this checkout. Also compared against `ccd5775` to isolate the
LVGL migration from earlier runtime changes. Work covers display setup, storage,
JavaScript bindings and timers, restart handling, serial polling, and timekeeping.

The v9 display API, byte counts instead of `sizeof(lv_color_t)`, and native
`LV_COLOR_FORMAT_RGB565_SWAPPED` are appropriate for this panel. Native swapped
RGB565 is supported by [LVGL 9.5](https://docs.lvgl.io/9.5/main-modules/display/color_format.html).
The synchronous panel flush correctly calls `lv_display_flush_ready()` only
after transmission. A second draw buffer would not overlap work with this driver.

## Corrections

| Area | Finding and resulting behavior |
| --- | --- |
| Chart migration | v9 inserted chart enum members. JS retains 0=none, 1=line, 2=bar, 3=scatter. |
| Timers | Deleting the currently executing timer freed its context before the callback finished. Freeing is deferred; errors remain reportable. |
| Binding ownership | Chart series and meter subobjects must belong to the supplied widget. Type/range checks reject unsafe calls. |
| Line points/styles | Lines previously shared one mutable point array. Each now owns its points; rectangles use object-local styles. |
| Display/fallback | Both modes reuse one 42880-byte internal draw buffer. Fallback no longer allocates another 257280-byte PSRAM frame. |
| Screenshots | Temporary storage explicitly uses PSRAM; export strips stride padding and allocation slack. Default `malloc` may already use PSRAM depending on ESP32 allocator settings. |
| Timing | A monotonic tick callback replaces 1000 periodic callbacks per second. Runtime sleeps respect imminent LVGL deadlines; fallback yields CPU time. |
| SD storage | Arduino forwards mount frequency to `max_freq_khz`; probing now requests 400 kHz and the faster remount 10000 kHz. Serial recovery follows the same path. |
| Pins | Conflicting GPIO 33/21 aliases depended on include order. Definitions now consistently resolve to GPIO 21; the former boot path already resolved to 21 through an override. |
| Serial/configuration | Normal input is incremental and bounded. Colors are validated, shorthand hex expands correctly, and brightness is clamped. |
| Recovery | Incomplete/oversized scripts are rejected; failed evaluations clean up partial apps. Reloads drop image caches and consume restart requests under the existing lock. |
| Network time | NTP reconnect setup and JS clock reads no longer wait for synchronization on the rendering task. Initial boot still allows time for TLS clock setup. |

## Evidence and remaining device checks

The original migration uses 3030291 flash bytes and 102664 static RAM bytes.
The reviewed build uses 3030763 flash bytes and 103448 static RAM bytes; runtime
buffer savings are separate from these linker totals. Both compile with the versions in
[Firmware Validation](FIRMWARE_VALIDATION.md). Native tests pass AddressSanitizer,
UndefinedBehaviorSanitizer, and leak checks. Running the timer regression against
the original implementation reproduces a heap-use-after-free.

No board was connected during review. Frame rate, QSPI timing, SD behavior,
long-run memory use, and physical power-button behavior still require the device
smoke test. Synchronous HTTP and MQTT operations can still delay the LVGL task;
a fully asynchronous network architecture is not implemented here. Removed v8
chart zoom/tick APIs remain documented no-ops, and meter gradient/label-gap
compatibility remains approximate.
