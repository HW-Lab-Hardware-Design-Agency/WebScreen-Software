#pragma once
#include <lvgl.h>

// LVGL 8.3 leaves data_size unset in lv_snapshot_take_to_buf().
static bool webscreen_snapshot_take(lv_obj_t *screen, lv_img_dsc_t *snapshot,
                                    void *pixels, uint32_t size) {
  if (!pixels || lv_snapshot_take_to_buf(screen, LV_IMG_CF_TRUE_COLOR, snapshot,
                                        pixels, size) != LV_RES_OK) return false;
  snapshot->data_size = (uint32_t)snapshot->header.w * snapshot->header.h * sizeof(lv_color_t);
  return true;
}
