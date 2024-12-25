/********************************************************************************
 * Single-file Arduino Sketch
 * Demonstrates:
 *   - Elk + Wi-Fi + SD_MMC + LVGL
 *   - Registering a normal 'S' driver for typical SD usage
 *   - Registering a 'M' memory driver to feed data from PSRAM (for GIFs)
 *   - show_gif_from_sd("/wallpaper.gif") loads the file into PSRAM, then tells
 *     LVGL to open "M:mygif" from memory (bypassing SD streaming).
 *   - Basic label placeholders, directory listing, etc.
 *   - ***NEW***: draw_label, draw_rect, show_image to draw shapes/images from SD.
 *   - ***FIX***: in show_image() we strip extra quotes so it won't become S:"/messi.png".
 *
 * Prerequisites:
 *   1) "pins_config.h" in the same folder (pin definitions, LVGL_LCD_BUF_SIZE, etc.).
 *   2) "rm67162.h" for the AMOLED driver (in the same folder).
 *   3) "notification.h" if you want the compiled-in GIF approach (optional).
 *   4) Place a JavaScript file "/script.js" on your SD card that calls
 *      show_gif_from_sd("/wallpaper.gif"), draw_label(...), show_image(...), etc.
 *   5) Have "/messi.png" in your SD card root if you want to show that image.
 *
 * Copy/paste into a file named, e.g., "main.ino"
 ********************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include <stdio.h>

// 1) Include pin + config
#include "pins_config.h"

// 2) LVGL + display driver
#include <lvgl.h>
#include "rm67162.h"  // Your custom display driver

// 3) Elk
extern "C" {
  #include "elk.h"
}

// ------------------ Elk Memory Buffer ------------------
static uint8_t elk_memory[4096]; // Increase if needed

// ------------------ Global Elk Instance ----------------
struct js *js = NULL;

// ---------------------------------------------------------------------------------
// Section A: LVGL + Display init
// ---------------------------------------------------------------------------------
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf = NULL;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

void init_lvgl_display() {
  Serial.println("Initializing display...");

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  rm67162_init();
  lcd_setRotation(1);

  lv_init();

  // Allocate LVGL draw buffer in PSRAM
  buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE);
  if (!buf) {
    Serial.println("Failed to allocate LVGL buffer in PSRAM");
    return;
  }

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_LCD_BUF_SIZE);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  Serial.println("LVGL + Display initialized.");
}

void lvgl_loop() {
  lv_timer_handler();
}

// ---------------------------------------------------------------------------------
// Section B: Normal "S" driver for reading files from SD
// ---------------------------------------------------------------------------------
typedef struct {
  File file;
} lv_arduino_fs_file_t;

static void * my_open_cb(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
  String fullPath = String("/") + path;
  const char * modeStr = (mode == LV_FS_MODE_WR) ? FILE_WRITE : FILE_READ;
  File f = SD_MMC.open(fullPath, modeStr);
  if(!f) {
    Serial.printf("my_open_cb: failed %s\n", fullPath.c_str());
    return NULL;
  }

  lv_arduino_fs_file_t * fp = new lv_arduino_fs_file_t();
  fp->file = f;
  return fp;
}

static lv_fs_res_t my_close_cb(lv_fs_drv_t * drv, void * file_p) {
  lv_arduino_fs_file_t * fp = (lv_arduino_fs_file_t *)file_p;
  if(!fp) return LV_FS_RES_INV_PARAM;
  fp->file.close();
  delete fp;
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_read_cb(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
  lv_arduino_fs_file_t * fp = (lv_arduino_fs_file_t *)file_p;
  if(!fp) return LV_FS_RES_INV_PARAM;
  *br = fp->file.read((uint8_t*)buf, btr);
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_write_cb(lv_fs_drv_t * drv, void * file_p, const void * buf, uint32_t btw, uint32_t * bw) {
  lv_arduino_fs_file_t * fp = (lv_arduino_fs_file_t *)file_p;
  if(!fp) return LV_FS_RES_INV_PARAM;
  *bw = fp->file.write((const uint8_t *)buf, btw);
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_seek_cb(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
  lv_arduino_fs_file_t * fp = (lv_arduino_fs_file_t *)file_p;
  if(!fp) return LV_FS_RES_INV_PARAM;

  SeekMode m = SeekSet;
  if(whence == LV_FS_SEEK_CUR) m = SeekCur;
  if(whence == LV_FS_SEEK_END) m = SeekEnd;

  fp->file.seek(pos, m);
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_tell_cb(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
  lv_arduino_fs_file_t * fp = (lv_arduino_fs_file_t *)file_p;
  if(!fp) return LV_FS_RES_INV_PARAM;
  *pos_p = fp->file.position();
  return LV_FS_RES_OK;
}

void init_lv_fs() {
  static lv_fs_drv_t fs_drv;
  lv_fs_drv_init(&fs_drv);

  fs_drv.letter = 'S';
  fs_drv.open_cb  = my_open_cb;
  fs_drv.close_cb = my_close_cb;
  fs_drv.read_cb  = my_read_cb;
  fs_drv.write_cb = my_write_cb; // optional
  fs_drv.seek_cb  = my_seek_cb;
  fs_drv.tell_cb  = my_tell_cb;

  lv_fs_drv_register(&fs_drv);
  Serial.println("LVGL FS driver 'S' registered");
}

// ---------------------------------------------------------------------------------
// Section B2: "Memory" driver 'M' - read from global PSRAM buffer
// ---------------------------------------------------------------------------------
typedef struct {
  size_t pos;       // current read position
} mem_file_t;

// The pointer + size we store after loading from SD
static uint8_t *g_gifBuffer = NULL;
static size_t   g_gifSize   = 0;

// Open callback for the memory driver
static void * my_mem_open_cb(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
  mem_file_t * mf = new mem_file_t();
  mf->pos = 0;
  return mf;
}

static lv_fs_res_t my_mem_close_cb(lv_fs_drv_t * drv, void * file_p) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if(!mf) return LV_FS_RES_INV_PARAM;
  delete mf;
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_mem_read_cb(lv_fs_drv_t * drv, void * file_p,
                                  void * buf, uint32_t btr, uint32_t * br) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if(!mf) return LV_FS_RES_INV_PARAM;

  size_t remaining = g_gifSize - mf->pos;
  if(btr > remaining) btr = remaining;

  memcpy(buf, g_gifBuffer + mf->pos, btr);
  mf->pos += btr;
  *br = btr;
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_mem_write_cb(lv_fs_drv_t * drv, void * file_p,
                                   const void * buf, uint32_t btw, uint32_t * bw) {
  // We don't support writing in the memory buffer
  *bw = 0;
  return LV_FS_RES_NOT_IMP;
}

static lv_fs_res_t my_mem_seek_cb(lv_fs_drv_t * drv, void * file_p,
                                  uint32_t pos, lv_fs_whence_t whence) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if(!mf) return LV_FS_RES_INV_PARAM;

  size_t newpos = mf->pos;
  if(whence == LV_FS_SEEK_SET) newpos = pos;
  else if(whence == LV_FS_SEEK_CUR) newpos += pos;
  else if(whence == LV_FS_SEEK_END) newpos = g_gifSize + pos;

  if(newpos > g_gifSize) newpos = g_gifSize;
  mf->pos = newpos;
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_mem_tell_cb(lv_fs_drv_t * drv, void * file_p,
                                  uint32_t * pos_p) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if(!mf) return LV_FS_RES_INV_PARAM;
  *pos_p = mf->pos;
  return LV_FS_RES_OK;
}

void init_mem_fs() {
  static lv_fs_drv_t mem_drv;
  lv_fs_drv_init(&mem_drv);

  mem_drv.letter = 'M'; // "M" for "memory"
  mem_drv.open_cb  = my_mem_open_cb;
  mem_drv.close_cb = my_mem_close_cb;
  mem_drv.read_cb  = my_mem_read_cb;
  mem_drv.write_cb = my_mem_write_cb; // not supported
  mem_drv.seek_cb  = my_mem_seek_cb;
  mem_drv.tell_cb  = my_mem_tell_cb;

  lv_fs_drv_register(&mem_drv);
  Serial.println("LVGL FS driver 'M' registered (for memory-based GIFs)");
}

// ---------------------------------------------------------------------------------
// Section C: Elk-Facing Functions
// ---------------------------------------------------------------------------------

// (C1) Basic print
static jsval_t js_print(struct js *js, jsval_t *args, int nargs) {
  for(int i=0; i<nargs; i++) {
    const char *str = js_str(js, args[i]);
    if(str) Serial.println(str);
    else    Serial.println("print: argument is not a string");
  }
  return js_mknull();
}

// (C2) wifi_connect("ssid","pass")
static jsval_t js_wifi_connect(struct js *js, jsval_t *args, int nargs) {
  if(nargs != 2) return js_mkfalse();
  const char *ssid_with_quotes = js_str(js, args[0]);
  const char *pass_with_quotes = js_str(js, args[1]);
  if(!ssid_with_quotes || !pass_with_quotes) return js_mkfalse();

  // strip quotes
  String ssid(ssid_with_quotes);
  String pass(pass_with_quotes);
  if(ssid.startsWith("\"") && ssid.endsWith("\"")) {
    ssid = ssid.substring(1, ssid.length()-1);
  }
  if(pass.startsWith("\"") && pass.endsWith("\"")) {
    pass = pass.substring(1, pass.length()-1);
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

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected");
    return js_mktrue();
  } else {
    Serial.println("Failed to connect to Wi-Fi");
    return js_mkfalse();
  }
}

// (C3) wifi_status()
static jsval_t js_wifi_status(struct js *js, jsval_t *args, int nargs) {
  return (WiFi.status() == WL_CONNECTED) ? js_mktrue() : js_mkfalse();
}

// (C4) wifi_get_ip()
static jsval_t js_wifi_get_ip(struct js *js, jsval_t *args, int nargs) {
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to Wi-Fi");
    return js_mknull();
  }
  IPAddress ip = WiFi.localIP();
  String ipStr = ip.toString();
  return js_mkstr(js, ipStr.c_str(), ipStr.length());
}

// (C5) delay(ms)
static jsval_t js_delay(struct js *js, jsval_t *args, int nargs) {
  if(nargs != 1) return js_mknull();
  double ms = js_getnum(args[0]);
  delay((unsigned long)ms);
  return js_mknull();
}

// (C6) sd_read_file(path)
static jsval_t js_sd_read_file(struct js *js, jsval_t *args, int nargs) {
  if(nargs != 1) return js_mknull();
  const char *path = js_str(js, args[0]);
  if(!path) return js_mknull();

  File file = SD_MMC.open(path);
  if(!file) {
    Serial.printf("Failed to open file: %s\n", path);
    return js_mknull();
  }
  String content = file.readString();
  file.close();
  return js_mkstr(js, content.c_str(), content.length());
}

// (C7) sd_write_file(path, data)
static jsval_t js_sd_write_file(struct js *js, jsval_t *args, int nargs) {
  if(nargs != 2) return js_mkfalse();
  const char *path = js_str(js, args[0]);
  const char *data = js_str(js, args[1]);
  if(!path || !data) return js_mkfalse();

  File f = SD_MMC.open(path, FILE_WRITE);
  if(!f) {
    Serial.printf("Failed to open for writing: %s\n", path);
    return js_mkfalse();
  }
  f.write((const uint8_t *)data, strlen(data));
  f.close();
  return js_mktrue();
}

// (C8) sd_list_dir(path)
static jsval_t js_sd_list_dir(struct js *js, jsval_t *args, int nargs) {
  if(nargs != 1) return js_mknull();
  const char *path_with_quotes = js_str(js, args[0]);
  if(!path_with_quotes) return js_mknull();

  // strip quotes
  String path(path_with_quotes);
  if(path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length()-1);
  }

  File root = SD_MMC.open(path);
  if(!root) {
    Serial.printf("Failed to open directory: %s\n", path.c_str());
    return js_mknull();
  }
  if(!root.isDirectory()) {
    Serial.println("Not a directory");
    root.close();
    return js_mknull();
  }

  char fileList[512];
  int fileListLen = 0;

  File f = root.openNextFile();
  while(f) {
    const char* type = f.isDirectory() ? "DIR: " : "FILE: ";
    const char* name = f.name();

    int len = snprintf(fileList + fileListLen,
                       sizeof(fileList) - fileListLen,
                       "%s%s\n", type, name);
    if(len < 0 || len >= (int)(sizeof(fileList) - fileListLen)) {
      break; // buffer full or error
    }
    fileListLen += len;
    f = root.openNextFile();
  }
  root.close();

  return js_mkstr(js, fileList, fileListLen);
}

// ---------------------------------------------------------------------------------
// D) Load GIF from SD into g_gifBuffer, then let LVGL read from memory
// ---------------------------------------------------------------------------------

bool load_gif_into_ram(const char * path) {
  File f = SD_MMC.open(path, FILE_READ);
  if(!f) {
    Serial.printf("Failed to open %s\n", path);
    return false;
  }
  size_t fileSize = f.size();
  Serial.printf("File %s is %u bytes\n", path, (unsigned)fileSize);

  // Allocate from PSRAM
  uint8_t *tmp = (uint8_t *)ps_malloc(fileSize);
  if(!tmp) {
    Serial.printf("Failed to allocate %u bytes in PSRAM\n", (unsigned)fileSize);
    f.close();
    return false;
  }

  size_t bytesRead = f.read(tmp, fileSize);
  f.close();
  if(bytesRead < fileSize) {
    Serial.printf("Failed to read full file: only %u of %u\n",
                  (unsigned)bytesRead, (unsigned)fileSize);
    free(tmp);
    return false;
  }

  g_gifBuffer = tmp;
  g_gifSize   = fileSize;
  Serial.println("GIF loaded into PSRAM successfully");
  return true;
}

static jsval_t js_show_gif_from_sd(struct js *js, jsval_t *args, int nargs) {
  if(nargs < 1) {
    Serial.println("show_gif_from_sd: expects 1 argument (path)");
    return js_mknull();
  }
  const char *rawPath = js_str(js, args[0]);
  if(!rawPath) return js_mknull();

  String path = String(rawPath);
  if(path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }

  if(!load_gif_into_ram(path.c_str())) {
    Serial.println("Could not load GIF into RAM");
    return js_mknull();
  }

  lv_obj_t *gif = lv_gif_create(lv_scr_act());
  lv_gif_set_src(gif, "M:mygif"); // memory driver
  lv_obj_align(gif, LV_ALIGN_CENTER, 0, 0);

  Serial.printf("Showing GIF from memory driver (file was %s)\n", path.c_str());
  return js_mknull();
}

// ---------------------------------------------------------------------------------
// *** Drawing + Image
// ---------------------------------------------------------------------------------

/**
 * draw_label(text, x, y)
 * Creates an LVGL label at (x,y)
 */
