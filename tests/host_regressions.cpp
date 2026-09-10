#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <fstream>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>
#include <lvgl.h>
#include <lvgl_private.h>
#include <src/widgets/arclabel/lv_arclabel_private.h>
#include "elk.h"
#include "pins_config.h"
static_assert(INPUT_PIN == 21, "Button must stay off the octal PSRAM bus");
#include "webscreen_serial_line.h"
#include "webscreen_config_parse.h"
#include "webscreen_snapshot.h"
#include "webscreen_line.h"

static_assert(LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR == 5, "Tests target LVGL 9.5");
#define LOG(...) ((void)0)
#define LOGF(...) ((void)0)
static uint32_t now_ms;
static uint32_t millis() { return now_ms; }
static struct { size_t getFreeHeap() { return 1024 * 1024; } } ESP;
static struct js *js;
static bool g_js_gc_requested;
static unsigned errors;
extern "C" void webscreen_runtime_request_restart(const char *) {}
extern "C" void webscreen_runtime_request_restart_auto(const char *) {}
extern "C" void webscreen_runtime_note_js_error(const char *) { errors++; }
#include "ws_elk_timers.h"
#include "ws_lvgl_mem_fs.h"

// The binding fragments use the runtime's object registry and RAM image table.
static std::vector<lv_obj_t *> objects;
static int store_lv_obj(lv_obj_t *object) {
  objects.push_back(object);
  return (int)objects.size() - 1;
}
static lv_obj_t *get_lv_obj(int handle) {
  return handle >= 0 && (size_t)handle < objects.size() ? objects[handle] : nullptr;
}
static const int MAX_RAM_IMAGES = 16;
static struct { bool used; lv_image_dsc_t dsc; } g_ram_images[MAX_RAM_IMAGES];
static const lv_font_t *get_font_for_size(int size) {
  if (size == 20) return &lv_font_montserrat_20;
  if (size == 28) return &lv_font_montserrat_28;
  if (size == 34) return &lv_font_montserrat_34;
  if (size == 48) return &lv_font_montserrat_48;
  return &lv_font_montserrat_14;
}
#include "ws_lvgl_styles.h"
#include "ws_lvgl_charts.h"
#include "ws_lvgl_arclabel.h"

using Binding = jsval_t (*)(struct js *, jsval_t *, int);
static jsval_t call(Binding binding, std::initializer_list<double> numbers) {
  std::vector<jsval_t> args;
  for (double number : numbers) args.push_back(js_mknum(number));
  return binding(js, args.data(), (int)args.size());
}
static void eval(const char *code) {
  jsval_t result = js_eval(js, code, strlen(code));
  if (js_type(result) == JS_ERR) fprintf(stderr, "%s\n", js_str(js, result));
  assert(js_type(result) != JS_ERR);
}
static unsigned timer_count() {
  unsigned count = 0;
  for (auto *timer = lv_timer_get_next(nullptr); timer; timer = lv_timer_get_next(timer)) {
    if (timer->timer_cb == elk_timer_cb) count++;
  }
  return count;
}

static void test_serial_and_configuration() {
  WebscreenSerialLine<8> input;
  for (char c : {'/', 'h', 'e', 'l', 'p', '\r'}) assert(input.push(c) == input.Pending);
  assert(input.push('\n') == input.Ready);
  assert(strcmp(input.data(), "/help") == 0);
  for (int i = 0; i < 10000; i++) assert(input.push('x') == input.Pending);
  assert(input.push('\n') == input.Overflow);
  for (char c : {'/', 's', 't', 'a', 't', 's'}) input.push(c);
  assert(input.push('\n') == input.Ready);
  assert(strcmp(input.data(), "/stats") == 0);
  assert(input.push('\n') == input.Ready && input.data()[0] == '\0');
  assert(webscreen_parse_color("#ABC", 0) == 0xAABBCC);
  assert(webscreen_parse_color("#12abEF", 0) == 0x12ABEF);
  for (const char *bad : {(const char *)nullptr, "", "#", "123456", "#1", "#12zz56", "#1234567"}) {
    assert(webscreen_parse_color(bad, 0x123456) == 0x123456);
  }
}

