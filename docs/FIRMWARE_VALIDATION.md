# Firmware Validation

## Build

The review build uses Arduino CLI 1.5.1, ESP32 core 3.3.2, LVGL 9.5.0,
ArduinoJson 7.4.2, NimBLE-Arduino 2.3.1, and PubSubClient 2.8.
Install those libraries and copy the repository's `lv_conf.h` beside the LVGL
library. From the repository root, for a 16 MB flash / OPI PSRAM board:

```sh
cp lv_conf.h ~/Arduino/libraries/lv_conf.h
arduino-cli compile \
  -b esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CDCOnBoot=cdc \
  --build-path /tmp/webscreen-build --warnings all -j 6 webscreen
```

Build into `/tmp`; the tracked exports in `webscreen/build/` are release artifacts.
A successful compile does not validate panel timing or SD-card signal integrity.

## Native regressions

Requires Python 3, GCC/G++, and the AddressSanitizer/UndefinedBehaviorSanitizer
runtime libraries. Tests compile the installed LVGL with the repository's
configuration and run real Elk timer and chart/meter bindings:

```sh
python3 tests/run_host_tests.py --build-dir /tmp/webscreen-host-tests
```

Use `--lvgl /path/to/lvgl` for a different library location. The optional
`--sanitizer-lib-dir /path/to/libs` supports locally extracted sanitizer runtimes.
Leak checking requires a host environment that permits process inspection.

Coverage includes timer self-deletion (success and error paths), invalid timer
periods, chart enum compatibility, cross-owner handles, invalid meter ranges,
independent line points, memory filesystem seeks/EOF/repeated registration,
RGB565 rendering and packed export, serial overflow recovery, and color parsing.
These tests do not emulate ESP32 hardware or network stacks.

## Device smoke test

1. Copy `tests/migration_smoke.js` to the root of a FAT32 SD card. Run
   `/load migration_smoke.js` at 115200 baud.
2. Expect a changing **bar** chart, rotating meter needle, two separate bottom
   lines, and one `Self-deleting timer passed` message. Check `/errors`.
3. Run `/screenshot`; decoded data must contain exactly **257280 bytes** for
   536×240 RGB565. Check colors, text, and needle position against the panel.
4. Repeat `/restart_app` and `/screenshot` at least 20 times; record `/stats`
   after warm-up and check for continuing memory loss or resets.
5. Send a partial console line without a newline and verify the power button
   stays responsive. Check short press, long press, and brightness 0/200.
6. Boot with no SD card, missing script, malformed configuration, and an app
   that creates a timer then raises an error. Verify fallback/safe-mode display
   and serial recovery. Fallback requires configuration plus `/reboot` to start JS.
7. Test multiple SD cards, Wi-Fi disconnect/reconnect, unavailable NTP, HTTPS
   certificate validation, and MQTT broker loss/recovery. Reconnect-time NTP
   setup must not freeze animations; boot retains its initial clock grace period.

Record board revision, supply, SD card, dependency versions, logs, and captures.
