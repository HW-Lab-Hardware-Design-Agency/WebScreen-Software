# WebScreen JavaScript API Reference

WebScreen exposes a comprehensive set of functions to JavaScript applications running on the ESP32 with LVGL and the Elk engine. The configuration file (`webscreen.json`) on the SD card determines which JavaScript file to execute.

## Configuration Example

```json
{
  "settings": {
    "wifi": {
      "ssid": "MyNetwork",
      "pass": "MyPassword"
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
  "timezone": "EST5EDT,M3.2.0,M11.1.0",
  "script": "my_app.js"
}
```

## JavaScript API Functions

The following functions are available in your JavaScript applications:

### Core Functions

- **print(message)**
  Print a message to the serial console for debugging.

- **mem_stats()**
  Print memory statistics (ESP32 heap and LVGL memory usage) to the serial console. Returns the free heap size in bytes. Useful for debugging memory issues.

- **mem_info()**
  Returns a JSON string describing all memory pools: internal heap (`heap_free`, `heap_min`, `heap_largest`), PSRAM (`psram_free`), and the JavaScript arena (`js_used`, `js_total`). All values are in bytes.
  ```javascript
  let info = mem_info();
  print(info);  // {"heap_free":123456,"heap_min":98760,...,"js_used":40960,"js_total":262144}
  let used = toNumber(parse_json_value(info, "js_used"));
  ```

- **gc()**
  Request a JavaScript garbage collection. The collection runs at the next safe point (between timer callbacks), not immediately inside the call. Returns the number of free bytes in the JS arena before the collection.
  ```javascript
  let free_before = gc();
  print("Arena free: " + numberToString(free_before));
  ```

- **delay(milliseconds)**
  Pause execution for the specified number of milliseconds.

- **create_timer(function_name, period_ms)**
  Create an LVGL timer that calls the named global function every `period_ms` milliseconds (1–2147483647; invalid periods are ignored). A callback may safely delete its own timer.
  ```javascript
  let update = function() { print("tick"); };
  create_timer("update", 1000);
  ```

- **timer_delete(function_name)**
  Stop and delete the timer created with `create_timer(function_name, period_ms)`. Returns `true` if a matching timer was found and deleted, `false` otherwise.
  ```javascript
  create_timer("update", 1000);
  // ... later ...
  timer_delete("update");
  ```

### Display Control

- **set_brightness(value)**
  Set the display brightness. Value ranges from 0 (off) to 255 (maximum). Returns the applied brightness value, or -1 on error.
  ```javascript
  set_brightness(200);   // Set brightness to ~78%
  set_brightness(0);     // Turn off display backlight
  set_brightness(255);   // Maximum brightness
  ```

- **get_brightness()**
  Returns the current display brightness value (0-255).
  ```javascript
  let current = get_brightness();
  print("Current brightness: " + numberToString(current));
  ```

### Button Events

The power button's short press (< 3s) can be delivered to JavaScript; the long press (>= 3s) is always handled by the firmware as power-off and never reaches JS. While an app is using the button, the default display on/off toggle is suppressed so the press is yours to handle.

Button state is reset whenever the app is restarted or switched: the handler is cleared, the default toggle is restored, and any queued presses are drained.

- **on_button("fn_name")**
  Register the named global function as the button handler. On each short press it is called as `fn_name(1)` at the next safe point. While a handler is registered the default display toggle is suppressed. Call `on_button("")` to release the button and restore the default toggle. Returns `true` on success, `false` if the name is too long.
  ```javascript
  let onPress = function(evt) { print("button pressed"); };
  on_button("onPress");
  // ... later ...
  on_button("");   // release, default toggle returns
  ```

- **get_button_event()**
  Poll-style alternative to `on_button()`. Returns `1` if a short press was pending (consuming it), or `0` otherwise. Pair it with `button_set_toggle(false)` so polled presses are not also consumed by the default toggle.
  ```javascript
  button_set_toggle(false);
  let update = function() {
    if (get_button_event()) {
      print("button pressed");
    }
  };
  create_timer("update", 100);
  ```