static void test_timers() {
  js_set(js, js_glob(js), "create_timer", js_mkfun(js_create_timer));
  js_set(js, js_glob(js), "timer_delete", js_mkfun(js_timer_delete));
  eval("let once = function() { timer_delete('once'); }; create_timer('once', 1);");
  assert(timer_count() == 1);
  now_ms += 5;
  lv_timer_handler();
  assert(timer_count() == 0);
  // The error path used to access the freed context too.
  eval("let failing = function() { timer_delete('failing'); missing(); }; create_timer('failing', 1);");
  now_ms += 5;
  lv_timer_handler();
  assert(timer_count() == 0);
  eval("create_timer('once', 0); create_timer('once', -1); create_timer('once', 1e20);");
  assert(timer_count() == 0);
  eval("let normal = function() {}; create_timer('normal', 1);");
  now_ms += 5;
  lv_timer_handler();
  assert(timer_count() == 1 && errors == 1);
  delete_all_elk_timers();
}

static void test_lines() {
  lv_obj_t *first = lv_line_create(lv_scr_act());
  lv_obj_t *second = lv_line_create(lv_scr_act());
  lv_point_precise_t points[] = {{1, 2}, {30, 40}};
  assert(webscreen_line_set_points(first, points, 2));
  points[0] = {90, 100};
  assert(webscreen_line_set_points(second, points, 2));
  auto *line = (lv_line_t *)first;
  assert(line->point_array.constant[0].x == 1);
  points[0] = {10, 20};
  assert(webscreen_line_set_points(first, points, 2));
  assert(((lv_line_t *)second)->point_array.constant[0].x == 90);
  assert(!webscreen_line_set_points(first, points, 17));
  lv_obj_delete(first);
  lv_obj_delete(second);
}

static void test_charts_and_meters() {
  int first = (int)js_getnum(call(js_lv_chart_create, {}));
  int second = (int)js_getnum(call(js_lv_chart_create, {}));
  call(js_lv_chart_set_type, {(double)first, 2});
  assert(lv_chart_get_type(get_lv_obj(first)) == LV_CHART_TYPE_BAR);
  call(js_lv_chart_set_type, {(double)first, 3});
  assert(lv_chart_get_type(get_lv_obj(first)) == LV_CHART_TYPE_SCATTER);
  call(js_lv_chart_set_type, {(double)second, 4});
  assert(lv_chart_get_type(get_lv_obj(second)) == LV_CHART_TYPE_CURVE);
  call(js_lv_chart_set_type, {(double)second, 5});
  assert(lv_chart_get_type(get_lv_obj(second)) == LV_CHART_TYPE_CURVE);
  call(js_lv_chart_set_point_count, {(double)first, 4});
  call(js_lv_chart_set_point_count, {(double)second, 1024});
  int series = (int)js_getnum(call(js_lv_chart_add_series, {(double)first, 0xffffff, 0}));
  auto *values = lv_chart_get_y_array(get_lv_obj(first), get_chart_series(series));
  call(js_lv_chart_set_next_value, {(double)second, (double)series, 42});
  assert(values[0] == LV_CHART_POINT_NONE);
  call(js_lv_chart_set_next_value2, {(double)first, (double)series, 1, 42});
  assert(values[0] == 42);
  call(js_lv_chart_set_point_count, {(double)first, -1});
  assert(lv_chart_get_point_count(get_lv_obj(first)) == 4);
  int label = store_lv_obj(lv_label_create(lv_scr_act()));
  call(js_lv_chart_set_type, {(double)label, 2});

  int meter = (int)js_getnum(call(js_lv_meter_create, {}));
  int scale = (int)js_getnum(call(js_lv_meter_add_scale, {(double)meter}));
  call(js_lv_meter_set_scale_ticks, {(double)meter, (double)scale, 1, 1, 10, 0});
  assert(lv_scale_get_total_tick_count(get_lv_obj(meter)) >= 2);
  call(js_lv_meter_set_scale_range, {(double)meter, (double)scale, 10, 10, 270, 135});
  assert(g_meter_scales[scale]->min == 0 && g_meter_scales[scale]->max == 100);
  assert(get_meter_scale(scale, get_lv_obj(first)) == nullptr);
  int indicator = (int)js_getnum(call(js_lv_meter_add_needle_line, {(double)meter, (double)scale, 2, 0xffffff, -10}));
  assert(indicator >= 0);
  call(js_lv_meter_set_indicator_value, {(double)meter, (double)indicator, 50});
  call(js_lv_meter_set_indicator_value, {(double)first, (double)indicator, 10});
  assert(g_meter_indicators[indicator]->end == 50);
  release_subobjects_owned_by(get_lv_obj(meter));
  assert(get_meter_scale(scale, get_lv_obj(meter)) == nullptr);
  release_subobjects_owned_by(lv_scr_act());
  lv_obj_clean(lv_scr_act());
  objects.clear();
}

