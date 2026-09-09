// ws_elk_util.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
#include "webscreen_js_strings.h"

/******************************************************************************
 * A2) Shared binding helpers
 *
 * The bridge repeats two patterns in dozens of bindings; both live here so
 * they exist exactly once:
 *
 *  - js_arg_str(): JS string argument -> Arduino String. js_str() stringifies
 *    string VALUES with surrounding quotes, so naive use produces paths like
 *    "\"/app.js\"" — historically each binding hand-stripped the quotes (or
 *    forgot to, which is how path bugs were born). js_getstr() gives the raw
 *    bytes for real strings; js_str() remains the fallback so non-string
 *    values keep stringifying exactly as before.
 ******************************************************************************/
static String js_arg_str(struct js *js, jsval_t v) {
  size_t len = 0;
  char *p = js_getstr(js, v, &len);  // Raw bytes, only for actual JS strings
  if (p != NULL) {
    return String(p, (unsigned int)len);  // Copy now — the arena may move on GC
  }
  const char *raw = js_str(js, v);  // Stringified non-string value (no quotes)
  if (raw == NULL) {
    return String();
  }
  String s(raw);
  if (s.length() >= 2 && s.startsWith("\"") && s.endsWith("\"")) {
    s = s.substring(1, s.length() - 1);
  }
  return s;
}
