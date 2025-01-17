#pragma once

#include <lvgl.h>
#include <HTTPClient.h>

// For BLE
#include <NimBLEDevice.h>

// NimBLE globals
static NimBLEServer*         g_bleServer    = nullptr;
static NimBLECharacteristic* g_bleChar      = nullptr;
static bool                  g_bleConnected = false;

// 4) Elk
extern "C" {
  #include "elk.h"
}

/******************************************************************************
 * A) Elk Memory + Global Instances
 ******************************************************************************/
static uint8_t elk_memory[16 * 1024]; // 16 KB
struct js *js = NULL;               // Global Elk instance
// Adjust as needed
#define MAX_RAM_IMAGES  16

// 4) Memory storage
struct RamImage {
  bool     used;          // Is this slot in use?
  uint8_t *buffer;        // Raw image data allocated from ps_malloc()
  size_t   size;          // Byte size of that buffer
  lv_img_dsc_t dsc;       // The descriptor we pass to lv_img_set_src()
};

static RamImage g_ram_images[MAX_RAM_IMAGES];

void init_ram_images() {
  for (int i = 0; i < MAX_RAM_IMAGES; i++) {
    g_ram_images[i].used   = false;
    g_ram_images[i].buffer = NULL;
    g_ram_images[i].size   = 0;
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
    Serial.printf("my_open_cb: failed to open %s\n", fullPath.c_str());
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
  *br = fp->file.read((uint8_t*)buf, btr);
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
  fs_drv.open_cb  = my_open_cb;
  fs_drv.close_cb = my_close_cb;
  fs_drv.read_cb  = my_read_cb;
  fs_drv.write_cb = my_write_cb;  
  fs_drv.seek_cb  = my_seek_cb;
  fs_drv.tell_cb  = my_tell_cb;

  lv_fs_drv_register(&fs_drv);
  Serial.println("LVGL FS driver 'S' registered");
}

/******************************************************************************
 * D) "M" Memory Driver (for GIF usage)
 ******************************************************************************/
typedef struct {
  size_t pos;
} mem_file_t;

static uint8_t *g_gifBuffer = NULL;
static size_t   g_gifSize   = 0;

static void *my_mem_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode) {
  mem_file_t *mf = new mem_file_t();
  mf->pos = 0;
  return mf;
}

static lv_fs_res_t my_mem_close_cb(lv_fs_drv_t *drv, void *file_p) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if(!mf) return LV_FS_RES_INV_PARAM;
  delete mf;
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_mem_read_cb(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if(!mf) return LV_FS_RES_INV_PARAM;

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
  if(!mf) return LV_FS_RES_INV_PARAM;

  size_t newpos = mf->pos;
  if(whence == LV_FS_SEEK_SET) newpos = pos;
  else if(whence == LV_FS_SEEK_CUR) newpos += pos;
  else if(whence == LV_FS_SEEK_END) newpos = g_gifSize + pos;

  if(newpos > g_gifSize) newpos = g_gifSize;
  mf->pos = newpos;
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_mem_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if(!mf) return LV_FS_RES_INV_PARAM;
  *pos_p = mf->pos;
  return LV_FS_RES_OK;
}

void init_mem_fs() {
  static lv_fs_drv_t mem_drv;
  lv_fs_drv_init(&mem_drv);

  mem_drv.letter = 'M';
  mem_drv.open_cb  = my_mem_open_cb;
  mem_drv.close_cb = my_mem_close_cb;
  mem_drv.read_cb  = my_mem_read_cb;
  mem_drv.write_cb = my_mem_write_cb;
  mem_drv.seek_cb  = my_mem_seek_cb;
  mem_drv.tell_cb  = my_mem_tell_cb;

  lv_fs_drv_register(&mem_drv);
  Serial.println("LVGL FS driver 'M' registered (for memory-based GIFs)");
}

/******************************************************************************
 * B) LVGL + Display
 ******************************************************************************/
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf = NULL;

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  // Calculate width/height from the area
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  // Push the rendered data to the display
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);

  // Tell LVGL flush is done
  lv_disp_flush_ready(disp);
}

void init_lvgl_display() {
  Serial.println("Initializing display...");

  // Turn on backlight / screen power
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  // Init the AMOLED driver & set rotation
  rm67162_init();
  lcd_setRotation(1);

  // Init LVGL
  lv_init();

  // Allocate LVGL draw buffer in PSRAM
  buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE);
  if (!buf) {
    Serial.println("Failed to allocate LVGL buffer in PSRAM");
    return;
  }

  // Initialize LVGL draw buffer
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_LCD_BUF_SIZE);

  // Register the display driver
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
  // Call LVGL's timer handler
  lv_timer_handler();
}

/******************************************************************************
 * E) Elk-Facing Functions (print, Wi-Fi, SD ops, etc.)
 ******************************************************************************/
static jsval_t js_print(struct js *js, jsval_t *args, int nargs) {
  for (int i = 0; i < nargs; i++) {
    const char *str = js_str(js, args[i]);
    if (str) Serial.println(str);
    else     Serial.println("print: argument is not a string");
  }
  return js_mknull();
}

// Wi-Fi connect
static jsval_t js_wifi_connect(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) return js_mkfalse();
  const char* ssidQ  = js_str(js, args[0]);
  const char* passQ  = js_str(js, args[1]);
  if (!ssidQ || !passQ) return js_mkfalse();

  // Strip quotes if any
  String ssid(ssidQ);
  String pass(passQ);
  if (ssid.startsWith("\"") && ssid.endsWith("\"")) {
    ssid = ssid.substring(1, ssid.length()-1);
  }
  if (pass.startsWith("\"") && pass.endsWith("\"")) {
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

// Delay in JS: "delay(ms)"
static jsval_t js_delay(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  double ms = js_getnum(args[0]);
  delay((unsigned long)ms);
  return js_mknull();
}

// sd_read_file(path)
static jsval_t js_sd_read_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  const char* path = js_str(js, args[0]);
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

// sd_write_file(path, data)
static jsval_t js_sd_write_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) return js_mkfalse();
  const char *path = js_str(js, args[0]);
  const char *data = js_str(js, args[1]);
  if(!path || !data) return js_mkfalse();

  File f = SD_MMC.open(path, FILE_WRITE);
  if(!f) {
    Serial.printf("Failed to open for writing: %s\n", path);
    return js_mkfalse();
  }
  f.write((const uint8_t*)data, strlen(data));
  f.close();
  return js_mktrue();
}

// sd_list_dir(path)
static jsval_t js_sd_list_dir(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  const char* pathQ = js_str(js, args[0]);
  if (!pathQ) return js_mknull();

  // Strip quotes
  String path(pathQ);
  if (path.startsWith("\"") && path.endsWith("\"")) {
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

  // Collect listing
  char fileList[512];
  int fileListLen = 0;

  File f = root.openNextFile();
  while(f) {
    const char* type = f.isDirectory() ? "DIR: " : "FILE: ";
    const char* name = f.name();
    int len = snprintf(fileList + fileListLen, sizeof(fileList)-fileListLen,
                       "%s%s\n", type, name);
    if (len<0 || len>=(int)(sizeof(fileList)-fileListLen)) break;
    fileListLen += len;
    f = root.openNextFile();
  }
  root.close();
  return js_mkstr(js, fileList, fileListLen);
}

/******************************************************************************
 * F) Load GIF from SD => g_gifBuffer => "M:mygif"
 ******************************************************************************/
bool load_gif_into_ram(const char *path) {
  File f = SD_MMC.open(path, FILE_READ);
  if(!f) {
    Serial.printf("Failed to open %s\n", path);
    return false;
  }
  size_t fileSize = f.size();
  Serial.printf("File %s is %u bytes\n", path, (unsigned)fileSize);

  uint8_t* tmp = (uint8_t*)ps_malloc(fileSize);
  if(!tmp) {
    Serial.printf("Failed to allocate %u bytes in PSRAM\n",(unsigned)fileSize);
    f.close();
    return false;
  }
  size_t bytesRead = f.read(tmp, fileSize);
  f.close();
  if(bytesRead < fileSize) {
    Serial.printf("Failed to read full file: only %u of %u\n",
                  (unsigned)bytesRead,(unsigned)fileSize);
    free(tmp);
    return false;
  }
  g_gifBuffer = tmp;
  g_gifSize   = fileSize;
  Serial.println("GIF loaded into PSRAM successfully");
  return true;
}

static jsval_t js_show_gif_from_sd(struct js *js, jsval_t *args, int nargs) {
  if(nargs<1) {
    Serial.println("show_gif_from_sd: expects path");
    return js_mknull();
  }
  const char* rawPath = js_str(js, args[0]);
  if(!rawPath) return js_mknull();

  // Strip quotes
  String path(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length()-1);
  }

  if(!load_gif_into_ram(path.c_str())) {
    Serial.println("Could not load GIF into RAM");
    return js_mknull();
  }

  lv_obj_t *gif = lv_gif_create(lv_scr_act());
  lv_gif_set_src(gif, "M:mygif");   // memory-based
  lv_obj_align(gif, LV_ALIGN_CENTER, 0, 0);

  Serial.printf("Showing GIF from memory driver (file was %s)\n", path.c_str());
  return js_mknull();
}


/******************************************************************************
 * J) Load + Execute JS from SD
 ******************************************************************************/
bool load_image_file_into_ram(const char *path, RamImage *outImg) {
  // 1) Open file
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    Serial.printf("Failed to open %s\n", path);
    return false;
  }
  size_t fileSize = f.size();
  Serial.printf("File %s is %u bytes\n", path, (unsigned)fileSize);

  // 2) Allocate PSRAM
  uint8_t *buf = (uint8_t *)ps_malloc(fileSize);
  if (!buf) {
    Serial.printf("Failed to allocate %u bytes in PSRAM\n", (unsigned)fileSize);
    f.close();
    return false;
  }

  // 3) Read all data
  size_t bytesRead = f.read(buf, fileSize);
  f.close();
  if (bytesRead < fileSize) {
    Serial.printf("Failed to read full file: only %u of %u\n",
                  (unsigned)bytesRead, (unsigned)fileSize);
    free(buf);
    return false;
  }

  // 4) Fill out the RamImage struct
  outImg->used   = true;
  outImg->buffer = buf;
  outImg->size   = fileSize;

  // 5) Fill out the lv_img_dsc_t with minimal info
  //    - If it's a "raw" or "true color" format, you can do:
  lv_img_dsc_t *d = &outImg->dsc;
  memset(d, 0, sizeof(*d));

  // Basic mandatory fields:
  d->data_size       = fileSize;
  d->data            = buf;
  d->header.always_zero = 0;
  d->header.w       = 200;
  d->header.h       = 200;
  d->header.cf      = LV_IMG_CF_TRUE_COLOR; 
  // or LV_IMG_CF_RAW if using a custom decoder

  // If you can't know width/height from file alone, you may just guess or parse
  // For a PNG/JPG you'd typically use an external decoder to fill w,h

  Serial.println("Image loaded into PSRAM successfully");
  return true;
}

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

/******************************************************************************
 * G) Basic draw_label, draw_rect, show_image from SD
 ******************************************************************************/