static void test_memory_filesystem() {
  init_mem_fs();
  init_mem_fs();
  char letters[32] = {};
  lv_fs_get_letters(letters);
  unsigned registrations = 0;
  for (char c : letters) if (c == 'M') registrations++;
  assert(registrations == 1);
  assert(my_mem_open_cb(nullptr, "gif", LV_FS_MODE_RD) == nullptr);
  uint8_t content[] = {1, 2, 3, 4, 5};
  g_gifBuffer = content;
  g_gifSize = sizeof(content);
  auto *file = my_mem_open_cb(nullptr, "gif", LV_FS_MODE_RD);
  assert(file);
  assert(my_mem_seek_cb(nullptr, file, (uint32_t)-2, LV_FS_SEEK_END) == LV_FS_RES_OK);
  uint8_t result[8] = {};
  uint32_t read = 0;
  assert(my_mem_read_cb(nullptr, file, result, sizeof(result), &read) == LV_FS_RES_OK);
  assert(read == 2 && result[0] == 4 && result[1] == 5);
  assert(my_mem_seek_cb(nullptr, file, (uint32_t)-10, LV_FS_SEEK_CUR) == LV_FS_RES_INV_PARAM);
  g_gifSize = 1;
  assert(my_mem_read_cb(nullptr, file, result, sizeof(result), &read) == LV_FS_RES_OK && read == 0);
  my_mem_close_cb(nullptr, file);
  g_gifBuffer = nullptr;
  g_gifSize = 0;
}

static void test_screenshot() {
  // Padding bytes must not appear in the packed serial payload, even across chunks.
  uint8_t pixels[] = {1, 2, 3, 4, 99, 99, 5, 6, 7, 8, 99, 99};
  lv_draw_buf_t snapshot = {};
  snapshot.header.w = 2;
  snapshot.header.h = 2;
  snapshot.header.stride = 6;
  snapshot.data = pixels;
  snapshot.data_size = sizeof(pixels);
  uint8_t packed[8];
  assert(webscreen_snapshot_read(&snapshot, 0, packed, 5) == 5);
  assert(webscreen_snapshot_read(&snapshot, 5, packed + 5, 3) == 3);
  for (size_t i = 0; i < sizeof(packed); i++) assert(packed[i] == i + 1);
  assert(webscreen_snapshot_read(&snapshot, 8, packed, 1) == 0);

  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xff0000), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
  lv_obj_update_layout(lv_scr_act());
  std::vector<uint8_t> storage(LV_DRAW_BUF_SIZE(536, 240, LV_COLOR_FORMAT_RGB565));
  assert(lv_draw_buf_init(&snapshot, 536, 240, LV_COLOR_FORMAT_RGB565,
                         LV_STRIDE_AUTO, storage.data(), storage.size()) == LV_RESULT_OK);
  assert(lv_snapshot_take_to_draw_buf(lv_scr_act(), LV_COLOR_FORMAT_RGB565, &snapshot) == LV_RESULT_OK);
  assert(webscreen_snapshot_read(&snapshot, 0, packed, 2) == 2);
  assert(packed[0] == 0 && packed[1] == 0xf8);  // Little-endian RGB565 red.
  size_t count = 0;
  uint8_t chunk[57];
  while (size_t n = webscreen_snapshot_read(&snapshot, count, chunk, sizeof(chunk))) count += n;
  assert(count == 536 * 240 * 2);
}

static void register_arc_labels() {
  js_set(js, js_glob(js), "create_arc_label", js_mkfun(js_create_arc_label));
  js_set(js, js_glob(js), "arc_label_set_text", js_mkfun(js_arc_label_set_text));
  js_set(js, js_glob(js), "arc_label_set_angles", js_mkfun(js_arc_label_set_angles));
  js_set(js, js_glob(js), "arc_label_set_direction", js_mkfun(js_arc_label_set_direction));
}

