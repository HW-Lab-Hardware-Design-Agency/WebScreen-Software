#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include <stdio.h>

// 1) Include your pin + display config
#include "pins_config.h"

// 2) LVGL + display driver
#include <lvgl.h>
#include "rm67162.h"
#include "notification.h"  // Must define extern const lv_img_dsc_t notification

// 3) Elk
extern "C" {
  #include "elk.h"
}

// ------------------ Elk Memory Buffer ------------------
static uint8_t elk_memory[4096]; // Increase if needed

// ------------------ Global Elk Instance ----------------
struct js *js = NULL;

// ------------------ LVGL + Display ---------------------
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf = NULL;

/**
 * Flush callback for LVGL to push data to the rm67162 driver.
 */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  // Convert lv_color_t to uint16_t * if needed
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);

  lv_disp_flush_ready(disp);
}

/**
 * Initialize the display and LVGL.
 */
void init_lvgl_display() {
  Serial.println("Initializing display...");

  // Turn on backlight / screen power (board-specific)
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  // Init your AMOLED display
  rm67162_init();
  // rotate if needed
  lcd_setRotation(1);

  // Initialize LVGL
  lv_init();

  // Allocate buffer from PSRAM (if available)
  buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE);
  if (!buf) {
    Serial.println("Failed to allocate LVGL buffer in PSRAM");
    return;
  }

  // Initialize draw buffer
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_LCD_BUF_SIZE);

  // Register the display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;  // from pins_config.h
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;  // from pins_config.h
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  Serial.println("LVGL + Display initialized.");
}

/**
 * Must be called periodically in loop() to update LVGL tasks.
 */
void lvgl_loop() {
  lv_timer_handler();
}

// -------------- Basic Elk Functions (Wi-Fi, SD, Print) --------------
static jsval_t js_print(struct js *js, jsval_t *args, int nargs) {
  for (int i = 0; i < nargs; i++) {
    const char *str = js_str(js, args[i]);
    if (str) Serial.println(str);
    else     Serial.println("print: argument is not a string");
  }
  return js_mknull();
}

static jsval_t js_wifi_connect(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) return js_mkfalse();
  const char *ssid_with_quotes = js_str(js, args[0]);
  const char *pass_with_quotes = js_str(js, args[1]);
  if (!ssid_with_quotes || !pass_with_quotes) return js_mkfalse();

  String ssid(ssid_with_quotes);
  String pass(pass_with_quotes);
  if (ssid.startsWith("\"") && ssid.endsWith("\"")) {
    ssid = ssid.substring(1, ssid.length() - 1);
  }
  if (pass.startsWith("\"") && pass.endsWith("\"")) {
    pass = pass.substring(1, pass.length() - 1);
  }

  Serial.printf("Connecting to Wi-Fi SSID: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  int attempts = 20;
  while (WiFi.status() != WL_CONNECTED && attempts > 0) {
    delay(500);
    Serial.print(".");
    attempts--;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected");
    return js_mktrue();
  } else {
    Serial.println("Failed to connect to Wi-Fi");
    return js_mkfalse();
  }
}

static jsval_t js_wifi_status(struct js *js, jsval_t *args, int nargs) {
  return (WiFi.status() == WL_CONNECTED) ? js_mktrue() : js_mkfalse();
}

static jsval_t js_wifi_get_ip(struct js *js, jsval_t *args, int nargs) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to Wi-Fi");
    return js_mknull();
  }
  IPAddress ip = WiFi.localIP();
  String ipStr = ip.toString();
  return js_mkstr(js, ipStr.c_str(), ipStr.length());
}

static jsval_t js_delay(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  double ms = js_getnum(args[0]);
  delay((unsigned long)ms);
  return js_mknull();
}

static jsval_t js_sd_read_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  const char *path = js_str(js, args[0]);
  if (!path) return js_mknull();

  File file = SD_MMC.open(path);
  if (!file) {
    Serial.printf("Failed to open file: %s\n", path);
    return js_mknull();
  }
  String content = file.readString();
  file.close();
  return js_mkstr(js, content.c_str(), content.length());
}

static jsval_t js_sd_write_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) return js_mkfalse();
  const char *path = js_str(js, args[0]);
  const char *data = js_str(js, args[1]);
  if (!path || !data) return js_mkfalse();

  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    Serial.printf("Failed to open file for writing: %s\n", path);
    return js_mkfalse();
  }
  file.write((const uint8_t *)data, strlen(data));
  file.close();
  return js_mktrue();
}

