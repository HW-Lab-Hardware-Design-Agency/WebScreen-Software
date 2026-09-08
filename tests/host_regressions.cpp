#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>
#include <lvgl.h>
#include <lvgl_private.h>
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
#include "ws_lvgl_chart_api.h"
#include "ws_lvgl_charts.h"

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

int main() {
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
  lv_display_delete(display);
  lv_deinit();
  puts("PASS: serial, configuration, timer deletion, line ownership, charts/meters, memory filesystem, RGB565 screenshots");
}
