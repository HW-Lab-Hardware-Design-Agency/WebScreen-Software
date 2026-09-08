#pragma once
#include "webscreen_js_args.h"

static jsval_t js_str_length(struct js *engine, jsval_t *args, int nargs) {
  size_t length = 0;
  if (nargs) js_getstr(engine, args[0], &length);
  return js_mknum((double)length);
}

// Byte offsets match Elk strings; a negative length means through the end.
static jsval_t js_str_substring(struct js *engine, jsval_t *args, int nargs) {
  if (nargs < 3) return js_mkstr(engine, "", 0);
  size_t size = 0;
  const char *data = js_getstr(engine, args[0], &size);
  if (!data) return js_mkstr(engine, "", 0);
  double requested_start = js_getnum(args[1]), requested_length = js_getnum(args[2]);
  if (js_type(args[1]) != JS_NUM || js_type(args[2]) != JS_NUM ||
      !std::isfinite(requested_start) || !std::isfinite(requested_length)) {
    return js_mkerr(engine, "substring offsets must be finite numbers");
  }
  size_t start = requested_start <= 0 ? 0 : requested_start >= (double)size ? size : (size_t)requested_start;
  size_t remaining = size - start;
  size_t length = requested_length < 0 || requested_length >= (double)remaining ? remaining : (size_t)requested_length;
  return js_mkstr(engine, data + start, length);
}