static jsval_t js_sd_list_dir(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  const char *path_with_quotes = js_str(js, args[0]);
  if (!path_with_quotes) return js_mknull();

  String path(path_with_quotes);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }

  File root = SD_MMC.open(path);
  if (!root) {
    Serial.printf("Failed to open directory: %s\n", path.c_str());
    return js_mknull();
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    root.close();
    return js_mknull();
  }

  // buffer for listing
  char fileList[512];
  int fileListLen = 0;

  File f = root.openNextFile();
  while (f) {
    const char* type = f.isDirectory() ? "DIR: " : "FILE: ";
    const char* name = f.name();

    int len = snprintf(fileList + fileListLen,
                       sizeof(fileList) - fileListLen,
                       "%s%s\n", type, name);
    if (len < 0 || len >= (int)(sizeof(fileList) - fileListLen)) {
      break; // buffer full or error
    }
    fileListLen += len;
    f = root.openNextFile();
  }
  root.close();

  return js_mkstr(js, fileList, fileListLen);
}

// ------------------ Simple LVGL Placeholders ------------------
static jsval_t js_lvgl_display_label(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mknull();
  const char *text = js_str(js, args[0]);
  Serial.printf("[Placeholder] lvgl_display_label: %s\n", text);
  return js_mknull();
}

static jsval_t js_lvgl_display_image(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mknull();
  const char *path = js_str(js, args[0]);
  Serial.printf("[Placeholder] lvgl_display_image: %s\n", path);
  return js_mknull();
}

// ------------------ Advanced LVGL Calls (Handles, GIF, etc.) ------------------
static const int MAX_OBJECTS = 16;
static lv_obj_t *g_lv_obj_map[MAX_OBJECTS] = { nullptr };

static int store_lv_obj(lv_obj_t *obj) {
  for (int i = 0; i < MAX_OBJECTS; i++) {
    if (!g_lv_obj_map[i]) {
      g_lv_obj_map[i] = obj;
      return i;
    }
  }
  return -1; // No free slot
}

static lv_obj_t *get_lv_obj(int handle) {
  if (handle < 0 || handle >= MAX_OBJECTS) return nullptr;
  return g_lv_obj_map[handle];
}

// If you want to hide the GIF until after the label scroll finishes:
static lv_obj_t *g_gifObj = NULL; // We'll store the GIF object pointer here

// Example: create a label
static jsval_t js_lv_create_label(struct js *js, jsval_t *args, int nargs) {
  // Create on the active screen
  lv_obj_t *label = lv_label_create(lv_scr_act());
  if (!label) return js_mknull();

  // Basic style
  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_text_font(&style, &lv_font_montserrat_40);
  lv_style_set_text_color(&style, lv_color_white());
  lv_style_set_bg_color(&style, lv_color_black());
  lv_style_set_pad_all(&style, 5);
  lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);

  lv_obj_add_style(label, &style, 0);

  // Optionally set an initial position so it's visible
  // e.g. top-left corner:
  lv_obj_set_pos(label, 0, 0);

  int handle = store_lv_obj(label);
  if (handle < 0) return js_mknull();
  return js_mknum(handle);
}

static jsval_t js_lv_label_set_text(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int) js_getnum(args[0]);
  lv_obj_t *label = get_lv_obj(handle);
  if (!label) return js_mknull();

  const char *txt = js_str(js, args[1]);
  if (!txt) txt = "";
  lv_label_set_text(label, txt);
  return js_mknull();
}

// Animation callback to update y-pos
static void scroll_anim_cb(void *var, int32_t v) {
  lv_obj_set_y((lv_obj_t *)var, v);
}

