# Firmware validation (LVGL 8.3)

## Build

Tested with Arduino ESP32 3.3.2, LVGL 8.3.11, ArduinoJson 7.4.2,
PubSubClient 2.8, and NimBLE-Arduino 2.3.1. Install the repository's
`lv_conf.h` beside the LVGL library before compiling.

```sh
arduino-cli compile \
  -b esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CDCOnBoot=cdc \
  --build-path /tmp/webscreen-build --warnings all -j 6 webscreen
```

When another branch uses LVGL 9, keep a separate library collection containing
`libraries/lvgl/` (8.3.11) and this branch's `libraries/lv_conf.h`. Add
`--library /path/to/libraries/lvgl` to the command. Check the selected library
version; do not compile this branch against LVGL 9 or its configuration.
Build into `/tmp`; tracked `webscreen/build/` exports are release artifacts.

The JS robustness pass was validated on 2026-09-08 with the versions above:
the ESP32-S3 build passed at 2,972,459 bytes of flash (94%) and 110,696 bytes of
static RAM (33%). The native sanitizer suite passed. Physical-device checks
below remain pending; these results do not measure on-device frame rate.

## Native regression tests

Requires Python 3, GCC/G++, and address/undefined-behavior sanitizer runtimes.
The tests compile real LVGL 8.3 and the repository's Elk engine and binding
headers, using an in-memory display and a deterministic clock.

```sh
python3 tests/run_host_tests.py \
  --lvgl /path/to/lvgl-8.3.11 \
  --build-dir /tmp/webscreen-lvgl8-tests
```

Use `--sanitizer-lib-dir /path/to/runtime/libs` when sanitizer libraries are
installed outside the compiler's search path. AddressSanitizer, LeakSanitizer,
and UndefinedBehaviorSanitizer remain enabled, including float-to-integer overflow checks.

Coverage includes serial overflow/recovery, color parsing, timer self-deletion
and errors, execution budgets, independent line storage, widget ownership,
chart zoom/ticks and scatter cleanup, meter registry limits, label text,
memory filesystem seeks, and a complete 536×240 RGB565_SWAP snapshot. JS app
regressions also cover elapsed-time deadlines, cancellation and subsequent eval
recovery, expired source buffers, non-terminated string input, substring bounds,
stale widget handles, timer/object quotas, invalid numeric arguments, MQTT queue
overflow and subscription restoration, and RAM-image bounds/reference lifetimes.

## Device checks

1. Boot with a normal SD card, missing card, invalid configuration, empty script,
   and a script exceeding 1 MiB. Failure should leave fallback serial usable.
2. Copy `tests/firmware_smoke.js` to SD and run `/load firmware_smoke.js`.
   Verify the bar chart, native meter needle, two independent lines, and the
   self-deleting timer message. Repeat `/restart_app` and watch `/stats` for
   stable memory use. Repeat with GIF and RAM-image apps.
3. Run `/eval for (;;) {}` and a deliberately failing timer. Check `/errors`,
   verify serial recovery, then load a valid app. A failing startup script
   must leave no partial UI or timers running behind the error screen.
4. Send a partial serial line slowly, then a line longer than 1023 bytes.
   Animations should continue; the next valid command should work.
5. Test brightness 0/255, short/long presses on GPIO 21, and display toggling
   while animating. Check panel colors and `/screenshot`; decoded payloads
   must contain exactly 257,280 bytes, in big-endian RGB565 order.
6. Test offline operation, WiFi reconnect, unreachable NTP/MQTT servers,
   `/ping`, MQTT subscriptions after reconnect, and BLE connect/write/notify.
7. Subscribe to two MQTT topics, send bursts of messages, and inspect topic/payload
   ordering plus `mqtt_dropped()`. Reconnect the broker and verify both topics
   resume. Check that oversized publications fail rather than send partial JSON.
8. Write and read a temporary SD file containing quotes and newlines; compare
   exact content and delete it. Load a short raw-image file and expect -1.
   Try freeing a valid RAM image while its widget or meter needle uses it:
   expect `false`, then delete the consumers and expect `true`.

Host tests and builds cannot validate panel timing, SD reliability, power
behavior, or radio operation. HTTP/MQTT calls and interactive serial transfers
can still block while their synchronous operations run.