- **button_set_toggle(enabled)**
  Keep (`true`) or suppress (`false`) the default display on/off toggle. Used by poll-style apps that read `get_button_event()` without registering a handler.

### String Utilities

- **str_index_of(haystack, needle)**  
  Returns the index of `needle` in `haystack` (or -1 if not found).

- **str_substring(str, start, length)**  
  Returns a substring of `str` starting at `start` with the given `length`.

- **toNumber(string)**  
  Convert a string to a number.

- **numberToString(number)**  
  Convert a number to a string.

- **str_split(str, sep, idx)**
  Split `str` by `sep` and return the `idx`-th field (0-based), or `null` past the last field. The separator may be multiple characters. Empty fields are returned as `""`.
  ```javascript
  str_split("a,b,c", ",", 1);   // "b"
  str_split("a,,c", ",", 1);    // "" (empty field)
  str_split("a,b", ",", 5);     // null (past the end)
  ```

- **str_split_count(str, sep)**
  Returns the number of fields produced by splitting `str` on `sep`. An empty string returns `0`; a string with no separator returns `1`.
  ```javascript
  str_split_count("a,b,c", ",");   // 3
  ```

### Number Formatting

- **format_number(value, decimals)**
  Format `value` as a fixed-point string with the given number of decimals (clamped to 0-9).
  ```javascript
  format_number(3.14159, 2);   // "3.14"
  format_number(42, 0);        // "42"
  ```

- **pad_number(value, width)**
  Format `value` as a zero-padded integer string of at least `width` digits (clamped to 1-16).
  ```javascript
  pad_number(7, 2);    // "07"
  pad_number(123, 2);  // "123"
  ```

- **random()** / **random(max)** / **random(min, max)**
  Returns a random number from the ESP32 hardware RNG:
  - `random()` — a float in `[0, 1)`
  - `random(max)` — an integer in `[0, max)`
  - `random(min, max)` — an integer in `[min, max)`
  ```javascript
  random();         // e.g. 0.4821...
  random(6);        // integer 0-5
  random(1, 7);     // integer 1-6 (a die roll)
  ```

### WiFi Functions

- **wifi_connect(ssid, password)**  
  Connect to the WiFi network using the provided SSID and password.

- **wifi_status()**  
  Returns a boolean indicating whether the device is connected to WiFi.

- **wifi_get_ip()**  
  Returns the local IP address as a string.

### NTP Time Functions

Time is automatically synchronized via NTP when the device connects to WiFi. The timezone is configured in `webscreen.json` using a POSIX TZ string (see the WebScreen Admin tool for a full dropdown of all timezones).

- **ntp_synced()**
  Returns `true` if NTP time has been synchronized, `false` otherwise. Always check this before using other time functions.
  ```javascript
  if (ntp_synced()) {
    print("Time is available");
  }
  ```

- **get_hours()**
  Returns the current hour (0-23) in the configured timezone, or -1 if NTP is not synced.

- **get_minutes()**
  Returns the current minute (0-59), or -1 if NTP is not synced.

- **get_seconds()**
  Returns the current second (0-59), or -1 if NTP is not synced.

- **get_year()**
  Returns the current year (e.g. 2026), or -1 if NTP is not synced.

- **get_month()**
  Returns the current month (1-12), or -1 if NTP is not synced.

- **get_day()**
  Returns the current day of the month (1-31), or -1 if NTP is not synced.

- **get_weekday()**
  Returns the day of the week (0=Sunday, 6=Saturday), or -1 if NTP is not synced.

- **get_epoch()**
  Returns the current Unix timestamp in seconds, or -1 if time is not valid (before 2021).

- **format_time(fmt)** / **format_time(fmt, epoch)**
  Returns a `strftime`-formatted string of the current local time, or of the given `epoch` (interpreted in the configured local timezone). See `man strftime` for format specifiers.
  ```javascript
  format_time("%H:%M:%S");      // "14:05:09"
  format_time("%a %d %b");      // "Fri 13 Jun"
  format_time("%Y-%m-%d", 1771337025);  // format a specific epoch
  ```