static jsval_t js_create_scroll_animation(struct js *js, jsval_t *args, int nargs) {
  // usage: create_scroll_animation(labelHandle, startY, endY, duration)
  if (nargs < 4) return js_mknull();
  int labelHandle = (int) js_getnum(args[0]);
  lv_obj_t *label = get_lv_obj(labelHandle);
  if (!label) return js_mknull();

  int start = (int) js_getnum(args[1]);
  int end   = (int) js_getnum(args[2]);
  int duration = (int) js_getnum(args[3]);

  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, label);
  lv_anim_set_values(&a, start, end);
  lv_anim_set_time(&a, duration);
  lv_anim_set_exec_cb(&a, scroll_anim_cb);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_set_repeat_count(&a, 2);

  // We remove the line that hides the label automatically.
  // Instead, we show the GIF at the end of the animation, if present.
  lv_anim_set_ready_cb(&a, [](lv_anim_t *anim) {
    Serial.println("Label scroll animation done!");
    // If you want to hide the label at the end, uncomment below:
    // lv_obj_t *obj = (lv_obj_t *)anim->var;
    // lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);

    // Now unhide the GIF if we have it
    if (g_gifObj) {
      lv_obj_clear_flag(g_gifObj, LV_OBJ_FLAG_HIDDEN);
    }
  });

  lv_anim_start(&a);
  return js_mknull();
}

// Show a GIF from notification.h, hidden by default
extern const lv_img_dsc_t notification;

static jsval_t js_show_gif(struct js *js, jsval_t *args, int nargs) {
  // Create a gif on the active screen
  lv_obj_t *gif = lv_gif_create(lv_scr_act());
  lv_gif_set_src(gif, &notification);
  lv_obj_align(gif, LV_ALIGN_CENTER, 0, 0);

  // Hide the GIF now, unhide after label scroll
  lv_obj_add_flag(gif, LV_OBJ_FLAG_HIDDEN);
  g_gifObj = gif; // store globally to unhide it in the animation callback

  int handle = store_lv_obj(gif);
  if (handle < 0) return js_mknull();
  return js_mknum(handle);
}

// ------------------ Register All Functions ------------------
void register_js_functions() {
  jsval_t global = js_glob(js);

  // Basic
  js_set(js, global, "print",        js_mkfun(js_print));
  js_set(js, global, "wifi_connect", js_mkfun(js_wifi_connect));
  js_set(js, global, "wifi_status",  js_mkfun(js_wifi_status));
  js_set(js, global, "wifi_get_ip",  js_mkfun(js_wifi_get_ip));
  js_set(js, global, "delay",        js_mkfun(js_delay));
  js_set(js, global, "sd_read_file", js_mkfun(js_sd_read_file));
  js_set(js, global, "sd_write_file",js_mkfun(js_sd_write_file));
  js_set(js, global, "sd_list_dir",  js_mkfun(js_sd_list_dir));

  // Placeholders
  js_set(js, global, "lvgl_display_label", js_mkfun(js_lvgl_display_label));
  js_set(js, global, "lvgl_display_image", js_mkfun(js_lvgl_display_image));

  // Advanced LVGL
  js_set(js, global, "lv_create_label",         js_mkfun(js_lv_create_label));
  js_set(js, global, "lv_label_set_text",       js_mkfun(js_lv_label_set_text));
  js_set(js, global, "create_scroll_animation", js_mkfun(js_create_scroll_animation));
  js_set(js, global, "show_gif",                js_mkfun(js_show_gif));
}

// ------------------ Load/Execute JS from SD ----------------
bool load_and_execute_js_script(const char* path) {
  Serial.printf("Loading JavaScript script from: %s\n", path);

  File file = SD_MMC.open(path);
  if (!file) {
    Serial.println("Failed to open JavaScript script file");
    return false;
  }

  String jsScript = file.readString();
  file.close();

  jsval_t res = js_eval(js, jsScript.c_str(), jsScript.length());
  if (js_type(res) == JS_ERR) {
    const char *error = js_str(js, res);
    Serial.printf("Error executing script: %s\n", error);
    return false;
  }
  Serial.println("JavaScript script executed successfully");
  return true;
}

// ------------------ Arduino Setup & Loop -------------------
void setup() {
  Serial.begin(115200);
  delay(2000);

  // Mount SD card
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("Card Mount Failed");
    return;
  }

  // Wi-Fi in station mode
  WiFi.mode(WIFI_STA);

  // Init LVGL display
  init_lvgl_display();

  // Create Elk
  js = js_create(elk_memory, sizeof(elk_memory));
  if (!js) {
    Serial.println("Failed to initialize Elk");
    return;
  }
  // Register all JS functions
  register_js_functions();

  // Load & execute script from SD
  if (!load_and_execute_js_script("/script.js")) {
    Serial.println("Failed to load and execute JavaScript script");
  }
}

void loop() {
  // Keep LVGL updated
  lvgl_loop();
  delay(5);
}
