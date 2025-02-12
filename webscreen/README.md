# WebScreen JavaScript API Reference

WebScreen exposes a rich set of functions to JavaScript applications running on the ESP32 with LVGL and the Elk engine. By using the configuration file (e.g., `webscreen.json`), the user can select which JavaScript file to execute. For example, a sample configuration file might look like:

```json
{
  "settings": {
    "wifi": {
      "ssid": "SSID",
      "pass": "Password"
    }
  },
  "script": "timeapi_app.js",
  "last_read": 2
}
```

In this example, the key `"script"` indicates that the file `/timeapi_app.js` will be loaded from the SD card. (If not specified, the default is set to `"app.js"`.)

---

## Exposed Functions

The following is a categorized list of all functions that are bridged from C/C++ to JavaScript:

### Basic Functions
- **print(msg)**  
  Print a message to the serial console.
  
- **delay(ms)**  
  Pause execution for the specified number of milliseconds.

### Wi‑Fi Functions
- **wifi_connect(ssid, pass)**  
  Connect to the Wi‑Fi network using the provided SSID and password.

- **wifi_status()**  
  Returns a boolean indicating whether the device is connected to Wi‑Fi.

- **wifi_get_ip()**  
  Returns the local IP address as a string.

### String Utilities
- **str_index_of(haystack, needle)**  
  Returns the index of `needle` in `haystack` (or -1 if not found).

- **str_substring(str, start, length)**  
  Returns a substring of `str` starting at `start` with the given `length`.

### HTTP Functions
- **http_get(url)**  
  Makes an HTTPS GET request to the provided URL and returns the raw response as a string.

- **http_post(url, body)**  
  Sends an HTTPS POST request with a JSON body. Returns the raw response as a string.

- **http_delete(url)**  
  Sends an HTTPS DELETE request to the given URL.

- **http_set_ca_cert_from_sd(path)**  
  Loads an SSL certificate (in PEM format) from the SD card.  
  _Note:_ Use this to supply a full chain certificate for secure API calls.

- **http_set_header(key, value)**  
  Sets a custom HTTP header that will be included in subsequent requests.

- **http_clear_headers()**  
  Clears all custom HTTP headers.

- **parse_json_value(jsonString, key)**  
  Parses a JSON string and extracts the value associated with the specified key.

### SD Card Functions
- **sd_read_file(path)**  
  Reads the content of a file from the SD card.

- **sd_write_file(path, data)**  
  Writes data to a file on the SD card.

- **sd_list_dir(path)**  
  Returns a list of files in the specified directory.

- **sd_delete_file(path)**  
  Deletes a file from the SD card.

### BLE (Bluetooth Low Energy) Functions
- **ble_init(devName, serviceUUID, charUUID)**  
  Initializes the BLE service with the given device name, service, and characteristic UUIDs.

- **ble_is_connected()**  
  Returns whether a BLE client is connected.

- **ble_write(data)**  
  Sends data via BLE.

### GIF and Image Functions
- **show_gif_from_sd(path)**  
  Loads a GIF file from the SD card into memory and displays it.

- **create_image(path, x, y)**  
  Creates an image object from a file on the SD card at the given coordinates. Returns a handle.

- **create_image_from_ram(path, x, y)**  
  Loads an image file into RAM and creates an image object. Returns a handle.

- **rotate_obj(handle, angle)**  
  Rotates the object identified by the handle by the specified angle.

- **move_obj(handle, x, y)**  
  Moves the object to the specified (x, y) coordinates.

- **animate_obj(handle, x0, y0, x1, y1, duration)**  
  Animates an object from a starting position `(x0, y0)` to an ending position `(x1, y1)` over the given duration (in milliseconds).

### UI Drawing and Label Functions
- **draw_label(text, x, y)**  
  Draws a label with the specified text at (x, y).  
  _Alternate:_ Use **create_label(x, y)** which returns a handle, then **label_set_text(handle, text)** to update the text.

- **draw_rect(x, y, w, h)**  
  Draws a rectangle at position (x, y) with width `w` and height `h`.

- **show_image(path, x, y)**  
  Displays an image loaded from the SD card at (x, y).

### Style and Object Property Functions
- **create_style()**  
  Creates a new style object and returns a handle.

- **obj_add_style(objHandle, styleHandle, partOrState)**  
  Applies a style to an object.