**Example: Simple Clock**
```javascript
// Wait for NTP sync
if (wifi_status() && ntp_synced()) {
  let h = get_hours();
  let m = get_minutes();
  let s = get_seconds();
  let time_str = numberToString(h) + ":" +
                 (m < 10 ? "0" : "") + numberToString(m) + ":" +
                 (s < 10 ? "0" : "") + numberToString(s);
  set_text(time_label, time_str);
}
```

### HTTP Functions

- **http_get(url)**
  Perform an HTTP GET request to the specified URL. Returns the response body.
  Supports both HTTP and HTTPS, with custom ports.
  ```javascript
  // Standard ports
  http_get("http://example.com/api")       // port 80
  http_get("https://example.com/api")      // port 443

  // Custom ports
  http_get("http://192.168.1.20:2000/api")
  http_get("https://myserver.com:8443/api")
  ```

- **http_post(url, data)**
  Perform an HTTP POST request with the given data. Supports HTTP/HTTPS and custom ports.
  ```javascript
  http_post("http://192.168.1.20:3000/api", '{"key":"value"}')
  ```

- **http_delete(url)**
  Perform an HTTP DELETE request. Supports HTTP/HTTPS and custom ports.

- **http_set_ca_cert_from_sd(certificate_path)**
  Load a CA certificate from SD card for HTTPS requests.

- **http_set_header(name, value)**
  Add a custom HTTP header for subsequent requests.

- **http_clear_headers()**
  Clear all custom HTTP headers.

- **parse_json_value(json_string, key)**
  Parse a JSON string and extract the value for the specified key.

### SD Card Functions

- **sd_read_file(filepath)**  
  Read the contents of a file from the SD card.

- **sd_write_file(filepath, content)**  
  Write content to a file on the SD card.

- **sd_list_dir(directory)**  
  List the contents of a directory on the SD card.

- **sd_delete_file(filepath)**  
  Delete a file from the SD card.

### Bluetooth LE Functions

- **ble_init(device_name)**  
  Initialize BLE with the specified device name.

- **ble_is_connected()**  
  Check if a BLE client is connected.

- **ble_write(data)**  
  Send data over BLE to connected clients.

### MQTT Functions

- **mqtt_init(broker, port, client_id)**  
  Initialize MQTT connection with broker details.

- **mqtt_connect(username, password)**  
  Connect to the MQTT broker with optional credentials.

- **mqtt_publish(topic, message)**  
  Publish a message to an MQTT topic.

- **mqtt_subscribe(topic)**  
  Subscribe to an MQTT topic.

- **mqtt_loop()**  
  Process MQTT messages. Call regularly in your main loop.

- **mqtt_on_message(callback_function_name)**  
  Set the callback function name for handling incoming MQTT messages.

- **mqtt_dropped()**
  Returns the number of incoming MQTT messages that were lost because the single message slot was overwritten before the app consumed it, or because a payload was truncated. Useful for detecting that an app is polling too slowly.
  ```javascript
  if (mqtt_dropped() > 0) {
    print("Losing MQTT messages - poll faster");
  }
  ```

### UI Drawing Functions

- **draw_label(text, x, y)**  
  Draw a simple text label at the specified coordinates.

- **draw_rect(x, y, width, height [, color])**
  Draw a rectangle with the specified dimensions. Color is optional (defaults to green 0x00ff00). Returns a handle that can be used with `move_obj()`, `rotate_obj()`, etc.

- **show_image(filepath, x, y)**  
  Display an image from SD card at the specified position.

- **show_gif_from_sd(filepath, x, y)**
  Display an animated GIF from SD card at the specified position. Loading a new GIF automatically frees the previous one's buffer.
  ```javascript
  show_gif_from_sd("/animation.gif", 100, 50)
  ```
  **Note:** For best performance, keep GIFs under 50KB. Large animated GIFs may cause memory issues.