static void test_arc_labels() {
  register_arc_labels();
  eval("let curved = create_arc_label('Curved text', 10, 20, 86);");
  int handle = (int)js_getnum(js_eval(js, "curved;", 7));
  lv_obj_t *object = get_lv_obj(handle);
  assert(object && lv_obj_check_type(object, &lv_arclabel_class));
  assert(lv_arclabel_get_radius(object) == 86);
  assert(strcmp((char *)lv_obj_get_user_data(object), "Curved text") == 0);
  eval("arc_label_set_text(curved, 'Updated ' + 'text');");
  js_gc(js);
  assert(strcmp(((lv_arclabel_t *)object)->text, "Updated text") == 0);
  assert(js_getbool(call(js_arc_label_set_angles, {(double)handle, 270, 180})));
  assert(lv_arclabel_get_angle_start(object) == 270);
  assert(lv_arclabel_get_angle_size(object) == 180);
  assert(js_getbool(call(js_arc_label_set_direction, {(double)handle, 1})));
  assert(lv_arclabel_get_dir(object) == LV_ARCLABEL_DIR_COUNTER_CLOCKWISE);

  for (double bad : {-1.0, 360.0, 1.5, 1e30, std::numeric_limits<double>::infinity()}) {
    assert(!js_getbool(call(js_arc_label_set_angles, {(double)handle, bad, 180})));
  }
  assert(!js_getbool(call(js_arc_label_set_angles, {(double)handle, 0, 0})));
  assert(!js_getbool(call(js_arc_label_set_direction, {(double)handle, 2})));
  assert(lv_arclabel_get_angle_start(object) == 270);
  assert(lv_arclabel_get_angle_size(object) == 180);
  assert(js_getnum(js_eval(js, "create_arc_label('bad', 0, 0, 0);", ~0U)) == -1);
  assert(js_getnum(js_eval(js, "create_arc_label(123, 0, 0, 50);", ~0U)) == -1);
  std::string too_long(256, 'x');
  jsval_t text_args[] = {js_mknum(handle), js_mkstr(js, too_long.data(), too_long.size())};
  assert(!js_getbool(js_arc_label_set_text(js, text_args, 2)));
  assert(strcmp(((lv_arclabel_t *)object)->text, "Updated text") == 0);

  int ordinary = store_lv_obj(lv_label_create(lv_screen_active()));
  assert(!js_getbool(call(js_arc_label_set_angles, {(double)ordinary, 0, 180})));
  lv_obj_delete(object);
  objects[handle] = nullptr;
  assert(!js_getbool(call(js_arc_label_set_angles, {(double)handle, 0, 180})));
  lv_obj_clean(lv_screen_active());
  objects.clear();
}

// Host adapters for platform-dependent bindings; UI styles, charts, meters,
// arc labels and timer dispatch below use the production binding fragments.
static std::string button_callback;
static jsval_t host_draw_rect(struct js *, jsval_t *args, int) {
  lv_obj_t *object = lv_obj_create(lv_screen_active());
  lv_obj_set_pos(object, js_getnum(args[0]), js_getnum(args[1]));
  lv_obj_set_size(object, js_getnum(args[2]), js_getnum(args[3]));
  lv_obj_set_style_bg_color(object, lv_color_hex(js_getnum(args[4])), 0);
  lv_obj_set_style_radius(object, 5, 0);
  return js_mknum(store_lv_obj(object));
}
static jsval_t host_move(struct js *, jsval_t *args, int) {
  lv_obj_set_pos(get_lv_obj(js_getnum(args[0])), js_getnum(args[1]), js_getnum(args[2]));
  return js_mknull();
}
static jsval_t host_button(struct js *engine, jsval_t *args, int) {
  size_t size = 0;
  const char *name = js_getstr(engine, args[0], &size);
  button_callback.assign(name, size);
  return js_mktrue();
}
static jsval_t host_number(struct js *engine, jsval_t *args, int) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%.0f", js_getnum(args[0]));
  return js_mkstr(engine, buffer, strlen(buffer));
}
static jsval_t host_print(struct js *, jsval_t *, int) { return js_mknull(); }

