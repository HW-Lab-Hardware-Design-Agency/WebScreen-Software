// Fragment of lvgl_elk.h; uses the shared object registry.
#include <cmath>
#include <climits>
#include <cstring>

static bool ws_arc_label_integer(jsval_t arg, int32_t min, int32_t max, int32_t *value) {
  if (js_type(arg) != JS_NUM) return false;
  double number = js_getnum(arg);
  if (!std::isfinite(number) || number < min || number > max || std::trunc(number) != number) return false;
  *value = (int32_t)number;
  return true;
}

static lv_obj_t *ws_arc_label_object(jsval_t arg) {
  int32_t handle;
  if (!ws_arc_label_integer(arg, 0, INT32_MAX, &handle)) return nullptr;
  lv_obj_t *object = get_lv_obj(handle);
  if (!object || !lv_obj_is_valid(object) || !lv_obj_check_type(object, &lv_arclabel_class)) return nullptr;
  return object;
}

static bool ws_arc_label_text(struct js *js, jsval_t arg, char *buffer) {
  size_t length = 0;
  const char *text = js_getstr(js, arg, &length);
  if (!text || length > 255 || memchr(text, '\0', length)) return false;
  memcpy(buffer, text, length);
  buffer[length] = '\0';
  return true;
}

static bool ws_arc_label_replace_text(lv_obj_t *object, const char *text) {
  // Own a copy outside Elk's moving arena; allocation failure preserves the old text.
  size_t size = strlen(text) + 1;
  char *copy = (char *)lv_malloc(size);
  if (!copy) return false;
  memcpy(copy, text, size);
  void *previous = lv_obj_get_user_data(object);
  lv_arclabel_set_text_static(object, copy);
  lv_obj_set_user_data(object, copy);
  lv_free(previous);
  return true;
}

static jsval_t js_create_arc_label(struct js *js, jsval_t *args, int nargs) {
  int32_t x, y, radius;
  char text[256];
  if (nargs != 4 || !ws_arc_label_text(js, args[0], text) ||
      !ws_arc_label_integer(args[1], -4096, 4096, &x) ||
      !ws_arc_label_integer(args[2], -4096, 4096, &y) ||
      !ws_arc_label_integer(args[3], 8, 256, &radius)) return js_mknum(-1);

  lv_obj_t *object = lv_arclabel_create(lv_screen_active());
  if (!object) return js_mknum(-1);
  auto *event = lv_obj_add_event_cb(object, [](lv_event_t *event) {
    lv_obj_t *object = lv_event_get_target_obj(event);
    lv_free(lv_obj_get_user_data(object));
    lv_obj_set_user_data(object, nullptr);
  }, LV_EVENT_DELETE, nullptr);
  if (!event || !ws_arc_label_replace_text(object, text)) {
    lv_obj_delete(object);
    return js_mknum(-1);
  }

  // x/y are the bounding square's top-left; leave room for the glyphs outside the radius.
  lv_obj_set_size(object, 2 * (radius + 16), 2 * (radius + 16));
  lv_obj_set_pos(object, x, y);
  lv_arclabel_set_radius(object, radius);
  lv_arclabel_set_text_horizontal_align(object, LV_ARCLABEL_TEXT_ALIGN_CENTER);
  lv_arclabel_set_text_vertical_align(object, LV_ARCLABEL_TEXT_ALIGN_CENTER);
  lv_arclabel_set_angle_start(object, 210);
  lv_arclabel_set_angle_size(object, 120);
  int handle = store_lv_obj(object);
  if (handle < 0) lv_obj_delete(object);
  return js_mknum(handle);
}

static jsval_t js_arc_label_set_text(struct js *js, jsval_t *args, int nargs) {
  char text[256];
  if (nargs != 2 || !ws_arc_label_text(js, args[1], text)) return js_mkfalse();
  lv_obj_t *object = ws_arc_label_object(args[0]);
  return object && ws_arc_label_replace_text(object, text) ? js_mktrue() : js_mkfalse();
}

static jsval_t js_arc_label_set_angles(struct js *js, jsval_t *args, int nargs) {
  int32_t start, sweep;
  if (nargs != 3 || !ws_arc_label_integer(args[1], 0, 359, &start) ||
      !ws_arc_label_integer(args[2], 1, 360, &sweep)) return js_mkfalse();
  lv_obj_t *object = ws_arc_label_object(args[0]);
  if (!object) return js_mkfalse();
  lv_arclabel_set_angle_start(object, start);
  lv_arclabel_set_angle_size(object, sweep);
  return js_mktrue();
}

static jsval_t js_arc_label_set_direction(struct js *js, jsval_t *args, int nargs) {
  int32_t direction;
  if (nargs != 2 || !ws_arc_label_integer(args[1], 0, 1, &direction)) return js_mkfalse();
  lv_obj_t *object = ws_arc_label_object(args[0]);
  if (!object) return js_mkfalse();
  lv_arclabel_set_dir(object, (lv_arclabel_dir_t)direction);
  return js_mktrue();
}