- **gif_free()**
  Delete the GIF widget created by `show_gif_from_sd()` and free its PSRAM buffer. Returns `true` if anything was freed, `false` if no GIF was loaded.
  ```javascript
  show_gif_from_sd("/animation.gif", 100, 50);
  // ... later, when the GIF is no longer needed ...
  gif_free();
  ```

### LVGL Widget Functions

#### Label Widgets

- **create_label(x, y)**
  Create a new label widget at the specified position.
  ```javascript
  let label = create_label(100, 50);
  label_set_text(label, "Hello!");
  ```

- **label_set_text(label, text)**
  Set the text content of a label.

#### Image Widgets

- **create_image(filepath, x, y)**
  Create an image widget from an SD card file at the given position. Returns an object handle, or -1 on failure.

- **create_image_from_ram(filepath, x, y)**
  Load an image file from SD card into a PSRAM buffer (a "RAM image" slot, max 16) and create an image widget from it. Returns an object handle, or -1 on failure. The slot index used is printed to the serial log; slots are assigned lowest-free-first starting at 0.

- **ram_image_free(slot)**
  Free a RAM image slot's PSRAM buffer and mark the slot reusable. Returns `true` on success, `false` for an invalid or unused slot.
  **Important:** if a live image widget still displays this slot, delete that widget first with `obj_delete()` — LVGL keeps a raw pointer to the buffer and would render freed memory.
  ```javascript
  let img = create_image_from_ram("/photo.bin", 0, 0);  // uses slot 0 if free
  // ... later ...
  obj_delete(img);
  ram_image_free(0);
  ```

#### Object Manipulation

- **rotate_obj(object, angle)**  
  Rotate an object by the specified angle.

- **move_obj(object, x, y)**  
  Move an object to the specified coordinates.

- **animate_obj(object, property, target_value, duration)**  
  Animate an object property over time.

- **obj_delete(handle)**
  Delete an object created through the handle-based API (and all of its children) and free its registry slot. Handles of deleted descendants are invalidated too, so they can never be served stale. Invalid handles are reported as an error instead of crashing.
  ```javascript
  let label = create_label(100, 50);
  // ... later ...
  obj_delete(label);
  ```

#### Object Properties

- **obj_set_size(object, width, height)**  
  Set the size of an object.

- **obj_align(object, align_type)**  
  Align an object within its parent.

- **obj_add_flag(object, flag)**  
  Add a flag to an object.

- **obj_clear_flag(object, flag)**  
  Remove a flag from an object.

- **obj_set_scroll_dir(object, direction)**  
  Set scroll direction for an object.

- **obj_set_scrollbar_mode(object, mode)**  
  Set scrollbar display mode.

- **obj_set_scroll_snap_x(object, enable)**  
  Enable/disable scroll snapping on X axis.

- **obj_set_scroll_snap_y(object, enable)**  
  Enable/disable scroll snapping on Y axis.

#### Flexbox Layout

- **obj_set_flex_flow(object, flow)**  
  Set flexbox flow direction.

- **obj_set_flex_align(object, main_align, cross_align, track_align)**  
  Set flexbox alignment properties.

### Style Functions

#### Style Creation

- **create_style()**  
  Create a new style object.

- **obj_add_style(object, style, selector)**  
  Apply a style to an object.

#### Background Styles

- **style_set_bg_color(style, color)**  
  Set background color.

- **style_set_bg_opa(style, opacity)**  
  Set background opacity.

- **style_set_radius(style, radius)**  
  Set corner radius.

#### Border Styles

- **style_set_border_color(style, color)**  
  Set border color.

- **style_set_border_width(style, width)**  
  Set border width.

- **style_set_border_opa(style, opacity)**  
  Set border opacity.

- **style_set_border_side(style, sides)**  
  Set which sides have borders.

#### Text Styles

- **style_set_text_color(style, color)**  
  Set text color.

- **style_set_text_font(style, font)**  
  Set text font.

- **style_set_text_align(style, align)**  
  Set text alignment.

- **style_set_text_letter_space(style, space)**  
  Set letter spacing.

