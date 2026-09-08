#pragma once
#include "elk.h"

static constexpr uint32_t WEBSCREEN_JS_CALLBACK_TIMEOUT_MS = 5000;
static constexpr uint32_t WEBSCREEN_JS_STARTUP_TIMEOUT_MS = 10000;
static constexpr uint32_t WEBSCREEN_JS_EVAL_TIMEOUT_MS = 1000;

struct WebscreenJsDeadline {
  int64_t expires_us;
  int64_t (*clock_us)();
  bool (*cancelled)();
};

static const char *webscreen_js_interrupt(void *context) {
  auto *deadline = (WebscreenJsDeadline *)context;
  if (deadline->cancelled && deadline->cancelled()) return "app restart requested";
  return deadline->clock_us() >= deadline->expires_us ? "execution timeout" : nullptr;
}

static jsval_t webscreen_eval_with_deadline(struct js *engine, const char *code, size_t length,
                                           uint32_t budget_ms, int64_t (*clock_us)(), bool (*cancelled)()) {
  WebscreenJsDeadline deadline{clock_us() + (int64_t)budget_ms * 1000, clock_us, cancelled};
  js_setinterrupt(engine, webscreen_js_interrupt, &deadline);
  jsval_t result = js_eval(engine, code, length);
  // Check after the final native call too; native network calls are synchronous.
  if (js_type(result) != JS_ERR) {
    jsval_t interrupted = js_checkinterrupt(engine);
    if (js_type(interrupted) == JS_ERR) result = interrupted;
  }
  js_setinterrupt(engine, nullptr, nullptr);
  return result;
}