static jsval_t js_lvgl_draw_label(struct js *js, jsval_t *args, int nargs) {
  if(nargs < 3) {
    Serial.println("draw_label: expects text, x, y");
    return js_mknull();
  }
  const char *labelText = js_str(js, args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  lv_obj_t *label = lv_label_create(lv_scr_act());
  if(labelText) lv_label_set_text(label, labelText);
  lv_obj_set_pos(label, x, y);

  Serial.printf("draw_label: '%s' at (%d,%d)\n", labelText, x, y);
  return js_mknull();
}

/**
 * draw_rect(x, y, w, h)
 */
static jsval_t js_lvgl_draw_rect(struct js *js, jsval_t *args, int nargs) {
  if(nargs < 4) {
    Serial.println("draw_rect: expects x, y, w, h");
    return js_mknull();
  }
  int x = (int)js_getnum(args[0]);
  int y = (int)js_getnum(args[1]);
  int w = (int)js_getnum(args[2]);
  int h = (int)js_getnum(args[3]);

  lv_obj_t *rect = lv_obj_create(lv_scr_act());
  lv_obj_set_size(rect, w, h);
  lv_obj_set_pos(rect, x, y);

  // optional style
  static lv_style_t styleRect;
  lv_style_init(&styleRect);
  lv_style_set_bg_color(&styleRect, lv_color_hex(0x00ff00)); // green
  lv_style_set_radius(&styleRect, 5);
  lv_obj_add_style(rect, &styleRect, 0);

  Serial.printf("draw_rect: at (%d,%d) size (%d,%d)\n", x, y, w, h);
  return js_mknull();
}

/**
 * show_image("/messi.png", x, y)
 * Fix: Strip quotes, so path becomes "S:/messi.png" not "S:"/messi.png"
 */
static jsval_t js_lvgl_show_image(struct js *js, jsval_t *args, int nargs) {
  if(nargs < 3) {
    Serial.println("show_image: expects path, x, y");
    return js_mknull();
  }
  const char *rawPath = js_str(js, args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  if(!rawPath) {
    Serial.println("show_image: invalid path");
    return js_mknull();
  }

  // Convert to Arduino String
  String path = String(rawPath);
  // strip leading/trailing quotes if present
  if(path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }

  // e.g. "/messi.png" => "S:/messi.png"
  String lvglPath = "S:" + path;

  lv_obj_t *img = lv_img_create(lv_scr_act());
  lv_img_set_src(img, lvglPath.c_str());
  lv_obj_set_pos(img, x, y);

  Serial.printf("show_image: '%s' at (%d,%d)\n", lvglPath.c_str(), x, y);
  return js_mknull();
}

// ---------------------------------------------------------------------------------
// Section F: Register all JavaScript functions
// ---------------------------------------------------------------------------------
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

  // For GIF
  js_set(js, global, "show_gif_from_sd", js_mkfun(js_show_gif_from_sd));

  // ***NEW*** shape/drawing + image
  js_set(js, global, "draw_label",  js_mkfun(js_lvgl_draw_label));
  js_set(js, global, "draw_rect",   js_mkfun(js_lvgl_draw_rect));
  js_set(js, global, "show_image",  js_mkfun(js_lvgl_show_image));
}

// ---------------------------------------------------------------------------------
// Section G: Load and Execute a JavaScript script from SD
// ---------------------------------------------------------------------------------
bool load_and_execute_js_script(const char* path) {
  Serial.printf("Loading JavaScript script from: %s\n", path);

  File file = SD_MMC.open(path);
  if(!file) {
    Serial.println("Failed to open JavaScript script file");
    return false;
  }

  String jsScript = file.readString();
  file.close();

  jsval_t res = js_eval(js, jsScript.c_str(), jsScript.length());
  if(js_type(res) == JS_ERR) {
    const char *error = js_str(js, res);
    Serial.printf("Error executing script: %s\n", error);
    return false;
  }
  Serial.println("JavaScript script executed successfully");
  return true;
}

// ---------------------------------------------------------------------------------
// Section H: Arduino setup() + loop()
// ---------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(2000);

  // 1) Mount SD card
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if(!SD_MMC.begin("/sdcard", true, false, 5000000)) {
    Serial.println("Card Mount Failed");
    return;
  }

  // 2) Wi-Fi in station mode
  WiFi.mode(WIFI_STA);

  // 3) Initialize LVGL display
  init_lvgl_display();

  // 4) Register 'S' for normal SD usage
  init_lv_fs();

  // 4b) Register 'M' for memory usage (GIF approach)
  init_mem_fs();

  // 5) Create Elk
  js = js_create(elk_memory, sizeof(elk_memory));
  if(!js) {
    Serial.println("Failed to initialize Elk");
    return;
  }
  register_js_functions();

  // 6) Load & execute "/script.js" from SD
  if(!load_and_execute_js_script("/script.js")) {
    Serial.println("Failed to load and execute JavaScript script");
  }
}

void loop() {
  lvgl_loop();
  delay(5);
}