- **style_set_text_line_space(style, space)**  
  Set line spacing.

- **style_set_text_decor(style, decoration)**  
  Set text decoration (underline, strikethrough, etc.).

#### Padding and Sizing

- **style_set_pad_all(style, padding)**  
  Set padding on all sides.

- **style_set_pad_left(style, padding)**  
  Set left padding.

- **style_set_pad_right(style, padding)**  
  Set right padding.

- **style_set_pad_top(style, padding)**  
  Set top padding.

- **style_set_pad_bottom(style, padding)**  
  Set bottom padding.

- **style_set_width(style, width)**  
  Set object width.

- **style_set_height(style, height)**  
  Set object height.

- **style_set_x(style, x)**  
  Set X position.

- **style_set_y(style, y)**  
  Set Y position.

#### Shadow and Outline

- **style_set_shadow_width(style, width)**  
  Set shadow width.

- **style_set_shadow_color(style, color)**  
  Set shadow color.

- **style_set_shadow_ofs_x(style, offset)**  
  Set shadow X offset.

- **style_set_shadow_ofs_y(style, offset)**  
  Set shadow Y offset.

- **style_set_outline_width(style, width)**  
  Set outline width.

- **style_set_outline_color(style, color)**  
  Set outline color.

- **style_set_outline_pad(style, padding)**  
  Set outline padding.

#### Image Styles

- **style_set_img_recolor(style, color)**  
  Set image recolor.

- **style_set_img_recolor_opa(style, opacity)**  
  Set image recolor opacity.

#### Transform Styles

- **style_set_transform_angle(style, angle)**  
  Set transformation angle.

#### Line Styles

- **style_set_line_color(style, color)**  
  Set line color.

- **style_set_line_width(style, width)**  
  Set line width.

- **style_set_line_rounded(style, rounded)**  
  Enable/disable rounded line ends.

### Advanced Widgets

#### Meter Widget

Since the LVGL 9.5 migration the meter functions are implemented on top of the
`lv_scale` widget (LVGL removed `lv_meter` in v9). The JavaScript API below is
unchanged; two cosmetic parameters are accepted but ignored because v9 has no
equivalent: the gradient color of `lv_meter_add_scale_lines` and the
`label_gap` of `lv_meter_set_scale_major_ticks`.

Scales and indicators are returned to JavaScript as small slot-index handles (not pointers): `lv_meter_add_scale()` returns a scale handle and the `lv_meter_add_*` indicator functions return indicator handles, with **-1 on failure**. Pass the returned handle back into the corresponding `lv_meter_set_*` functions; an invalid handle produces an error instead of crashing the device.

- **lv_meter_create(parent)**  
  Create a meter widget. Returns an object handle.

- **lv_meter_add_scale(meter)**  
  Add a scale to the meter. Returns a scale handle, or -1 on failure.

- **lv_meter_set_scale_ticks(meter, scale, count, width, length, color)**  
  Configure scale tick marks; `count` must be 2–1000.

- **lv_meter_set_scale_major_ticks(meter, scale, nth, width, length, color, label_gap)**  
  Configure major tick marks with labels; `nth` must be at least 1.

- **lv_meter_set_scale_range(meter, scale, min, max, angle_range, rotation)**  
  Set the scale's value range and angular range. Require `max > min`, an angle range of 1–360 degrees, and `(max - min) * angle_range <= 2147483647`; invalid ranges are ignored.

- **lv_meter_add_arc(meter, scale, width, color, r_mod)**
  Add an arc indicator to the meter. Returns an indicator handle, or -1 on failure.

- **lv_meter_add_scale_lines(meter, scale, color_main, color_grad, local, width_mod)**
  Add scale lines to the meter. Returns an indicator handle, or -1 on failure.

- **lv_meter_add_needle_line(meter, scale, width, color, r_mod)**  
  Add a needle line indicator. Returns an indicator handle, or -1 on failure.

- **lv_meter_add_needle_img(meter, scale, img_src, pivot_x, pivot_y)**  
  Add a needle image indicator. Returns an indicator handle, or -1 on failure.

