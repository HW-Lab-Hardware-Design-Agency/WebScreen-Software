#pragma once

#include <lvgl.h>
#include <HTTPClient.h>
#include "tick.h"

// For BLE
#include <NimBLEDevice.h>

#include <WiFi.h>  // WiFi library that also provides WiFiClient
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>  // For MQTT

#include <vector>
#include <utility>  // for std::pair

#include "globals.h"

// Global WiFiClient + PubSubClient
static WiFiClient g_wifiClient;
static PubSubClient g_mqttClient(g_wifiClient);

// HTTP client certificate.
static char *g_httpCAcert = nullptr;  // Will hold entire PEM cert from SD
static std::vector<std::pair<String, String>> g_http_headers;

// NimBLE globals
static NimBLEServer *g_bleServer = nullptr;
static NimBLECharacteristic *g_bleChar = nullptr;
static bool g_bleConnected = false;

#define JS_GC_THRESHOLD 0.90

extern "C" {
#include "elk.h"
}

// For storing a JavaScript callback to handle incoming messages
static char g_mqttCallbackName[32];  // Big enough for a function name
static unsigned long lastMqttReconnectAttempt = 0;
static unsigned long lastWiFiReconnectAttempt = 0;

/******************************************************************************
 * A) Elk Memory + Global Instances
 ******************************************************************************/
#define ELK_HEAP_BYTES (48 * 1024)
static uint8_t elk_memory[ELK_HEAP_BYTES];
struct js *js = NULL;  // Global Elk instance
// Adjust as needed
#define MAX_RAM_IMAGES 16

struct RamImage {
  bool used;         // Is this slot in use?
  uint8_t *buffer;   // Raw image data allocated from ps_malloc()
  size_t size;       // Byte size of that buffer
  lv_img_dsc_t dsc;  // The descriptor we pass to lv_img_set_src()
};

static RamImage g_ram_images[MAX_RAM_IMAGES];
void init_ram_images() {
  for (int i = 0; i < MAX_RAM_IMAGES; i++) {
    g_ram_images[i].used = false;
    g_ram_images[i].buffer = NULL;
    g_ram_images[i].size = 0;
    // g_ram_images[i].dsc can remain zeroed
  }
}

/******************************************************************************
 * C) "S" Driver for Reading Files from SD
 ******************************************************************************/
typedef struct {
  File file;
} lv_arduino_fs_file_t;

static void *my_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode) {
  String fullPath = String("/") + path;
  const char *modeStr = (mode == LV_FS_MODE_WR) ? FILE_WRITE : FILE_READ;
  File f = SD_MMC.open(fullPath, modeStr);
  if (!f) {
    LOGF("my_open_cb: failed to open %s\n", fullPath.c_str());
    return NULL;
  }

  lv_arduino_fs_file_t *fp = new lv_arduino_fs_file_t();
  fp->file = f;
  return fp;
}

static lv_fs_res_t my_close_cb(lv_fs_drv_t *drv, void *file_p) {
  lv_arduino_fs_file_t *fp = (lv_arduino_fs_file_t *)file_p;
  if (!fp) return LV_FS_RES_INV_PARAM;
  fp->file.close();
  delete fp;
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br) {
  lv_arduino_fs_file_t *fp = (lv_arduino_fs_file_t *)file_p;
  if (!fp) return LV_FS_RES_INV_PARAM;
  *br = fp->file.read((uint8_t *)buf, btr);
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_write_cb(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw) {
  lv_arduino_fs_file_t *fp = (lv_arduino_fs_file_t *)file_p;
  if (!fp) return LV_FS_RES_INV_PARAM;
  *bw = fp->file.write((const uint8_t *)buf, btw);
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence) {
  lv_arduino_fs_file_t *fp = (lv_arduino_fs_file_t *)file_p;
  if (!fp) return LV_FS_RES_INV_PARAM;

  SeekMode m = SeekSet;
  if (whence == LV_FS_SEEK_CUR) m = SeekCur;
  if (whence == LV_FS_SEEK_END) m = SeekEnd;

  fp->file.seek(pos, m);
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p) {
  lv_arduino_fs_file_t *fp = (lv_arduino_fs_file_t *)file_p;
  if (!fp) return LV_FS_RES_INV_PARAM;
  *pos_p = fp->file.position();
  return LV_FS_RES_OK;
}
void init_lv_fs() {
  static lv_fs_drv_t fs_drv;
  lv_fs_drv_init(&fs_drv);

  fs_drv.letter = 'S';
  fs_drv.open_cb = my_open_cb;
  fs_drv.close_cb = my_close_cb;
  fs_drv.read_cb = my_read_cb;
  fs_drv.write_cb = my_write_cb;
  fs_drv.seek_cb = my_seek_cb;
  fs_drv.tell_cb = my_tell_cb;

  lv_fs_drv_register(&fs_drv);
  LOG("LVGL FS driver 'S' registered");
}

/******************************************************************************
 * D) "M" Memory Driver (for GIF usage)
 ******************************************************************************/
typedef struct {
  size_t pos;
} mem_file_t;

static uint8_t *g_gifBuffer = NULL;
static size_t g_gifSize = 0;

static void *my_mem_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode) {
  mem_file_t *mf = new mem_file_t();
  mf->pos = 0;
  return mf;
}

static lv_fs_res_t my_mem_close_cb(lv_fs_drv_t *drv, void *file_p) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if (!mf) return LV_FS_RES_INV_PARAM;
  delete mf;
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_mem_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if (!mf) return LV_FS_RES_INV_PARAM;

  size_t remaining = g_gifSize - mf->pos;
  if (btr > remaining) btr = remaining;

  memcpy(buf, g_gifBuffer + mf->pos, btr);
  mf->pos += btr;
  *br = btr;
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_mem_write_cb(lv_fs_drv_t *drv, void *file_p, const void *buf, uint32_t btw, uint32_t *bw) {
  *bw = 0;
  return LV_FS_RES_NOT_IMP;
}

static lv_fs_res_t my_mem_seek_cb(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if (!mf) return LV_FS_RES_INV_PARAM;

  size_t newpos = mf->pos;
  if (whence == LV_FS_SEEK_SET) newpos = pos;
  else if (whence == LV_FS_SEEK_CUR) newpos += pos;
  else if (whence == LV_FS_SEEK_END) newpos = g_gifSize + pos;

  if (newpos > g_gifSize) newpos = g_gifSize;
  mf->pos = newpos;
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_mem_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if (!mf) return LV_FS_RES_INV_PARAM;
  *pos_p = mf->pos;
  return LV_FS_RES_OK;
}
void init_mem_fs() {
  static lv_fs_drv_t mem_drv;
  lv_fs_drv_init(&mem_drv);

  mem_drv.letter = 'M';
  mem_drv.open_cb = my_mem_open_cb;
  mem_drv.close_cb = my_mem_close_cb;
  mem_drv.read_cb = my_mem_read_cb;
  mem_drv.write_cb = my_mem_write_cb;
  mem_drv.seek_cb = my_mem_seek_cb;
  mem_drv.tell_cb = my_mem_tell_cb;

  lv_fs_drv_register(&mem_drv);
  LOG("LVGL FS driver 'M' registered (for memory-based GIFs)");
}

/******************************************************************************
 * B) LVGL + Display
 ******************************************************************************/
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf = NULL;
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {  // Calculate width/height from the area
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  // Push the rendered data to the display
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);

  // Tell LVGL flush is done
  lv_disp_flush_ready(disp);
}
void init_lvgl_display() {
  LOG("Initializing display...");

  // Turn on backlight / screen power
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  // Init the AMOLED driver & set rotation
  rm67162_init();
  lcd_setRotation(1);

  // Init LVGL
  lv_init();
  start_lvgl_tick();

  // Use double buffering: draw BUF in internal RAM (DMA capable),
  // flush BUF in PSRAM (big but non‑DMA).
  static const uint32_t DRAW_BUF_LINES = 40;  // tweak later
  static lv_color_t draw_buf_int[EXAMPLE_LCD_H_RES * DRAW_BUF_LINES];
  buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE);  // PSRAM
  if (!buf) {
    LOG("Failed to allocate LVGL buffer in PSRAM");
    return;
  }

  // Initialize LVGL draw buffer
  lv_disp_draw_buf_init(&draw_buf, draw_buf_int, buf, EXAMPLE_LCD_H_RES * DRAW_BUF_LINES);

  // Register the display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  lv_obj_t *scr = lv_scr_act();

  // Set the screen background color directly on the screen object
  lv_obj_set_style_bg_color(scr, lv_color_hex(g_bg_color), 0);

  // Set the default text color for any labels created on the screen
  // (This will be inherited by children unless they have their own color set)
  lv_obj_set_style_text_color(scr, lv_color_hex(g_fg_color), 0);

  LOG("LVGL + Display initialized.");
}
void lvgl_loop() {  // Call LVGL's timer handler
  lv_timer_handler();
}

/******************************************************************************
 * E) Elk-Facing Functions (print, Wi-Fi, SD ops, etc.)
 ******************************************************************************/
static jsval_t js_print(struct js *js, jsval_t *args, int nargs) {
  for (int i = 0; i < nargs; i++) {
    const char *str = js_str(js, args[i]);
    if (str) LOG(str);
    else LOG("print: argument is not a string");
  }
  return js_mknull();
}

// Wi-Fi connect
static jsval_t js_wifi_connect(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) return js_mkfalse();
  const char *ssidQ = js_str(js, args[0]);
  const char *passQ = js_str(js, args[1]);
  if (!ssidQ || !passQ) return js_mkfalse();

  // Strip quotes if any
  String ssid(ssidQ);
  String pass(passQ);
  if (ssid.startsWith("\"") && ssid.endsWith("\"")) {
    ssid = ssid.substring(1, ssid.length() - 1);
  }
  if (pass.startsWith("\"") && pass.endsWith("\"")) {
    pass = pass.substring(1, pass.length() - 1);
  }

  LOGF("Connecting to Wi-Fi SSID: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  for (uint32_t i = 0; i < 20 && WiFi.status() != WL_CONNECTED; ++i) {
    vTaskDelay(pdMS_TO_TICKS(250));
    LOG(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    LOG("Wi-Fi connected");
    return js_mktrue();
  } else {
    LOG("Failed to connect to Wi-Fi");
    return js_mkfalse();
  }
}

static jsval_t js_wifi_status(struct js *js, jsval_t *args, int nargs) {
  return (WiFi.status() == WL_CONNECTED) ? js_mktrue() : js_mkfalse();
}

static jsval_t js_wifi_get_ip(struct js *js, jsval_t *args, int nargs) {
  if (WiFi.status() != WL_CONNECTED) {
    LOG("Not connected to Wi-Fi");
    return js_mknull();
  }
  IPAddress ip = WiFi.localIP();
  String ipStr = ip.toString();
  return js_mkstr(js, ipStr.c_str(), ipStr.length());
}

// Delay in JS: "delay(ms)"
static jsval_t js_delay(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  double ms = js_getnum(args[0]);
  vTaskDelay(pdMS_TO_TICKS((unsigned long)ms));
  return js_mknull();
}

// LVGL Timer Bridging Functions

// This C++ function will be the callback for LVGL. It will execute a JS function.
static void elk_timer_cb(lv_timer_t *timer) {
  char *func_name = (char *)timer->user_data;

  if (func_name != NULL && js != NULL) {  // Construct a snippet of JavaScript to call the function, e.g., "my_func();"
    char snippet[64];
    snprintf(snippet, sizeof(snippet), "%s();", func_name);

    // Use js_eval to execute the function call.
    jsval_t res = js_eval(js, snippet, strlen(snippet));
    if (js_type(res) == JS_ERR) {
      LOGF("[TIMER CB] Error executing JS function '%s': %s\n", func_name, js_str(js, res));
    }
  }
}

// This is the function we will expose to JavaScript.
// It creates an LVGL timer that will call our C++ callback.
static jsval_t js_create_timer(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) {
    LOG("create_timer expects: function_name, period_ms");
    return js_mknull();
  }

  size_t func_name_len;
  char *func_name_str = js_getstr(js, args[0], &func_name_len);
  double period = js_getnum(args[1]);

  if (!func_name_str || func_name_len == 0) {
    return js_mknull();
  }

  char *name_for_timer = (char *)malloc(func_name_len + 1);
  if (!name_for_timer) {
    LOG("Failed to allocate memory for timer callback name");
    return js_mknull();
  }
  memcpy(name_for_timer, func_name_str, func_name_len);
  name_for_timer[func_name_len] = '\0';

  // Create the LVGL timer
  lv_timer_create(elk_timer_cb, (uint32_t)period, name_for_timer);

  LOGF("Created LVGL timer to call JS function '%s' every %dms\n", name_for_timer, (int)period);
  return js_mknull();
}

