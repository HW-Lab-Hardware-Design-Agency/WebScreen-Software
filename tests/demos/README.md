# LVGL 9.5 Demo Pack

Five standalone apps for the 536 × 240 WebScreen display. They use the firmware's
Elk JavaScript subset and need no Wi-Fi, images, fonts on the SD card, or additional
libraries. Use the migration firmware with the arc-label bindings already used by
[`lvgl95_showcase.js`](../lvgl95_showcase.js).

## Install and run

1. In the Serial IDE, connect WebScreen and open **Files** at `/`.
2. Upload the five `.js` files in this folder together.
3. In **Serial Console**, enter one of the commands below. Wait for
   `DEMO READY:` and `JS app restarted successfully` before starting another app
   or requesting a file listing.

| Command | What to check | Short press |
| --- | --- | --- |
| `/load lvgl95_arc_text.js` | Two circular text labels with different radii and fonts | Clockwise → counterclockwise → paused |
| `/load lvgl95_charts.js` | Two synthetic signals and preserved chart type numbering | Curve → line → bar → scatter → paused |
| `/load lvgl95_gauges.js` | Three independent needles and colored progress arcs | Pause / resume |
| `/load lvgl95_typography.js` | Font sizes, wrapping, contrast, and RGB565 color order | Dark / light |
| `/load lvgl95_motion.js` | Independent line point buffers and a moving marker | Pause / resume |

Long press remains power-off. Loading without `save` leaves your normal boot app
unchanged. Typography has no timer; the animated demos use one timer and reuse
their objects and styles. Gauge/chart values are synthetic, not device telemetry.
To cycle modes from the browser console, use `/eval demo_button(1);`. This runs the
same JavaScript callback as a short press; also test the physical button itself.

## Test each app

- Exercise every short-press mode, including returning to the first mode.
- Capture `/screenshot` in each mode; pause animation first if available.
- Check `/errors` and `/stats`, run `/gc`, then check that the app still responds.
- Run `/restart_app` and switch between demos repeatedly. Check for continuing
  heap loss, stale widgets, error reports, or spontaneous reboots.
- A PSRAM initialization failure or failure to allocate the Elk heap prevents JS
  testing. Power-cycle first; if it persists, inspect the board/build configuration
  and retain the serial boot log. A successful file upload does not establish
  runtime stability.

## Host verification

From the repository root, run `python3 tests/run_host_tests.py`. The suite executes
every mode through Elk and the production LVGL bindings, checks pause behavior and
stable object/timer counts, forces garbage collection, repeats app loads, and runs
ASan/UBSan/leak checks. It writes a PPM preview for every mode in its build directory.
These checks complement testing on the physical board.
