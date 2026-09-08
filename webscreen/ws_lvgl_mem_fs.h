// Memory GIF filesystem, included by ws_lvgl_fs.h.
#include <new>
/******************************************************************************
 * D) "M" Memory Driver (for GIF usage)
 ******************************************************************************/
typedef struct {
  size_t pos;
} mem_file_t;

static uint8_t *g_gifBuffer = NULL;
static size_t g_gifSize = 0;

static void *my_mem_open_cb(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode) {
  if (mode != LV_FS_MODE_RD || !g_gifBuffer || !g_gifSize) return NULL;
  mem_file_t *mf = new (std::nothrow) mem_file_t();
  if (!mf) return NULL;
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

  // LVGL represents relative negative offsets as uint32_t (e.g. GIF rewinds).
  int64_t newpos;
  if (whence == LV_FS_SEEK_SET) newpos = pos;
  else if (whence == LV_FS_SEEK_CUR) newpos = (int64_t)mf->pos + (int32_t)pos;
  else if (whence == LV_FS_SEEK_END) newpos = (int64_t)g_gifSize + (int32_t)pos;
  else return LV_FS_RES_INV_PARAM;
  if (newpos < 0 || (uint64_t)newpos > g_gifSize) return LV_FS_RES_INV_PARAM;
  mf->pos = (size_t)newpos;
  return LV_FS_RES_OK;
}

static lv_fs_res_t my_mem_tell_cb(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p) {
  mem_file_t *mf = (mem_file_t *)file_p;
  if (!mf) return LV_FS_RES_INV_PARAM;
  *pos_p = mf->pos;
  return LV_FS_RES_OK;
}
void init_mem_fs() {
  static bool registered = false;
  if (registered) return;
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
  registered = true;
  LOG("LVGL FS driver 'M' registered (for memory-based GIFs)");
}