static jsval_t js_lvgl_draw_label(struct js *js, jsval_t *args, int nargs) {
  if(nargs<3) {
    Serial.println("draw_label: expects text, x, y");
    return js_mknull();
  }
  const char* labelText = js_str(js, args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  lv_obj_t *label = lv_label_create(lv_scr_act());
  if(labelText) lv_label_set_text(label, labelText);
  lv_obj_set_pos(label, x, y);

  Serial.printf("draw_label: '%s' at (%d,%d)\n", labelText, x, y);
  return js_mknull();
}

static jsval_t js_lvgl_draw_rect(struct js *js, jsval_t *args, int nargs) {
  if(nargs<4) {
    Serial.println("draw_rect: expects x,y,w,h");
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

  Serial.printf("draw_rect: at (%d,%d), size(%d,%d)\n", x, y, w, h);
  return js_mknull();
}

static jsval_t js_lvgl_show_image(struct js *js, jsval_t *args, int nargs) {
  if(nargs<3) {
    Serial.println("show_image: expects path,x,y");
    return js_mknull();
  }
  const char* rawPath = js_str(js, args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  if(!rawPath) {
    Serial.println("show_image: invalid path");
    return js_mknull();
  }
  // Build "S:/filename"
  String path(rawPath);
  if(path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length()-1);
  }
  String lvglPath = "S:" + path;

  lv_obj_t *img = lv_img_create(lv_scr_act());
  lv_img_set_src(img, lvglPath.c_str());
  lv_obj_set_pos(img, x, y);

  Serial.printf("show_image: '%s' at (%d,%d)\n", lvglPath.c_str(), x, y);
  return js_mknull();
}

/******************************************************************************
 * G2) create_image, rotate_obj, move_obj, animate_obj (Object Handle Approach)
 ******************************************************************************/
static const int MAX_OBJECTS = 16;
static lv_obj_t *g_lv_obj_map[MAX_OBJECTS] = { nullptr };

static int store_lv_obj(lv_obj_t *obj) {
  for(int i=0; i<MAX_OBJECTS; i++) {
    if(!g_lv_obj_map[i]) {
      g_lv_obj_map[i] = obj;
      return i;
    }
  }
  return -1; // No free slot
}
static lv_obj_t* get_lv_obj(int handle) {
  if(handle<0 || handle>=MAX_OBJECTS) return nullptr;
  return g_lv_obj_map[handle];
}

// Helper functions to extract RGB components from lv_color_t
uint8_t get_red(lv_color_t color) {
    return (color.full >> 11) & 0x1F; // 5 bits
}

uint8_t get_green(lv_color_t color) {
    return (color.full >> 5) & 0x3F; // 6 bits
}

uint8_t get_blue(lv_color_t color) {
    return color.full & 0x1F; // 5 bits
}

// create_image("/messi.png", x,y) => returns handle
static jsval_t js_create_image(struct js *js, jsval_t *args, int nargs) {
  if(nargs<3) {
    Serial.println("create_image: expects path,x,y");
    return js_mknum(-1);
  }
  const char* rawPath = js_str(js, args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);
  if(!rawPath) return js_mknum(-1);

  String path(rawPath);
  if(path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length()-1);
  }
  String fullPath = "S:" + path;

  lv_obj_t *img = lv_img_create(lv_scr_act());
  lv_img_set_src(img, fullPath.c_str());
  lv_obj_set_pos(img, x, y);

  int handle = store_lv_obj(img);
  Serial.printf("create_image: '%s' => handle %d\n", fullPath.c_str(), handle);
  return js_mknum(handle);
}

// create_image_from_ram("/somefile.bin", x, y)
static jsval_t js_create_image_from_ram(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) {
    Serial.println("create_image_from_ram: expects path, x, y");
    return js_mknum(-1);
  }

  // 1) Parse arguments
  const char *rawPath = js_str(js, args[0]);
  int x = (int) js_getnum(args[1]);
  int y = (int) js_getnum(args[2]);
  if (!rawPath) return js_mknum(-1);

  // 2) Find a free RamImage slot
  int slot = -1;
  for (int i = 0; i < MAX_RAM_IMAGES; i++) {
    if (!g_ram_images[i].used) { slot = i; break; }
  }
  if (slot < 0) {
    Serial.println("No free RamImage slots!");
    return js_mknum(-1);
  }
  RamImage *ri = &g_ram_images[slot];

  // 3) Strip quotes
  String path = String(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }

  // 4) Actually load the file into the RamImage
  if (!load_image_file_into_ram(path.c_str(), ri)) {
    Serial.println("Could not load image into RAM");
    return js_mknum(-1);
  }

  // 5) Create the LVGL object
  lv_obj_t *img = lv_img_create(lv_scr_act());
  // 6) lv_img_set_src with the in-RAM descriptor
  lv_img_set_src(img, &ri->dsc);  // <--- the magic

  // 7) Set position
  lv_obj_set_pos(img, x, y);

  // 8) Store it in our handle-based system
  int handle = store_lv_obj(img);
  Serial.printf("create_image_from_ram: '%s' => ram slot=%d => handle %d\n",
                path.c_str(), slot, handle);
  return js_mknum(handle);
}

// rotate_obj(handle, angle)
static jsval_t js_rotate_obj(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) {
    Serial.println("rotate_obj: expects handle, angle");
    return js_mknull();
  }
  int handle = (int)js_getnum(args[0]);
  int angle  = (int)js_getnum(args[1]); // 0..3600 => 0..360 deg

  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) {
    Serial.println("rotate_obj: invalid handle");
    return js_mknull();
  }
  // For lv_img in LVGL => set angle
  lv_img_set_angle(obj, angle);
  Serial.printf("rotate_obj: handle=%d angle=%d\n", handle, angle);
  return js_mknull();
}

// move_obj(handle, x, y)
static jsval_t js_move_obj(struct js *js, jsval_t *args, int nargs) {
  if(nargs<3) {
    Serial.println("move_obj: expects handle,x,y");
    return js_mknull();
  }
  int handle = (int)js_getnum(args[0]);
  int x      = (int)js_getnum(args[1]);
  int y      = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) {
    Serial.println("move_obj: invalid handle");
    return js_mknull();
  }
  lv_obj_set_pos(obj, x, y);
  Serial.printf("move_obj: handle=%d => pos(%d,%d)\n", handle, x, y);
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
  if(nargs<5) {
    Serial.println("animate_obj: expects handle,x0,y0,x1,y1,[duration]");
    return js_mknull();
  }
  int handle   = (int)js_getnum(args[0]);
  int x0       = (int)js_getnum(args[1]);
  int y0       = (int)js_getnum(args[2]);
  int x1       = (int)js_getnum(args[3]);
  int y1       = (int)js_getnum(args[4]);
  int duration = (nargs>=6)? (int)js_getnum(args[5]) : 1000;

  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) {
    Serial.println("animate_obj: invalid handle");
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

  Serial.printf("animate_obj: handle=%d from(%d,%d) to(%d,%d), dur=%d\n",
                handle, x0,y0, x1,y1, duration);
  return js_mknull();
}

/******************************************************************************
 * H) Style Handles + Full Style Setters
 ******************************************************************************/
static const int MAX_STYLES = 32;
static lv_style_t *g_style_map[MAX_STYLES] = { nullptr };

static lv_style_t* get_lv_style(int handle) {
  if(handle<0 || handle>=MAX_STYLES) return nullptr;
  return g_style_map[handle];
}

// create_style()
static jsval_t js_create_style(struct js *js, jsval_t *args, int nargs) {
  for(int i=0; i<MAX_STYLES; i++) {
    if(!g_style_map[i]) {
      lv_style_t *st = new lv_style_t;
      lv_style_init(st);
      g_style_map[i] = st;
      Serial.printf("create_style => handle %d\n", i);
      return js_mknum(i);
    }
  }
  Serial.println("create_style => no free style slots");
  return js_mknum(-1);
}

// obj_add_style(objHandle, styleHandle, partOrState)
static jsval_t js_obj_add_style(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int objHandle   = (int)js_getnum(args[0]);
  int styleHandle = (int)js_getnum(args[1]);
  int partState   = 0;
  if(nargs>=3) partState = (int)js_getnum(args[2]);

  lv_obj_t* obj    = get_lv_obj(objHandle);
  lv_style_t* st   = get_lv_style(styleHandle);
  if(!obj || !st) {
    Serial.println("obj_add_style => invalid handle");
    return js_mknull();
  }
  lv_obj_add_style(obj, st, partState);
  return js_mknull();
}

// ***Full style property setters***
static jsval_t js_style_set_radius(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int radius = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_radius(st, (lv_coord_t)radius);
  return js_mknull();
}

static jsval_t js_style_set_bg_opa(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int opaVal = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_bg_opa(st, (lv_opa_t)opaVal);
  return js_mknull();
}