// sd_read_file(path)
static jsval_t js_sd_read_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  const char *rawPath = js_str(js, args[0]);
  if (!rawPath) return js_mknull();

  // Strip quotes if present
  String path(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path.remove(0, 1);
    path.remove(path.length() - 1, 1);
  }

  File file = SD_MMC.open(path);
  if (!file) {
    LOGF("Failed to open file: %s\n", path.c_str());
    return js_mknull();
  }
  String content = file.readString();
  file.close();
  return js_mkstr(js, content.c_str(), content.length());
}

// sd_write_file(path, data)
static jsval_t js_sd_write_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) return js_mkfalse();
  const char *path = js_str(js, args[0]);
  const char *data = js_str(js, args[1]);
  if (!path || !data) return js_mkfalse();

  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    LOGF("Failed to open for writing: %s\n", path);
    return js_mkfalse();
  }
  f.write((const uint8_t *)data, strlen(data));
  f.close();
  return js_mktrue();
}

// sd_list_dir(path)
static jsval_t js_sd_list_dir(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  const char *pathQ = js_str(js, args[0]);
  if (!pathQ) return js_mknull();

  // Strip quotes
  String path(pathQ);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }

  File root = SD_MMC.open(path);
  if (!root) {
    LOGF("Failed to open directory: %s\n", path.c_str());
    return js_mknull();
  }
  if (!root.isDirectory()) {
    LOG("Not a directory");
    root.close();
    return js_mknull();
  }

  // Collect listing
  char fileList[512];
  int fileListLen = 0;

  File f = root.openNextFile();
  while (f) {
    const char *type = f.isDirectory() ? "DIR: " : "FILE: ";
    const char *name = f.name();
    int len = snprintf(fileList + fileListLen, sizeof(fileList) - fileListLen,
                       "%s%s\n", type, name);
    if (len < 0 || len >= (int)(sizeof(fileList) - fileListLen)) break;
    fileListLen += len;
    f = root.openNextFile();
  }
  root.close();
  return js_mkstr(js, fileList, fileListLen);
}

// Helper function to convert a JS string to a JS number
static jsval_t js_to_number(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) {
    return js_mknum(0);  // Return 0 if no argument
  }

  // If it's already a number, just return it.
  if (js_type(args[0]) == JS_NUM) {
    return args[0];
  }

  // Get the string value from the JS argument
  size_t len;
  const char *str = js_getstr(js, args[0], &len);
  if (!str) {
    return js_mknum(0);  // Return 0 if not a valid string
  }

  // Convert the C-string to a double and return as a JS number
  return js_mknum(atof(str));
}

// Helper function to convert a JS number to a JS string
static jsval_t js_number_to_string(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) {
    return js_mkstr(js, "", 0);
  }

  uint8_t type = js_type(args[0]);

  if (type == JS_NUM) {
    char buf[32];
    double num = js_getnum(args[0]);
    // Using "%.17g" is how the Elk engine itself formats numbers
    snprintf(buf, sizeof(buf), "%.17g", num);
    return js_mkstr(js, buf, strlen(buf));
  } else if (type == JS_STR) {  // If it's already a string (like from parse_json_value), just return it
    return args[0];
  }

  return js_mkstr(js, "", 0);
}

/******************************************************************************
 * F) Load GIF from SD => g_gifBuffer => "M:mygif"
 ******************************************************************************/

bool load_gif_into_ram(const char *path) {
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    LOGF("Failed to open %s\n", path);
    return false;
  }
  size_t fileSize = f.size();
  LOGF("File %s is %u bytes\n", path, (unsigned)fileSize);

  uint8_t *tmp = (uint8_t *)ps_malloc(fileSize);
  if (!tmp) {
    LOGF("Failed to allocate %u bytes in PSRAM\n", (unsigned)fileSize);
    f.close();
    return false;
  }
  size_t bytesRead = f.read(tmp, fileSize);
  f.close();
  if (bytesRead < fileSize) {
    LOGF("Failed to read full file: only %u of %u\n",
         (unsigned)bytesRead, (unsigned)fileSize);
    free(tmp);
    return false;
  }
  g_gifBuffer = tmp;
  g_gifSize = fileSize;
  LOG("GIF loaded into PSRAM successfully");
  return true;
}

static jsval_t js_show_gif_from_sd(struct js *js, jsval_t *args, int nargs) {  // Check if we have enough arguments (path, x, y)
  if (nargs < 3) {
    LOG("show_gif_from_sd: expects path, x, y");
    return js_mknull();
  }

  // Argument 0: Get the path string
  const char *rawPath = js_str(js, args[0]);
  if (!rawPath) return js_mknull();

  // Strip quotes from the path
  String path(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }

  // Argument 1 & 2: Get the x and y coordinates
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  // Load the specified GIF file into RAM
  if (!load_gif_into_ram(path.c_str())) {
    LOG("Could not load GIF into RAM");
    return js_mknull();
  }

  // Create the LVGL gif object
  lv_obj_t *gif = lv_gif_create(lv_scr_act());
  // Set the source to the in-memory driver 'M'
  lv_gif_set_src(gif, "M:mygif");

  // Set the position using the x and y coordinates from JavaScript
  lv_obj_set_pos(gif, x, y);

  // Update the log to show the new coordinates
  LOGF("Showing GIF from memory driver (file was %s) at (%d,%d)\n", path.c_str(), x, y);
  return js_mknull();
}

/******************************************************************************
 * J) Load + Execute JS from SD
 ******************************************************************************/

bool load_image_file_into_ram(const char *path, RamImage *outImg) {
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    LOGF("Failed to open %s\n", path);
    return false;
  }
  size_t fileSize = f.size();
  LOGF("File %s is %u bytes\n", path, (unsigned)fileSize);

  uint8_t *buf = (uint8_t *)ps_malloc(fileSize);
  if (!buf) {
    LOGF("Failed to allocate %u bytes in PSRAM\n", (unsigned)fileSize);
    f.close();
    return false;
  }

  size_t bytesRead = f.read(buf, fileSize);
  f.close();
  if (bytesRead < fileSize) {
    LOGF("Failed to read full file: only %u of %u\n",
         (unsigned)bytesRead, (unsigned)fileSize);
    free(buf);
    return false;
  }

  outImg->used = true;
  outImg->buffer = buf;
  outImg->size = fileSize;

  //    - If it's a "raw" or "true color" format, you can do:
  lv_img_dsc_t *d = &outImg->dsc;
  memset(d, 0, sizeof(*d));

  // Basic mandatory fields:
  d->data_size = fileSize;
  d->data = buf;
  d->header.always_zero = 0;
  d->header.w = 200;
  d->header.h = 200;
  d->header.cf = LV_IMG_CF_TRUE_COLOR;
  // or LV_IMG_CF_RAW if using a custom decoder

  // If you can't know width/height from file alone, you may just guess or parse
  // For a PNG/JPG you'd typically use an external decoder to fill w,h

  LOG("Image loaded into PSRAM successfully");
  return true;
}
bool load_and_execute_js_script(const char *path) {
  LOGF("Loading JavaScript script from: %s\n", path);

  File file = SD_MMC.open(path);
  if (!file) {
    LOG("Failed to open JavaScript script file");
    return false;
  }
  String jsScript = file.readString();
  file.close();

  jsval_t res = js_eval(js, jsScript.c_str(), jsScript.length());
  if (js_type(res) == JS_ERR) {
    const char *error = js_str(js, res);
    LOGF("Error executing script: %s\n", error);
    return false;
  }
  LOG("JavaScript script executed successfully");
  return true;
}

/******************************************************************************
 * G) Basic draw_label, draw_rect, show_image from SD
 ******************************************************************************/
static const lv_font_t *get_font_for_size(int size) {  // Map the integer size to specific built-in Montserrat fonts
  if (size == 20) return &lv_font_montserrat_20;
  if (size == 28) return &lv_font_montserrat_28;
  if (size == 34) return &lv_font_montserrat_34;
  if (size == 40) return &lv_font_montserrat_40;
  if (size == 44) return &lv_font_montserrat_44;
  if (size == 48) return &lv_font_montserrat_48;
  return &lv_font_montserrat_14;
}

static jsval_t js_lvgl_draw_label(struct js *js, jsval_t *args, int nargs) {  // We expect at least 3 args: text, x, y. 4th arg is optional fontSize
  if (nargs < 3) {
    LOG("draw_label: expects text, x, y, [fontSize]");
    return js_mknull();
  }

  const char *rawText = js_str(js, args[0]);
  if (!rawText) return js_mknull();
  String txt(rawText);
  if (txt.startsWith("\"") && txt.endsWith("\"")) {
    txt.remove(0, 1);
    txt.remove(txt.length() - 1, 1);
  }

  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, txt.c_str());
  lv_obj_set_pos(label, x, y);

  if (nargs >= 4) {
    int fontSize = (int)js_getnum(args[3]);
    const lv_font_t *font = get_font_for_size(fontSize);
    lv_obj_set_style_text_font(label, font, 0);
  }

  return js_mknull();
}

static jsval_t js_lvgl_draw_rect(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 4) {
    LOG("draw_rect: expects x,y,w,h");
    return js_mknull();
  }
  int x = (int)js_getnum(args[0]);
  int y = (int)js_getnum(args[1]);
  int w = (int)js_getnum(args[2]);
  int h = (int)js_getnum(args[3]);

  lv_obj_t *rect = lv_obj_create(lv_scr_act());
  lv_obj_set_size(rect, w, h);
  lv_obj_set_pos(rect, x, y);

  // optional styling
  static lv_style_t styleRect;
  lv_style_init(&styleRect);
  lv_style_set_bg_color(&styleRect, lv_color_hex(0x00ff00));
  lv_style_set_radius(&styleRect, 5);
  lv_obj_add_style(rect, &styleRect, 0);

  LOGF("draw_rect: at (%d,%d), size(%d,%d)\n", x, y, w, h);
  return js_mknull();
}

static jsval_t js_lvgl_show_image(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) {
    LOG("show_image: expects path,x,y");
    return js_mknull();
  }
  const char *rawPath = js_str(js, args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  if (!rawPath) {
    LOG("show_image: invalid path");
    return js_mknull();
  }
  // Build "S:/filename"
  String path(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }
  String lvglPath = "S:" + path;

  lv_obj_t *img = lv_img_create(lv_scr_act());
  lv_img_set_src(img, lvglPath.c_str());
  lv_obj_set_pos(img, x, y);

  LOGF("show_image: '%s' at (%d,%d)\n", lvglPath.c_str(), x, y);
  return js_mknull();
}

/******************************************************************************
 * G2) create_image, rotate_obj, move_obj, animate_obj (Object Handle Approach)
 ******************************************************************************/
// std::vector‑based registry ----
#include <vector>
#include <mutex>
static std::vector<lv_obj_t *> g_objects;
static std::mutex g_obj_mtx;

static int store_lv_obj(lv_obj_t *obj) {
  std::lock_guard<std::mutex> lock(g_obj_mtx);
  for (size_t i = 0; i < g_objects.size(); ++i)
    if (!g_objects[i]) {
      g_objects[i] = obj;
      return (int)i;
    }
  g_objects.push_back(obj);
  return (int)(g_objects.size() - 1);
}

static lv_obj_t *get_lv_obj(int h) {
  std::lock_guard<std::mutex> lock(g_obj_mtx);
  return (h >= 0 && h < (int)g_objects.size()) ? g_objects[h] : nullptr;
}

static void release_lv_obj(int h) {
  std::lock_guard<std::mutex> lock(g_obj_mtx);
  if (h >= 0 && h < (int)g_objects.size()) g_objects[h] = nullptr;
}

// Helper functions to extract RGB components from lv_color_t

uint8_t get_red(lv_color_t color) {
  return (color.full >> 11) & 0x1F;  // 5 bits
}
uint8_t get_green(lv_color_t color) {
  return (color.full >> 5) & 0x3F;  // 6 bits
}
uint8_t get_blue(lv_color_t color) {
  return color.full & 0x1F;  // 5 bits
}