The following functions allow you to set various style properties:
- **style_set_radius(styleHandle, radius)**
- **style_set_bg_opa(styleHandle, opaValue)**
- **style_set_bg_color(styleHandle, colorHex)**
- **style_set_border_color(styleHandle, colorHex)**
- **style_set_border_width(styleHandle, width)**
- **style_set_border_opa(styleHandle, opaValue)**
- **style_set_border_side(styleHandle, sideFlags)**
- **style_set_outline_width(styleHandle, width)**
- **style_set_outline_color(styleHandle, colorHex)**
- **style_set_outline_pad(styleHandle, pad)**
- **style_set_shadow_width(styleHandle, width)**
- **style_set_shadow_color(styleHandle, colorHex)**
- **style_set_shadow_ofs_x(styleHandle, offset)**
- **style_set_shadow_ofs_y(styleHandle, offset)**
- **style_set_img_recolor(styleHandle, colorHex)**
- **style_set_img_recolor_opa(styleHandle, opaValue)**
- **style_set_transform_angle(styleHandle, angle)**
- **style_set_text_color(styleHandle, colorHex)**
- **style_set_text_letter_space(styleHandle, spacing)**
- **style_set_text_line_space(styleHandle, spacing)**
- **style_set_text_font(styleHandle, fontSize)**
- **style_set_text_align(styleHandle, alignMode)** (0 = LEFT, 1 = CENTER, 2 = RIGHT, 3 = AUTO)
- **style_set_text_decor(styleHandle, decorFlags)**
- **style_set_line_color(styleHandle, colorHex)**
- **style_set_line_width(styleHandle, width)**
- **style_set_line_rounded(styleHandle, bool)**
- **style_set_pad_all(styleHandle, pad)**
- **style_set_pad_left(styleHandle, pad)**
- **style_set_pad_right(styleHandle, pad)**
- **style_set_pad_top(styleHandle, pad)**
- **style_set_pad_bottom(styleHandle, pad)**
- **style_set_pad_ver(styleHandle, pad)**
- **style_set_pad_hor(styleHandle, pad)**
- **style_set_width(styleHandle, width)**
- **style_set_height(styleHandle, height)**
- **style_set_x(styleHandle, xValue)**
- **style_set_y(styleHandle, yValue)**

Additional object manipulation functions include:
- **obj_set_size(handle, w, h)**
- **obj_align(handle, alignConst, xOffset, yOffset)**
- **obj_set_scroll_snap_x(handle, mode)**
- **obj_set_scroll_snap_y(handle, mode)**
- **obj_add_flag(handle, flag)**
- **obj_clear_flag(handle, flag)**
- **obj_set_scroll_dir(handle, direction)**
- **obj_set_scrollbar_mode(handle, mode)**
- **obj_set_flex_flow(handle, flowEnum)**
- **obj_set_flex_align(handle, main, cross, track)**
- **obj_set_style_clip_corner(handle, enable, part)**
- **obj_set_style_base_dir(handle, baseDir, part)**

### Meter, MsgBox, Span, Window, TileView, and Line Functions
- **lv_meter_create()**  
  Creates a new meter widget.
  
- **lv_meter_add_scale()**, **lv_meter_set_scale_ticks()**, **lv_meter_set_scale_major_ticks()**, **lv_meter_set_scale_range()**  
  Configure the meter's scale and tick marks.
  
- **lv_meter_add_arc()**, **lv_meter_add_scale_lines()**, **lv_meter_add_needle_line()**, **lv_meter_add_needle_img()**  
  Add different types of indicators to the meter.
  
- **lv_meter_set_indicator_start_value()**, **lv_meter_set_indicator_end_value()**, **lv_meter_set_indicator_value()**  
  Set indicator values on the meter.

- **lv_msgbox_create(title, text, buttonArray, isModal)**  
  Creates a message box with a title, text, and an array of button labels.
  
- **lv_msgbox_get_active_btn_text()**  
  Returns the text of the currently active button.

- **lv_spangroup_create()**, **lv_spangroup_set_align()**, **lv_spangroup_set_overflow()**, **lv_spangroup_set_indent()**, **lv_spangroup_set_mode()**, **lv_spangroup_new_span()**  
  Functions for creating and managing a group of spans (text segments).

