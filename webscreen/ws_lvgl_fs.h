// ws_lvgl_fs.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

/******************************************************************************
 * C) "S" Driver for Reading Files from SD
 ******************************************************************************/
typedef struct {
  File file;
} lv_arduino_fs_file_t;

#include <new>

static void *my_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode) {
  String fullPath = String("/") + path;
  const char *modeStr = (mode == LV_FS_MODE_WR) ? FILE_WRITE : FILE_READ;
  File f = SD_MMC.open(fullPath, modeStr);
  if (!f) {
    LOGF("my_open_cb: failed to open %s\n", fullPath.c_str());
    return NULL;
  }

  lv_arduino_fs_file_t *fp = new (std::nothrow) lv_arduino_fs_file_t();
  if (!fp) return NULL;
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

  return fp->file.seek(pos, m) ? LV_FS_RES_OK : LV_FS_RES_FS_ERR;
}

static lv_fs_res_t my_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p) {
  lv_arduino_fs_file_t *fp = (lv_arduino_fs_file_t *)file_p;
  if (!fp) return LV_FS_RES_INV_PARAM;
  *pos_p = fp->file.position();
  return LV_FS_RES_OK;
}
void init_lv_fs() {
  static bool registered = false;
  if (registered) return;
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
  registered = true;
  LOG("LVGL FS driver 'S' registered");
}

#include "ws_lvgl_mem_fs.h"