- **lv_meter_set_indicator_value(meter, indicator, value)**  
  Set the value of an indicator.

Scale and indicator handles must belong to the supplied meter. Calls with mismatched handles are ignored.

#### Chart Widget

Charts plot one or more data series. `lv_chart_create()` returns an object handle; `lv_chart_add_series()` returns a small slot-index series handle (not a pointer), with **-1 on failure**. Pass both handles back into the `lv_chart_set_next_value*` functions. Series must belong to that chart; `lv_chart_set_next_value2` requires a scatter chart. The various axis/range constants (`LV_CHART_TYPE_*`, `LV_CHART_AXIS_*`, `LV_CHART_UPDATE_MODE_*`) are passed as integers.

- **lv_chart_create(parent)**  
  Create a chart widget. Returns an object handle.

- **lv_chart_set_type(chart, type)**  
  Use the preserved LVGL 8 numeric API: **0 = none, 1 = line, 2 = bar, 3 = scatter**. These values differ from the LVGL 9.5 C enum; invalid types are ignored.

- **lv_chart_set_div_line_count(chart, y_div, x_div)**  
  Set the number of horizontal and vertical division lines.

- **lv_chart_set_update_mode(chart, mode)**  
  Set the update mode (e.g. `LV_CHART_UPDATE_MODE_SHIFT`, `LV_CHART_UPDATE_MODE_CIRCULAR`).

- **lv_chart_set_range(chart, axis, min, max)**  
  Set the value range for an axis (e.g. `LV_CHART_AXIS_PRIMARY_Y`).

- **lv_chart_set_point_count(chart, count)**  
  Set the number of points shown per series (1–4096; invalid counts are ignored).

- **lv_chart_refresh(chart)**  
  Redraw the chart after changing its data.

- **lv_chart_add_series(chart, color, axis)**  
  Add a data series with the given color and axis. Returns a series handle, or -1 on failure.

- **lv_chart_set_next_value(chart, series, value)**  
  Append the next Y value to a series (for line/bar charts).

- **lv_chart_set_next_value2(chart, series, x_value, y_value)**  
  Append the next X/Y pair to a series (for scatter charts).

- **lv_chart_set_axis_tick(chart, axis, major_len, minor_len, major_count, minor_count, label_enable, draw_size)**  
  No-op since LVGL 9.5 (the underlying API was removed upstream). Kept so older scripts keep running.

- **lv_chart_set_zoom_x(chart, zoom)**  
  No-op since LVGL 9.5, same reason as above.

- **lv_chart_set_zoom_y(chart, zoom)**  
  No-op since LVGL 9.5, same reason as above.

#### Spangroup Widget (Rich Text)

- **lv_spangroup_create(parent)**  
  Create a spangroup widget for rich text.

- **lv_spangroup_set_align(spangroup, align)**  
  Set text alignment in spangroup.

- **lv_spangroup_set_overflow(spangroup, overflow)**  
  Set text overflow behavior.

- **lv_spangroup_set_indent(spangroup, indent)**  
  Set text indentation.

- **lv_spangroup_set_mode(spangroup, mode)**  
  Set spangroup display mode.

- **lv_spangroup_new_span(spangroup)**  
  Create a new span within the spangroup. Returns a span handle (small slot index, max 16 spans), or -1 if no slot is free. Pass this handle to the `lv_span_set_text*` functions; an invalid handle produces an error instead of crashing.

- **lv_span_set_text(span, text)**  
  Set text content of a span. The text is copied into LVGL-owned storage.

- **lv_span_set_text_static(span, text)**  
  Same as `lv_span_set_text()` — the name is kept for compatibility, but the text is always copied. (A truly static reference would point into the JS arena, which the garbage collector moves.)

- **lv_spangroup_refr_mode(spangroup)**  
  Refresh the spangroup display.

#### Line Widget

- **lv_line_create(parent)**  
  Create a line widget.

- **lv_line_set_points(line, points)**  
  Set the points that define the line.

## Usage Examples

### Basic WiFi and HTTP

