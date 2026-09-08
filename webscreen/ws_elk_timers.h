// Elk timers: included by ws_elk_basics.h on the LVGL-owning task.
#include <math.h>
#include "webscreen_js_args.h"
#include "webscreen_js_execution.h"

// In webscreen_runtime.cpp: restart the app in place, never a reboot; _auto keeps safe mode and counts toward the give-up ladder.
extern "C" void webscreen_runtime_request_restart(const char *reason);
extern "C" void webscreen_runtime_request_restart_auto(const char *reason);
extern "C" void webscreen_runtime_note_js_error(const char *msg);
extern "C" jsval_t webscreen_runtime_eval_guarded(const char *code, size_t length, uint32_t budget_ms);

struct ElkTimerCtx {
  bool executing;
  bool deleted;
  uint32_t streak;  // Consecutive failed evals of THIS timer's function
  char name[56];    // JS function name to call
};

// Consecutive failed evals of one timer before requesting an in-place app restart.
static const uint32_t JS_ERROR_STREAK_LIMIT = 10;
static const unsigned WEBSCREEN_MAX_JS_TIMERS = 32;

static uint32_t g_js_error_streak = 0;

static void release_elk_timer_context(ElkTimerCtx *ctx) {
  if (!ctx) return;
  if (ctx->executing) ctx->deleted = true;
  else free(ctx);
}

static void elk_timer_cb(lv_timer_t *timer) {
  ElkTimerCtx *ctx = (ElkTimerCtx *)timer->user_data;
  if (ctx == NULL || js == NULL) return;
  const char *func_name = ctx->name;

  // Low internal DRAM (shared with TLS/lwip/LVGL): shed this tick; js_gc() only compacts the PSRAM arena.
  size_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < 20000) {
    static uint32_t lastWarning = 0;
    uint32_t now = millis();
    if (now - lastWarning > 5000) {  // Warn every 5 seconds max
      LOGF("[TIMER CB] Low internal heap (%u bytes) — skipping JS tick\n", freeHeap);
      lastWarning = now;
    }
    return;
  }

  // Between evals is a GC-safe point (no Elk C frames on the stack).
  if (g_js_gc_requested || js_usage(js) > (js_total(js) / 4) * 3) {
    g_js_gc_requested = false;
    js_gc(js);
  }

  char snippet[64];
  snprintf(snippet, sizeof(snippet), "%s();", func_name);

  ctx->executing = true;
  jsval_t res = webscreen_runtime_eval_guarded(snippet, strlen(snippet), WEBSCREEN_JS_CALLBACK_TIMEOUT_MS);
  ctx->executing = false;
  // timer_delete() may have removed this timer while its JS callback ran.
  if (ctx->deleted) {
    if (js_type(res) == JS_ERR) {
      char message[128];
      snprintf(message, sizeof(message), "timer %s: %s", ctx->name, js_str(js, res));
      webscreen_runtime_note_js_error(message);
    }
    free(ctx);
    return;
  }
  if (js_type(res) == JS_ERR) {
    ctx->streak++;
    const char *errstr = js_str(js, res);
    LOGF("[TIMER CB] Error in %s (streak %u/%u): %s | arena %u/%u, heap %u\n",
         snippet, (unsigned)ctx->streak, (unsigned)JS_ERROR_STREAK_LIMIT, errstr,
         (unsigned)js_usage(js), (unsigned)js_total(js), (unsigned)ESP.getFreeHeap());
    {
      char rec[128];
      snprintf(rec, sizeof(rec), "timer %s: %s", ctx->name, errstr ? errstr : "?");
      webscreen_runtime_note_js_error(rec);
    }
    if (ctx->streak == 1) {
      js_gc(js);
    }
    if (ctx->streak >= JS_ERROR_STREAK_LIMIT) {
      ctx->streak = 0;
      webscreen_runtime_request_restart_auto("repeated JS timer errors");
    }
  } else {
    ctx->streak = 0;
  }
}

// Delete every create_timer() timer and free its malloc'd context.
static void delete_all_elk_timers() {
  lv_timer_t *t = lv_timer_get_next(NULL);
  while (t != NULL) {
    lv_timer_t *next = lv_timer_get_next(t);
    if (t->timer_cb == elk_timer_cb) {
      release_elk_timer_context((ElkTimerCtx *)t->user_data);
      lv_timer_del(t);
    }
    t = next;
  }
}

// timer_delete("fname") => delete the timer created with create_timer("fname", ms).
static jsval_t js_timer_delete(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  size_t len = 0;
  char *name = js_getstr(js, args[0], &len);
  if (!name || len == 0) return js_mkfalse();

  lv_timer_t *t = lv_timer_get_next(NULL);
  while (t != NULL) {
    lv_timer_t *next = lv_timer_get_next(t);
    ElkTimerCtx *ctx = (ElkTimerCtx *)t->user_data;
    if (t->timer_cb == elk_timer_cb && ctx != NULL &&
        strlen(ctx->name) == len && memcmp(ctx->name, name, len) == 0) {
      release_elk_timer_context(ctx);
      lv_timer_del(t);  // Safe even for the currently-running timer in LVGL 8
      return js_mktrue();
    }
    t = next;
  }
  return js_mkfalse();
}

static jsval_t js_create_timer(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) {
    LOG("create_timer expects: function_name, period_ms");
    return js_mknull();
  }

  size_t func_name_len;
  char *func_name_str = js_getstr(js, args[0], &func_name_len);
  double period = js_getnum(args[1]);

  if (!webscreen_callback_name(func_name_str, func_name_len) || js_type(args[1]) != JS_NUM ||
      !isfinite(period) || period < 1 || period > INT32_MAX) {
    return js_mkfalse();
  }

  unsigned count = 0;
  for (auto *timer = lv_timer_get_next(nullptr); timer; timer = lv_timer_get_next(timer)) {
    if (timer->timer_cb != elk_timer_cb) continue;
    count++;
    auto *ctx = (ElkTimerCtx *)timer->user_data;
    if (ctx && strlen(ctx->name) == func_name_len && memcmp(ctx->name, func_name_str, func_name_len) == 0) {
      lv_timer_set_period(timer, (uint32_t)period);
      lv_timer_reset(timer);
      return js_mknull();
    }
  }
  if (count >= WEBSCREEN_MAX_JS_TIMERS) return js_mkfalse();

  ElkTimerCtx *ctx = (ElkTimerCtx *)malloc(sizeof(ElkTimerCtx));
  if (!ctx) {
    LOG("Failed to allocate memory for timer context");
    return js_mkfalse();
  }
  ctx->executing = false;
  ctx->deleted = false;
  ctx->streak = 0;
  if (func_name_len >= sizeof(ctx->name)) {
    LOGF("create_timer: function name too long (max %u chars)\n",
         (unsigned)(sizeof(ctx->name) - 1));
    free(ctx);
    return js_mkfalse();
  }
  memcpy(ctx->name, func_name_str, func_name_len);
  ctx->name[func_name_len] = '\0';

  if (!lv_timer_create(elk_timer_cb, (uint32_t)period, ctx)) {
    free(ctx);
    return js_mkfalse();
  }

  LOGF("Created LVGL timer to call JS function '%s' every %dms\n", ctx->name, (int)period);
  return js_mknull();
}