// create_image("/messi.png", x,y) => returns handle
static jsval_t js_create_image(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) {
    LOG("create_image: expects path,x,y");
    return js_mknum(-1);
  }
  const char *rawPath = js_str(js, args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);
  if (!rawPath) return js_mknum(-1);

  String path(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }
  String fullPath = "S:" + path;

  lv_obj_t *img = lv_img_create(lv_scr_act());
  lv_img_set_src(img, fullPath.c_str());
  lv_obj_set_pos(img, x, y);

  int handle = store_lv_obj(img);
  LOGF("create_image: '%s' => handle %d\n", fullPath.c_str(), handle);
  return js_mknum(handle);
}

// create_image_from_ram("/somefile.bin", x, y)
static jsval_t js_create_image_from_ram(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) {
    LOG("create_image_from_ram: expects path, x, y");
    return js_mknum(-1);
  }

  const char *rawPath = js_str(js, args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);
  if (!rawPath) return js_mknum(-1);

  int slot = -1;
  for (int i = 0; i < MAX_RAM_IMAGES; i++) {
    if (!g_ram_images[i].used) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    LOG("No free RamImage slots!");
    return js_mknum(-1);
  }
  RamImage *ri = &g_ram_images[slot];

  String path = String(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }

  if (!load_image_file_into_ram(path.c_str(), ri)) {
    LOG("Could not load image into RAM");
    return js_mknum(-1);
  }

  lv_obj_t *img = lv_img_create(lv_scr_act());
  lv_img_set_src(img, &ri->dsc);  // <--- the magic

  lv_obj_set_pos(img, x, y);

  int handle = store_lv_obj(img);
  LOGF("create_image_from_ram: '%s' => ram slot=%d => handle %d\n",
       path.c_str(), slot, handle);
  return js_mknum(handle);
}

// rotate_obj(handle, angle)
static jsval_t js_rotate_obj(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) {
    LOG("rotate_obj: expects handle, angle");
    return js_mknull();
  }
  int handle = (int)js_getnum(args[0]);
  int angle = (int)js_getnum(args[1]);  // 0..3600 => 0..360 deg

  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) {
    LOG("rotate_obj: invalid handle");
    return js_mknull();
  }
  // For lv_img in LVGL => set angle
  lv_img_set_angle(obj, angle);
  LOGF("rotate_obj: handle=%d angle=%d\n", handle, angle);
  return js_mknull();
}

// move_obj(handle, x, y)
static jsval_t js_move_obj(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) {
    LOG("move_obj: expects handle,x,y");
    return js_mknull();
  }
  int handle = (int)js_getnum(args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) {
    LOG("move_obj: invalid handle");
    return js_mknull();
  }
  lv_obj_set_pos(obj, x, y);
  LOGF("move_obj: handle=%d => pos(%d,%d)\n", handle, x, y);
  return js_mknull();
}

// We'll animate X + Y with two separate anims
static void anim_x_cb(void *var, int32_t v) {
  lv_obj_t *obj = (lv_obj_t *)var;
  lv_obj_set_x(obj, v);
}
static void anim_y_cb(void *var, int32_t v) {
  lv_obj_t *obj = (lv_obj_t *)var;
  lv_obj_set_y(obj, v);
}

// animate_obj(handle, x0,y0, x1,y1, duration)
static jsval_t js_animate_obj(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 5) {
    LOG("animate_obj: expects handle,x0,y0,x1,y1,[duration]");
    return js_mknull();
  }
  int handle = (int)js_getnum(args[0]);
  int x0 = (int)js_getnum(args[1]);
  int y0 = (int)js_getnum(args[2]);
  int x1 = (int)js_getnum(args[3]);
  int y1 = (int)js_getnum(args[4]);
  int duration = (nargs >= 6) ? (int)js_getnum(args[5]) : 1000;

  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) {
    LOG("animate_obj: invalid handle");
    return js_mknull();
  }
  // Start pos
  lv_obj_set_pos(obj, x0, y0);

  // Animate X
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, x0, x1);
  lv_anim_set_time(&a, duration);
  lv_anim_set_exec_cb(&a, anim_x_cb);
  lv_anim_start(&a);

  // Animate Y
  lv_anim_t a2;
  lv_anim_init(&a2);
  lv_anim_set_var(&a2, obj);
  lv_anim_set_values(&a2, y0, y1);
  lv_anim_set_time(&a2, duration);
  lv_anim_set_exec_cb(&a2, anim_y_cb);
  lv_anim_start(&a2);

  LOGF("animate_obj: handle=%d from(%d,%d) to(%d,%d), dur=%d\n",
       handle, x0, y0, x1, y1, duration);
  return js_mknull();
}

/******************************************************************************
 * H) Style Handles + Full Style Setters
 ******************************************************************************/
static const int MAX_STYLES = 32;
static lv_style_t *g_style_map[MAX_STYLES] = { nullptr };

static lv_style_t *get_lv_style(int handle) {
  if (handle < 0 || handle >= MAX_STYLES) return nullptr;
  return g_style_map[handle];
}
static jsval_t js_create_label(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknum(-1);  // need x,y
  int x = (int)js_getnum(args[0]);
  int y = (int)js_getnum(args[1]);

  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_obj_set_pos(label, x, y);

  int handle = store_lv_obj(label);
  return js_mknum(handle);
}

static jsval_t js_label_set_text(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int lblHandle = (int)js_getnum(args[0]);
  const char *rawText = js_str(js, args[1]);
  if (!rawText) return js_mknull();

  // Convert to an Arduino String so we can trim quotes.
  String txt(rawText);

  // If the string starts and ends with " and is longer than 1 char,
  // remove those outer quotes.
  if (txt.startsWith("\"") && txt.endsWith("\"") && txt.length() > 1) {
    txt.remove(0, 1);                 // remove leading "
    txt.remove(txt.length() - 1, 1);  // remove trailing "
  }

  // Retrieve the lv_obj_t* from the handle
  lv_obj_t *label = get_lv_obj(lblHandle);
  if (!label) return js_mknull();

  // Finally set the text (now without extra quotes)
  lv_label_set_text(label, txt.c_str());
  return js_mknull();
}

// style_set_text_font(styleHandle, fontSize)
static jsval_t js_style_set_text_font(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int fontSize = (int)js_getnum(args[1]);

  // Convert the style handle to an lv_style_t*
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();

  // Pick a built-in font
  const lv_font_t *font = get_font_for_size(fontSize);

  // Apply it
  lv_style_set_text_font(st, font);

  return js_mknull();
}

// style_set_text_align(styleHandle, align)
static jsval_t js_style_set_text_align(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int alignVal = (int)js_getnum(args[1]);  // e.g. 0 for LEFT, 1 for CENTER, etc.

  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();

  // In LVGL 8.x: LV_TEXT_ALIGN_LEFT=0, _CENTER=1, _RIGHT=2, _AUTO=3
  lv_style_set_text_align(st, (lv_text_align_t)alignVal);

  return js_mknull();
}

// create_style()
static jsval_t js_create_style(struct js *js, jsval_t *args, int nargs) {
  for (int i = 0; i < MAX_STYLES; i++) {
    if (!g_style_map[i]) {
      lv_style_t *st = new lv_style_t;
      lv_style_init(st);
      g_style_map[i] = st;
      LOGF("create_style => handle %d\n", i);
      return js_mknum(i);
    }
  }
  LOG("create_style => no free style slots");
  return js_mknum(-1);
}

// obj_add_style(objHandle, styleHandle, partOrState)
static jsval_t js_obj_add_style(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int objHandle = (int)js_getnum(args[0]);
  int styleHandle = (int)js_getnum(args[1]);
  int partState = 0;
  if (nargs >= 3) partState = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(objHandle);
  lv_style_t *st = get_lv_style(styleHandle);
  if (!obj || !st) {
    LOG("obj_add_style => invalid handle");
    return js_mknull();
  }
  lv_obj_add_style(obj, st, partState);
  return js_mknull();
}

// ***Full style property setters***
static jsval_t js_style_set_radius(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int radius = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_radius(st, (lv_coord_t)radius);
  return js_mknull();
}

static jsval_t js_style_set_bg_opa(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int opaVal = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_bg_opa(st, (lv_opa_t)opaVal);
  return js_mknull();
}