```javascript
// Connect to WiFi
wifi_connect("MyNetwork", "MyPassword");

// Wait for connection
while (!wifi_status()) {
  delay(1000);
  print("Connecting to WiFi...");
}

// Make HTTP request
let response = http_get("https://api.example.com/data");
print("Response: " + response);
```

### MQTT Communication

```javascript
// Initialize MQTT
mqtt_init("mqtt.example.com", 1883, "webscreen_device");
mqtt_connect("username", "password");

// Publish data
mqtt_publish("sensors/temperature", "25.5");

// Subscribe to topic
mqtt_subscribe("commands/display");

// Set callback for messages
mqtt_on_message("handleMqttMessage");

function handleMqttMessage(topic, message) {
  print("Received: " + topic + " = " + message);
}

// Main loop
while (true) {
  mqtt_loop();
  delay(100);
}
```

### UI Creation

```javascript
// Create a label
let label = create_label(null);
label_set_text(label, "Hello WebScreen!");

// Create and apply styles
let style = create_style();
style_set_text_color(style, "#FF0000");
style_set_bg_color(style, "#FFFFFF");
style_set_pad_all(style, 10);
obj_add_style(label, style, 0);

// Position the label
obj_align(label, "center");
```

### File Operations

```javascript
// Read configuration
let config = sd_read_file("/config.json");
let settings = parse_json_value(config, "settings");

// Write log file
let timestamp = numberToString(Date.now());
sd_write_file("/logs/" + timestamp + ".txt", "Application started");

// List directory contents
let files = sd_list_dir("/apps/");
print("Available apps: " + files);
```

## Error Handling

All functions return appropriate values to indicate success or failure. Always check return values for robust applications:

```javascript
if (!wifi_connect("MyNetwork", "MyPassword")) {
  print("WiFi connection failed");
  return;
}

let response = http_get("https://api.example.com/data");
if (response === null || response === "") {
  print("HTTP request failed");
  return;
}
```

### Error Messages and Line Numbers

JavaScript errors carry a 1-based source line number suffix, e.g.:

```
ERROR: 'foo' not found (line 12)
```

For an error raised inside a function body, the line number is **relative to that function's first line** (Elk re-parses each function body as a separate snippet), not the line within the whole file. Errors from timer callbacks, button callbacks, and `/eval` are also recorded and can be reviewed with the `/errors` serial command.

### Runtime Guards

Runaway scripts no longer crash or reboot the device — they get a JavaScript error instead:

- **Step limit**: each evaluation (the initial script run, and each timer callback) has a statement budget of 2,000,000 statements. An infinite loop terminates with a `step limit` error.
- **C-stack guard**: deeply recursive code terminates with a `C stack` error instead of overflowing the task stack and crashing the device.

Errors raised in a timer callback are printed to the serial console together with arena and heap statistics. After 10 consecutive failing callbacks, the firmware restarts the JavaScript app in place (no device reboot); if the script keeps failing, the error is shown on the display and the device stays alive so you can fix the script over serial (`/load`, `/restart_app`).

## Performance Considerations

- Use `delay()` appropriately to prevent blocking the system
- Call `mqtt_loop()` regularly when using MQTT
- Minimize frequent file I/O operations
- Cache frequently accessed data in variables
- Use appropriate data types for memory efficiency

## LVGL Configuration

WebScreen uses LVGL 9.5 with the following configuration:

### Display Settings
- **Color Depth**: 16-bit (RGB565)
- **DPI**: 130
- **Refresh Period**: 30ms

### Available Fonts

The following Montserrat font sizes are enabled:

| Size | Constant | Usage |
|------|----------|-------|
| 14 | `14` | Default font, small text |
| 20 | `20` | Body text |
| 28 | `28` | Subheadings |
| 34 | `34` | Medium headings |
| 40 | `40` | Large headings |
| 44 | `44` | Extra large |
| 48 | `48` | Display text |

```javascript
// Example: Set font size
style_set_text_font(style, 48);  // Use largest font
style_set_text_font(style, 14);  // Use smallest/default font
```

