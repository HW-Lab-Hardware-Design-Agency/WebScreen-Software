#pragma once
#include <lvgl.h>

// LVGL retains point arrays. Each JS-created line must own its own copy.
static bool webscreen_line_set_points(lv_obj_t *line, const lv_point_precise_t *points, uint32_t count) {
  if (!lv_obj_check_type(line, &lv_line_class) || count == 0 || count > 16) return false;
  auto *copy = (lv_point_precise_t *)lv_obj_get_user_data(line);
  if (!copy) {
    copy = (lv_point_precise_t *)lv_malloc(16 * sizeof(*copy));
    if (!copy) return false;
    lv_obj_set_user_data(line, copy);
    lv_obj_add_event_cb(line, [](lv_event_t *event) {
      lv_free(lv_event_get_user_data(event));
    }, LV_EVENT_DELETE, copy);
  }
  lv_obj_invalidate(line);
  lv_memcpy(copy, points, count * sizeof(*copy));
  lv_line_set_points(line, copy, count);
  return true;
}