static jsval_t js_style_set_bg_color(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH   = (int)js_getnum(args[0]);
  double color = js_getnum(args[1]); // numeric hex
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_bg_color(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_border_color(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH    = (int)js_getnum(args[0]);
  double color  = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_border_color(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_border_width(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int bw     = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_border_width(st, bw);
  return js_mknull();
}

static jsval_t js_style_set_border_opa(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int opa    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_border_opa(st, (lv_opa_t)opa);
  return js_mknull();
}

static jsval_t js_style_set_border_side(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int side   = (int)js_getnum(args[1]); // e.g. LV_BORDER_SIDE_BOTTOM|LV_BORDER_SIDE_RIGHT
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_border_side(st, side);
  return js_mknull();
}

// Outline
static jsval_t js_style_set_outline_width(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int w      = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_outline_width(st, w);
  return js_mknull();
}

static jsval_t js_style_set_outline_color(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH  = (int)js_getnum(args[0]);
  double col  = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_outline_color(st, lv_color_hex((uint32_t)col));
  return js_mknull();
}

static jsval_t js_style_set_outline_pad(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_outline_pad(st, pad);
  return js_mknull();
}

// Shadow
static jsval_t js_style_set_shadow_width(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int w      = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_shadow_width(st, w);
  return js_mknull();
}

static jsval_t js_style_set_shadow_color(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH   = (int)js_getnum(args[0]);
  double color = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_shadow_color(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_shadow_ofs_x(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int ofs    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_shadow_ofs_x(st, ofs);
  return js_mknull();
}

static jsval_t js_style_set_shadow_ofs_y(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int ofs    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_shadow_ofs_y(st, ofs);
  return js_mknull();
}

// Image recolor, transform
static jsval_t js_style_set_img_recolor(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH   = (int)js_getnum(args[0]);
  double color = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_img_recolor(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_img_recolor_opa(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int opa    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_img_recolor_opa(st, (lv_opa_t)opa);
  return js_mknull();
}

static jsval_t js_style_set_transform_angle(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int angle  = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_transform_angle(st, (lv_coord_t)angle);
  return js_mknull();
}

// Text
static jsval_t js_style_set_text_color(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH   = (int)js_getnum(args[0]);
  double color = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_text_color(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_text_letter_space(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int space  = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_text_letter_space(st, space);
  return js_mknull();
}

static jsval_t js_style_set_text_line_space(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int space  = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_text_line_space(st, space);
  return js_mknull();
}

static jsval_t js_style_set_text_decor(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int decor  = (int)js_getnum(args[1]); // e.g. LV_TEXT_DECOR_UNDERLINE
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_text_decor(st, decor);
  return js_mknull();
}

// Line
static jsval_t js_style_set_line_color(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH   = (int)js_getnum(args[0]);
  double color = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_line_color(st, lv_color_hex((uint32_t)color));
  return js_mknull();
}

static jsval_t js_style_set_line_width(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int w      = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_line_width(st, w);
  return js_mknull();
}

static jsval_t js_style_set_line_rounded(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH  = (int)js_getnum(args[0]);
  bool round  = (bool)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_line_rounded(st, round);
  return js_mknull();
}

// Padding
static jsval_t js_style_set_pad_all(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_pad_all(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_left(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_pad_left(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_right(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_pad_right(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_top(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_pad_top(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_bottom(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_pad_bottom(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_ver(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_pad_ver(st, pad);
  return js_mknull();
}

static jsval_t js_style_set_pad_hor(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int pad    = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_pad_hor(st, pad);
  return js_mknull();
}

// Some dimension-related style props
static jsval_t js_style_set_width(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int w      = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_width(st, (lv_coord_t)w);
  return js_mknull();
}

static jsval_t js_style_set_height(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int h      = (int)js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_height(st, (lv_coord_t)h);
  return js_mknull();
}

static jsval_t js_style_set_x(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH  = (int)js_getnum(args[0]);
  double val  = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_x(st, (lv_coord_t)val);
  return js_mknull();
}

static jsval_t js_style_set_y(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int styleH  = (int)js_getnum(args[0]);
  double val  = js_getnum(args[1]);
  lv_style_t *st = get_lv_style(styleH);
  if(!st) return js_mknull();
  lv_style_set_y(st, (lv_coord_t)val);
  return js_mknull();
}

/******************************************************************************
 * H2) Additional object property functions
 ******************************************************************************/
static jsval_t js_obj_set_size(struct js *js, jsval_t *args, int nargs) {
  if(nargs<3) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int w      = (int)js_getnum(args[1]);
  int h      = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) {
    Serial.printf("obj_set_size => invalid handle %d\n", handle);
    return js_mknull();
  }
  lv_obj_set_size(obj, w, h);
  return js_mknull();
}

// obj_align(objHandle, alignConst, xOfs, yOfs)
static jsval_t js_obj_align(struct js *js, jsval_t *args, int nargs) {
  if(nargs<4) return js_mknull();
  int handle   = (int)js_getnum(args[0]);
  int alignVal = (int)js_getnum(args[1]);
  int xOfs     = (int)js_getnum(args[2]);
  int yOfs     = (int)js_getnum(args[3]);

  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) {
    Serial.printf("obj_align => invalid handle %d\n", handle);
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
  if(nargs<2) return js_mknull();
  int handle     = (int)js_getnum(args[0]);
  int snap_mode  = (int)js_getnum(args[1]); // numeric for LV_SCROLL_SNAP_x
  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) return js_mknull();
  lv_obj_set_scroll_snap_x(obj, (lv_scroll_snap_t)snap_mode);
  return js_mknull();
}

static jsval_t js_obj_set_scroll_snap_y(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int handle     = (int)js_getnum(args[0]);
  int snap_mode  = (int)js_getnum(args[1]);
  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) return js_mknull();
  lv_obj_set_scroll_snap_y(obj, (lv_scroll_snap_t)snap_mode);
  return js_mknull();
}

static jsval_t js_obj_add_flag(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int flag   = (int)js_getnum(args[1]);
  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) return js_mknull();
  lv_obj_add_flag(obj, (lv_obj_flag_t)flag);
  return js_mknull();
}

static jsval_t js_obj_clear_flag(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int flag   = (int)js_getnum(args[1]);
  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) return js_mknull();
  lv_obj_clear_flag(obj, (lv_obj_flag_t)flag);
  return js_mknull();
}

static jsval_t js_obj_set_scroll_dir(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int dir    = (int)js_getnum(args[1]); // e.g. LV_DIR_VER or ...
  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) return js_mknull();
  lv_obj_set_scroll_dir(obj, (lv_dir_t)dir);
  return js_mknull();
}

static jsval_t js_obj_set_scrollbar_mode(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int mode   = (int)js_getnum(args[1]); // e.g. LV_SCROLLBAR_MODE_OFF
  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) return js_mknull();
  lv_obj_set_scrollbar_mode(obj, (lv_scrollbar_mode_t)mode);
  return js_mknull();
}

static jsval_t js_obj_set_flex_flow(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mknull();
  int handle   = (int)js_getnum(args[0]);
  int flowEnum = (int)js_getnum(args[1]); // e.g. LV_FLEX_FLOW_ROW_WRAP
  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) return js_mknull();
  lv_obj_set_flex_flow(obj, (lv_flex_flow_t)flowEnum);
  return js_mknull();
}

static jsval_t js_obj_set_flex_align(struct js *js, jsval_t *args, int nargs) {
  if(nargs<4) return js_mknull();
  int handle     = (int)js_getnum(args[0]);
  int main_place = (int)js_getnum(args[1]);
  int cross_place= (int)js_getnum(args[2]);
  int track_place= (int)js_getnum(args[3]);
  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) return js_mknull();
  lv_obj_set_flex_align(obj, (lv_flex_align_t)main_place,
                             (lv_flex_align_t)cross_place,
                             (lv_flex_align_t)track_place);
  return js_mknull();
}

static jsval_t js_obj_set_style_clip_corner(struct js *js, jsval_t *args, int nargs) {
  if(nargs<3) return js_mknull();
  int handle  = (int)js_getnum(args[0]);
  bool en     = (bool)js_getnum(args[1]);
  int part    = (int)js_getnum(args[2]);
  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) return js_mknull();
  lv_obj_set_style_clip_corner(obj, en, part);
  return js_mknull();
}

static jsval_t js_obj_set_style_base_dir(struct js *js, jsval_t *args, int nargs) {
  if(nargs<3) return js_mknull();
  int handle   = (int)js_getnum(args[0]);
  int base_dir = (int)js_getnum(args[1]); // e.g. LV_BASE_DIR_RTL
  int part     = (int)js_getnum(args[2]);
  lv_obj_t *obj = get_lv_obj(handle);
  if(!obj) return js_mknull();
  lv_obj_set_style_base_dir(obj, (lv_base_dir_t)base_dir, part);
  return js_mknull();
}

/*******************************************************
 * CHART BRIDGING
 *******************************************************/

static jsval_t js_lv_chart_create(struct js *js, jsval_t *args, int nargs) {
    // Creates a chart object on the current screen
    lv_obj_t *chart = lv_chart_create(lv_scr_act());
    // Optionally set default size or alignment
    lv_obj_set_size(chart, 200, 150);
    lv_obj_center(chart);

    // Store in your handle-based system
    int handle = store_lv_obj(chart);
    Serial.printf("lv_chart_create => handle %d\n", handle);

    // Return handle to JS
    return js_mknum(handle);
}

static jsval_t js_lv_chart_set_type(struct js *js, jsval_t *args, int nargs) {
    // (handle, lv_chart_type int)
    if(nargs < 2) return js_mknull();
    int h = (int)js_getnum(args[0]);
    int t = (int)js_getnum(args[1]); // e.g. LV_CHART_TYPE_LINE, LV_CHART_TYPE_BAR, etc.

    lv_obj_t *obj = get_lv_obj(h);
    if(!obj) return js_mknull();

    lv_chart_set_type(obj, (lv_chart_type_t)t);
    return js_mknull();
}

static jsval_t js_lv_chart_set_div_line_count(struct js *js, jsval_t *args, int nargs) {
    // (handle, y_div, x_div)
    if(nargs < 3) return js_mknull();
    int h = (int)js_getnum(args[0]);
    int y_div = (int)js_getnum(args[1]);
    int x_div = (int)js_getnum(args[2]);

    lv_obj_t *obj = get_lv_obj(h);
    if(!obj) return js_mknull();

    lv_chart_set_div_line_count(obj, y_div, x_div);
    return js_mknull();
}

static jsval_t js_lv_chart_set_update_mode(struct js *js, jsval_t *args, int nargs) {
    // (handle, mode)
    // e.g. mode = LV_CHART_UPDATE_MODE_SHIFT, LV_CHART_UPDATE_MODE_CIRCULAR
    if(nargs < 2) return js_mknull();
    int h    = (int)js_getnum(args[0]);
    int mode = (int)js_getnum(args[1]);

    lv_obj_t *obj = get_lv_obj(h);
    if(!obj) return js_mknull();

    lv_chart_set_update_mode(obj, (lv_chart_update_mode_t)mode);
    return js_mknull();
}

static jsval_t js_lv_chart_set_range(struct js *js, jsval_t *args, int nargs) {
    // (handle, axis, min, max)
    // e.g. axis=LV_CHART_AXIS_PRIMARY_Y, min=0, max=100
    if(nargs < 4) return js_mknull();
    int h    = (int)js_getnum(args[0]);
    int axis = (int)js_getnum(args[1]);
    int mn   = (int)js_getnum(args[2]);
    int mx   = (int)js_getnum(args[3]);

    lv_obj_t *obj = get_lv_obj(h);
    if(!obj) return js_mknull();

    lv_chart_set_range(obj, (lv_chart_axis_t)axis, mn, mx);
    return js_mknull();
}

static jsval_t js_lv_chart_set_point_count(struct js *js, jsval_t *args, int nargs) {
    // (handle, count)
    if(nargs < 2) return js_mknull();
    int h = (int)js_getnum(args[0]);
    int c = (int)js_getnum(args[1]);

    lv_obj_t *obj = get_lv_obj(h);
    if(!obj) return js_mknull();

    lv_chart_set_point_count(obj, c);
    return js_mknull();
}

static jsval_t js_lv_chart_refresh(struct js *js, jsval_t *args, int nargs) {
    // (handle)
    if(nargs < 1) return js_mknull();
    int h = (int)js_getnum(args[0]);

    lv_obj_t *obj = get_lv_obj(h);
    if(!obj) return js_mknull();

    lv_chart_refresh(obj);
    return js_mknull();
}

static jsval_t js_lv_chart_add_series(struct js *js, jsval_t *args, int nargs) {
    // (handle, color, axis)
    if(nargs < 3) return js_mknull();
    int h     = (int)js_getnum(args[0]);
    double col= js_getnum(args[1]);
    int axis  = (int)js_getnum(args[2]);

    lv_obj_t *obj = get_lv_obj(h);
    if(!obj) return js_mknull();

    lv_chart_series_t *ser = lv_chart_add_series(obj, lv_color_hex((uint32_t)col), (lv_chart_axis_t)axis);
    // Return the pointer as a number (NOT safe on 64-bit, but workable for small usage)
    // Alternatively, you can store it in a separate map if you want handle-based approach for series.
    intptr_t p = (intptr_t)(ser);
    return js_mknum((double)p);
}

static jsval_t js_lv_chart_set_next_value(struct js *js, jsval_t *args, int nargs) {
    // (chartHandle, seriesPtr, value)
    if(nargs < 3) return js_mknull();
    int h = (int)js_getnum(args[0]);
    intptr_t sp = (intptr_t)js_getnum(args[1]);
    int val = (int)js_getnum(args[2]);

    lv_obj_t *chart = get_lv_obj(h);
    if(!chart) return js_mknull();

    lv_chart_series_t *ser = (lv_chart_series_t *)sp;
    lv_chart_set_next_value(chart, ser, val);
    return js_mknull();
}

static jsval_t js_lv_chart_set_next_value2(struct js *js, jsval_t *args, int nargs) {
    // (chartHandle, seriesPtr, xVal, yVal)
    if(nargs < 4) return js_mknull();
    int h = (int)js_getnum(args[0]);
    intptr_t sp = (intptr_t)js_getnum(args[1]);
    int xval = (int)js_getnum(args[2]);
    int yval = (int)js_getnum(args[3]);

    lv_obj_t *chart = get_lv_obj(h);
    if(!chart) return js_mknull();

    lv_chart_series_t *ser = (lv_chart_series_t *)sp;
    lv_chart_set_next_value2(chart, ser, xval, yval);
    return js_mknull();
}

static jsval_t js_lv_chart_set_axis_tick(struct js *js, jsval_t *args, int nargs) {
    // (chartH, axis, majorLen, minorLen, majorCnt, minorCnt, label_en, draw_size)
    if(nargs < 8) return js_mknull();
    int h       = (int)js_getnum(args[0]);
    int axis    = (int)js_getnum(args[1]);
    int majorLen= (int)js_getnum(args[2]);
    int minorLen= (int)js_getnum(args[3]);
    int majorCnt= (int)js_getnum(args[4]);
    int minorCnt= (int)js_getnum(args[5]);
    bool label  = (bool)js_getnum(args[6]);
    int drawSiz = (int)js_getnum(args[7]);

    lv_obj_t *chart = get_lv_obj(h);
    if(!chart) return js_mknull();

    lv_chart_set_axis_tick(chart, (lv_chart_axis_t)axis, majorLen, minorLen,
                           majorCnt, minorCnt, label, drawSiz);
    return js_mknull();
}

static jsval_t js_lv_chart_set_zoom_x(struct js *js, jsval_t *args, int nargs) {
    // (chartH, zoom)
    if(nargs < 2) return js_mknull();
    int h   = (int)js_getnum(args[0]);
    int zm  = (int)js_getnum(args[1]);
    lv_obj_t *chart = get_lv_obj(h);
    if(!chart) return js_mknull();

    lv_chart_set_zoom_x(chart, zm);
    return js_mknull();
}

static jsval_t js_lv_chart_set_zoom_y(struct js *js, jsval_t *args, int nargs) {
    // (chartH, zoom)
    if(nargs < 2) return js_mknull();
    int h   = (int)js_getnum(args[0]);
    int zm  = (int)js_getnum(args[1]);
    lv_obj_t *chart = get_lv_obj(h);
    if(!chart) return js_mknull();

    lv_chart_set_zoom_y(chart, zm);
    return js_mknull();
}

static jsval_t js_lv_chart_get_y_array(struct js *js, jsval_t *args, int nargs) {
    // (chartH, seriesPtr) -> returns a pointer number to the array
    if(nargs < 2) return js_mknull();
    int h      = (int)js_getnum(args[0]);
    intptr_t sp= (intptr_t)js_getnum(args[1]);

    lv_obj_t *chart = get_lv_obj(h);
    if(!chart) return js_mknull();

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

static jsval_t js_lv_meter_create(struct js *js, jsval_t *args, int nargs) {
    // no params
    lv_obj_t *m = lv_meter_create(lv_scr_act());
    int handle = store_lv_obj(m);
    return js_mknum(handle);
}

static jsval_t js_lv_meter_add_scale(struct js *js, jsval_t *args, int nargs) {
    // (meterHandle) -> scale pointer as number
    if(nargs<1) return js_mknull();
    int mh = (int)js_getnum(args[0]);
    lv_obj_t *mt = get_lv_obj(mh);
    if(!mt) return js_mknull();

    lv_meter_scale_t *sc = lv_meter_add_scale(mt);
    // Return pointer as numeric
    intptr_t p = (intptr_t)sc;
    return js_mknum((double)p);
}

static jsval_t js_lv_meter_set_scale_ticks(struct js *js, jsval_t *args, int nargs) {
    // (meterH, scalePtr, cnt, width, length, color)
    if(nargs<6) return js_mknull();
    int mH     = (int)js_getnum(args[0]);
    intptr_t scP = (intptr_t)js_getnum(args[1]);
    int cnt    = (int)js_getnum(args[2]);
    int width  = (int)js_getnum(args[3]);
    int length = (int)js_getnum(args[4]);
    double col = js_getnum(args[5]);

    lv_obj_t *mt = get_lv_obj(mH);
    if(!mt) return js_mknull();
    lv_meter_scale_t *sc = (lv_meter_scale_t*) scP;
    lv_meter_set_scale_ticks(mt, sc, cnt, width, length, lv_color_hex((uint32_t)col));
    return js_mknull();
}

static jsval_t js_lv_meter_set_scale_major_ticks(struct js *js, jsval_t *args, int nargs) {
    // (meterH, scalePtr, freq, width, length, color, label_gap)
    if(nargs<7) return js_mknull();
    int mH     = (int)js_getnum(args[0]);
    intptr_t scP = (intptr_t)js_getnum(args[1]);
    int freq   = (int)js_getnum(args[2]);
    int width  = (int)js_getnum(args[3]);
    int length = (int)js_getnum(args[4]);
    double col = js_getnum(args[5]);
    int label_gap = (int)js_getnum(args[6]);

    lv_obj_t *mt = get_lv_obj(mH);
    if(!mt) return js_mknull();

    lv_meter_scale_t *sc = (lv_meter_scale_t*) scP;
    lv_meter_set_scale_major_ticks(mt, sc, freq, width, length, lv_color_hex((uint32_t)col), label_gap);
    return js_mknull();
}

static jsval_t js_lv_meter_set_scale_range(struct js *js, jsval_t *args, int nargs) {
    // (meterH, scalePtr, min, max, angle_range, rotation)
    if(nargs<6) return js_mknull();
    int mH   = (int)js_getnum(args[0]);
    intptr_t scP= (intptr_t)js_getnum(args[1]);
    int minV = (int)js_getnum(args[2]);
    int maxV = (int)js_getnum(args[3]);
    int angleRange = (int)js_getnum(args[4]);
    int rotation   = (int)js_getnum(args[5]);

    lv_obj_t *mt = get_lv_obj(mH);
    if(!mt) return js_mknull();
    lv_meter_scale_t *sc = (lv_meter_scale_t*) scP;
    lv_meter_set_scale_range(mt, sc, minV, maxV, angleRange, rotation);
    return js_mknull();
}

// meter indicator creation
static jsval_t js_lv_meter_add_arc(struct js *js, jsval_t *args, int nargs) {
    // (meterH, scalePtr, width, color, rMod)
    // returns indicator pointer
    if(nargs<5) return js_mknull();
    int mH     = (int)js_getnum(args[0]);
    intptr_t scP = (intptr_t)js_getnum(args[1]);
    int width  = (int)js_getnum(args[2]);
    double col = js_getnum(args[3]);
    int rMod   = (int)js_getnum(args[4]);

    lv_obj_t *mt = get_lv_obj(mH);
    if(!mt) return js_mknull();
    lv_meter_scale_t *sc = (lv_meter_scale_t *)scP;

    lv_meter_indicator_t *ind = lv_meter_add_arc(mt, sc, width, lv_color_hex((uint32_t)col), rMod);
    intptr_t ret = (intptr_t)ind;
    return js_mknum((double)ret);
}

static jsval_t js_lv_meter_add_scale_lines(struct js *js, jsval_t *args, int nargs) {
    // (meterH, scalePtr, color_main, color_grad, local, width_mod)
    // returns indicator pointer
    if(nargs<6) return js_mknull();
    int mH         = (int)js_getnum(args[0]);
    intptr_t scP   = (intptr_t)js_getnum(args[1]);
    double colorM  = js_getnum(args[2]);
    double colorG  = js_getnum(args[3]);
    bool local     = (bool)js_getnum(args[4]);
    int widthMod   = (int)js_getnum(args[5]);

    lv_obj_t *mt = get_lv_obj(mH);
    if(!mt) return js_mknull();
    lv_meter_scale_t *sc = (lv_meter_scale_t *)scP;

    lv_meter_indicator_t *ind = lv_meter_add_scale_lines(mt, sc,
        lv_color_hex((uint32_t)colorM),
        lv_color_hex((uint32_t)colorG),
        local, widthMod
    );
    intptr_t ret = (intptr_t)ind;
    return js_mknum((double)ret);
}

static jsval_t js_lv_meter_add_needle_line(struct js *js, jsval_t *args, int nargs) {
    // (meterH, scalePtr, width, color, rMod)
    if(nargs<5) return js_mknull();
    int mH     = (int)js_getnum(args[0]);
    intptr_t scP= (intptr_t)js_getnum(args[1]);
    int width  = (int)js_getnum(args[2]);
    double col = js_getnum(args[3]);
    int rMod   = (int)js_getnum(args[4]);

    lv_obj_t *mt = get_lv_obj(mH);
    if(!mt) return js_mknull();
    lv_meter_scale_t *sc = (lv_meter_scale_t *)scP;

    lv_meter_indicator_t *ind = lv_meter_add_needle_line(mt, sc, width, lv_color_hex((uint32_t)col), rMod);
    intptr_t ret = (intptr_t)ind;
    return js_mknum((double)ret);
}

static jsval_t js_lv_meter_add_needle_img(struct js *js, jsval_t *args, int nargs) {
    // (meterH, scalePtr, srcAddr, pivot_x, pivot_y)
    // returns indicator pointer
    if(nargs<5) return js_mknull();
    int mH        = (int)js_getnum(args[0]);
    intptr_t scP  = (intptr_t)js_getnum(args[1]);
    // "srcAddr" is an image source pointer or something
    intptr_t srcPtr= (intptr_t)js_getnum(args[2]);
    int pivotX    = (int)js_getnum(args[3]);
    int pivotY    = (int)js_getnum(args[4]);

    // If we have a global or static "LV_IMG_DECLARE(img_hand);" we normally pass &img_hand
    // from JS. That means we store "img_hand" pointer in a variable. 
    // For simplicity let's assume srcPtr is the actual pointer to an lv_img_dsc_t.

    const lv_img_dsc_t * src_dsc = (const lv_img_dsc_t *)srcPtr;

    lv_obj_t *mt = get_lv_obj(mH);
    if(!mt) return js_mknull();
    lv_meter_scale_t *sc = (lv_meter_scale_t *)scP;

    lv_meter_indicator_t *ind = lv_meter_add_needle_img(mt, sc, src_dsc, pivotX, pivotY);
    intptr_t ret = (intptr_t)ind;
    return js_mknum((double)ret);
}

// meter set indicator
static jsval_t js_lv_meter_set_indicator_start_value(struct js *js, jsval_t *args, int nargs) {
    // (meterH, indicatorPtr, startVal)
    if(nargs<3) return js_mknull();
    int mH      = (int)js_getnum(args[0]);
    intptr_t indP= (intptr_t)js_getnum(args[1]);
    int stVal   = (int)js_getnum(args[2]);

    lv_obj_t *mt = get_lv_obj(mH);
    if(!mt) return js_mknull();
    lv_meter_indicator_t *ind = (lv_meter_indicator_t*) indP;

    lv_meter_set_indicator_start_value(mt, ind, stVal);
    return js_mknull();
}

static jsval_t js_lv_meter_set_indicator_end_value(struct js *js, jsval_t *args, int nargs) {
    // (meterH, indicatorPtr, endVal)
    if(nargs<3) return js_mknull();
    int mH      = (int)js_getnum(args[0]);
    intptr_t indP= (intptr_t)js_getnum(args[1]);
    int endVal  = (int)js_getnum(args[2]);

    lv_obj_t *mt = get_lv_obj(mH);
    if(!mt) return js_mknull();
    lv_meter_indicator_t *ind = (lv_meter_indicator_t*) indP;

    lv_meter_set_indicator_end_value(mt, ind, endVal);
    return js_mknull();
}

static jsval_t js_lv_meter_set_indicator_value(struct js *js, jsval_t *args, int nargs) {
    // (meterH, indicatorPtr, val)
    if(nargs<3) return js_mknull();
    int mH      = (int)js_getnum(args[0]);
    intptr_t indP= (intptr_t)js_getnum(args[1]);
    int val     = (int)js_getnum(args[2]);

    lv_obj_t *mt = get_lv_obj(mH);
    if(!mt) return js_mknull();
    lv_meter_indicator_t *ind = (lv_meter_indicator_t*) indP;

    lv_meter_set_indicator_value(mt, ind, val);
    return js_mknull();
}

/********************************************************************************
 * SPINBOX
 ********************************************************************************/
static jsval_t js_lv_spinbox_create(struct js *js, jsval_t *args, int nargs) {
    lv_obj_t *sb = lv_spinbox_create(lv_scr_act());
    int handle = store_lv_obj(sb);
    return js_mknum(handle);
}

static jsval_t js_lv_spinbox_set_range(struct js *js, jsval_t *args, int nargs) {
    // (spinboxH, min, max)
    if(nargs<3) return js_mknull();
    int h   = (int)js_getnum(args[0]);
    int mn  = (int)js_getnum(args[1]);
    int mx  = (int)js_getnum(args[2]);

    lv_obj_t *sb = get_lv_obj(h);
    if(!sb) return js_mknull();
    lv_spinbox_set_range(sb, mn, mx);
    return js_mknull();
}

static jsval_t js_lv_spinbox_set_digit_format(struct js *js, jsval_t *args, int nargs) {
    // (spinboxH, digit_count, separator_pos)
    if(nargs<3) return js_mknull();
    int h        = (int)js_getnum(args[0]);
    int digCount = (int)js_getnum(args[1]);
    int sepPos   = (int)js_getnum(args[2]);

    lv_obj_t *sb = get_lv_obj(h);
    if(!sb) return js_mknull();
    lv_spinbox_set_digit_format(sb, digCount, sepPos);
    return js_mknull();
}

static jsval_t js_lv_spinbox_step_prev(struct js *js, jsval_t *args, int nargs) {
    // (spinboxH)
    if(nargs<1) return js_mknull();
    int h = (int)js_getnum(args[0]);
    lv_obj_t *sb = get_lv_obj(h);
    if(!sb) return js_mknull();
    lv_spinbox_step_prev(sb);
    return js_mknull();
}
static jsval_t js_lv_spinbox_step_next(struct js *js, jsval_t *args, int nargs) {
    // (spinboxH)
    if(nargs<1) return js_mknull();
    int h = (int)js_getnum(args[0]);
    lv_obj_t *sb = get_lv_obj(h);
    if(!sb) return js_mknull();
    lv_spinbox_step_next(sb);
    return js_mknull();
}

static jsval_t js_lv_spinbox_increment(struct js *js, jsval_t *args, int nargs) {
    // (spinboxH)
    if(nargs<1) return js_mknull();
    int h = (int)js_getnum(args[0]);
    lv_obj_t *sb = get_lv_obj(h);
    if(!sb) return js_mknull();
    lv_spinbox_increment(sb);
    return js_mknull();
}

static jsval_t js_lv_spinbox_decrement(struct js *js, jsval_t *args, int nargs) {
    // (spinboxH)
    if(nargs<1) return js_mknull();
    int h = (int)js_getnum(args[0]);
    lv_obj_t *sb = get_lv_obj(h);
    if(!sb) return js_mknull();
    lv_spinbox_decrement(sb);
    return js_mknull();
}

/********************************************************************************
 * MSGBOX
 ********************************************************************************/
static jsval_t js_lv_msgbox_create(struct js *js, jsval_t *args, int nargs) {
    // (parentH or -1), titleStr, txtStr, an array of const char*(?), add_ok? 
    // This is tricky because we need a char** for the buttons. Let's do a simpler approach:
    // We might do: (title, text, btn1, btn2, ...) or the user can pass a single string "btn1,btn2"
    // For simplicity let's just do an overloaded approach: (title, text, "OK\nClose"), or (NULL for last param)
    // We can't easily parse an array from Elk JS. Let's do a simpler approach: 
    // (title, text, "OK,Close", is_modal_bool)
    if(nargs<4) return js_mknull();

    const char* title = js_str(js, args[0]);
    const char* msg   = js_str(js, args[1]);
    const char* btns  = js_str(js, args[2]);
    bool addModal     = (bool)js_getnum(args[3]);

    // parse the "OK,Close" into a static array of char*
    // E.g. we can do a quick split
    static const char * defBtns[16];
    memset(defBtns, 0, sizeof(defBtns));

    char tmp[256];
    strncpy(tmp, btns ? btns : "", sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';

    int idx=0;
    char *token = strtok(tmp, ",");
    while(token && idx< (int)(sizeof(defBtns)/sizeof(defBtns[0]) -1 )) {
        defBtns[idx++] = token;
        token = strtok(NULL, ",");
    }
    defBtns[idx] = NULL; // terminator

    lv_obj_t * mb = lv_msgbox_create(NULL, title, msg, defBtns[0] ? defBtns : NULL, addModal);
    int handle = store_lv_obj(mb);
    return js_mknum(handle);
}

static jsval_t js_lv_msgbox_get_active_btn_text(struct js *js, jsval_t *args, int nargs) {
    // (msgboxH) -> string
    if(nargs<1) return js_mkstr(js, "", 0);
    int h = (int)js_getnum(args[0]);
    lv_obj_t *mb = get_lv_obj(h);
    if(!mb) return js_mkstr(js,"",0);

    const char* t = lv_msgbox_get_active_btn_text(mb);
    if(!t) t = "";
    return js_mkstr(js, t, strlen(t));
}

/********************************************************************************
 * ROLLER
 ********************************************************************************/
static jsval_t js_lv_roller_create(struct js *js, jsval_t *args, int nargs) {
    // no param
    lv_obj_t *roll = lv_roller_create(lv_scr_act());
    int handle = store_lv_obj(roll);
    return js_mknum(handle);
}

static jsval_t js_lv_roller_set_options(struct js *js, jsval_t *args, int nargs) {
    // (rollerH, "str with \n", modeEnum)
    if(nargs<3) return js_mknull();
    int h  = (int)js_getnum(args[0]);
    const char* opts = js_str(js, args[1]);
    int mode = (int)js_getnum(args[2]); // e.g. LV_ROLLER_MODE_NORMAL or LV_ROLLER_MODE_INFINITE

    lv_obj_t *roll = get_lv_obj(h);
    if(!roll) return js_mknull();

    lv_roller_set_options(roll, opts ? opts : "", (lv_roller_mode_t)mode);
    return js_mknull();
}

static jsval_t js_lv_roller_set_visible_row_count(struct js *js, jsval_t *args, int nargs) {
    // (rollerH, rowCount)
    if(nargs<2) return js_mknull();
    int h = (int)js_getnum(args[0]);
    int r = (int)js_getnum(args[1]);
    lv_obj_t *roll = get_lv_obj(h);
    if(!roll) return js_mknull();
    lv_roller_set_visible_row_count(roll, r);
    return js_mknull();
}

static jsval_t js_lv_roller_get_selected_str(struct js *js, jsval_t *args, int nargs) {
    // (rollerH) -> string
    if(nargs<1) return js_mkstr(js,"",0);
    int h = (int)js_getnum(args[0]);
    lv_obj_t *roll = get_lv_obj(h);
    if(!roll) return js_mkstr(js,"",0);

    char buf[64];
    lv_roller_get_selected_str(roll, buf, sizeof(buf));
    return js_mkstr(js, buf, strlen(buf));
}

static jsval_t js_lv_roller_set_selected(struct js *js, jsval_t *args, int nargs) {
    // (rollerH, sel, animOff0or1)
    if(nargs<3) return js_mknull();
    int h = (int)js_getnum(args[0]);
    int sel = (int)js_getnum(args[1]);
    int anim = (int)js_getnum(args[2]);

    lv_obj_t *roll = get_lv_obj(h);
    if(!roll) return js_mknull();

    lv_roller_set_selected(roll, sel, anim ? LV_ANIM_ON : LV_ANIM_OFF);
    return js_mknull();
}

/*******************************************************
 * BUTTON BRIDGING
 *******************************************************/

// create_button(parentHandle, x, y, width, height) => returns handle
static jsval_t js_lv_btn_create(struct js *js, jsval_t *args, int nargs) {
    if(nargs < 5) {
        Serial.println("lv_btn_create: expects parentHandle, x, y, width, height");
        return js_mknull();
    }
    int parentHandle = (int)js_getnum(args[0]);
    int x = (int)js_getnum(args[1]);
    int y = (int)js_getnum(args[2]);
    int width = (int)js_getnum(args[3]);
    int height = (int)js_getnum(args[4]);

    lv_obj_t *parent = get_lv_obj(parentHandle);
    if(!parent) {
        Serial.println("lv_btn_create: invalid parent handle");
        return js_mknull();
    }

    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);

    int handle = store_lv_obj(button);
    Serial.printf("lv_btn_create => handle %d\n", handle);
    return js_mknum(handle);
}

// button_set_text(buttonHandle, "Button Text")
static jsval_t js_lv_button_set_text(struct js *js, jsval_t *args, int nargs) {
    if(nargs < 2) {
        Serial.println("lv_button_set_text: expects buttonHandle, text");
        return js_mknull();
    }
    int btnHandle = (int)js_getnum(args[0]);
    const char* text = js_str(js, args[1]);

    lv_obj_t *button = get_lv_obj(btnHandle);
    if(!button) {
        Serial.println("lv_button_set_text: invalid button handle");
        return js_mknull();
    }

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text ? text : "");
    lv_obj_center(label); // Center the label within the button

    Serial.printf("lv_button_set_text: handle=%d, text=%s\n", btnHandle, text ? text : "");
    return js_mknull();
}


/********************************************************************************
 * SLIDER: Possibly we already have basic bridging? 
 * Additional calls: 
 *   lv_slider_set_mode, lv_slider_set_value, lv_slider_set_left_value, lv_slider_get_value, lv_slider_get_left_value
 ********************************************************************************/

static jsval_t js_lv_slider_create(struct js *js, jsval_t *args, int nargs) {
    // no params
    lv_obj_t *sld = lv_slider_create(lv_scr_act());
    int handle = store_lv_obj(sld);
    return js_mknum(handle);
}

static jsval_t js_lv_slider_set_mode(struct js *js, jsval_t *args, int nargs) {
    // (sliderH, modeEnum) -> e.g. LV_SLIDER_MODE_NORMAL, LV_SLIDER_MODE_RANGE
    if(nargs<2) return js_mknull();
    int h   = (int)js_getnum(args[0]);
    int md  = (int)js_getnum(args[1]);

    lv_obj_t *sld = get_lv_obj(h);
    if(!sld) return js_mknull();
    lv_slider_set_mode(sld, (lv_slider_mode_t)md);
    return js_mknull();
}

static jsval_t js_lv_slider_set_value(struct js *js, jsval_t *args, int nargs) {
    // (sliderH, val, animOff0or1)
    if(nargs<3) return js_mknull();
    int h      = (int)js_getnum(args[0]);
    int val    = (int)js_getnum(args[1]);
    bool anim  = (bool)js_getnum(args[2]);

    lv_obj_t *sld = get_lv_obj(h);
    if(!sld) return js_mknull();
    lv_slider_set_value(sld, val, anim ? LV_ANIM_ON : LV_ANIM_OFF);
    return js_mknull();
}

static jsval_t js_lv_slider_set_left_value(struct js *js, jsval_t *args, int nargs) {
    // (sliderH, val, animOff0or1)
    if(nargs<3) return js_mknull();
    int h      = (int)js_getnum(args[0]);
    int val    = (int)js_getnum(args[1]);
    bool anim  = (bool)js_getnum(args[2]);

    lv_obj_t *sld = get_lv_obj(h);
    if(!sld) return js_mknull();
    lv_slider_set_left_value(sld, val, anim ? LV_ANIM_ON : LV_ANIM_OFF);
    return js_mknull();
}

static jsval_t js_lv_slider_get_value(struct js *js, jsval_t *args, int nargs) {
    // (sliderH) -> int
    if(nargs<1) return js_mknum(0);
    int h = (int)js_getnum(args[0]);

    lv_obj_t *sld = get_lv_obj(h);
    if(!sld) return js_mknum(0);

    int v = lv_slider_get_value(sld);
    return js_mknum((double)v);
}

static jsval_t js_lv_slider_get_left_value(struct js *js, jsval_t *args, int nargs) {
    // (sliderH) -> int
    if(nargs<1) return js_mknum(0);
    int h = (int)js_getnum(args[0]);

    lv_obj_t *sld = get_lv_obj(h);
    if(!sld) return js_mknum(0);

    int v = lv_slider_get_left_value(sld);
    return js_mknum((double)v);
}


/********************************************************************************
 * SPAN
 ********************************************************************************/
static jsval_t js_lv_spangroup_create(struct js *js, jsval_t *args, int nargs) {
    lv_obj_t * spg = lv_spangroup_create(lv_scr_act());
    int handle = store_lv_obj(spg);
    return js_mknum(handle);
}

static jsval_t js_lv_spangroup_set_align(struct js *js, jsval_t *args, int nargs) {
    // (spangroupH, alignEnum=LV_TEXT_ALIGN_LEFT/CENTER/RIGHT/AUTO)
    if(nargs<2) return js_mknull();
    int h   = (int)js_getnum(args[0]);
    int alg = (int)js_getnum(args[1]);
    lv_obj_t *spg = get_lv_obj(h);
    if(!spg) return js_mknull();

    lv_spangroup_set_align(spg, (lv_text_align_t)alg);
    return js_mknull();
}

static jsval_t js_lv_spangroup_set_overflow(struct js *js, jsval_t *args, int nargs) {
    // (spangroupH, overflowEnum=LV_SPAN_OVERFLOW_CLIP/ELLIPSIS)
    if(nargs<2) return js_mknull();
    int h   = (int)js_getnum(args[0]);
    int ovf = (int)js_getnum(args[1]);
    lv_obj_t *spg = get_lv_obj(h);
    if(!spg) return js_mknull();

    lv_spangroup_set_overflow(spg, (lv_span_overflow_t)ovf);
    return js_mknull();
}

static jsval_t js_lv_spangroup_set_indent(struct js *js, jsval_t *args, int nargs) {
    // (spangroupH, indentPX)
    if(nargs<2) return js_mknull();
    int h = (int)js_getnum(args[0]);
    int indent = (int)js_getnum(args[1]);
    lv_obj_t *spg = get_lv_obj(h);
    if(!spg) return js_mknull();

    lv_spangroup_set_indent(spg, indent);
    return js_mknull();
}

static jsval_t js_lv_spangroup_set_mode(struct js *js, jsval_t *args, int nargs) {
    // (spangroupH, mode=LV_SPAN_MODE_FIXED/NOWRAP/BREAK)
    if(nargs<2) return js_mknull();
    int h   = (int)js_getnum(args[0]);
    int md  = (int)js_getnum(args[1]);
    lv_obj_t *spg = get_lv_obj(h);
    if(!spg) return js_mknull();

    lv_spangroup_set_mode(spg, (lv_span_mode_t)md);
    return js_mknull();
}

static jsval_t js_lv_spangroup_new_span(struct js *js, jsval_t *args, int nargs) {
    // (spangroupH) -> pointer
    if(nargs<1) return js_mknull();
    int h = (int)js_getnum(args[0]);
    lv_obj_t *spg = get_lv_obj(h);
    if(!spg) return js_mknull();

    lv_span_t * sp = lv_spangroup_new_span(spg);
    intptr_t ret = (intptr_t)sp;
    return js_mknum((double)ret);
}

static jsval_t js_lv_span_set_text(struct js *js, jsval_t *args, int nargs) {
    // (spanPtr, text)
    if(nargs<2) return js_mknull();
    intptr_t spP = (intptr_t)js_getnum(args[0]);
    const char* txt = js_str(js, args[1]);
    if(!txt) return js_mknull();

    lv_span_t * sp = (lv_span_t *)spP;
    lv_span_set_text(sp, txt);
    return js_mknull();
}

static jsval_t js_lv_span_set_text_static(struct js *js, jsval_t *args, int nargs) {
    // (spanPtr, text) => static
    if(nargs<2) return js_mknull();
    intptr_t spP = (intptr_t)js_getnum(args[0]);
    const char* txt = js_str(js, args[1]);
    if(!txt) return js_mknull();

    lv_span_t * sp = (lv_span_t *)spP;
    lv_span_set_text_static(sp, txt);
    return js_mknull();
}

static jsval_t js_lv_spangroup_refr_mode(struct js *js, jsval_t *args, int nargs) {
    // (spangroupH)
    if(nargs<1) return js_mknull();
    int h = (int)js_getnum(args[0]);
    lv_obj_t *spg = get_lv_obj(h);
    if(!spg) return js_mknull();

    lv_spangroup_refr_mode(spg);
    return js_mknull();
}

/********************************************************************************
 * WIN (Window)
 ********************************************************************************/
static jsval_t js_lv_win_create(struct js *js, jsval_t *args, int nargs) {
    // (parentH or -1, headerHeight)
    int parentH = -1;
    int headerHeight = 40;
    if(nargs>0) parentH = (int)js_getnum(args[0]);
    if(nargs>1) headerHeight = (int)js_getnum(args[1]);

    lv_obj_t *parent = (parentH<0)? lv_scr_act() : get_lv_obj(parentH);
    if(!parent) parent = lv_scr_act();

    lv_obj_t * win = lv_win_create(parent, headerHeight);
    int handle = store_lv_obj(win);
    return js_mknum(handle);
}

static jsval_t js_lv_win_add_btn(struct js *js, jsval_t *args, int nargs) {
    // (winH, symbolOrText, btnWidth)
    if(nargs<3) return js_mknum(-1);
    int wh = (int)js_getnum(args[0]);
    const char* txt = js_str(js, args[1]);
    int width = (int)js_getnum(args[2]);

    lv_obj_t *win = get_lv_obj(wh);
    if(!win) return js_mknum(-1);

    lv_obj_t *btn = lv_win_add_btn(win, txt, width);
    int handle = store_lv_obj(btn);
    return js_mknum(handle);
}

static jsval_t js_lv_win_add_title(struct js *js, jsval_t *args, int nargs) {
    // (winH, "mytitle")
    if(nargs<2) return js_mknull();
    int wh = (int)js_getnum(args[0]);
    const char* t = js_str(js, args[1]);
    if(!t) return js_mknull();

    lv_obj_t *win = get_lv_obj(wh);
    if(!win) return js_mknull();

    lv_win_add_title(win, t);
    return js_mknull();
}

static jsval_t js_lv_win_get_content(struct js *js, jsval_t *args, int nargs) {
    // (winH) => handle
    if(nargs<1) return js_mknum(-1);
    int wh = (int)js_getnum(args[0]);
    lv_obj_t *win = get_lv_obj(wh);
    if(!win) return js_mknum(-1);

    lv_obj_t * c = lv_win_get_content(win);
    int handle = store_lv_obj(c);
    return js_mknum(handle);
}

/********************************************************************************
 * TUTORIAL FOR TILES (TileView)
 ********************************************************************************/
static jsval_t js_lv_tileview_create(struct js *js, jsval_t *args, int nargs) {
    // no param
    lv_obj_t * tv = lv_tileview_create(lv_scr_act());
    int handle = store_lv_obj(tv);
    return js_mknum(handle);
}

static jsval_t js_lv_tileview_add_tile(struct js *js, jsval_t *args, int nargs) {
    // (tileviewH, col, row, allowedDir) => returns tile page handle
    if(nargs<4) return js_mknum(-1);
    int tvH = (int)js_getnum(args[0]);
    int col = (int)js_getnum(args[1]);
    int row = (int)js_getnum(args[2]);
    int dir = (int)js_getnum(args[3]); // e.g. LV_DIR_LEFT|LV_DIR_RIGHT etc.

    lv_obj_t *tv = get_lv_obj(tvH);
    if(!tv) return js_mknum(-1);

    lv_obj_t * tile = lv_tileview_add_tile(tv, col, row, dir);
    int handle = store_lv_obj(tile);
    return js_mknum(handle);
}


/*******************************************************
 * LIST BRIDGING
 *******************************************************/

static jsval_t js_lv_list_create(struct js *js, jsval_t *args, int nargs) {
    // create a list
    lv_obj_t *list = lv_list_create(lv_scr_act());
    int handle = store_lv_obj(list);
    Serial.printf("lv_list_create => handle %d\n", handle);
    return js_mknum(handle);
}

static jsval_t js_lv_list_add_btn(struct js *js, jsval_t *args, int nargs) {
    // (listH, iconSymbolOrNULL, txt)
    if(nargs < 3) return js_mknull();
    int lh = (int)js_getnum(args[0]);
    const char* icon = js_str(js, args[1]);
    const char* txt  = js_str(js, args[2]);

    lv_obj_t *list = get_lv_obj(lh);
    if(!list) return js_mknull();

    // If icon is "NULL" or "", pass NULL
    const char* useIcon = NULL;
    if(icon && strlen(icon) > 0) {
        useIcon = icon; // e.g. LV_SYMBOL_OK
    }

    lv_obj_t *btn = lv_list_add_btn(list, useIcon, txt ? txt : "");
    int handle = store_lv_obj(btn);
    Serial.printf("lv_list_add_btn => handle %d\n", handle);
    return js_mknum(handle);
}

static jsval_t js_lv_list_add_text(struct js *js, jsval_t *args, int nargs) {
    // (listH, txt)
    if(nargs < 2) return js_mknull();
    int lh = (int)js_getnum(args[0]);
    const char* txt = js_str(js, args[1]);

    lv_obj_t *list = get_lv_obj(lh);
    if(!list) return js_mknull();

    lv_obj_t *obj = lv_list_add_text(list, txt ? txt : "");
    int handle = store_lv_obj(obj);
    Serial.printf("lv_list_add_text => handle %d\n", handle);
    return js_mknum(handle);
}

static jsval_t js_lv_list_get_btn_text(struct js *js, jsval_t *args, int nargs) {
    // (listH, btnH) => returns text string
    if(nargs < 2) return js_mkstr(js, "", 0);
    int listH = (int)js_getnum(args[0]);
    int btnH  = (int)js_getnum(args[1]);

    lv_obj_t *list = get_lv_obj(listH);
    lv_obj_t *btn  = get_lv_obj(btnH);
    if(!list || !btn) return js_mkstr(js, "", 0);

    const char* txt = lv_list_get_btn_text(list, btn);
    if(!txt) txt = "";
    return js_mkstr(js, txt, strlen(txt));
}


/*******************************************************
 * LINE BRIDGING
 *******************************************************/

static jsval_t js_lv_line_create(struct js *js, jsval_t *args, int nargs) {
    lv_obj_t *line = lv_line_create(lv_scr_act());
    int handle = store_lv_obj(line);
    Serial.printf("lv_line_create => handle %d\n", handle);
    return js_mknum(handle);
}

// Setting points requires us to parse an array of {x,y} from JS or something
// For simplicity, here's a bridging that receives e.g. (lineH, x0, y0, x1, y1, x2, y2, ...)
static jsval_t js_lv_line_set_points(struct js *js, jsval_t *args, int nargs) {
    // Must have at least (lineH, x0, y0)
    if(nargs < 3) return js_mknull();
    int h = (int)js_getnum(args[0]);

    // The rest are coordinate pairs
    int pairCount = (nargs - 1) / 2; // minus 1 for the handle, then each 2 = one point
    if(pairCount < 1) return js_mknull();

    lv_obj_t *line = get_lv_obj(h);
    if(!line) return js_mknull();

    static lv_point_t points[32]; // up to 16 points
    if(pairCount > 16) pairCount = 16; // clamp

    int idx = 1; // start reading from arg[1]
    for(int i=0; i<pairCount; i++) {
        int x = (int)js_getnum(args[idx++]);
        int y = (int)js_getnum(args[idx++]);
        points[i].x = x;
        points[i].y = y;
    }

    lv_line_set_points(line, points, pairCount);
    return js_mknull();
}


/*******************************************************
 * LED BRIDGING
 *******************************************************/

static jsval_t js_lv_led_create(struct js *js, jsval_t *args, int nargs) {
    lv_obj_t *led = lv_led_create(lv_scr_act());
    int handle = store_lv_obj(led);
    Serial.printf("lv_led_create => handle %d\n", handle);
    return js_mknum(handle);
}

static jsval_t js_lv_led_on(struct js *js, jsval_t *args, int nargs) {
    if(nargs<1) return js_mknull();
    int h = (int)js_getnum(args[0]);
    lv_obj_t *led = get_lv_obj(h);
    if(!led) return js_mknull();

    lv_led_on(led);
    return js_mknull();
}

static jsval_t js_lv_led_off(struct js *js, jsval_t *args, int nargs) {
    if(nargs<1) return js_mknull();
    int h = (int)js_getnum(args[0]);
    lv_obj_t *led = get_lv_obj(h);
    if(!led) return js_mknull();

    lv_led_off(led);
    return js_mknull();
}

static jsval_t js_lv_led_set_brightness(struct js *js, jsval_t *args, int nargs) {
    if(nargs<2) return js_mknull();
    int h    = (int)js_getnum(args[0]);
    int bright = (int)js_getnum(args[1]);
    lv_obj_t *led = get_lv_obj(h);
    if(!led) return js_mknull();

    lv_led_set_brightness(led, bright);
    return js_mknull();
}

static jsval_t js_lv_led_set_color(struct js *js, jsval_t *args, int nargs) {
    if(nargs<2) return js_mknull();
    int h    = (int)js_getnum(args[0]);
    double col = js_getnum(args[1]);
    lv_obj_t *led = get_lv_obj(h);
    if(!led) return js_mknull();

    lv_led_set_color(led, lv_color_hex((uint32_t)col));
    return js_mknull();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~ 1) HTTP ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// http_get(url)
static jsval_t js_http_get(struct js *js, jsval_t *args, int nargs) {
  if(nargs<1) return js_mkstr(js,"",0);
  const char* url = js_str(js, args[0]);
  if(!url) return js_mkstr(js,"",0);

  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();
  if(httpCode<=0) {
    http.end();
    return js_mkstr(js,"",0);
  }
  String payload = http.getString();
  http.end();
  return js_mkstr(js, payload.c_str(), payload.length());
}

// http_post(url, body)
static jsval_t js_http_post(struct js *js, jsval_t *args, int nargs) {
  if(nargs<2) return js_mkstr(js,"",0);
  const char* url  = js_str(js, args[0]);
  const char* body = js_str(js, args[1]);
  if(!url || !body) return js_mkstr(js,"",0);

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST((uint8_t*)body, strlen(body));
  if(httpCode<=0) {
    http.end();
    return js_mkstr(js,"",0);
  }
  String payload = http.getString();
  http.end();
  return js_mkstr(js, payload.c_str(), payload.length());
}

// Similarly for PUT, PATCH, DELETE (example: http_delete)
static jsval_t js_http_delete(struct js *js, jsval_t *args, int nargs) {
  if(nargs<1) return js_mkstr(js,"",0);
  const char* url = js_str(js, args[0]);
  if(!url) return js_mkstr(js,"",0);

  HTTPClient http;
  http.begin(url);
  int httpCode = http.sendRequest("DELETE");
  if(httpCode<=0) {
    http.end();
    return js_mkstr(js,"",0);
  }
  String payload = http.getString();
  http.end();
  return js_mkstr(js, payload.c_str(), payload.length());
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~ 4) Extended SD ops ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// We already have sd_list_dir, sd_read_file, sd_write_file. Add file delete:
static jsval_t js_sd_delete_file(struct js *js, jsval_t *args, int nargs) {
  if(nargs<1) return js_mkfalse();
  const char* path = js_str(js, args[0]);
  if(!path) return js_mkfalse();

  String fullPath = String(path);
  if(SD_MMC.exists(fullPath)) {
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
  void onConnect(NimBLEServer* pServer) {
    g_bleConnected = true;
    Serial.println("BLE device connected");
  }

  void onDisconnect(NimBLEServer* pServer) {
    g_bleConnected = false;
    Serial.println("BLE device disconnected");
    pServer->startAdvertising();
  }
};

class MyCharCallbacks : public NimBLECharacteristicCallbacks {
 public:
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string rxData = pCharacteristic->getValue();
    Serial.printf("BLE Received: %s\n", rxData.c_str());
  }
};

// ble_init(devName, serviceUUID, charUUID)
static jsval_t js_ble_init(struct js *js, jsval_t *args, int nargs) {
  if(nargs < 3) return js_mkfalse();
  const char* devName  = js_str(js, args[0]);
  const char* svcUUID  = js_str(js, args[1]);
  const char* charUUID = js_str(js, args[2]);
  if(!devName || !svcUUID || !charUUID) return js_mkfalse();

  // Initialize NimBLE
  NimBLEDevice::init(devName);

  // Create server
  g_bleServer = NimBLEDevice::createServer();
  g_bleServer->setCallbacks(new MyServerCallbacks());

  // Create a BLE service
  NimBLEService* pService = g_bleServer->createService(svcUUID);

  // Create a BLE Characteristic
  g_bleChar = pService->createCharacteristic(
      charUUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  g_bleChar->setCallbacks(new MyCharCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  g_bleServer->getAdvertising()->start();
  Serial.println("NimBLE advertising started");
  return js_mktrue();
}

// ble_is_connected() => bool
static jsval_t js_ble_is_connected(struct js *js, jsval_t *args, int nargs) {
  return g_bleConnected ? js_mktrue() : js_mkfalse();
}

// ble_write(str)
static jsval_t js_ble_write(struct js *js, jsval_t *args, int nargs) {
  if(!g_bleChar) return js_mkfalse();
  if(nargs < 1) return js_mkfalse();
  const char* data = js_str(js, args[0]);
  if(!data) return js_mkfalse();

  g_bleChar->setValue(data);
  g_bleChar->notify();
  return js_mktrue();
}

/******************************************************************************
 * I) Register All JS Functions
 ******************************************************************************/
void register_js_functions() {
  jsval_t global = js_glob(js);

  // Basic
  js_set(js, global, "print",        js_mkfun(js_print));
  js_set(js, global, "wifi_connect", js_mkfun(js_wifi_connect));
  js_set(js, global, "wifi_status",  js_mkfun(js_wifi_status));
  js_set(js, global, "wifi_get_ip",  js_mkfun(js_wifi_get_ip));
  js_set(js, global, "delay",        js_mkfun(js_delay));

  // HTTP
  js_set(js, global, "http_get",    js_mkfun(js_http_get));
  js_set(js, global, "http_post",   js_mkfun(js_http_post));
  js_set(js, global, "http_delete", js_mkfun(js_http_delete));

  // SD functions
  js_set(js, global, "sd_read_file", js_mkfun(js_sd_read_file));
  js_set(js, global, "sd_write_file",js_mkfun(js_sd_write_file));
  js_set(js, global, "sd_list_dir",  js_mkfun(js_sd_list_dir));
  js_set(js, global, "sd_delete_file", js_mkfun(js_sd_delete_file));

  // BLE
  js_set(js, global, "ble_init",         js_mkfun(js_ble_init));
  js_set(js, global, "ble_is_connected", js_mkfun(js_ble_is_connected));
  js_set(js, global, "ble_write",        js_mkfun(js_ble_write));

  // GIF from memory
  js_set(js, global, "show_gif_from_sd", js_mkfun(js_show_gif_from_sd));

  // Basic shapes
  js_set(js, global, "draw_label",   js_mkfun(js_lvgl_draw_label));
  js_set(js, global, "draw_rect",    js_mkfun(js_lvgl_draw_rect));
  js_set(js, global, "show_image",   js_mkfun(js_lvgl_show_image));

  // Handle-based image creation + transforms
  js_set(js, global, "create_image",          js_mkfun(js_create_image));
  js_set(js, global, "create_image_from_ram", js_mkfun(js_create_image_from_ram));
  js_set(js, global, "rotate_obj",            js_mkfun(js_rotate_obj));
  js_set(js, global, "move_obj",              js_mkfun(js_move_obj));
  js_set(js, global, "animate_obj",           js_mkfun(js_animate_obj));

  // Style creation + property setters
  js_set(js, global, "create_style",              js_mkfun(js_create_style));
  js_set(js, global, "obj_add_style",             js_mkfun(js_obj_add_style));

  js_set(js, global, "style_set_radius",          js_mkfun(js_style_set_radius));
  js_set(js, global, "style_set_bg_opa",          js_mkfun(js_style_set_bg_opa));
  js_set(js, global, "style_set_bg_color",        js_mkfun(js_style_set_bg_color));
  js_set(js, global, "style_set_border_color",    js_mkfun(js_style_set_border_color));
  js_set(js, global, "style_set_border_width",    js_mkfun(js_style_set_border_width));
  js_set(js, global, "style_set_border_opa",      js_mkfun(js_style_set_border_opa));
  js_set(js, global, "style_set_border_side",     js_mkfun(js_style_set_border_side));
  js_set(js, global, "style_set_outline_width",   js_mkfun(js_style_set_outline_width));
  js_set(js, global, "style_set_outline_color",   js_mkfun(js_style_set_outline_color));
  js_set(js, global, "style_set_outline_pad",     js_mkfun(js_style_set_outline_pad));
  js_set(js, global, "style_set_shadow_width",    js_mkfun(js_style_set_shadow_width));
  js_set(js, global, "style_set_shadow_color",    js_mkfun(js_style_set_shadow_color));
  js_set(js, global, "style_set_shadow_ofs_x",    js_mkfun(js_style_set_shadow_ofs_x));
  js_set(js, global, "style_set_shadow_ofs_y",    js_mkfun(js_style_set_shadow_ofs_y));
  js_set(js, global, "style_set_img_recolor",     js_mkfun(js_style_set_img_recolor));
  js_set(js, global, "style_set_img_recolor_opa", js_mkfun(js_style_set_img_recolor_opa));
  js_set(js, global, "style_set_transform_angle",  js_mkfun(js_style_set_transform_angle));
  js_set(js, global, "style_set_text_color",      js_mkfun(js_style_set_text_color));
  js_set(js, global, "style_set_text_letter_space", js_mkfun(js_style_set_text_letter_space));
  js_set(js, global, "style_set_text_line_space", js_mkfun(js_style_set_text_line_space));
  js_set(js, global, "style_set_text_decor",      js_mkfun(js_style_set_text_decor));
  js_set(js, global, "style_set_line_color",      js_mkfun(js_style_set_line_color));
  js_set(js, global, "style_set_line_width",      js_mkfun(js_style_set_line_width));
  js_set(js, global, "style_set_line_rounded",    js_mkfun(js_style_set_line_rounded));
  js_set(js, global, "style_set_pad_all",         js_mkfun(js_style_set_pad_all));
  js_set(js, global, "style_set_pad_left",        js_mkfun(js_style_set_pad_left));
  js_set(js, global, "style_set_pad_right",       js_mkfun(js_style_set_pad_right));
  js_set(js, global, "style_set_pad_top",         js_mkfun(js_style_set_pad_top));
  js_set(js, global, "style_set_pad_bottom",      js_mkfun(js_style_set_pad_bottom));
  js_set(js, global, "style_set_pad_ver",         js_mkfun(js_style_set_pad_ver));
  js_set(js, global, "style_set_pad_hor",         js_mkfun(js_style_set_pad_hor));
  js_set(js, global, "style_set_width",           js_mkfun(js_style_set_width));
  js_set(js, global, "style_set_height",          js_mkfun(js_style_set_height));
  js_set(js, global, "style_set_x",               js_mkfun(js_style_set_x));
  js_set(js, global, "style_set_y",               js_mkfun(js_style_set_y));

  // Object property setters
  js_set(js, global, "obj_set_size",        js_mkfun(js_obj_set_size));
  js_set(js, global, "obj_align",           js_mkfun(js_obj_align));

  // Scroll, flex, flags
  js_set(js, global, "obj_set_scroll_snap_x",   js_mkfun(js_obj_set_scroll_snap_x));
  js_set(js, global, "obj_set_scroll_snap_y",   js_mkfun(js_obj_set_scroll_snap_y));
  js_set(js, global, "obj_add_flag",            js_mkfun(js_obj_add_flag));
  js_set(js, global, "obj_clear_flag",          js_mkfun(js_obj_clear_flag));
  js_set(js, global, "obj_set_scroll_dir",      js_mkfun(js_obj_set_scroll_dir));
  js_set(js, global, "obj_set_scrollbar_mode",  js_mkfun(js_obj_set_scrollbar_mode));
  js_set(js, global, "obj_set_flex_flow",       js_mkfun(js_obj_set_flex_flow));
  js_set(js, global, "obj_set_flex_align",      js_mkfun(js_obj_set_flex_align));
  js_set(js, global, "obj_set_style_clip_corner", js_mkfun(js_obj_set_style_clip_corner));
  js_set(js, global, "obj_set_style_base_dir",  js_mkfun(js_obj_set_style_base_dir));

  //==================== METER ============================
  js_set(js, global, "lv_meter_create",                   js_mkfun(js_lv_meter_create));
  js_set(js, global, "lv_meter_add_scale",                js_mkfun(js_lv_meter_add_scale));
  js_set(js, global, "lv_meter_set_scale_ticks",          js_mkfun(js_lv_meter_set_scale_ticks));
  js_set(js, global, "lv_meter_set_scale_major_ticks",    js_mkfun(js_lv_meter_set_scale_major_ticks));
  js_set(js, global, "lv_meter_set_scale_range",          js_mkfun(js_lv_meter_set_scale_range));
  js_set(js, global, "lv_meter_add_arc",                  js_mkfun(js_lv_meter_add_arc));
  js_set(js, global, "lv_meter_add_scale_lines",          js_mkfun(js_lv_meter_add_scale_lines));
  js_set(js, global, "lv_meter_add_needle_line",          js_mkfun(js_lv_meter_add_needle_line));
  js_set(js, global, "lv_meter_add_needle_img",           js_mkfun(js_lv_meter_add_needle_img));
  js_set(js, global, "lv_meter_set_indicator_start_value",js_mkfun(js_lv_meter_set_indicator_start_value));
  js_set(js, global, "lv_meter_set_indicator_end_value",  js_mkfun(js_lv_meter_set_indicator_end_value));
  js_set(js, global, "lv_meter_set_indicator_value",      js_mkfun(js_lv_meter_set_indicator_value));

  //==================== SPINBOX =========================
  js_set(js, global, "lv_spinbox_create",           js_mkfun(js_lv_spinbox_create));
  js_set(js, global, "lv_spinbox_set_range",        js_mkfun(js_lv_spinbox_set_range));
  js_set(js, global, "lv_spinbox_set_digit_format", js_mkfun(js_lv_spinbox_set_digit_format));
  js_set(js, global, "lv_spinbox_step_prev",        js_mkfun(js_lv_spinbox_step_prev));
  js_set(js, global, "lv_spinbox_step_next",        js_mkfun(js_lv_spinbox_step_next));
  js_set(js, global, "lv_spinbox_increment",        js_mkfun(js_lv_spinbox_increment));
  js_set(js, global, "lv_spinbox_decrement",        js_mkfun(js_lv_spinbox_decrement));

  //==================== MSGBOX ==========================
  js_set(js, global, "lv_msgbox_create",            js_mkfun(js_lv_msgbox_create));
  js_set(js, global, "lv_msgbox_get_active_btn_text", js_mkfun(js_lv_msgbox_get_active_btn_text));

  //==================== ROLLER =========================
  js_set(js, global, "lv_roller_create",         js_mkfun(js_lv_roller_create));
  js_set(js, global, "lv_roller_set_options",    js_mkfun(js_lv_roller_set_options));
  js_set(js, global, "lv_roller_set_visible_row_count", js_mkfun(js_lv_roller_set_visible_row_count));
  js_set(js, global, "lv_roller_get_selected_str", js_mkfun(js_lv_roller_get_selected_str));
  js_set(js, global, "lv_roller_set_selected",   js_mkfun(js_lv_roller_set_selected));

  //==================== SLIDER (additional) =============
  js_set(js, global, "lv_slider_create",         js_mkfun(js_lv_slider_create));
  js_set(js, global, "lv_slider_set_mode",       js_mkfun(js_lv_slider_set_mode));
  js_set(js, global, "lv_slider_set_value",      js_mkfun(js_lv_slider_set_value));
  js_set(js, global, "lv_slider_set_left_value", js_mkfun(js_lv_slider_set_left_value));
  js_set(js, global, "lv_slider_get_value",      js_mkfun(js_lv_slider_get_value));
  js_set(js, global, "lv_slider_get_left_value", js_mkfun(js_lv_slider_get_left_value));

  //==================== SPAN ============================
  js_set(js, global, "lv_spangroup_create",      js_mkfun(js_lv_spangroup_create));
  js_set(js, global, "lv_spangroup_set_align",   js_mkfun(js_lv_spangroup_set_align));
  js_set(js, global, "lv_spangroup_set_overflow",js_mkfun(js_lv_spangroup_set_overflow));
  js_set(js, global, "lv_spangroup_set_indent",  js_mkfun(js_lv_spangroup_set_indent));
  js_set(js, global, "lv_spangroup_set_mode",    js_mkfun(js_lv_spangroup_set_mode));
  js_set(js, global, "lv_spangroup_new_span",    js_mkfun(js_lv_spangroup_new_span));
  js_set(js, global, "lv_span_set_text",         js_mkfun(js_lv_span_set_text));
  js_set(js, global, "lv_span_set_text_static",  js_mkfun(js_lv_span_set_text_static));
  js_set(js, global, "lv_spangroup_refr_mode",   js_mkfun(js_lv_spangroup_refr_mode));

  //==================== WIN =============================
  js_set(js, global, "lv_win_create",             js_mkfun(js_lv_win_create));
  js_set(js, global, "lv_win_add_btn",            js_mkfun(js_lv_win_add_btn));
  js_set(js, global, "lv_win_add_title",          js_mkfun(js_lv_win_add_title));
  js_set(js, global, "lv_win_get_content",        js_mkfun(js_lv_win_get_content));

  //==================== TILEVIEW ========================
  js_set(js, global, "lv_tileview_create",        js_mkfun(js_lv_tileview_create));
  js_set(js, global, "lv_tileview_add_tile",      js_mkfun(js_lv_tileview_add_tile));

  // ---------- LIST bridging
  js_set(js, global, "lv_list_create",          js_mkfun(js_lv_list_create));
  js_set(js, global, "lv_list_add_btn",         js_mkfun(js_lv_list_add_btn));
  js_set(js, global, "lv_list_add_text",        js_mkfun(js_lv_list_add_text));
  js_set(js, global, "lv_list_get_btn_text",    js_mkfun(js_lv_list_get_btn_text));

  // ---------- LINE bridging
  js_set(js, global, "lv_line_create",          js_mkfun(js_lv_line_create));
  js_set(js, global, "lv_line_set_points",      js_mkfun(js_lv_line_set_points));

  // ---------- LED bridging
  js_set(js, global, "lv_led_create",           js_mkfun(js_lv_led_create));
  js_set(js, global, "lv_led_on",               js_mkfun(js_lv_led_on));
  js_set(js, global, "lv_led_off",              js_mkfun(js_lv_led_off));
  js_set(js, global, "lv_led_set_brightness",   js_mkfun(js_lv_led_set_brightness));
  js_set(js, global, "lv_led_set_color",        js_mkfun(js_lv_led_set_color));

  // ---------- BUTTON bridging
  js_set(js, global, "lv_btn_create", js_mkfun(js_lv_btn_create));
  js_set(js, global, "lv_button_set_text", js_mkfun(js_lv_button_set_text));
}

//------------------------------------------------------------------------------
// K) The elk_task -- runs Elk + bridging in a separate FreeRTOS task
//------------------------------------------------------------------------------
static void elk_task(void *pvParam) {
  // 1) Create Elk
  js = js_create(elk_memory, sizeof(elk_memory));
  if(!js) {
    Serial.println("Failed to initialize Elk in elk_task");
    // Delete this task if you want
    vTaskDelete(NULL);
    return;
  }

  // 2) Register bridging
  register_js_functions();

  // 3) Load & execute your script
  if(!load_and_execute_js_script("/script.js")) {
    Serial.println("Failed to load and execute JavaScript script");
  } else {
    Serial.println("Script executed successfully in elk_task");
  }

  // 4) Now keep running lv_timer_handler() or your lvgl_loop
  // so that the UI remains active
  for(;;) {
    lv_timer_handler();
    delay(5);
    // or lvgl_loop() if you prefer
  }

  // If you ever want to exit the task, do:
  // vTaskDelete(NULL);
}