**Note:** Only these specific sizes are available. Using other sizes (e.g., 16, 24, 32) will not work.

### Enabled Widgets

| Widget | Status | Notes |
|--------|--------|-------|
| Arc | ✅ Enabled | Circular progress/gauge |
| Button | ✅ Enabled | Clickable buttons |
| Button Matrix | ✅ Enabled | Grid of buttons |
| Canvas | ✅ Enabled | Custom drawing |
| Image | ✅ Enabled | Display images |
| Label | ✅ Enabled | Text display |
| Line | ✅ Enabled | Line drawing |
| Chart | ✅ Enabled | Data visualization |
| Scale | ✅ Enabled | Gauge/speedometer (backs the meter API) |
| Span | ✅ Enabled | Rich text |

### Disabled Widgets

The following widgets are **not available** to save memory:
- Bar, Slider, Switch
- Checkbox, Dropdown, Roller
- Textarea, Table
- Calendar, Keyboard
- List, Menu, Message Box
- Spinbox, Spinner
- Tabview, Tileview, Window

### Supported Image Formats

| Format | Status | Notes |
|--------|--------|-------|
| PNG | ✅ Enabled | Recommended for icons |
| JPG | ✅ Enabled | Baseline JPG via TJpgDec. SJPG (split JPG) is not supported by LVGL 9 |
| GIF | ✅ Enabled | Animated images |
| BMP | ❌ Disabled | Not supported |

### Layout Systems

- **Flexbox** (`LV_USE_FLEX`): ✅ Enabled - CSS-like flexible layouts
- **Grid** (`LV_USE_GRID`): ✅ Enabled - CSS-like grid layouts

### Themes

- **Default Theme**: ✅ Enabled (Dark mode)
- **Basic Theme**: ✅ Enabled
- **Mono Theme**: ✅ Enabled

### Drawing Features

- Complex drawing (shadows, gradients, rounded corners): ✅ Enabled
- Text selection: ✅ Enabled
- UTF-8 encoding: ✅ Enabled

## Memory Guidelines

WebScreen uses the Elk JavaScript engine with **256KB of heap memory** (the "JS arena") allocated in PSRAM by default. The size is configurable with the flat `"js_heap_kb"` key in `webscreen.json` (clamped to 64-1024 KB). Garbage collection runs automatically when the arena fills up; use `mem_info()` and `gc()` to observe and influence it. To ensure stable operation:

### Script Size
- Keep scripts under **3KB** for best stability
- Larger scripts may work but consume more memory during parsing

### Styles and Labels
- Limit the number of styles to **5 or fewer** per app
- Limit the number of labels to **10 or fewer** per app
- Reuse styles across multiple labels when possible

### GIF Animations
- Keep GIFs under **50KB** for reliable playback
- Use small dimensions (100x100 or less)
- Reduce the number of frames and colors
- Large GIFs (>100KB) may cause cache errors and crashes

### Timer Callbacks
- Use **one timer callback** when possible
- Each additional timer consumes memory
- Keep callback functions simple

### Example: Memory-Efficient App
```javascript
"use strict";

// Minimal styles (reuse where possible)
let bigStyle = create_style();
style_set_text_font(bigStyle, 48);
style_set_text_color(bigStyle, 0xFFFFFF);
style_set_text_align(bigStyle, 1);

let smallStyle = create_style();
style_set_text_font(smallStyle, 20);
style_set_text_color(smallStyle, 0x888888);

// Create labels
let title = create_label(268, 80);
obj_add_style(title, bigStyle, 0);
label_set_text(title, "Hello");

let subtitle = create_label(268, 140);
obj_add_style(subtitle, smallStyle, 0);
label_set_text(subtitle, "World");

// Single timer callback
let update = function() {
  // Keep it simple
  label_set_text(title, "Updated");
};

create_timer("update", 1000);
```

This API provides comprehensive access to WebScreen's hardware and software capabilities, enabling the creation of sophisticated embedded applications with rich user interfaces and network connectivity.