- **lv_span_set_text(spanHandle, text)** and **lv_span_set_text_static(spanHandle, text)**
  
- **lv_win_create(parent, headerHeight)**, **lv_win_add_btn()**, **lv_win_add_title()**, **lv_win_get_content()**  
  Create and manage window objects.

- **lv_tileview_create()**, **lv_tileview_add_tile()**  
  Functions for creating tile views.

- **lv_line_create()**, **lv_line_set_points()**  
  Create a line object and set its points.

### MQTT Functions
- **mqtt_init(broker, port)**
- **mqtt_connect(clientID, [user, pass])**
- **mqtt_publish(topic, message)**
- **mqtt_subscribe(topic)**
- **mqtt_loop()**
- **mqtt_on_message(callback)**  
  Set a JavaScript callback function to be invoked when an MQTT message is received.

---

## Example Applications

Below are several example JavaScript applications that demonstrate the use of the API:

### Example 1: Time API Application

```js
"use strict";

print("Starting JavaScript execution...");

// 1) Try to load CA cert from SD
let certOk = http_set_ca_cert_from_sd("/timeapi.pem");
if(!certOk) {
  print("Could not load CA from /timeapi.pem. We'll use setInsecure() fallback.");
}

// 2) Connect to Wi-Fi
print("Connecting to Wi-Fi...");
let ok = wifi_connect("SSID", "Password");
if(!ok) {
    print("Wi-Fi connection failed!");
    return;
}

// 3) Wait until connected (use for(;;) since 'while' isn't in Elk)
for(;;) {
    if (wifi_status()) break;
    delay(500);
    print("Waiting for Wi-Fi...");
}
print("Connected! IP: " + wifi_get_ip());

// 4) Fetch JSON from timeapi.io
let url = "https://timeapi.io/api/time/current/zone?timeZone=Asia/Tokyo";
let jsonResponse = http_get(url);
if(jsonResponse === "") {
    print("HTTP GET failed, can't continue.");
    return;
}

// 5) Parse "date" and "time" fields from the JSON
let dateVal = parse_json_value(jsonResponse, "date");
let timeVal = parse_json_value(jsonResponse, "time");
print("Got date=" + dateVal + ", time=" + timeVal);

// 6) Create a style for the date label
let dateStyle = create_style();
style_set_text_font(dateStyle, 40);
style_set_text_color(dateStyle, 0xFFFFFF);
style_set_bg_color(dateStyle, 0x000000);
style_set_pad_all(dateStyle, 5);
style_set_text_align(dateStyle, 1);

// 7) Create and style the date label
let dateLabel = create_label(20, 60);
obj_add_style(dateLabel, dateStyle, 0);
label_set_text(dateLabel, "Date: " + dateVal);

// 8) Create a style for the time label
let timeStyle = create_style();
style_set_text_font(timeStyle, 28);
style_set_text_color(timeStyle, 0x00FFFF);
style_set_bg_color(timeStyle, 0x000000);
style_set_pad_all(timeStyle, 5);
style_set_text_align(timeStyle, 1);

// 9) Create and style the time label
let timeLabel = create_label(20, 120);
obj_add_style(timeLabel, timeStyle, 0);
label_set_text(timeLabel, "Time: " + timeVal);

print("Script execution completed.");
```

### Example 2: Animated Scrolling Label with GIF

```js
"use strict";

print("Starting JavaScript execution...");

// Connect to Wi-Fi
if (wifi_connect("SSID", "Password")) {
  print("Wi-Fi connected successfully. IP: " + wifi_get_ip());
} else {
  print("Failed to connect to Wi-Fi.");
}

// Create a scrolling label
let labelHandle = draw_label(
  "This is a long text that will be scrolled automatically. Add more text to see the effect.",
  150, 240
);
print("Label created with handle: " + labelHandle);

// Create a GIF image object
let gifHandle = create_image("/messi.png", 0, 0);
print("GIF created with handle: " + gifHandle);

// Hide the GIF initially (using a flag value, e.g., 1 for hidden)
obj_add_flag(gifHandle, 1);

// Animate the label to scroll from y=240 to y=-100 over 10 seconds
animate_obj(labelHandle, 150, 240, 150, -100, 10000);
print("Scroll animation initiated.");

// Wait for animation (simulate delay)
delay(10000);
print("Animation duration elapsed.");

// Switch visibility: Hide the label and show the GIF
obj_add_flag(labelHandle, 1);
obj_clear_flag(gifHandle, 1);
print("Label hidden and GIF displayed.");
  
// List SD card files
let files = sd_list_dir("/");
if (files) {
  print("Files on SD card:\n" + files);
} else {
  print("Failed to list files or directory is empty.");
}

print("Script execution completed.");
```

