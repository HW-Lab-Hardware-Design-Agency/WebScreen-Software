#include <Arduino.h>
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include <stdio.h>

// 1) Pin definitions + config
#include "pins_config.h"

// 2) LVGL + display driver
#include <lvgl.h>
#include "rm67162.h"

// 4) Memory storage
struct RamImage {
  bool     used;          // Is this slot in use?
  uint8_t *buffer;        // Raw image data allocated from ps_malloc()
  size_t   size;          // Byte size of that buffer
  lv_img_dsc_t dsc;       // The descriptor we pass to lv_img_set_src()
};

// Adjust as needed
#define MAX_RAM_IMAGES  8

static RamImage g_ram_images[MAX_RAM_IMAGES];

// 4) Elk
extern "C" {
  #include "elk.h"
}

/******************************************************************************
 * A) Elk Memory + Global Instances
 ******************************************************************************/
static uint8_t elk_memory[4096];    // Increase if needed
struct js *js = NULL;               // Global Elk instance

/******************************************************************************
 * B) LVGL + Display
 ******************************************************************************/
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

  // 3) Strip any quotes, build full SD path "S:/filename" if needed
  String path = String(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }
  // We'll just do "/filename" for SD_MMC.open(...)
  // or if you prefer "S:/filename" you can remove the slash
  String filePath = path;  

  // 4) Actually load the file into the RamImage
  if (!load_image_file_into_ram(filePath.c_str(), ri)) {
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
                filePath.c_str(), slot, handle);
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
  js_set(js, global, "sd_read_file", js_mkfun(js_sd_read_file));
  js_set(js, global, "sd_write_file",js_mkfun(js_sd_write_file));
  js_set(js, global, "sd_list_dir",  js_mkfun(js_sd_list_dir));

  // GIF from memory
  js_set(js, global, "show_gif_from_sd", js_mkfun(js_show_gif_from_sd));

  // Basic shapes
  js_set(js, global, "draw_label",   js_mkfun(js_lvgl_draw_label));
  js_set(js, global, "draw_rect",    js_mkfun(js_lvgl_draw_rect));
  js_set(js, global, "show_image",   js_mkfun(js_lvgl_show_image));

  // Handle-based image creation + transforms
  js_set(js, global, "create_image", js_mkfun(js_create_image));
  js_set(js, global, "create_image_from_ram", js_mkfun(js_create_image_from_ram));
  js_set(js, global, "rotate_obj",   js_mkfun(js_rotate_obj));
  js_set(js, global, "move_obj",     js_mkfun(js_move_obj));
  js_set(js, global, "animate_obj",  js_mkfun(js_animate_obj));

  // ***NEW*** style creation + property setters
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

  // ***NEW*** object property setters
  js_set(js, global, "obj_set_size",  js_mkfun(js_obj_set_size));
  js_set(js, global, "obj_align",     js_mkfun(js_obj_align));
}

/******************************************************************************
 * J) Load + Execute JS from SD
 ******************************************************************************/
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
  //    - If it's a "raw" (non-LVGL-native) image, you might need an external decoder
  //    - For simplicity, let's assume it's a prepared "True color" or "RAW" format:
  lv_img_dsc_t *d = &outImg->dsc;
  memset(d, 0, sizeof(*d));

  // Basic mandatory fields:
  d->data_size       = fileSize;
  d->data            = buf;
  // If the file is EXACTLY LVGL's "true color" or "raw" format, you can do:
  d->header.always_zero = 0;
  d->header.w       = 200;
  d->header.h       = 200;
  d->header.cf      = LV_IMG_CF_TRUE_COLOR; 
  // or LV_IMG_CF_RAW if using a custom decoder

  // If you can't know width/height from file alone, you may default them or parse
  // For a PNG/JPG you'd typically use an external decoder to fill w,h

  Serial.println("Image loaded into PSRAM successfully");
  return true;
}

/******************************************************************************
 * K) Arduino setup() & loop()
 ******************************************************************************/
void setup() {
  Serial.begin(115200);
  delay(2000);

  // 1) Mount SD card
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
  if(!SD_MMC.begin("/sdcard", true, false, 1000000)) {
    Serial.println("Card Mount Failed");
    return;
  }

  // 2) Wi-Fi (station mode)
  WiFi.mode(WIFI_STA);

  // 3) Initialize LVGL display
  init_lvgl_display();

  // 4) 'S' driver for normal SD usage
  init_lv_fs();

  // 4b) 'M' driver for memory usage (GIF)
  init_mem_fs();

  // 4c) Init Memory storage
  init_ram_images();

  // 5) Create Elk
  js = js_create(elk_memory, sizeof(elk_memory));
  if(!js) {
    Serial.println("Failed to initialize Elk");
    return;
  }
  register_js_functions();

  // 6) Load + execute "/script.js" from SD
  if(!load_and_execute_js_script("/script.js")) {
    Serial.println("Failed to load and execute JavaScript script");
  }
}

void loop() {
  lvgl_loop();
  delay(5);
}
