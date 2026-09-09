#pragma once
#include <lvgl.h>

// Headerless RGB565 data must exactly match the descriptor dimensions.
static bool webscreen_raw_image_valid(size_t bytes, int width, int height) {
  return width > 0 && width <= 2047 && height > 0 && height <= 2047 &&
         bytes <= 2 * 1024 * 1024 && bytes == (size_t)width * height * sizeof(lv_color_t);
}

static bool webscreen_image_in_use(lv_obj_t *root, const lv_img_dsc_t *image) {
  if (!root) return false;
  if (lv_obj_has_class(root, &lv_img_class) && lv_img_get_src(root) == image) return true;
  if (lv_obj_has_class(root, &lv_meter_class)) {
    auto *meter = (lv_meter_t *)root;
    auto *indicator = (lv_meter_indicator_t *)_lv_ll_get_head(&meter->indicator_ll);
    for (; indicator; indicator = (lv_meter_indicator_t *)_lv_ll_get_next(&meter->indicator_ll, indicator)) {
      if (indicator->type == LV_METER_INDICATOR_TYPE_NEEDLE_IMG && indicator->type_data.needle_img.src == image) return true;
    }
  }
  for (uint32_t i = 0; i < lv_obj_get_child_cnt(root); i++) {
    if (webscreen_image_in_use(lv_obj_get_child(root, i), image)) return true;
  }
  return false;
}