static void register_showcase_bindings() {
  register_arc_labels();
  struct { const char *name; Binding binding; } bindings[] = {
    {"create_label", js_create_label}, {"label_set_text", js_label_set_text},
    {"create_style", js_create_style}, {"obj_add_style", js_obj_add_style},
    {"obj_set_size", js_obj_set_size}, {"style_set_text_font", js_style_set_text_font},
    {"obj_align", js_obj_align},
    {"style_set_text_color", js_style_set_text_color}, {"style_set_text_align", js_style_set_text_align},
    {"style_set_radius", js_style_set_radius}, {"style_set_border_width", js_style_set_border_width},
    {"style_set_pad_all", js_style_set_pad_all}, {"style_set_bg_opa", js_style_set_bg_opa},
    {"style_set_bg_color", js_style_set_bg_color}, {"lv_meter_create", js_lv_meter_create},
    {"lv_meter_add_scale", js_lv_meter_add_scale}, {"lv_meter_set_scale_ticks", js_lv_meter_set_scale_ticks},
    {"lv_meter_set_scale_major_ticks", js_lv_meter_set_scale_major_ticks},
    {"lv_meter_set_scale_range", js_lv_meter_set_scale_range}, {"lv_meter_add_arc", js_lv_meter_add_arc},
    {"lv_meter_set_indicator_start_value", js_lv_meter_set_indicator_start_value},
    {"lv_meter_set_indicator_end_value", js_lv_meter_set_indicator_end_value},
    {"lv_meter_add_needle_line", js_lv_meter_add_needle_line},
    {"lv_meter_set_indicator_value", js_lv_meter_set_indicator_value},
    {"lv_chart_create", js_lv_chart_create}, {"lv_chart_set_type", js_lv_chart_set_type},
    {"lv_chart_set_point_count", js_lv_chart_set_point_count},
    {"lv_chart_set_div_line_count", js_lv_chart_set_div_line_count},
    {"lv_chart_set_range", js_lv_chart_set_range}, {"lv_chart_add_series", js_lv_chart_add_series},
    {"lv_chart_set_next_value", js_lv_chart_set_next_value}, {"create_timer", js_create_timer},
    {"lv_chart_set_next_value2", js_lv_chart_set_next_value2},
    {"lv_line_create", js_lv_line_create}, {"lv_line_set_points", js_lv_line_set_points},
    {"style_set_line_width", js_style_set_line_width}, {"style_set_line_color", js_style_set_line_color},
    {"style_set_line_rounded", js_style_set_line_rounded},
    {"draw_rect", host_draw_rect}, {"move_obj", host_move}, {"on_button", host_button},
    {"numberToString", host_number}, {"print", host_print}
  };
  for (auto &binding : bindings) js_set(js, js_glob(js), binding.name, js_mkfun(binding.binding));
}

static void capture_showcase(const std::string &path) {
  lv_obj_update_layout(lv_screen_active());
  std::vector<uint8_t> storage(LV_DRAW_BUF_SIZE(536, 240, LV_COLOR_FORMAT_RGB565));
  lv_draw_buf_t snapshot;
  assert(lv_draw_buf_init(&snapshot, 536, 240, LV_COLOR_FORMAT_RGB565,
                         LV_STRIDE_AUTO, storage.data(), storage.size()) == LV_RESULT_OK);
  assert(lv_snapshot_take_to_draw_buf(lv_screen_active(), LV_COLOR_FORMAT_RGB565, &snapshot) == LV_RESULT_OK);
  std::ofstream image(path, std::ios::binary);
  assert(image);
  image << "P6\n536 240\n255\n";
  for (size_t y = 0; y < 240; y++) {
    for (size_t x = 0; x < 536; x++) {
      uint8_t bytes[2];
      assert(webscreen_snapshot_read(&snapshot, (y * 536 + x) * 2, bytes, 2) == 2);
      uint16_t pixel = bytes[0] | (uint16_t(bytes[1]) << 8);
      char rgb[] = {char(((pixel >> 11) & 31) * 255 / 31),
                    char(((pixel >> 5) & 63) * 255 / 63), char((pixel & 31) * 255 / 31)};
      image.write(rgb, 3);
    }
  }
}

static void test_showcase(const char *script_path, const char *output_prefix) {
  std::ifstream input(script_path);
  assert(input);
  std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  alignas(8) static char app_arena[256 * 1024];
  for (int run = 0; run < 3; run++) {
    js = js_create(app_arena, sizeof(app_arena));
    button_callback.clear();
    register_showcase_bindings();
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    unsigned previous_errors = errors;
    eval(source.c_str());
    size_t object_count = objects.size();
    assert(timer_count() == 1 && button_callback == "lab_button");
    int chart = js_getnum(js_eval(js, "graph;", 6));
    lv_obj_update_layout(lv_screen_active());
    lv_area_t chart_area;
    lv_obj_get_coords(get_lv_obj(chart), &chart_area);
    assert(chart_area.x1 == 249 && chart_area.y1 == 78);
    assert(chart_area.x2 < 536 && chart_area.y2 < 240);
    assert(lv_chart_get_type(get_lv_obj(chart)) == LV_CHART_TYPE_CURVE);
    for (int phase = 0; phase < 3; phase++) {
      double before = js_getnum(js_eval(js, "turns;", 6));
      for (int tick = 0; tick < 300; tick++) {
        if (tick % 75 == 0) g_js_gc_requested = true;
        now_ms += 80;
        lv_timer_handler();
      }
      double after = js_getnum(js_eval(js, "turns;", 6));
      assert((phase == 2) == (before == after));
      assert(errors == previous_errors && objects.size() == object_count);
      assert(timer_count() == 1);
      if (run == 0) capture_showcase(std::string(output_prefix) + "-" + std::to_string(phase) + ".ppm");
      eval("lab_button(1);");
      assert(lv_chart_get_type(get_lv_obj(chart)) == (phase == 2 ? LV_CHART_TYPE_CURVE : LV_CHART_TYPE_BAR));
    }
    delete_all_elk_timers();
    release_subobjects_owned_by(lv_screen_active());
    lv_obj_clean(lv_screen_active());
    objects.clear();
    for (auto *&style : g_style_map) {
      if (style) { lv_style_reset(style); delete style; style = nullptr; }
    }
  }
}

