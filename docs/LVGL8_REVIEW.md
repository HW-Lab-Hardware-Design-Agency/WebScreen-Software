# LVGL 8 stability backport

The `dev` branch at `0dc791f` predates the runtime cleanup that supported the
LVGL 9.5 work. This backport reuses the relevant fixes from `cec0f30` and the
LVGL 8 runtime prerequisites in `ccd5775`, while retaining LVGL 8.3 descriptors,
assets, byte ordering, style setters, chart axes/zoom, and native meters.

## Changes

- Share one 42,880-byte internal draw buffer between JavaScript and fallback.
  The JavaScript path no longer allocates its additional 257,280-byte PSRAM
  display buffer. Panel transactions are serialized; SPI transfer capacity
  matches the synchronous 32,768-byte chunks.
- Read a monotonic clock instead of scheduling 1,000 tick callbacks per second.
  Poll serial without waiting for newlines and service runtime requests every
  few milliseconds. NTP reconnect setup no longer waits on the graphics task.
- Use GPIO 21 consistently, correct SD mount frequencies to kHz, preserve zero
  brightness, stream configuration parsing, and widen deep-sleep arithmetic.
- Reload apps on their owning task, clean timers/UI/media in order, invalidate
  image caches, and recover from script errors without forced maintenance
  reboots. Record errors and real runtime measurements.
- Bound script loading/execution, protect Elk from allocation failures, give
  each line its own point storage, use object-owned rectangle styles, and
  validate chart/meter ownership and filesystem seeks.
- Match NimBLE 2 callback signatures and expose notifications correctly.
  Existing ping and MQTT functionality remains available.

## LVGL 8 adaptations

The sanitizer tests exposed an X-array leak in the native scatter chart
destructor. A delete event leaves scatter mode before native destruction,
which releases those arrays through LVGL's own API. New series also initialize
X-array ownership and values. See the [LVGL 8.3.11 chart implementation](https://github.com/lvgl/lvgl/blob/v8.3.11/src/extra/widgets/chart/lv_chart.c).

Screenshots use LVGL 8's `lv_snapshot_take_to_buf` with PSRAM storage and
explicitly fill the descriptor's missing `data_size`. They retain the
`RGB565_SWAP` serial format. No LVGL 9 compatibility shims are required.

## Validation and limits

The original branch and the backport compile for ESP32-S3 with LVGL 8.3.11.
The native regression suite runs with address, leak, and undefined-behavior
sanitizers. See [validation commands and device checks](FIRMWARE_VALIDATION.md).

| Build | Flash bytes | Static RAM bytes |
| --- | ---: | ---: |
| Original `dev` (`0dc791f`) | 2,910,423 | 101,600 |
| Stability backport | 2,951,483 | 104,192 |

The backport fits the 3 MiB app partition (93% used). The additional recovery
and console support increases flash and static RAM, while removing the
257,280-byte display allocation from PSRAM during JavaScript startup.

These checks do not establish a hardware performance measurement. Physical
display, SD, power, and radio tests remain necessary. Synchronous network
calls can still delay UI updates; this backport does not make them asynchronous.
