#pragma once
#include <cmath>
#include <cstdint>
#include "elk.h"

// Validate before native integer casts. Elk's nonnumeric values are NaN-boxed.
template<jsval_t (*Binding)(struct js *, jsval_t *, int), uint64_t NumericMask = UINT64_MAX>
static jsval_t webscreen_checked_binding(struct js *engine, jsval_t *args, int nargs) {
  for (int i = 0; i < nargs && i < 64; i++) {
    if (!(NumericMask & (UINT64_C(1) << i))) continue;
    if (js_type(args[i]) == JS_TRUE || js_type(args[i]) == JS_FALSE) {
      args[i] = js_mknum(js_getbool(args[i]));
    }
    double value = js_getnum(args[i]);
    if (js_type(args[i]) != JS_NUM || !std::isfinite(value) ||
        value < INT32_MIN || value > INT32_MAX) {
      return js_mkerr(engine, "argument %d must be a finite 32-bit number", i + 1);
    }
  }
  return Binding(engine, args, nargs);
}

static bool webscreen_callback_name(const char *name, size_t length) {
  if (!name || !length || length > 55) return false;
  for (size_t i = 0; i < length; i++) {
    unsigned char c = name[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$') continue;
    if (i && c >= '0' && c <= '9') continue;
    return false;
  }
  return true;
}