static void test_demo_pack(const std::filesystem::path &directory, const std::filesystem::path &output) {
  std::vector<std::filesystem::path> scripts;
  for (auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().extension() == ".js") scripts.push_back(entry.path());
  }
  assert(scripts.size() == 5);
  std::sort(scripts.begin(), scripts.end());
  alignas(8) static char arena[256 * 1024];
  for (auto &script : scripts) {
    std::ifstream input(script);
    std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    for (int run = 0; run < 2; run++) {
      js = js_create(arena, sizeof(arena));
      js_setgct(js, sizeof(arena) / 4 * 3);
      js_setmaxsteps(js, 2 * 1000 * 1000);
      register_showcase_bindings();
      button_callback.clear();
      g_js_gc_requested = false;
      unsigned previous_errors = errors;
      eval(source.c_str());
      assert(button_callback == "demo_button");
      size_t object_count = objects.size();
      unsigned timers = timer_count();
      assert(timers <= 1);
      int modes = js_getnum(js_eval(js, "demo_modes;", ~0U));
      assert(modes >= 2 && modes <= 5);
      for (int mode = 0; mode < modes; mode++) {
        bool paused = js_getbool(js_eval(js, "demo_paused;", ~0U));
        double before = js_getnum(js_eval(js, "demo_frame;", ~0U));
        for (int tick = 0; tick < 437; tick++) {
          // Match explicit /gc and automatic timer collection on the board.
          if (tick % 71 == 0) js_gc(js);
          now_ms += 250;
          lv_timer_handler();
        }
        double after = js_getnum(js_eval(js, "demo_frame;", ~0U));
        assert(paused ? before == after : after > before);
        assert(errors == previous_errors);
        assert(objects.size() == object_count && timer_count() == timers);
        if (run == 0) {
          capture_showcase((output / (script.stem().string() + "-" + std::to_string(mode) + ".ppm")).string());
        }
        eval("demo_button(1);");
        assert(js_getnum(js_eval(js, "demo_mode;", ~0U)) == (mode + 1) % modes);
      }
      delete_all_elk_timers();
      release_subobjects_owned_by(lv_screen_active());
      lv_obj_clean(lv_screen_active());
      objects.clear();
      for (auto *&style : g_style_map) {
        if (style) { lv_style_reset(style); delete style; style = nullptr; }
      }
    }
    printf("PASS: %s (all modes, repeated loads, GC, snapshots)\n", script.filename().c_str());
  }
}

int main(int argc, char **argv) {
  test_serial_and_configuration();
  lv_init();
  lv_tick_set_cb(millis);
  auto *display = lv_display_create(536, 240);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565_SWAPPED);
  static uint8_t draw_pixels[536 * 40 * 2];
  lv_display_set_buffers(display, draw_pixels, nullptr, sizeof(draw_pixels), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, [](lv_display_t *disp, const lv_area_t *, uint8_t *) { lv_display_flush_ready(disp); });
  alignas(8) static char arena[256 * 1024];
  js = js_create(arena, sizeof(arena));
  assert(js);
  test_timers();
  test_lines();
  test_charts_and_meters();
  test_memory_filesystem();
  test_screenshot();
  test_arc_labels();
  assert(argc == 4);
  test_showcase(argv[1], argv[2]);
  test_demo_pack(argv[3], std::filesystem::path(argv[2]).parent_path());
  lv_display_delete(display);
  lv_deinit();
  puts("PASS: serial, configuration, timers, lines, charts/meters, filesystem, screenshots, arc labels, LVGL Lab");
}
