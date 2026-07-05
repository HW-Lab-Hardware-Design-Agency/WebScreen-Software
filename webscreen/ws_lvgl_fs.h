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

  // A stale reader can hold pos beyond g_gifSize after a smaller GIF replaces
  // a larger one; without this check (g_gifSize - pos) underflows to ~SIZE_MAX
  // and the memcpy below reads far past the buffer.
  if (g_gifBuffer == NULL || mf->pos >= g_gifSize) {
    *br = 0;
    return LV_FS_RES_OK;
  }
  size_t remaining = g_gifSize - mf->pos;
  if (btr > remaining) btr = (uint32_t)remaining;

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