static jsval_t js_style_set_bg_color(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  double color = js_getnum(args[1]);  // numeric hex
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_bg_color(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_border_color(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  double color = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_border_color(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_border_width(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int bw = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_border_width(st, bw);
  return js_mknull();
}

static jsval_t js_style_set_border_opa(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int opa = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_border_opa(st, (lv_opa_t)opa);
  return js_mknull();
}

static jsval_t js_style_set_border_side(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int side = (int)js_getnum(args[1]);  // e.g. LV_BORDER_SIDE_BOTTOM|LV_BORDER_SIDE_RIGHT
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_border_side(st, side);
  return js_mknull();
}

// Outline
static jsval_t js_style_set_outline_width(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int w = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_outline_width(st, w);
  return js_mknull();
}

static jsval_t js_style_set_outline_color(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  double col = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_outline_color(st, lv_color_hex((uint32_t)col));
  return js_mknull();
}

static jsval_t js_style_set_outline_pad(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_outline_pad(st, pad);
  return js_mknull();
}

// Shadow
static jsval_t js_style_set_shadow_width(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int w = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_shadow_width(st, w);
  return js_mknull();
}

static jsval_t js_style_set_shadow_color(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  double color = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_shadow_color(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_shadow_ofs_x(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int ofs = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_shadow_ofs_x(st, ofs);
  return js_mknull();
}

static jsval_t js_style_set_shadow_ofs_y(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int ofs = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_shadow_ofs_y(st, ofs);
  return js_mknull();
}

// Image recolor, transform
static jsval_t js_style_set_img_recolor(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  double color = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_img_recolor(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_img_recolor_opa(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int opa = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_img_recolor_opa(st, (lv_opa_t)opa);
  return js_mknull();
}

static jsval_t js_style_set_transform_angle(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int angle = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_transform_angle(st, (lv_coord_t)angle);
  return js_mknull();
}

// Text
static jsval_t js_style_set_text_color(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  double color = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_text_color(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_text_letter_space(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int space = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_text_letter_space(st, space);
  return js_mknull();
}

static jsval_t js_style_set_text_line_space(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int space = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_text_line_space(st, space);
  return js_mknull();
}

static jsval_t js_style_set_text_decor(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int decor = (int)js_getnum(args[1]);  // e.g. LV_TEXT_DECOR_UNDERLINE
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_text_decor(st, decor);
  return js_mknull();
}

// Line
static jsval_t js_style_set_line_color(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  double color = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_line_color(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_line_width(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int w = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_line_width(st, w);
  return js_mknull();
}

static jsval_t js_style_set_line_rounded(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  bool round = (bool)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_line_rounded(st, round);
  return js_mknull();
}

// Padding
static jsval_t js_style_set_pad_all(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_pad_all(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_left(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_pad_left(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_right(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_pad_right(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_top(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_pad_top(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_bottom(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_pad_bottom(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_ver(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_pad_ver(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_hor(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_pad_hor(st, pad);
  return js_mknull();
}

// Some dimension-related style props
static jsval_t js_style_set_width(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int w = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_width(st, (lv_coord_t)w);
  return js_mknull();
}

static jsval_t js_style_set_height(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int h = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_height(st, (lv_coord_t)h);
  return js_mknull();
}

static jsval_t js_style_set_x(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  double val = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_x(st, (lv_coord_t)val);
  return js_mknull();
}

static jsval_t js_style_set_y(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  double val = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();
  lv_style_set_y(st, (lv_coord_t)val);
  return js_mknull();
}

/******************************************************************************
 * H2) Additional object property functions
 ******************************************************************************/
static jsval_t js_obj_set_size(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int w = (int)js_getnum(args[1]);
  int h = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) {
    LOGF("obj_set_size => invalid handle %d\n", handle);
    return js_mknull();
  }
  lv_obj_set_size(obj, w, h);
  return js_mknull();
}

// obj_align(objHandle, alignConst, xOfs, yOfs)
static jsval_t js_obj_align(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 4) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int alignVal = (int)js_getnum(args[1]);
  int xOfs = (int)js_getnum(args[2]);
  int yOfs = (int)js_getnum(args[3]);

  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) {
    LOGF("obj_align => invalid handle %d\n", handle);
    return js_mknull();
  }
  lv_obj_align(obj, (lv_align_t)alignVal, xOfs, yOfs);
  return js_mknull();
}

/******************************************************************************
 * ***ADDED FOR NEW EXAMPLES***
 * For scrolling, flex, flags, etc.
 ******************************************************************************/
static jsval_t js_obj_set_scroll_snap_x(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int snap_mode = (int)js_getnum(args[1]);  // numeric for LV_SCROLL_SNAP_x
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_scroll_snap_x(obj, (lv_scroll_snap_t)snap_mode);
  return js_mknull();
}

static jsval_t js_obj_set_scroll_snap_y(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int snap_mode = (int)js_getnum(args[1]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_scroll_snap_y(obj, (lv_scroll_snap_t)snap_mode);
  return js_mknull();
}

static jsval_t js_obj_add_flag(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int flag = (int)js_getnum(args[1]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_add_flag(obj, (lv_obj_flag_t)flag);
  return js_mknull();
}

static jsval_t js_obj_clear_flag(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int flag = (int)js_getnum(args[1]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_clear_flag(obj, (lv_obj_flag_t)flag);
  return js_mknull();
}

static jsval_t js_obj_set_scroll_dir(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int dir = (int)js_getnum(args[1]);  // e.g. LV_DIR_VER or ...
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_scroll_dir(obj, (lv_dir_t)dir);
  return js_mknull();
}

static jsval_t js_obj_set_scrollbar_mode(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int mode = (int)js_getnum(args[1]);  // e.g. LV_SCROLLBAR_MODE_OFF
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_scrollbar_mode(obj, (lv_scrollbar_mode_t)mode);
  return js_mknull();
}

static jsval_t js_obj_set_flex_flow(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int flowEnum = (int)js_getnum(args[1]);  // e.g. LV_FLEX_FLOW_ROW_WRAP
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_flex_flow(obj, (lv_flex_flow_t)flowEnum);
  return js_mknull();
}

static jsval_t js_obj_set_flex_align(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 4) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int main_place = (int)js_getnum(args[1]);
  int cross_place = (int)js_getnum(args[2]);
  int track_place = (int)js_getnum(args[3]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_flex_align(obj, (lv_flex_align_t)main_place,
                        (lv_flex_align_t)cross_place,
                        (lv_flex_align_t)track_place);
  return js_mknull();
}

static jsval_t js_obj_set_style_clip_corner(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  bool en = (bool)js_getnum(args[1]);
  int part = (int)js_getnum(args[2]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_style_clip_corner(obj, en, part);
  return js_mknull();
}

static jsval_t js_obj_set_style_base_dir(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int base_dir = (int)js_getnum(args[1]);  // e.g. LV_BASE_DIR_RTL
  int part = (int)js_getnum(args[2]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_style_base_dir(obj, (lv_base_dir_t)base_dir, part);
  return js_mknull();
}

/*******************************************************
 * CHART BRIDGING
 *******************************************************/

static jsval_t js_lv_chart_create(struct js *js, jsval_t *args, int nargs) {  // Creates a chart object on the current screen
  lv_obj_t *chart = lv_chart_create(lv_scr_act());
  // Optionally set default size or alignment
  lv_obj_set_size(chart, 200, 150);
  lv_obj_center(chart);

  // Store in your handle-based system
  int handle = store_lv_obj(chart);
  LOGF("lv_chart_create => handle %d\n", handle);

  // Return handle to JS
  return js_mknum(handle);
}

static jsval_t js_lv_chart_set_type(struct js *js, jsval_t *args, int nargs) {  // (handle, lv_chart_type int)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int t = (int)js_getnum(args[1]);  // e.g. LV_CHART_TYPE_LINE, LV_CHART_TYPE_BAR, etc.

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_set_type(obj, (lv_chart_type_t)t);
  return js_mknull();
}

static jsval_t js_lv_chart_set_div_line_count(struct js *js, jsval_t *args, int nargs) {  // (handle, y_div, x_div)
  if (nargs < 3) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int y_div = (int)js_getnum(args[1]);
  int x_div = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_set_div_line_count(obj, y_div, x_div);
  return js_mknull();
}

static jsval_t js_lv_chart_set_update_mode(struct js *js, jsval_t *args, int nargs) {  // (handle, mode)
  // e.g. mode = LV_CHART_UPDATE_MODE_SHIFT, LV_CHART_UPDATE_MODE_CIRCULAR
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int mode = (int)js_getnum(args[1]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_set_update_mode(obj, (lv_chart_update_mode_t)mode);
  return js_mknull();
}

static jsval_t js_lv_chart_set_range(struct js *js, jsval_t *args, int nargs) {  // (handle, axis, min, max)
  // e.g. axis=LV_CHART_AXIS_PRIMARY_Y, min=0, max=100
  if (nargs < 4) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int axis = (int)js_getnum(args[1]);
  int mn = (int)js_getnum(args[2]);
  int mx = (int)js_getnum(args[3]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_set_range(obj, (lv_chart_axis_t)axis, mn, mx);
  return js_mknull();
}

static jsval_t js_lv_chart_set_point_count(struct js *js, jsval_t *args, int nargs) {  // (handle, count)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int c = (int)js_getnum(args[1]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_set_point_count(obj, c);
  return js_mknull();
}

static jsval_t js_lv_chart_refresh(struct js *js, jsval_t *args, int nargs) {  // (handle)
  if (nargs < 1) return js_mknull();
  int h = (int)js_getnum(args[0]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_refresh(obj);
  return js_mknull();
}

static jsval_t js_lv_chart_add_series(struct js *js, jsval_t *args, int nargs) {  // (handle, color, axis)
  if (nargs < 3) return js_mknull();
  int h = (int)js_getnum(args[0]);
  double col = js_getnum(args[1]);
  int axis = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_series_t *ser = lv_chart_add_series(obj, lv_color_hex((uint32_t)col), (lv_chart_axis_t)axis);
  // Return the pointer as a number (NOT safe on 64-bit, but workable for small usage)
  // Alternatively, you can store it in a separate map if you want handle-based approach for series.
  intptr_t p = (intptr_t)(ser);
  return js_mknum((double)p);
}

static jsval_t js_lv_chart_set_next_value(struct js *js, jsval_t *args, int nargs) {  // (chartHandle, seriesPtr, value)
  if (nargs < 3) return js_mknull();
  int h = (int)js_getnum(args[0]);
  intptr_t sp = (intptr_t)js_getnum(args[1]);
  int val = (int)js_getnum(args[2]);

  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  lv_chart_series_t *ser = (lv_chart_series_t *)sp;
  lv_chart_set_next_value(chart, ser, val);
  return js_mknull();
}

static jsval_t js_lv_chart_set_next_value2(struct js *js, jsval_t *args, int nargs) {  // (chartHandle, seriesPtr, xVal, yVal)
  if (nargs < 4) return js_mknull();
  int h = (int)js_getnum(args[0]);
  intptr_t sp = (intptr_t)js_getnum(args[1]);
  int xval = (int)js_getnum(args[2]);
  int yval = (int)js_getnum(args[3]);

  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  lv_chart_series_t *ser = (lv_chart_series_t *)sp;
  lv_chart_set_next_value2(chart, ser, xval, yval);
  return js_mknull();
}

static jsval_t js_lv_chart_set_axis_tick(struct js *js, jsval_t *args, int nargs) {  // (chartH, axis, majorLen, minorLen, majorCnt, minorCnt, label_en, draw_size)
  if (nargs < 8) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int axis = (int)js_getnum(args[1]);
  int majorLen = (int)js_getnum(args[2]);
  int minorLen = (int)js_getnum(args[3]);
  int majorCnt = (int)js_getnum(args[4]);
  int minorCnt = (int)js_getnum(args[5]);
  bool label = (bool)js_getnum(args[6]);
  int drawSiz = (int)js_getnum(args[7]);

  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  lv_chart_set_axis_tick(chart, (lv_chart_axis_t)axis, majorLen, minorLen,
                         majorCnt, minorCnt, label, drawSiz);
  return js_mknull();
}

static jsval_t js_lv_chart_set_zoom_x(struct js *js, jsval_t *args, int nargs) {  // (chartH, zoom)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int zm = (int)js_getnum(args[1]);
  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  lv_chart_set_zoom_x(chart, zm);
  return js_mknull();
}

static jsval_t js_lv_chart_set_zoom_y(struct js *js, jsval_t *args, int nargs) {  // (chartH, zoom)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int zm = (int)js_getnum(args[1]);
  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  lv_chart_set_zoom_y(chart, zm);
  return js_mknull();
}

static jsval_t js_lv_chart_get_y_array(struct js *js, jsval_t *args, int nargs) {  // (chartH, seriesPtr) -> returns a pointer number to the array
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  intptr_t sp = (intptr_t)js_getnum(args[1]);

  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  lv_chart_series_t *ser = (lv_chart_series_t *)sp;
  lv_coord_t *arr = lv_chart_get_y_array(chart, ser);
  // Return pointer as numeric
  intptr_t ret = (intptr_t)arr;
  return js_mknum((double)ret);
}

// Similarly you can add bridging for lv_chart_set_ext_y_array, lv_chart_set_ext_x_array,
// lv_chart_get_x_array, lv_chart_get_pressed_point, lv_chart_set_cursor_point, etc.
// if your examples require them.
/********************************************************************************
 * METER
 ********************************************************************************/
// Example calls:
//   lv_meter_create, lv_meter_add_scale, lv_meter_set_scale_ticks,
//   lv_meter_set_scale_major_ticks, lv_meter_set_scale_range
//   lv_meter_add_arc, lv_meter_add_scale_lines, lv_meter_add_needle_line,
//   lv_meter_add_needle_img
//   lv_meter_set_indicator_start_value, lv_meter_set_indicator_end_value, lv_meter_set_indicator_value

static jsval_t js_lv_meter_create(struct js *js, jsval_t *args, int nargs) {  // no params
  lv_obj_t *m = lv_meter_create(lv_scr_act());
  int handle = store_lv_obj(m);
  return js_mknum(handle);
}

static jsval_t js_lv_meter_add_scale(struct js *js, jsval_t *args, int nargs) {  // (meterHandle) -> scale pointer as number
  if (nargs < 1) return js_mknull();
  int mh = (int)js_getnum(args[0]);
  lv_obj_t *mt = get_lv_obj(mh);
  if (!mt) return js_mknull();

  lv_meter_scale_t *sc = lv_meter_add_scale(mt);
  // Return pointer as numeric
  intptr_t p = (intptr_t)sc;
  return js_mknum((double)p);
}

static jsval_t js_lv_meter_set_scale_ticks(struct js *js, jsval_t *args, int nargs) {  // (meterH, scalePtr, cnt, width, length, color)
  if (nargs < 6) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  intptr_t scP = (intptr_t)js_getnum(args[1]);
  int cnt = (int)js_getnum(args[2]);
  int width = (int)js_getnum(args[3]);
  int length = (int)js_getnum(args[4]);
  double col = js_getnum(args[5]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = (lv_meter_scale_t *)scP;
  lv_meter_set_scale_ticks(mt, sc, cnt, width, length, lv_color_hex((uint32_t)col));
  return js_mknull();
}

static jsval_t js_lv_meter_set_scale_major_ticks(struct js *js, jsval_t *args, int nargs) {  // (meterH, scalePtr, freq, width, length, color, label_gap)
  if (nargs < 7) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  intptr_t scP = (intptr_t)js_getnum(args[1]);
  int freq = (int)js_getnum(args[2]);
  int width = (int)js_getnum(args[3]);
  int length = (int)js_getnum(args[4]);
  double col = js_getnum(args[5]);
  int label_gap = (int)js_getnum(args[6]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();

  lv_meter_scale_t *sc = (lv_meter_scale_t *)scP;
  lv_meter_set_scale_major_ticks(mt, sc, freq, width, length, lv_color_hex((uint32_t)col), label_gap);
  return js_mknull();
}

static jsval_t js_lv_meter_set_scale_range(struct js *js, jsval_t *args, int nargs) {  // (meterH, scalePtr, min, max, angle_range, rotation)
  if (nargs < 6) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  intptr_t scP = (intptr_t)js_getnum(args[1]);
  int minV = (int)js_getnum(args[2]);
  int maxV = (int)js_getnum(args[3]);
  int angleRange = (int)js_getnum(args[4]);
  int rotation = (int)js_getnum(args[5]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = (lv_meter_scale_t *)scP;
  lv_meter_set_scale_range(mt, sc, minV, maxV, angleRange, rotation);
  return js_mknull();
}

// meter indicator creation
static jsval_t js_lv_meter_add_arc(struct js *js, jsval_t *args, int nargs) {  // (meterH, scalePtr, width, color, rMod)
  // returns indicator pointer
  if (nargs < 5) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  intptr_t scP = (intptr_t)js_getnum(args[1]);
  int width = (int)js_getnum(args[2]);
  double col = js_getnum(args[3]);
  int rMod = (int)js_getnum(args[4]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = (lv_meter_scale_t *)scP;

  lv_meter_indicator_t *ind = lv_meter_add_arc(mt, sc, width, lv_color_hex((uint32_t)col), rMod);
  intptr_t ret = (intptr_t)ind;
  return js_mknum((double)ret);
}

static jsval_t js_lv_meter_add_scale_lines(struct js *js, jsval_t *args, int nargs) {  // (meterH, scalePtr, color_main, color_grad, local, width_mod)
  // returns indicator pointer
  if (nargs < 6) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  intptr_t scP = (intptr_t)js_getnum(args[1]);
  double colorM = js_getnum(args[2]);
  double colorG = js_getnum(args[3]);
  bool local = (bool)js_getnum(args[4]);
  int widthMod = (int)js_getnum(args[5]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = (lv_meter_scale_t *)scP;

  lv_meter_indicator_t *ind = lv_meter_add_scale_lines(mt, sc,
                                                       lv_color_hex((uint32_t)colorM),
                                                       lv_color_hex((uint32_t)colorG),
                                                       local, widthMod);
  intptr_t ret = (intptr_t)ind;
  return js_mknum((double)ret);
}

static jsval_t js_lv_meter_add_needle_line(struct js *js, jsval_t *args, int nargs) {  // (meterH, scalePtr, width, color, rMod)
  if (nargs < 5) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  intptr_t scP = (intptr_t)js_getnum(args[1]);
  int width = (int)js_getnum(args[2]);
  double col = js_getnum(args[3]);
  int rMod = (int)js_getnum(args[4]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = (lv_meter_scale_t *)scP;

  lv_meter_indicator_t *ind = lv_meter_add_needle_line(mt, sc, width, lv_color_hex((uint32_t)col), rMod);
  intptr_t ret = (intptr_t)ind;
  return js_mknum((double)ret);
}

static jsval_t js_lv_meter_add_needle_img(struct js *js, jsval_t *args, int nargs) {  // (meterH, scalePtr, srcAddr, pivot_x, pivot_y)
  // returns indicator pointer
  if (nargs < 5) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  intptr_t scP = (intptr_t)js_getnum(args[1]);
  // "srcAddr" is an image source pointer or something
  intptr_t srcPtr = (intptr_t)js_getnum(args[2]);
  int pivotX = (int)js_getnum(args[3]);
  int pivotY = (int)js_getnum(args[4]);

  // If we have a global or static "LV_IMG_DECLARE(img_hand);" we normally pass &img_hand
  // from JS. That means we store "img_hand" pointer in a variable.
  // For simplicity let's assume srcPtr is the actual pointer to an lv_img_dsc_t.

  const lv_img_dsc_t *src_dsc = (const lv_img_dsc_t *)srcPtr;

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = (lv_meter_scale_t *)scP;

  lv_meter_indicator_t *ind = lv_meter_add_needle_img(mt, sc, src_dsc, pivotX, pivotY);
  intptr_t ret = (intptr_t)ind;
  return js_mknum((double)ret);
}

// meter set indicator
static jsval_t js_lv_meter_set_indicator_start_value(struct js *js, jsval_t *args, int nargs) {  // (meterH, indicatorPtr, startVal)
  if (nargs < 3) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  intptr_t indP = (intptr_t)js_getnum(args[1]);
  int stVal = (int)js_getnum(args[2]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_indicator_t *ind = (lv_meter_indicator_t *)indP;

  lv_meter_set_indicator_start_value(mt, ind, stVal);
  return js_mknull();
}

static jsval_t js_lv_meter_set_indicator_end_value(struct js *js, jsval_t *args, int nargs) {  // (meterH, indicatorPtr, endVal)
  if (nargs < 3) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  intptr_t indP = (intptr_t)js_getnum(args[1]);
  int endVal = (int)js_getnum(args[2]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_indicator_t *ind = (lv_meter_indicator_t *)indP;

  lv_meter_set_indicator_end_value(mt, ind, endVal);
  return js_mknull();
}

static jsval_t js_lv_meter_set_indicator_value(struct js *js, jsval_t *args, int nargs) {  // (meterH, indicatorPtr, val)
  if (nargs < 3) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  intptr_t indP = (intptr_t)js_getnum(args[1]);
  int val = (int)js_getnum(args[2]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_indicator_t *ind = (lv_meter_indicator_t *)indP;

  lv_meter_set_indicator_value(mt, ind, val);
  return js_mknull();
}

/********************************************************************************
 * SPAN
 ********************************************************************************/
static jsval_t js_lv_spangroup_create(struct js *js, jsval_t *args, int nargs) {
  lv_obj_t *spg = lv_spangroup_create(lv_scr_act());
  int handle = store_lv_obj(spg);
  return js_mknum(handle);
}

static jsval_t js_lv_spangroup_set_align(struct js *js, jsval_t *args, int nargs) {  // (spangroupH, alignEnum=LV_TEXT_ALIGN_LEFT/CENTER/RIGHT/AUTO)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int alg = (int)js_getnum(args[1]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_spangroup_set_align(spg, (lv_text_align_t)alg);
  return js_mknull();
}

static jsval_t js_lv_spangroup_set_overflow(struct js *js, jsval_t *args, int nargs) {  // (spangroupH, overflowEnum=LV_SPAN_OVERFLOW_CLIP/ELLIPSIS)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int ovf = (int)js_getnum(args[1]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_spangroup_set_overflow(spg, (lv_span_overflow_t)ovf);
  return js_mknull();
}

static jsval_t js_lv_spangroup_set_indent(struct js *js, jsval_t *args, int nargs) {  // (spangroupH, indentPX)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int indent = (int)js_getnum(args[1]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_spangroup_set_indent(spg, indent);
  return js_mknull();
}

static jsval_t js_lv_spangroup_set_mode(struct js *js, jsval_t *args, int nargs) {  // (spangroupH, mode=LV_SPAN_MODE_FIXED/NOWRAP/BREAK)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int md = (int)js_getnum(args[1]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_spangroup_set_mode(spg, (lv_span_mode_t)md);
  return js_mknull();
}

static jsval_t js_lv_spangroup_new_span(struct js *js, jsval_t *args, int nargs) {  // (spangroupH) -> pointer
  if (nargs < 1) return js_mknull();
  int h = (int)js_getnum(args[0]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_span_t *sp = lv_spangroup_new_span(spg);
  intptr_t ret = (intptr_t)sp;
  return js_mknum((double)ret);
}

static jsval_t js_lv_span_set_text(struct js *js, jsval_t *args, int nargs) {  // (spanPtr, text)
  if (nargs < 2) return js_mknull();
  intptr_t spP = (intptr_t)js_getnum(args[0]);
  const char *txt = js_str(js, args[1]);
  if (!txt) return js_mknull();

  lv_span_t *sp = (lv_span_t *)spP;
  lv_span_set_text(sp, txt);
  return js_mknull();
}

static jsval_t js_lv_span_set_text_static(struct js *js, jsval_t *args, int nargs) {  // (spanPtr, text) => static
  if (nargs < 2) return js_mknull();
  intptr_t spP = (intptr_t)js_getnum(args[0]);
  const char *txt = js_str(js, args[1]);
  if (!txt) return js_mknull();

  lv_span_t *sp = (lv_span_t *)spP;
  lv_span_set_text_static(sp, txt);
  return js_mknull();
}

static jsval_t js_lv_spangroup_refr_mode(struct js *js, jsval_t *args, int nargs) {  // (spangroupH)
  if (nargs < 1) return js_mknull();
  int h = (int)js_getnum(args[0]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_spangroup_refr_mode(spg);
  return js_mknull();
}

/*******************************************************
 * LINE BRIDGING
 *******************************************************/

static jsval_t js_lv_line_create(struct js *js, jsval_t *args, int nargs) {
  lv_obj_t *line = lv_line_create(lv_scr_act());
  int handle = store_lv_obj(line);
  LOGF("lv_line_create => handle %d\n", handle);
  return js_mknum(handle);
}

// Setting points requires us to parse an array of {x,y} from JS or something
// For simplicity, here's a bridging that receives e.g. (lineH, x0, y0, x1, y1, x2, y2, ...)
static jsval_t js_lv_line_set_points(struct js *js, jsval_t *args, int nargs) {  // Must have at least (lineH, x0, y0)
  if (nargs < 3) return js_mknull();
  int h = (int)js_getnum(args[0]);

  // The rest are coordinate pairs
  int pairCount = (nargs - 1) / 2;  // minus 1 for the handle, then each 2 = one point
  if (pairCount < 1) return js_mknull();

  lv_obj_t *line = get_lv_obj(h);
  if (!line) return js_mknull();

  static lv_point_t points[32];        // up to 16 points
  if (pairCount > 16) pairCount = 16;  // clamp

  int idx = 1;  // start reading from arg[1]
  for (int i = 0; i < pairCount; i++) {
    int x = (int)js_getnum(args[idx++]);
    int y = (int)js_getnum(args[idx++]);
    points[i].x = x;
    points[i].y = y;
  }

  lv_line_set_points(line, points, pairCount);
  return js_mknull();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~ 1) HTTP ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Helper function to read the HTTP response body

String readHttpResponseBody(WiFiClient &client) {
  String headers;
  String body;
  bool chunked = false;
  int contentLength = -1;

  // Read headers - check both connected and available for robustness
  unsigned long timeout = millis() + 10000;  // 10 second timeout
  while ((client.connected() || client.available()) && millis() < timeout) {
    if (!client.available()) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    String line = client.readStringUntil('\n');
    line.trim();  // Remove \r and whitespace
    if (line.length() == 0) {  // Empty line = end of headers
      break;
    }
    headers += line + "\n";
    if (line.indexOf("Transfer-Encoding: chunked") >= 0) {
      chunked = true;
    }
    // Parse Content-Length
    if (line.startsWith("content-length:") || line.startsWith("Content-Length:")) {
      int colonPos = line.indexOf(':');
      if (colonPos > 0) {
        String lenStr = line.substring(colonPos + 1);
        lenStr.trim();
        contentLength = lenStr.toInt();
      }
    }
  }

  if (chunked) {
    timeout = millis() + 10000;
    while (millis() < timeout) {
      // Read chunk size
      String sizeLine = client.readStringUntil('\n');
      sizeLine.trim();
      int chunkSize = strtol(sizeLine.c_str(), NULL, 16);
      if (chunkSize <= 0) {            // No more chunks
        client.readStringUntil('\n');  // Read trailing \r\n
        break;
      }

      // Read the chunk data
      char buf[256];
      int bytesRead = 0;
      while (bytesRead < chunkSize) {
        int toRead = chunkSize - bytesRead;
        if (toRead > (int)sizeof(buf)) toRead = sizeof(buf);
        int n = client.readBytes(buf, toRead);
        if (n <= 0) break;  // Timeout or error
        body += String(buf).substring(0, n);
        bytesRead += n;
      }

      client.readStringUntil('\n');  // Read trailing \r\n
    }
  } else if (contentLength > 0) {
    // Use Content-Length to read exact number of bytes
    body.reserve(contentLength);
    int bytesRead = 0;
    timeout = millis() + 10000;
    while (bytesRead < contentLength && millis() < timeout) {
      if (client.available()) {
        char c = client.read();
        body += c;
        bytesRead++;
      } else if (!client.connected()) {
        break;
      } else {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
  } else {
    // No Content-Length, read until connection closes
    timeout = millis() + 10000;
    while ((client.connected() || client.available()) && millis() < timeout) {
      if (client.available()) {
        char c = client.read();
        body += c;
      } else {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
  }

  return body;
}

// Bridging function to parse JSON and extract a value for a given key
static jsval_t js_parse_json_value(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) {
    LOG("js_parse_json_value: Not enough arguments");
    return js_mkstr(js, "", 0);
  }

  // Retrieve JSON string
  size_t json_len;
  char *jsonStr_cstr = js_getstr(js, args[0], &json_len);
  if (!jsonStr_cstr) {
    LOG("js_parse_json_value: Argument 1 is not a string");
    return js_mkstr(js, "", 0);
  }
  String jsonStr(jsonStr_cstr, json_len);

  LOGF("js_parse_json_value: Retrieved JSON string (%d bytes): %s\n", json_len, jsonStr.c_str());

  // Retrieve key string
  size_t key_len;
  char *key_cstr = js_getstr(js, args[1], &key_len);
  if (!key_cstr) {
    LOG("js_parse_json_value: Argument 2 is not a string");
    return js_mkstr(js, "", 0);
  }
  String keyStr(key_cstr, key_len);

  LOGF("js_parse_json_value: Retrieved key string (%d bytes): %s\n", key_len, keyStr.c_str());

  // Strip surrounding quotes if present
  if (keyStr.startsWith("\"") && keyStr.endsWith("\"") && keyStr.length() >= 2) {
    keyStr = keyStr.substring(1, keyStr.length() - 1);
    LOGF("js_parse_json_value: Stripped quotes from key. New key: '%s'\n", keyStr.c_str());
  }

  // Parse JSON using ArduinoJson
  StaticJsonDocument<1024> doc;  // Adjust size as needed
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) {
    Serial.print("js_parse_json_value: JSON parse failed: ");
    LOG(error.c_str());
    return js_mkstr(js, "", 0);
  }

  // Check if JSON is an object
  if (!doc.is<JsonObject>()) {
    LOG("js_parse_json_value: Parsed JSON is not an object");
    return js_mkstr(js, "", 0);
  }

  LOG("js_parse_json_value: Parsed JSON keys and values:");
  JsonObject obj = doc.as<JsonObject>();
  for (JsonPair kv : obj) {
    String valStr = kv.value().as<String>();
    LOGF("Key: %s, Value: %s\n", kv.key().c_str(), valStr.c_str());
  }

  // Extract the value
  JsonVariant value = obj[keyStr.c_str()];

  // Check if the key exists
  if (value.isNull()) {
    LOGF("js_parse_json_value: Key '%s' not found or null\n", keyStr.c_str());
    return js_mkstr(js, "", 0);
  }

  // Convert the value to string, regardless of its type
  String resultStr;
  if (value.is<const char *>()) {
    resultStr = String(value.as<const char *>());
  } else if (value.is<double>()) {
    resultStr = String(value.as<double>());
  } else if (value.is<bool>()) {
    resultStr = value.as<bool>() ? "true" : "false";
  } else {  // For other types, attempt to stringify
    resultStr = String(value.as<String>());
  }

  LOGF("js_parse_json_value: Extracted '%s': %s\n", keyStr.c_str(), resultStr.c_str());

  // Return the extracted value to JavaScript
  return js_mkstr(js, resultStr.c_str(), resultStr.length());
}

// Bridging function to perform string index search
static jsval_t js_str_index_of(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) {
    LOG("str_index_of: Not enough arguments");
    return js_mknum(-1);
  }

  // Retrieve haystack string
  size_t haystack_len;
  char *haystack_cstr = js_getstr(js, args[0], &haystack_len);
  if (!haystack_cstr) {
    LOG("str_index_of: Argument 1 is not a string");
    return js_mknum(-1);
  }
  String haystackStr(haystack_cstr, haystack_len);

  // Retrieve needle string
  size_t needle_len;
  char *needle_cstr = js_getstr(js, args[1], &needle_len);
  if (!needle_cstr) {
    LOG("str_index_of: Argument 2 is not a string");
    return js_mknum(-1);
  }
  String needleStr(needle_cstr, needle_len);

  LOGF("str_index_of: Retrieved haystack ('%s') and needle ('%s')\n", haystackStr.c_str(), needleStr.c_str());

  if (haystackStr.startsWith("\"") && haystackStr.endsWith("\"") && haystackStr.length() >= 2) {
    haystackStr = haystackStr.substring(1, haystackStr.length() - 1);
    LOGF("str_index_of: Stripped quotes from haystack. New haystack: '%s'\n", haystackStr.c_str());
  }

  if (needleStr.startsWith("\"") && needleStr.endsWith("\"") && needleStr.length() >= 2) {
    needleStr = needleStr.substring(1, needleStr.length() - 1);
    LOGF("str_index_of: Stripped quotes from needle. New needle: '%s'\n", needleStr.c_str());
  }

  LOGF("str_index_of: Searching for '%s' in '%s'\n", needleStr.c_str(), haystackStr.c_str());

  int index = haystackStr.indexOf(needleStr);
  if (index == -1) {
    LOG("str_index_of: Needle not found");
    return js_mknum(-1);
  }

  LOGF("str_index_of: Found at index %d\n", index);
  return js_mknum(index);
}

// Bridging function to perform string substring extraction
static jsval_t js_str_substring(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) {
    LOG("str_substring: Not enough arguments");
    return js_mkstr(js, "", 0);
  }

  // Retrieve string
  size_t str_len;
  char *str_cstr = js_getstr(js, args[0], &str_len);
  if (!str_cstr) {
    LOG("str_substring: Argument 1 is not a string");
    return js_mkstr(js, "", 0);
  }
  String strStr(str_cstr, str_len);

  // Check if arguments 2 and 3 are numbers
  if (js_type(args[1]) != JS_NUM || js_type(args[2]) != JS_NUM) {
    LOG("str_substring: Arguments 2 and 3 must be numbers");
    return js_mkstr(js, "", 0);
  }

  // Extract numerical values
  int start = (int)js_getnum(args[1]);
  int length = (int)js_getnum(args[2]);

  LOGF("str_substring: Retrieved string ('%s'), start (%d), length (%d)\n", strStr.c_str(), start, length);

  if (strStr.startsWith("\"") && strStr.endsWith("\"") && strStr.length() >= 2) {
    strStr = strStr.substring(1, strStr.length() - 1);
    LOGF("str_substring: Stripped quotes from string. New string: '%s'\n", strStr.c_str());
  }

  // Handle negative length (extract until end)
  if (length < 0) {
    strStr = strStr.substring(start);
  } else {  // Ensure that start + length does not exceed string length
    int end = start + length;
    if (end > strStr.length()) {
      end = strStr.length();
    }
    strStr = strStr.substring(start, end);
  }

  LOGF("str_substring: Extracted substring '%s' with length %d\n", strStr.c_str(), length);
  return js_mkstr(js, strStr.c_str(), strStr.length());
}
static jsval_t js_http_get(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkstr(js, "", 0);
  const char *rawUrl = js_str(js, args[0]);
  if (!rawUrl) return js_mkstr(js, "", 0);

  // Convert to Arduino String
  String url(rawUrl);

  // Strip quotes if needed
  if (url.startsWith("\"") && url.endsWith("\"")) {
    url.remove(0, 1);
    url.remove(url.length() - 1, 1);
  }

  // Determine if HTTPS or HTTP
  bool useSSL = true;
  const String HTTPS_PREFIX = "https://";
  const String HTTP_PREFIX = "http://";

  if (url.startsWith(HTTPS_PREFIX)) {
    url.remove(0, HTTPS_PREFIX.length());
    useSSL = true;
    LOG("js_http_get => Using HTTPS");
  } else if (url.startsWith(HTTP_PREFIX)) {
    url.remove(0, HTTP_PREFIX.length());
    useSSL = false;
    LOG("js_http_get => Using HTTP");
  } else {
    useSSL = true;
    LOG("js_http_get => No prefix, assuming HTTPS");
  }

  int slashPos = url.indexOf('/');
  String host, path;
  if (slashPos < 0) {
    host = url;
    path = "/";
  } else {
    host = url.substring(0, slashPos);
    path = url.substring(slashPos);
  }

  LOG("Parsed host='" + host + "', path='" + path + "'");

  String response;

  if (useSSL) {
    WiFiClientSecure client;
    if (g_httpCAcert) {
      client.setCACert(g_httpCAcert);
      LOG("Using user-supplied CA cert");
    } else {
      client.setInsecure();
      LOG("No CA cert => setInsecure()");
    }

    LOGF("Connecting to '%s':443...\n", host.c_str());
    if (!client.connect(host.c_str(), 443)) {
      LOG("Connection failed!");
      return js_mkstr(js, "", 0);
    }
    LOG("Connected => sending GET request");

    client.print(String("GET ") + path + " HTTP/1.1\r\n");
    client.print(String("Host: ") + host + "\r\n");
    for (auto &hdr : g_http_headers) {
      client.print(hdr.first);
      client.print(": ");
      client.print(hdr.second);
      client.print("\r\n");
    }
    client.print("Connection: close\r\n\r\n");

    response = readHttpResponseBody(client);
    client.stop();
  } else {
    WiFiClient client;
    LOGF("Connecting to '%s':80...\n", host.c_str());
    if (!client.connect(host.c_str(), 80)) {
      LOG("Connection failed!");
      return js_mkstr(js, "", 0);
    }
    LOG("Connected => sending GET request");

    client.print(String("GET ") + path + " HTTP/1.1\r\n");
    client.print(String("Host: ") + host + "\r\n");
    for (auto &hdr : g_http_headers) {
      client.print(hdr.first);
      client.print(": ");
      client.print(hdr.second);
      client.print("\r\n");
    }
    client.print("Connection: close\r\n\r\n");

    response = readHttpResponseBody(client);
    client.stop();
  }

  LOGF("Done reading. response size=%d\n", response.length());

  return js_mkstr(js, response.c_str(), response.length());
}

static jsval_t js_http_post(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mkstr(js, "", 0);
  const char *rawUrl = js_str(js, args[0]);
  const char *body = js_str(js, args[1]);
  if (!rawUrl || !body) return js_mkstr(js, "", 0);

  // Convert to Arduino Strings for easy manipulation
  String url(rawUrl);
  String jsonBody(body);

  // Strip quotes if any
  if (url.startsWith("\"") && url.endsWith("\"")) {
    url.remove(0, 1);
    url.remove(url.length() - 1, 1);
  }
  if (jsonBody.startsWith("\"") && jsonBody.endsWith("\"")) {
    jsonBody.remove(0, 1);
    jsonBody.remove(jsonBody.length() - 1, 1);
  }

  // We'll assume "https://"
  const String HTTPS_PREFIX = "https://";
  if (url.startsWith(HTTPS_PREFIX)) {
    url.remove(0, HTTPS_PREFIX.length());
  }

  // Find first slash => host + path
  int slashPos = url.indexOf('/');
  String host, path;
  if (slashPos < 0) {
    host = url;
    path = "/";
  } else {
    host = url.substring(0, slashPos);
    path = url.substring(slashPos);
  }

  LOG("\njs_http_post => manual approach");
  LOG("Host: " + host);
  LOG("Path: " + path);
  LOGF("Body length=%d\n", jsonBody.length());

  // WiFiClientSecure
  WiFiClientSecure client;
  if (g_httpCAcert) {
    client.setCACert(g_httpCAcert);
    LOG("Using user-supplied CA cert (POST)");
  } else {
    client.setInsecure();
    LOG("No CA => setInsecure() (POST)");
  }

  // Connect on port 443
  LOGF("Connecting to '%s':443...\n", host.c_str());
  if (!client.connect(host.c_str(), 443)) {
    LOG("Connection failed (POST)!");
    return js_mkstr(js, "", 0);
  }
  LOG("Connected => sending POST request");

  // Construct POST request
  // e.g.:
  // POST /path HTTP/1.1
  // Content-Type: application/json
  // Content-Length: ...

  // {body}
  client.print(String("POST ") + path + " HTTP/1.1\r\n");
  client.print(String("Host: ") + host + "\r\n");

  // If you want custom headers from g_http_headers, do:
  // for(auto &hdr : g_http_headers) { ... }

  client.print("Content-Type: application/json\r\n");
  client.printf("Content-Length: %d\r\n", jsonBody.length());
  client.print("Connection: close\r\n\r\n");

  // Send body
  client.print(jsonBody);

  // Read entire response
  String response = readHttpResponseBody(client);
  client.stop();

  LOGF("Done POST. response size=%d\n", response.length());

  // Return entire raw HTTP response
  return js_mkstr(js, response.c_str(), response.length());
}

static jsval_t js_http_delete(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkstr(js, "", 0);
  const char *rawUrl = js_str(js, args[0]);
  if (!rawUrl) return js_mkstr(js, "", 0);

  String url(rawUrl);

  // Remove quotes if any
  if (url.startsWith("\"") && url.endsWith("\"")) {
    url.remove(0, 1);
    url.remove(url.length() - 1, 1);
  }

  // Assume https://
  const String HTTPS_PREFIX = "https://";
  if (url.startsWith(HTTPS_PREFIX)) {
    url.remove(0, HTTPS_PREFIX.length());
  }

  // Split host/path
  int slashPos = url.indexOf('/');
  String host, path;
  if (slashPos < 0) {
    host = url;
    path = "/";
  } else {
    host = url.substring(0, slashPos);
    path = url.substring(slashPos);
  }

  LOG("\njs_http_delete => manual approach");
  LOG("Host: " + host);
  LOG("Path: " + path);

  // WiFiClientSecure
  WiFiClientSecure client;
  if (g_httpCAcert) {
    client.setCACert(g_httpCAcert);
    LOG("Using user-supplied CA cert (DELETE)");
  } else {
    client.setInsecure();
    LOG("No CA => setInsecure() (DELETE)");
  }

  // Connect
  LOGF("Connecting to '%s':443...\n", host.c_str());
  if (!client.connect(host.c_str(), 443)) {
    LOG("Connection failed (DELETE)!");
    return js_mkstr(js, "", 0);
  }
  LOG("Connected => sending DELETE request");

  // e.g.:
  // DELETE /path HTTP/1.1
  client.print(String("DELETE ") + path + " HTTP/1.1\r\n");
  client.print(String("Host: ") + host + "\r\n");

  // If you want custom headers from g_http_headers, do:
  // for(auto &hdr : g_http_headers) { ... }

  client.print("Connection: close\r\n\r\n");

  // read entire response
  String body = readHttpResponseBody(client);
  client.stop();

  LOGF("Done DELETE. response size=%d\n", body.length());

  // Return entire raw HTTP response
  return js_mkstr(js, body.c_str(), body.length());
}

static jsval_t js_http_set_header(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mkfalse();

  const char *key = js_str(js, args[0]);
  const char *value = js_str(js, args[1]);
  if (!key || !value) return js_mkfalse();

  // Convert to Arduino Strings for easy storage
  String k(key), v(value);

  // Optionally strip leading/trailing quotes if needed
  if (k.startsWith("\"") && k.endsWith("\"")) {
    k.remove(0, 1);
    k.remove(k.length() - 1, 1);
  }
  if (v.startsWith("\"") && v.endsWith("\"")) {
    v.remove(0, 1);
    v.remove(v.length() - 1, 1);
  }

  // Append to our global vector
  g_http_headers.push_back(std::make_pair(k, v));
  LOGF("Added header: %s: %s\n", k.c_str(), v.c_str());
  return js_mktrue();
}

static jsval_t js_http_clear_headers(struct js *js, jsval_t *args, int nargs) {
  g_http_headers.clear();
  return js_mktrue();
}

static jsval_t js_http_set_ca_cert_from_sd(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  const char *rawPath = js_str(js, args[0]);
  if (!rawPath) return js_mkfalse();

  // Strip quotes if present
  String path(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }

  // Open file from SD
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    LOGF("Failed to open CA cert file: %s\n", path.c_str());
    return js_mkfalse();
  }

  size_t size = f.size();
  if (size == 0) {
    LOGF("CA file is empty: %s\n", path.c_str());
    f.close();
    return js_mkfalse();
  }

  // Reallocate or free old buffer
  if (g_httpCAcert) {
    free(g_httpCAcert);
    g_httpCAcert = nullptr;
  }

  // Allocate enough bytes (include space for trailing '\0')
  g_httpCAcert = (char *)malloc(size + 1);
  if (!g_httpCAcert) {
    LOG("Not enough RAM to store CA cert!");
    f.close();
    return js_mkfalse();
  }

  // Read the file
  size_t bytesRead = f.readBytes(g_httpCAcert, size);
  f.close();
  g_httpCAcert[bytesRead] = '\0';  // Null-terminate

  LOGF("Loaded CA cert (%u bytes) from SD file: %s\n", (unsigned)bytesRead, path.c_str());
  return js_mktrue();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~ 4) Extended SD ops ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// We already have sd_list_dir, sd_read_file, sd_write_file. Add file delete:
static jsval_t js_sd_delete_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  const char *path = js_str(js, args[0]);
  if (!path) return js_mkfalse();

  String fullPath = String(path);
  if (SD_MMC.exists(fullPath)) {
    bool ok = SD_MMC.remove(fullPath);
    return ok ? js_mktrue() : js_mkfalse();
  }
  return js_mkfalse();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~ 5) Basic BLE bridging ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Example usage from JS:
//   ble_init("ESP32-S3 Demo", "4fafc201-1fb5-459e-8fcc-c5c9c331914b", "beb5483e-36e1-4688-b7f5-ea07361b26a8");
//   ble_write("Hello from JS!");
//   if( ble_is_connected() ) { ... }

// Callbacks for NimBLE
class MyServerCallbacks : public NimBLEServerCallbacks {
public:
  void onConnect(NimBLEServer *pServer) {
    g_bleConnected = true;
    LOG("BLE device connected");
  }

  void onDisconnect(NimBLEServer *pServer) {
    g_bleConnected = false;
    LOG("BLE device disconnected");
    pServer->startAdvertising();
  }
};

class MyCharCallbacks : public NimBLECharacteristicCallbacks {
public:
  void onWrite(NimBLECharacteristic *pCharacteristic) {
    std::string rxData = pCharacteristic->getValue();
    LOGF("BLE Received: %s\n", rxData.c_str());
  }
};

// ble_init(devName, serviceUUID, charUUID)
static jsval_t js_ble_init(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) return js_mkfalse();
  const char *devName = js_str(js, args[0]);
  const char *svcUUID = js_str(js, args[1]);
  const char *charUUID = js_str(js, args[2]);
  if (!devName || !svcUUID || !charUUID) return js_mkfalse();

  // Initialize NimBLE
  NimBLEDevice::init(devName);

  // Create server
  g_bleServer = NimBLEDevice::createServer();
  g_bleServer->setCallbacks(new MyServerCallbacks());

  // Create a BLE service
  NimBLEService *pService = g_bleServer->createService(svcUUID);

  // Create a BLE Characteristic
  g_bleChar = pService->createCharacteristic(
    charUUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  g_bleChar->setCallbacks(new MyCharCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  g_bleServer->getAdvertising()->start();
  LOG("NimBLE advertising started");
  return js_mktrue();
}

// ble_is_connected() => bool
static jsval_t js_ble_is_connected(struct js *js, jsval_t *args, int nargs) {
  return g_bleConnected ? js_mktrue() : js_mkfalse();
}

// ble_write(str)
static jsval_t js_ble_write(struct js *js, jsval_t *args, int nargs) {
  if (!g_bleChar) return js_mkfalse();
  if (nargs < 1) return js_mkfalse();
  const char *data = js_str(js, args[0]);
  if (!data) return js_mkfalse();

  g_bleChar->setValue(data);
  g_bleChar->notify();
  return js_mktrue();
}

// MQTT message callback from PubSubClient

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  LOGF("[MQTT] Message arrived on topic '%s'\n", topic);

  // If we have a non-empty callback name, build a snippet and eval
  if (g_mqttCallbackName[0] != '\0') {  // Convert char* topic and payload to a C++ string
    String topicStr(topic);
    String msgStr;
    msgStr.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
      msgStr += (char)payload[i];
    }

    // Build snippet: myCallback('topicString','payloadString')
    char snippet[512];
    // Use %s for the function name, plus single quotes around the data
    snprintf(snippet, sizeof(snippet),
             "%s('%s','%s');",
             g_mqttCallbackName,
             topicStr.c_str(),
             msgStr.c_str());

    LOGF("[MQTT] Evaluating snippet: %s\n", snippet);

    // Evaluate snippet
    jsval_t res = js_eval(js, snippet, strlen(snippet));
    // Optionally check if res is error
    if (js_type(res) == JS_ERR) {
      Serial.print("[MQTT] Callback error: ");
      LOG(js_str(js, res));
    }
  }
}
// JavaScript-exposed bridging functions
// mqtt_init(broker, port)
static jsval_t js_mqtt_init(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mkfalse();
  const char *broker = js_str(js, args[0]);
  int port = (int)js_getnum(args[1]);

  if (!broker || port <= 0) return js_mkfalse();

  g_mqttClient.setServer(broker, port);
  g_mqttClient.setCallback(onMqttMessage);
  LOGF("[MQTT] init => broker=%s port=%d\n", broker, port);

  return js_mktrue();
}

// mqtt_connect(clientID, user, pass)
static jsval_t js_mqtt_connect(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  const char *clientID = js_str(js, args[0]);
  const char *user = (nargs >= 2) ? js_str(js, args[1]) : nullptr;
  const char *pass = (nargs >= 3) ? js_str(js, args[2]) : nullptr;

  bool ok = false;
  if (user && pass && strlen(user) && strlen(pass)) {
    ok = g_mqttClient.connect(clientID, user, pass);
  } else {
    ok = g_mqttClient.connect(clientID);
  }

  if (ok) {
    LOG("[MQTT] Connected successfully");
    return js_mktrue();
  } else {
    LOGF("[MQTT] Connect failed, rc=%d\n", g_mqttClient.state());
    return js_mkfalse();
  }
}

// mqtt_publish(topic, message)
static jsval_t js_mqtt_publish(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mkfalse();
  const char *topic = js_str(js, args[0]);
  const char *message = js_str(js, args[1]);
  if (!topic || !message) return js_mkfalse();

  bool ok = g_mqttClient.publish(topic, message);
  return ok ? js_mktrue() : js_mkfalse();
}

// mqtt_subscribe(topic)
static jsval_t js_mqtt_subscribe(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  const char *topic = js_str(js, args[0]);
  if (!topic) return js_mkfalse();

  bool ok = g_mqttClient.subscribe(topic);
  LOGF("[MQTT] Subscribed to '%s'? => %s\n", topic, ok ? "OK" : "FAIL");
  return ok ? js_mktrue() : js_mkfalse();
}

// mqtt_loop()
static jsval_t js_mqtt_loop(struct js *js, jsval_t *args, int nargs) {
  g_mqttClient.loop();
  return js_mknull();
}

// mqtt_on_message("myCallback")
static jsval_t js_mqtt_on_message(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();

  // Check if user passed a string naming the function
  size_t len = 0;
  char *str = js_getstr(js, args[0], &len);
  if (!str || len == 0 || len >= sizeof(g_mqttCallbackName)) {
    return js_mkfalse();
  }

  // Copy that name to our global
  memset(g_mqttCallbackName, 0, sizeof(g_mqttCallbackName));
  memcpy(g_mqttCallbackName, str, len);  // not zero-terminated by default
  g_mqttCallbackName[len] = '\0';

  Serial.print("[MQTT] JS callback name set to: ");
  LOG(g_mqttCallbackName);
  return js_mktrue();
}

// This function tries to connect to your MQTT broker

bool doMqttConnect() {  // extern const char* g_mqttBroker;
  // extern int         g_mqttPort;
  // g_mqttClient.setServer(g_mqttBroker, g_mqttPort);

  LOG("[MQTT] Checking broker connection...");

  // Attempt to connect with e.g. a default clientID (or user/pass if needed)
  bool ok = g_mqttClient.connect("WebScreenClient");
  if (!ok) {  // Print the reason code: g_mqttClient.state()
    LOGF("[MQTT] Connect fail, rc=%d\n", g_mqttClient.state());
    return false;
  }

  // If connected, subscribe to any default topic if you want:
  // g_mqttClient.subscribe("some/topic");

  LOG("[MQTT] Connected successfully");
  return true;
}

// This function tries to reconnect Wi-Fi if Wi-Fi is down

bool doWiFiReconnect() {
  LOG("[WiFi] Checking connection...");

  // If you have an SSID/pass stored
  // extern String g_ssid;
  // extern String g_pass;
  // WiFi.begin(g_ssid.c_str(), g_pass.c_str());

  // We'll do a quick wait for up to ~3 seconds, just for example:
  // (Tune to your needs)
  for (int i = 0; i < 15; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[WiFi] Reconnected. IP=");
      LOG(WiFi.localIP());
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  LOG("[WiFi] Still not connected");
  return false;
}

// Call this regularly to maintain Wi-Fi + MQTT

void wifiMqttMaintainLoop() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    // Try reconnect every 10 seconds
    if (now - lastWiFiReconnectAttempt > 10000) {
      lastWiFiReconnectAttempt = now;
      LOG("[WiFi] Connection lost, attempting recon...");
      doWiFiReconnect();
    }
    // If Wi-Fi is still down, we skip MQTT
    return;
  }

  if (!g_mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > 10000) {
      lastMqttReconnectAttempt = now;
      LOG("[MQTT] Lost MQTT, trying reconnect...");
      if (doMqttConnect()) {
        lastMqttReconnectAttempt = 0;
      }
    }
  }

  // If connected, let PubSubClient process inbound/outbound messages
  g_mqttClient.loop();
}

/******************************************************************************
 * I) Register All JS Functions
 ******************************************************************************/

void register_js_functions() {
  jsval_t global = js_glob(js);

  // Basic
  js_set(js, global, "print", js_mkfun(js_print));
  js_set(js, global, "wifi_connect", js_mkfun(js_wifi_connect));
  js_set(js, global, "wifi_status", js_mkfun(js_wifi_status));
  js_set(js, global, "wifi_get_ip", js_mkfun(js_wifi_get_ip));
  js_set(js, global, "delay", js_mkfun(js_delay));
  js_set(js, global, "create_timer", js_mkfun(js_create_timer));
  js_set(js, global, "toNumber", js_mkfun(js_to_number));
  js_set(js, global, "numberToString", js_mkfun(js_number_to_string));

  // bridging for indexOf / substring
  js_set(js, global, "str_index_of", js_mkfun(js_str_index_of));
  js_set(js, global, "str_substring", js_mkfun(js_str_substring));

  js_set(js, global, "http_get", js_mkfun(js_http_get));
  js_set(js, global, "http_post", js_mkfun(js_http_post));
  js_set(js, global, "http_delete", js_mkfun(js_http_delete));
  js_set(js, global, "http_set_ca_cert_from_sd", js_mkfun(js_http_set_ca_cert_from_sd));
  js_set(js, global, "parse_json_value", js_mkfun(js_parse_json_value));
  js_set(js, global, "http_set_header", js_mkfun(js_http_set_header));
  js_set(js, global, "http_clear_headers", js_mkfun(js_http_clear_headers));

  // SD functions
  js_set(js, global, "sd_read_file", js_mkfun(js_sd_read_file));
  js_set(js, global, "sd_write_file", js_mkfun(js_sd_write_file));
  js_set(js, global, "sd_list_dir", js_mkfun(js_sd_list_dir));
  js_set(js, global, "sd_delete_file", js_mkfun(js_sd_delete_file));

  js_set(js, global, "ble_init", js_mkfun(js_ble_init));
  js_set(js, global, "ble_is_connected", js_mkfun(js_ble_is_connected));
  js_set(js, global, "ble_write", js_mkfun(js_ble_write));

  // GIF from memory
  js_set(js, global, "show_gif_from_sd", js_mkfun(js_show_gif_from_sd));

  // Basic shapes and labels.
  js_set(js, global, "draw_label", js_mkfun(js_lvgl_draw_label));
  js_set(js, global, "draw_rect", js_mkfun(js_lvgl_draw_rect));
  js_set(js, global, "show_image", js_mkfun(js_lvgl_show_image));
  js_set(js, global, "create_label", js_mkfun(js_create_label));
  js_set(js, global, "label_set_text", js_mkfun(js_label_set_text));

  // Handle-based image creation + transforms
  js_set(js, global, "create_image", js_mkfun(js_create_image));
  js_set(js, global, "create_image_from_ram", js_mkfun(js_create_image_from_ram));
  js_set(js, global, "rotate_obj", js_mkfun(js_rotate_obj));
  js_set(js, global, "move_obj", js_mkfun(js_move_obj));
  js_set(js, global, "animate_obj", js_mkfun(js_animate_obj));

  // Style creation + property setters
  js_set(js, global, "create_style", js_mkfun(js_create_style));
  js_set(js, global, "obj_add_style", js_mkfun(js_obj_add_style));

  js_set(js, global, "style_set_radius", js_mkfun(js_style_set_radius));
  js_set(js, global, "style_set_bg_opa", js_mkfun(js_style_set_bg_opa));
  js_set(js, global, "style_set_bg_color", js_mkfun(js_style_set_bg_color));
  js_set(js, global, "style_set_border_color", js_mkfun(js_style_set_border_color));
  js_set(js, global, "style_set_border_width", js_mkfun(js_style_set_border_width));
  js_set(js, global, "style_set_border_opa", js_mkfun(js_style_set_border_opa));
  js_set(js, global, "style_set_border_side", js_mkfun(js_style_set_border_side));
  js_set(js, global, "style_set_outline_width", js_mkfun(js_style_set_outline_width));
  js_set(js, global, "style_set_outline_color", js_mkfun(js_style_set_outline_color));
  js_set(js, global, "style_set_outline_pad", js_mkfun(js_style_set_outline_pad));
  js_set(js, global, "style_set_shadow_width", js_mkfun(js_style_set_shadow_width));
  js_set(js, global, "style_set_shadow_color", js_mkfun(js_style_set_shadow_color));
  js_set(js, global, "style_set_shadow_ofs_x", js_mkfun(js_style_set_shadow_ofs_x));
  js_set(js, global, "style_set_shadow_ofs_y", js_mkfun(js_style_set_shadow_ofs_y));
  js_set(js, global, "style_set_img_recolor", js_mkfun(js_style_set_img_recolor));
  js_set(js, global, "style_set_img_recolor_opa", js_mkfun(js_style_set_img_recolor_opa));
  js_set(js, global, "style_set_transform_angle", js_mkfun(js_style_set_transform_angle));
  js_set(js, global, "style_set_text_color", js_mkfun(js_style_set_text_color));
  js_set(js, global, "style_set_text_letter_space", js_mkfun(js_style_set_text_letter_space));
  js_set(js, global, "style_set_text_line_space", js_mkfun(js_style_set_text_line_space));
  js_set(js, global, "style_set_text_font", js_mkfun(js_style_set_text_font));
  js_set(js, global, "style_set_text_align", js_mkfun(js_style_set_text_align));
  js_set(js, global, "style_set_text_decor", js_mkfun(js_style_set_text_decor));
  js_set(js, global, "style_set_line_color", js_mkfun(js_style_set_line_color));
  js_set(js, global, "style_set_line_width", js_mkfun(js_style_set_line_width));
  js_set(js, global, "style_set_line_rounded", js_mkfun(js_style_set_line_rounded));
  js_set(js, global, "style_set_pad_all", js_mkfun(js_style_set_pad_all));
  js_set(js, global, "style_set_pad_left", js_mkfun(js_style_set_pad_left));
  js_set(js, global, "style_set_pad_right", js_mkfun(js_style_set_pad_right));
  js_set(js, global, "style_set_pad_top", js_mkfun(js_style_set_pad_top));
  js_set(js, global, "style_set_pad_bottom", js_mkfun(js_style_set_pad_bottom));
  js_set(js, global, "style_set_pad_ver", js_mkfun(js_style_set_pad_ver));
  js_set(js, global, "style_set_pad_hor", js_mkfun(js_style_set_pad_hor));
  js_set(js, global, "style_set_width", js_mkfun(js_style_set_width));
  js_set(js, global, "style_set_height", js_mkfun(js_style_set_height));
  js_set(js, global, "style_set_x", js_mkfun(js_style_set_x));
  js_set(js, global, "style_set_y", js_mkfun(js_style_set_y));

  // Object property setters
  js_set(js, global, "obj_set_size", js_mkfun(js_obj_set_size));
  js_set(js, global, "obj_align", js_mkfun(js_obj_align));

  // Scroll, flex, flags
  js_set(js, global, "obj_set_scroll_snap_x", js_mkfun(js_obj_set_scroll_snap_x));
  js_set(js, global, "obj_set_scroll_snap_y", js_mkfun(js_obj_set_scroll_snap_y));
  js_set(js, global, "obj_add_flag", js_mkfun(js_obj_add_flag));
  js_set(js, global, "obj_clear_flag", js_mkfun(js_obj_clear_flag));
  js_set(js, global, "obj_set_scroll_dir", js_mkfun(js_obj_set_scroll_dir));
  js_set(js, global, "obj_set_scrollbar_mode", js_mkfun(js_obj_set_scrollbar_mode));
  js_set(js, global, "obj_set_flex_flow", js_mkfun(js_obj_set_flex_flow));
  js_set(js, global, "obj_set_flex_align", js_mkfun(js_obj_set_flex_align));
  js_set(js, global, "obj_set_style_clip_corner", js_mkfun(js_obj_set_style_clip_corner));
  js_set(js, global, "obj_set_style_base_dir", js_mkfun(js_obj_set_style_base_dir));

  //==================== METER ============================
  js_set(js, global, "lv_meter_create", js_mkfun(js_lv_meter_create));
  js_set(js, global, "lv_meter_add_scale", js_mkfun(js_lv_meter_add_scale));
  js_set(js, global, "lv_meter_set_scale_ticks", js_mkfun(js_lv_meter_set_scale_ticks));
  js_set(js, global, "lv_meter_set_scale_major_ticks", js_mkfun(js_lv_meter_set_scale_major_ticks));
  js_set(js, global, "lv_meter_set_scale_range", js_mkfun(js_lv_meter_set_scale_range));
  js_set(js, global, "lv_meter_add_arc", js_mkfun(js_lv_meter_add_arc));
  js_set(js, global, "lv_meter_add_scale_lines", js_mkfun(js_lv_meter_add_scale_lines));
  js_set(js, global, "lv_meter_add_needle_line", js_mkfun(js_lv_meter_add_needle_line));
  js_set(js, global, "lv_meter_add_needle_img", js_mkfun(js_lv_meter_add_needle_img));
  js_set(js, global, "lv_meter_set_indicator_start_value", js_mkfun(js_lv_meter_set_indicator_start_value));
  js_set(js, global, "lv_meter_set_indicator_end_value", js_mkfun(js_lv_meter_set_indicator_end_value));
  js_set(js, global, "lv_meter_set_indicator_value", js_mkfun(js_lv_meter_set_indicator_value));

  //==================== SPAN ============================
  js_set(js, global, "lv_spangroup_create", js_mkfun(js_lv_spangroup_create));
  js_set(js, global, "lv_spangroup_set_align", js_mkfun(js_lv_spangroup_set_align));
  js_set(js, global, "lv_spangroup_set_overflow", js_mkfun(js_lv_spangroup_set_overflow));
  js_set(js, global, "lv_spangroup_set_indent", js_mkfun(js_lv_spangroup_set_indent));
  js_set(js, global, "lv_spangroup_set_mode", js_mkfun(js_lv_spangroup_set_mode));
  js_set(js, global, "lv_spangroup_new_span", js_mkfun(js_lv_spangroup_new_span));
  js_set(js, global, "lv_span_set_text", js_mkfun(js_lv_span_set_text));
  js_set(js, global, "lv_span_set_text_static", js_mkfun(js_lv_span_set_text_static));
  js_set(js, global, "lv_spangroup_refr_mode", js_mkfun(js_lv_spangroup_refr_mode));

  // ---------- LINE bridging
  js_set(js, global, "lv_line_create", js_mkfun(js_lv_line_create));
  js_set(js, global, "lv_line_set_points", js_mkfun(js_lv_line_set_points));

  // MQTT bridging
  js_set(js, global, "mqtt_init", js_mkfun(js_mqtt_init));
  js_set(js, global, "mqtt_connect", js_mkfun(js_mqtt_connect));
  js_set(js, global, "mqtt_publish", js_mkfun(js_mqtt_publish));
  js_set(js, global, "mqtt_subscribe", js_mkfun(js_mqtt_subscribe));
  js_set(js, global, "mqtt_loop", js_mkfun(js_mqtt_loop));
  js_set(js, global, "mqtt_on_message", js_mkfun(js_mqtt_on_message));
}
// K) The elk_task -- runs Elk + bridging in a separate FreeRTOS task

static void elk_task(void *pvParam) {
  js = js_create(elk_memory, sizeof(elk_memory));
  if (!js) {
    LOG("Failed to initialize Elk in elk_task");
    // Delete this task if you want
    vTaskDelete(NULL);
    return;
  }

  register_js_functions();

  if (!load_and_execute_js_script(g_script_filename.c_str())) {
    LOGF("Failed to load and execute JavaScript script from %s\n", g_script_filename.c_str());
    // Optionally handle the error
  } else {
    LOG("Script executed successfully in elk_task");
  }

  // so that the UI remains active
  for (;;) {
    if (g_mqtt_enabled) {
      wifiMqttMaintainLoop();
    }
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