### Example 3: Scrolling Label with Toggle via Serial Input

```js
"use strict";

print("Starting JavaScript execution...");

// Connect to Wi-Fi
if (wifi_connect("SSID", "Password")) {
  print("Wi-Fi connected successfully. IP: " + wifi_get_ip());
} else {
  print("Failed to connect to Wi-Fi.");
}

// Create a scrolling label
let labelHandle = draw_label(
  "This is a long text that will be scrolled. Modify it via serial input.",
  150, 240, 525, 100, 1
);
print("Label created with handle: " + labelHandle);

// Create a GIF object and hide it initially
let gifHandle = create_gif("/wallpaper.gif", 168, 120, 100);
set_obj_hidden(gifHandle, true);
print("GIF created with handle: " + gifHandle);

// Function to start a scrolling animation
function createScrollAnimation(handle, startY, endY, duration) {
  animate_obj(handle, 0, startY, 0, endY, duration);
}

// Start scroll animation for the label
createScrollAnimation(labelHandle, 240, -100, 10000);
print("Scroll animation started.");

// Callback for animation completion: switch label and GIF visibility
function onAnimationComplete() {
  set_obj_hidden(labelHandle, true);
  set_obj_hidden(gifHandle, false);
  print("Animation completed: label hidden, GIF shown.");
}
setTimeout(onAnimationComplete, 10000);

// Callback for serial input to update label text and restart animation
function onSerialInput(receivedText) {
  print("Received serial input: " + receivedText);
  label_set_text(labelHandle, receivedText);
  set_obj_hidden(labelHandle, false);
  set_obj_hidden(gifHandle, true);
  createScrollAnimation(labelHandle, 240, -100, 10000);
}
register_serial_callback(onSerialInput);

print("Script execution completed.");
```

---

## Building and Using a Full Chain Certificate

For secure HTTPS connections (such as when calling APIs like [timeapi.io](https://timeapi.io)), you must supply a full chain certificate. A full chain certificate contains:

1. **Server Certificate:** The certificate issued to the server.
2. **Intermediate Certificate(s):** Certificate(s) that chain the server certificate to a trusted root.
3. **Root Certificate:** The trusted root certificate (often optional in the chain file).

### How to Build a Full Chain Certificate

1. **Obtain the Certificates:**  
   - If you are using a certificate authority (CA) such as Let's Encrypt, you will have a server certificate and an intermediate certificate.  
   - You can download the root certificate from your CA’s website if needed.
   - Another option is using [openssl](https://wiki.openssl.org/index.php/Command_Line_Utilities) command, for example `openssl s_client -connect timeapi.io:443 -servername timeapi.io -showcerts > timeapi.pem`. If you follow this method you can skip the next step.

2. **Combine Certificates into One File:**  
   Using a text editor or the `cat` command (on Unix/Linux), concatenate the certificates in the following order:
   - **Server Certificate**
   - **Intermediate Certificate(s)**
   - **(Optionally) Root Certificate**

   For example, on Linux:
   ```bash
   cat server.crt intermediate.crt root.crt > fullchain.pem
   ```

3. **Save in PEM Format:**  
   Ensure the file is in PEM format. The file should include the delimiters:
   ```
   -----BEGIN CERTIFICATE-----
   ... (base64 data) ...
   -----END CERTIFICATE-----
   ```

4. **Deploy on SD Card:**  
   Copy the `fullchain.pem` file to the SD card (for example, to the root directory) so that it can be loaded by our firmware.

5. **Usage in the Application:**  
   Call the function `http_set_ca_cert_from_sd("/fullchain.pem")` in your JavaScript code to load the certificate. This enables secure HTTPS connections by validating the server's certificate chain.

*Note:* In our example, you provided a certificate that includes multiple `-----BEGIN CERTIFICATE-----` sections. This is an example of a full chain certificate. When your application calls `http_set_ca_cert_from_sd()`, it loads the entire chain from SD so that secure connections can be established.
