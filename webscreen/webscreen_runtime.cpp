#include "webscreen_runtime.h"
#include "webscreen_main.h"
#include "webscreen_network.h"
#include "webscreen_base64.h"
#include <lvgl.h>
#include "tick.h"
#include "pins_config.h"
#include "rm67162.h"
#include "globals.h"
#include "lvgl_elk.h"

extern "C" {
#include "elk.h"
}
#include <WiFi.h>
#include <PubSubClient.h>
static bool g_javascript_active = false;
static bool g_fallback_active = false;
static String g_current_script_file = "";
static String g_fallback_text = "WebScreen v" WEBSCREEN_VERSION_STRING "\nFallback Mode\nSD card or script not found";
static String g_last_error = "";
static uint32_t g_runtime_start_time = 0;

extern struct js* js;
extern uint8_t *elk_memory;
extern size_t elk_memory_size;
extern bool init_elk_memory();
static TaskHandle_t g_js_task_handle = NULL;
static bool g_js_engine_initialized = false;
static String g_js_script_content = "";

// Script source in PSRAM — Elk stores function bodies as pointers into the
// original source string.  If that buffer lives on the regular heap, any heap
// corruption (e.g. from lwip/PubSubClient) silently damages function bodies
// and causes "; expected" parse errors.  PSRAM is a separate address space,
// immune to regular-heap overflow/corruption.
static char  *g_js_script_psram     = nullptr;
static size_t g_js_script_psram_len = 0;

static unsigned long g_last_mqtt_reconnect_attempt = 0;
static unsigned long g_last_wifi_reconnect_attempt = 0;

// g_wifiClient and g_mqttClient are defined in lvgl_elk.h (included above)
static uint32_t g_loop_count = 0;
static uint32_t g_avg_loop_time_us = 0;
static uint32_t g_max_loop_time_us = 0;

bool webscreen_runtime_start_javascript(const char* script_file) {
  if (!script_file) {
    g_last_error = "Script file path is NULL";
    return false;
  }

  WEBSCREEN_DEBUG_PRINTF("Starting JavaScript runtime with: %s\n", script_file);

  if (!SD_MMC.exists(script_file)) {
    g_last_error = "Script file not found: ";
    g_last_error += script_file;
    WEBSCREEN_DEBUG_PRINTLN(g_last_error.c_str());
    return false;
  }

  webscreen_runtime_shutdown();

  init_lvgl_display();

  if (!webscreen_runtime_init_sd_filesystem()) {
    g_last_error = "Failed to initialize SD filesystem";
    return false;
  }

  if (!webscreen_runtime_init_memory_filesystem()) {
    g_last_error = "Failed to initialize memory filesystem";
    return false;
  }

  if (!webscreen_runtime_init_ram_images()) {
    g_last_error = "Failed to initialize RAM images";
    return false;
  }

  if (!webscreen_runtime_init_javascript_engine()) {
    g_last_error = "Failed to initialize JavaScript engine";
    return false;
  }

  if (!webscreen_runtime_load_script(script_file)) {
    g_last_error = "Failed to load JavaScript script";
    return false;
  }

  if (!webscreen_runtime_start_javascript_task()) {
    g_last_error = "Failed to start JavaScript execution task";
    return false;
  }

  g_current_script_file = script_file;
  g_javascript_active = true;
  g_fallback_active = false;
  g_runtime_start_time = WEBSCREEN_MILLIS();
  g_last_error = "";

  WEBSCREEN_DEBUG_PRINTLN("JavaScript runtime started (simulated)");
  return true;
}
void webscreen_runtime_loop_javascript(void) {
  if (!g_javascript_active) {
    return;
  }

  vTaskDelay(pdMS_TO_TICKS(50));
}
void webscreen_runtime_shutdown(void) {
  if (g_javascript_active || g_fallback_active) {
    WEBSCREEN_DEBUG_PRINTLN("Shutting down runtime");
    if (g_js_task_handle != NULL) {
      vTaskDelete(g_js_task_handle);
      g_js_task_handle = NULL;
    }
    if (js) {

      js = NULL;
    }

    g_javascript_active = false;
    g_fallback_active = false;
    g_js_engine_initialized = false;
    g_current_script_file = "";
    g_js_script_content = "";
    if (g_js_script_psram) { free(g_js_script_psram); g_js_script_psram = nullptr; }
    g_js_script_psram_len = 0;
    g_last_error = "";
  }
}
bool webscreen_runtime_is_javascript_active(void) {
  return g_javascript_active;
}

const char* webscreen_runtime_get_javascript_status(void) {
  if (!g_javascript_active) {
    return "JavaScript runtime inactive";
  }

  static String status;
  status = "JavaScript active - Script: ";
  status += g_current_script_file;
  status += " - Uptime: ";
  status += (WEBSCREEN_MILLIS() - g_runtime_start_time);
  status += "ms";

  return status.c_str();
}
void webscreen_runtime_get_javascript_stats(uint32_t* exec_count,
                                            uint32_t* avg_time_us,
                                            uint32_t* error_count) {
  if (exec_count) *exec_count = g_loop_count;
  if (avg_time_us) *avg_time_us = g_avg_loop_time_us;
  if (error_count) *error_count = g_last_error.length() > 0 ? 1 : 0;
}
bool webscreen_runtime_is_fallback_active(void) {
  return g_fallback_active;
}
void webscreen_runtime_get_memory_usage(uint32_t* js_heap_used,
                                        uint32_t* lvgl_memory_used,
                                        uint32_t* total_runtime_memory) {
  // Real numbers now (this used to return hardcoded values).
  if (js_heap_used) *js_heap_used = js ? (uint32_t)js_usage(js) : 0;
  if (lvgl_memory_used) *lvgl_memory_used = (uint32_t)(ESP.getHeapSize() - ESP.getFreeHeap());
  if (total_runtime_memory) *total_runtime_memory = js ? (uint32_t)js_total(js) : 0;
}

void webscreen_runtime_get_js_arena(uint32_t* used, uint32_t* total) {
  if (used) *used = js ? (uint32_t)js_usage(js) : 0;
  if (total) *total = js ? (uint32_t)js_total(js) : 0;
}

bool webscreen_runtime_garbage_collect(void) {
  // Compacting the arena while the JS task is mid-eval corrupts it, so this
  // only requests a GC; the JS task runs it at its next safe point.
  if (g_javascript_active && js != NULL) {
    g_js_gc_requested = true;
    WEBSCREEN_DEBUG_PRINTLN("JavaScript garbage collection requested");
    return true;
  }
  return false;
}

void webscreen_runtime_set_js_heap_kb(int kb) {
  set_elk_heap_kb(kb);
}

const char* webscreen_runtime_get_last_error(void) {
  return g_last_error.length() > 0 ? g_last_error.c_str() : nullptr;
}
void webscreen_runtime_clear_errors(void) {
  g_last_error = "";
}
bool webscreen_runtime_has_errors(void) {
  return g_last_error.length() > 0;
}
void webscreen_runtime_print_status(void) {
  WEBSCREEN_DEBUG_PRINTLN("\n=== RUNTIME STATUS ===");
  WEBSCREEN_DEBUG_PRINTF("JavaScript Active: %s\n", g_javascript_active ? "Yes" : "No");
  WEBSCREEN_DEBUG_PRINTF("Fallback Active: %s\n", g_fallback_active ? "Yes" : "No");

  if (g_javascript_active) {
    WEBSCREEN_DEBUG_PRINTF("Script File: %s\n", g_current_script_file.c_str());
    WEBSCREEN_DEBUG_PRINTF("Runtime Uptime: %lu ms\n",
                           WEBSCREEN_MILLIS() - g_runtime_start_time);
  }

  if (g_fallback_active) {
    WEBSCREEN_DEBUG_PRINTF("Fallback Text: %s\n", g_fallback_text.c_str());
  }

  WEBSCREEN_DEBUG_PRINTF("Loop Count: %lu\n", g_loop_count);
  WEBSCREEN_DEBUG_PRINTF("Avg Loop Time: %lu us\n", g_avg_loop_time_us);
  WEBSCREEN_DEBUG_PRINTF("Max Loop Time: %lu us\n", g_max_loop_time_us);

  if (g_last_error.length() > 0) {
    WEBSCREEN_DEBUG_PRINTF("Last Error: %s\n", g_last_error.c_str());
  }

  WEBSCREEN_DEBUG_PRINTLN("======================\n");
}
bool webscreen_runtime_init_javascript_engine(void) {
  if (g_js_engine_initialized) {
    return true;
  }

  WEBSCREEN_DEBUG_PRINTLN("Initializing Elk JavaScript engine...");

  // Initialize Elk memory from PSRAM
  if (!init_elk_memory()) {
    WEBSCREEN_DEBUG_PRINTLN("Failed to allocate Elk memory");
    return false;
  }

  js = js_create(elk_memory, elk_memory_size);
  if (!js) {
    WEBSCREEN_DEBUG_PRINTLN("Failed to initialize Elk JavaScript engine");
    return false;
  }

  // Auto-GC is safe again: js_stmt() now skips collection whenever F_CALL is
  // set (see elk.c), so the heap is only ever compacted at top-level
  // statement boundaries — the same points where the old manual-GC scheme
  // already proved safe. Collect when the arena reaches 3/4.
  js_setgct(js, (elk_memory_size / 4) * 3);

  // Convert runaway scripts into recoverable JS errors instead of crashes:
  // - recursion blows the 24KB task stack without a C-stack ceiling
  // - while(true){} starves LVGL and the watchdog without a statement budget
  js_setmaxcss(js, 10 * 1024);
  js_setmaxsteps(js, 2 * 1000 * 1000);

  webscreen_runtime_register_js_functions();

  g_js_engine_initialized = true;
  WEBSCREEN_DEBUG_PRINTLN("JavaScript engine initialized successfully");
  return true;
}
bool webscreen_runtime_load_script(const char* script_file) {
  if (!script_file) {
    return false;
  }

  WEBSCREEN_DEBUG_PRINTF("Loading JavaScript script from: %s\n", script_file);

  File file = SD_MMC.open(script_file);
  if (!file) {
    WEBSCREEN_DEBUG_PRINTF("Failed to open script file: %s\n", script_file);
    return false;
  }

  // The script lives in PSRAM so Elk's function-body pointers survive any
  // regular-heap corruption from MQTT / lwip / PubSubClient operations.
  // One sized read straight into the buffer — readString() reads byte-at-a-
  // time with per-char timeout handling and reallocs the heap String as it
  // grows, which is needlessly slow over the 1-bit SD bus.
  size_t fsize = file.size();
  if (fsize == 0) {
    WEBSCREEN_DEBUG_PRINTLN("Script file is empty");
    file.close();
    return false;
  }

  if (g_js_script_psram) { free(g_js_script_psram); g_js_script_psram = nullptr; }
  g_js_script_psram_len = 0;
  g_js_script_content = "";

  char *buf = (char *)ps_malloc(fsize + 1);
  if (buf) {
    size_t got = file.read((uint8_t *)buf, fsize);
    file.close();
    buf[got] = '\0';
    if (got == 0) {
      free(buf);
      WEBSCREEN_DEBUG_PRINTLN("Script read failed");
      return false;
    }
    g_js_script_psram = buf;
    g_js_script_psram_len = got;
    WEBSCREEN_DEBUG_PRINTF("Script loaded to PSRAM (%u bytes)\n", (unsigned)got);
  } else {
    WEBSCREEN_DEBUG_PRINTLN("WARNING: ps_malloc failed for script, using heap copy (vulnerable to corruption)");
    g_js_script_content = file.readString();
    file.close();
    if (g_js_script_content.length() == 0) {
      return false;
    }
  }
  return true;
}
bool webscreen_runtime_start_javascript_task(void) {
  if (g_js_task_handle != NULL) {
    WEBSCREEN_DEBUG_PRINTLN("JavaScript task already running");
    return true;
  }

  WEBSCREEN_DEBUG_PRINTLN("Starting JavaScript execution task...");

  // Core 1: WiFi/lwip run high-priority on core 0 and used to starve the
  // JS/LVGL task exactly when the network was busy. Core 1 only hosts
  // loopTask (serial + power button polling); same priority 1 so FreeRTOS
  // time-slices them — a long JS eval can never lock out the power button.
  BaseType_t result = xTaskCreatePinnedToCore(
    webscreen_runtime_javascript_task,
    "WebScreenJS",
    24576,  // Stack size - increased from 16KB to 24KB for complex JS operations
    NULL,   // Parameters
    1,      // Priority
    &g_js_task_handle,
    1  // Core
  );

  if (result != pdPASS) {
    WEBSCREEN_DEBUG_PRINTLN("Failed to create JavaScript task");
    return false;
  }

  WEBSCREEN_DEBUG_PRINTLN("JavaScript task started successfully");
  return true;
}
// ---- In-place JS app restart -------------------------------------------
//
// The historical "recovery" for a misbehaving script was ESP.restart() — a
// full device reboot that users experienced as a crash loop. Instead, the JS
// task can now tear the app down and start it again in place: the display,
// WiFi connection and Elk arena all survive; only the script state is rebuilt.
// Restart is requested via flag (from the timer bridge on repeated JS errors,
// or from serial /load + /restart_app) and performed by the JS task itself in
// its own loop — it owns LVGL, so teardown needs no cross-task locking.
static volatile bool g_js_restart_requested = false;
static char g_js_restart_reason[64] = "";
static uint32_t g_js_restart_failures = 0;
static const uint32_t JS_RESTART_FAILURE_LIMIT = 2;
static volatile bool g_js_safe_mode = false;  // Restarts kept failing; wait for user
// Script switch requested by /load. Requesters write this fixed buffer (a
// String would be freed+reallocated under the JS task's feet); the JS task
// alone assigns g_current_script_file, at restart time.
static char g_js_pending_script[96] = "";
// Guards reason/pending-script/safe-mode handoff between loopTask and JS task.
static portMUX_TYPE g_js_restart_mux = portMUX_INITIALIZER_UNLOCKED;

// Give-up ladder for AUTOMATIC restarts (timer error streaks): a script that
// boots clean but whose timer callback always errors would otherwise restart
// successfully forever (eval OK resets the failure counter), churning the
// screen every ~1s. Three streak-triggered restarts without a healthy hour
// of the app in between park the device in safe mode instead.
static uint32_t g_js_auto_restart_cycles = 0;
static uint32_t g_js_last_auto_restart_ms = 0;
static const uint32_t JS_AUTO_RESTART_CYCLE_LIMIT = 3;
static const uint32_t JS_HEALTHY_INTERVAL_MS = 60000;

void webscreen_runtime_request_restart(const char *reason) {
  taskENTER_CRITICAL(&g_js_restart_mux);
  strlcpy(g_js_restart_reason, reason ? reason : "unspecified", sizeof(g_js_restart_reason));
  g_js_safe_mode = false;  // An explicit USER request always gets a fresh try
  g_js_auto_restart_cycles = 0;
  g_js_restart_requested = true;
  taskEXIT_CRITICAL(&g_js_restart_mux);
}

// Automatic escalation (elk_timer_cb error streak). Unlike an explicit
// request this must not lift safe mode — leftover timers of a broken script
// erroring against a parked engine would otherwise un-park it forever.
void webscreen_runtime_request_restart_auto(const char *reason) {
  taskENTER_CRITICAL(&g_js_restart_mux);
  if (g_js_safe_mode) {
    taskEXIT_CRITICAL(&g_js_restart_mux);
    return;  // Parked: only /restart_app or /load may revive the app
  }
  uint32_t now = WEBSCREEN_MILLIS();
  if (now - g_js_last_auto_restart_ms > JS_HEALTHY_INTERVAL_MS) {
    g_js_auto_restart_cycles = 0;  // App ran healthy for a while — fresh ladder
  }
  g_js_last_auto_restart_ms = now;
  g_js_auto_restart_cycles++;
  strlcpy(g_js_restart_reason, reason ? reason : "unspecified", sizeof(g_js_restart_reason));
  g_js_restart_requested = true;
  taskEXIT_CRITICAL(&g_js_restart_mux);
}

bool webscreen_runtime_load_new_script(const char *script_file) {
  if (!script_file || !SD_MMC.exists(script_file)) {
    return false;
  }
  // A truncated path would pass the exists() check above but fail to load
  // after tearing down the running app — reject oversized paths instead.
  if (strlen(script_file) >= sizeof(g_js_pending_script)) {
    WEBSCREEN_DEBUG_PRINTF("load_new_script: path too long (%u chars, max %u)\n",
                           (unsigned)strlen(script_file),
                           (unsigned)(sizeof(g_js_pending_script) - 1));
    return false;
  }
  taskENTER_CRITICAL(&g_js_restart_mux);
  strlcpy(g_js_pending_script, script_file, sizeof(g_js_pending_script));
  g_js_restart_failures = 0;
  taskEXIT_CRITICAL(&g_js_restart_mux);
  webscreen_runtime_request_restart("script change");
  return true;
}

// ---- /eval REPL ----------------------------------------------------------
//
// Same handoff discipline as g_js_pending_script: loopTask writes the fixed
// buffer only while the pending flag is clear; the JS task evaluates at its
// safe point (no Elk C frames on the stack) and clears the flag when done.
static char g_js_eval_buf[256] = "";
static volatile bool g_js_eval_pending = false;

bool webscreen_runtime_eval_snippet(const char *code) {
  if (!g_javascript_active || js == NULL || g_js_safe_mode) return false;
  if (g_js_eval_pending) return false;  // previous snippet still in flight
  if (!code || code[0] == '\0' || strlen(code) >= sizeof(g_js_eval_buf)) return false;
  strlcpy(g_js_eval_buf, code, sizeof(g_js_eval_buf));
  g_js_eval_pending = true;
  return true;
}

// ---- Last-error record (/errors) -----------------------------------------
//
// g_last_error (String) only ever captures startup failures; JS eval errors
// land here instead — a fixed buffer so any task can write it, guarded by
// the restart mux (the copy is a few hundred ns, fine inside a critical
// section).
static char g_js_last_error[160] = "";
static uint32_t g_js_last_error_ms = 0;

void webscreen_runtime_note_js_error(const char *msg) {
  if (!msg) return;
  taskENTER_CRITICAL(&g_js_restart_mux);
  strlcpy(g_js_last_error, msg, sizeof(g_js_last_error));
  g_js_last_error_ms = WEBSCREEN_MILLIS();
  taskEXIT_CRITICAL(&g_js_restart_mux);
}

void webscreen_runtime_print_error_report(void) {
  char last[sizeof(g_js_last_error)];
  uint32_t when, failures, cycles;
  bool safe_mode;
  taskENTER_CRITICAL(&g_js_restart_mux);
  strlcpy(last, g_js_last_error, sizeof(last));
  when = g_js_last_error_ms;
  failures = g_js_restart_failures;
  cycles = g_js_auto_restart_cycles;
  safe_mode = g_js_safe_mode;
  taskEXIT_CRITICAL(&g_js_restart_mux);

  Serial.println("\n=== JS Error Report ===");
  if (last[0] != '\0') {
    Serial.printf("Last JS error (%lus ago): %s\n",
                  (unsigned long)((WEBSCREEN_MILLIS() - when) / 1000), last);
  } else {
    Serial.println("Last JS error: none");
  }
  if (g_last_error.length() > 0) {
    Serial.printf("Startup error: %s\n", g_last_error.c_str());
  }
  Serial.printf("Restart failures: %lu/%lu\n",
                (unsigned long)failures, (unsigned long)JS_RESTART_FAILURE_LIMIT);
  Serial.printf("Auto-restart cycles: %lu/%lu\n",
                (unsigned long)cycles, (unsigned long)JS_AUTO_RESTART_CYCLE_LIMIT);
  Serial.printf("Safe mode: %s\n", safe_mode ? "YES (fix the script, then /load or /restart_app)" : "no");
  Serial.printf("Script: %s\n", g_current_script_file.length() > 0 ? g_current_script_file.c_str() : "(none)");
}

// ---- Button events --------------------------------------------------------
//
// Registered as the hardware button callback by dynamic_js_setup(); runs on
// loopTask. The counters live in ws_elk_basics.h next to their JS bindings.
void webscreen_runtime_notify_button(bool pressed) {
  if (pressed) {
    g_button_evt_produced++;
  }
}

// ---- /screenshot ----------------------------------------------------------
//
// Same handoff discipline as /eval: loopTask sets the flag, the JS task
// captures at its safe point (LVGL objects must not be touched from any
// other task) and streams the snapshot over Serial. USB-CDC ignores the
// virtual baud rate, so the ~340KB base64 dump takes low single-digit
// seconds; LVGL is paused for the duration, which is fine — the screen
// content is what's being captured.
static volatile bool g_js_screenshot_pending = false;

bool webscreen_runtime_request_screenshot(void) {
  if (!g_javascript_active || js == NULL) return false;
  if (g_js_screenshot_pending) return false;  // previous capture still in flight
  g_js_screenshot_pending = true;
  return true;
}

static void webscreen_runtime_stream_screenshot(void) {
  lv_img_dsc_t *snap = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
  if (snap == NULL) {
    Serial.println("[ERROR] Screenshot failed (snapshot allocation)");
    return;
  }

  Serial.printf("=== SCREENSHOT %ux%u RGB565%s ===\n",
                (unsigned)snap->header.w, (unsigned)snap->header.h,
                LV_COLOR_16_SWAP ? "_SWAP" : "");

  // 57 raw bytes -> 76 base64 chars per line (classic MIME width)
  const uint8_t *data = snap->data;
  size_t len = snap->data_size;
  char b64[80];
  size_t lines = 0;
  for (size_t off = 0; off < len; off += 57) {
    size_t chunk = len - off;
    if (chunk > 57) chunk = 57;
    webscreen_base64_encode(data + off, chunk, b64);
    Serial.println(b64);
    // Yield periodically so the USB-CDC TX buffer drains and the watchdog
    // stays fed on slow hosts.
    if ((++lines & 0xFF) == 0) vTaskDelay(1);
  }
  Serial.println("=== SCREENSHOT END ===");

  lv_snapshot_free(snap);
}

// Full-width wrapped error label so a broken script is visible on the device
// instead of a black screen.
static void webscreen_runtime_show_error_screen(const char *msg) {
  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, 500);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(label, msg);
}

// Evaluate the loaded script (PSRAM copy preferred). Returns true on success;
// on failure copies the JS error into err_buf (js_str points into the arena,
// which the next eval may clobber).
static bool webscreen_runtime_eval_script(char *err_buf, size_t err_len) {
  const char *script_src = g_js_script_psram ? g_js_script_psram : g_js_script_content.c_str();
  size_t      script_len = g_js_script_psram ? g_js_script_psram_len : g_js_script_content.length();

  if (!js || script_len == 0) {
    if (err_buf) strlcpy(err_buf, "no engine or empty script", err_len);
    return false;
  }
  jsval_t result = js_eval(js, script_src, script_len);
  if (js_type(result) == JS_ERR) {
    const char *error = js_str(js, result);
    if (err_buf) strlcpy(err_buf, error ? error : "unknown error", err_len);
    return false;
  }
  return true;
}

// Runs ON the JS task. Teardown order matters: UI first (deletes lv_img
// widgets referencing RAM-image descriptors), then media buffers, then comm.
static void webscreen_runtime_perform_inplace_restart(void) {
  // Consume the restart parameters under the lock (requests arrive from
  // loopTask); no heap operations inside the critical section.
  char reason[sizeof(g_js_restart_reason)];
  char pending[sizeof(g_js_pending_script)];
  taskENTER_CRITICAL(&g_js_restart_mux);
  strlcpy(reason, g_js_restart_reason, sizeof(reason));
  strlcpy(pending, g_js_pending_script, sizeof(pending));
  g_js_pending_script[0] = '\0';
  taskEXIT_CRITICAL(&g_js_restart_mux);
  if (pending[0] != '\0') {
    g_current_script_file = pending;  // String writes happen only on this task
  }

  WEBSCREEN_DEBUG_PRINTF("In-place JS app restart (reason: %s, script: %s)\n",
                         reason, g_current_script_file.c_str());

  elk_teardown_ui();
  elk_teardown_media();
  elk_teardown_comm();

  // Re-create the engine over the SAME arena and re-register the API.
  g_js_engine_initialized = false;
  if (!webscreen_runtime_init_javascript_engine()) {
    webscreen_runtime_show_error_screen("WebScreen: JS engine re-init failed.\nUse /reboot to power-cycle.");
    g_js_safe_mode = true;
    return;
  }

  char err[128] = "";
  bool ok = webscreen_runtime_load_script(g_current_script_file.c_str()) &&
            webscreen_runtime_eval_script(err, sizeof(err));

  if (ok) {
    if (g_js_auto_restart_cycles >= JS_AUTO_RESTART_CYCLE_LIMIT) {
      // The script evals clean but its timer callbacks keep erroring badly
      // enough to trigger restart after restart — restarting again would
      // churn the screen forever. Park instead; the timers the fresh eval
      // just created must go, or their error streaks would request again.
      delete_all_elk_timers();
      char msg[256];
      snprintf(msg, sizeof(msg),
               "WebScreen: script '%s' keeps failing in its timer callbacks.\n\n"
               "Fix the script, then run /load or /restart_app.",
               g_current_script_file.c_str());
      webscreen_runtime_show_error_screen(msg);
      // Timers are gone, so no AUTO request can race this window — but an
      // explicit /load or /restart_app from loopTask can; honor it instead
      // of trapping it under safe mode (explicit requests reset the ladder).
      taskENTER_CRITICAL(&g_js_restart_mux);
      if (!g_js_restart_requested) {
        g_js_safe_mode = true;
      }
      taskEXIT_CRITICAL(&g_js_restart_mux);
      WEBSCREEN_DEBUG_PRINTLN("JS app parked in safe mode (timer-error restart loop)");
      return;
    }
    g_js_restart_failures = 0;
    WEBSCREEN_DEBUG_PRINTLN("JS app restarted successfully");
    return;
  }

  g_js_restart_failures++;
  WEBSCREEN_DEBUG_PRINTF("JS app restart failed (%u/%u): %s\n",
                         g_js_restart_failures, JS_RESTART_FAILURE_LIMIT, err);
  webscreen_runtime_note_js_error(err);
  if (g_js_restart_failures >= JS_RESTART_FAILURE_LIMIT) {
    // Safe mode: stop retrying, show the error, keep the device alive —
    // serial commands still work, so the user can fix the script and /load.
    // A partially-evaluated script may have created timers before failing;
    // left alive they would error every tick and their streak would request
    // an auto restart, churning the error screen. Remove them first.
    delete_all_elk_timers();
    char msg[256];
    snprintf(msg, sizeof(msg),
             "WebScreen: script '%s' keeps failing:\n%s\n\n"
             "Fix the script, then run /load or /restart_app.",
             g_current_script_file.c_str(), err);
    webscreen_runtime_show_error_screen(msg);
    // Don't trap a fresh request that raced this failing restart: a /load
    // that landed after our last check deserves its try, not safe mode.
    taskENTER_CRITICAL(&g_js_restart_mux);
    if (!g_js_restart_requested) {
      g_js_safe_mode = true;
    }
    taskEXIT_CRITICAL(&g_js_restart_mux);
  } else {
    g_js_restart_requested = true;  // One more attempt
  }
}

void webscreen_runtime_javascript_task(void* pvParameters) {
  WEBSCREEN_DEBUG_PRINTLN("JavaScript task started");
  vTaskDelay(pdMS_TO_TICKS(100));

  char err[128] = "";
  if (!webscreen_runtime_eval_script(err, sizeof(err))) {
    WEBSCREEN_DEBUG_PRINT("JavaScript execution error: ");
    WEBSCREEN_DEBUG_PRINTLN(err);
    webscreen_runtime_note_js_error(err);
    // Make the failure visible on the device instead of a black screen.
    char msg[256];
    snprintf(msg, sizeof(msg),
             "WebScreen: script '%s' failed:\n%s\n\n"
             "Fix the script, then run /load or /restart_app.",
             g_current_script_file.c_str(), err);
    webscreen_runtime_show_error_screen(msg);
  } else {
    WEBSCREEN_DEBUG_PRINTLN("JavaScript script executed successfully");
  }

  for (;;) {
    if (g_js_restart_requested && !g_js_safe_mode) {
      g_js_restart_requested = false;
      webscreen_runtime_perform_inplace_restart();
    }
    // GC requested by JS gc() or serial — this point is outside any eval,
    // so compacting the arena is safe.
    if (g_js_gc_requested && js != NULL) {
      g_js_gc_requested = false;
      js_gc(js);
    }
    // /eval snippet queued by loopTask. The flag stays set until after the
    // eval so loopTask never overwrites a buffer in use. A snippet that
    // raced into a safe-mode transition is dropped, not left pending.
    if (g_js_eval_pending && js != NULL) {
      if (g_js_safe_mode) {
        Serial.println("[EVAL] dropped: app is parked in safe mode");
      } else {
        jsval_t v = js_eval(js, g_js_eval_buf, strlen(g_js_eval_buf));
        const char *s = js_str(js, v);
        Serial.printf("[EVAL] %s\n", s ? s : "(no result)");
        if (js_type(v) == JS_ERR && s) {
          char rec[160];
          snprintf(rec, sizeof(rec), "eval: %s", s);
          webscreen_runtime_note_js_error(rec);
        }
      }
      g_js_eval_pending = false;
    }
    // /screenshot queued by loopTask — capture at this safe point (no eval
    // in progress, LVGL idle between timer runs).
    if (g_js_screenshot_pending) {
      webscreen_runtime_stream_screenshot();
      g_js_screenshot_pending = false;
    }
    // Pending power-button short press -> registered JS handler
    elk_dispatch_button_event();
    // Unconditional: the maintain loop also handles WiFi reconnection,
    // which non-MQTT apps need too (it gates the MQTT work internally).
    webscreen_runtime_wifi_mqtt_maintain_loop();
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
extern void register_js_functions();
void webscreen_runtime_register_js_functions(void) {
  if (!js) {
    return;
  }

  WEBSCREEN_DEBUG_PRINTLN("Registering JavaScript API functions...");
  register_js_functions();

  WEBSCREEN_DEBUG_PRINTLN("JavaScript API functions registered successfully");
}
static bool g_was_wifi_connected = false;

void webscreen_runtime_wifi_mqtt_maintain_loop(void) {

  if (WiFi.status() != WL_CONNECTED) {
    g_was_wifi_connected = false;
    unsigned long now = WEBSCREEN_MILLIS();

    // Only reconnect when credentials were ever configured: offline devices
    // (no SSID in webscreen.json) are a supported mode, and reconnect-spam
    // against an empty STA config is just radio churn and log noise.
    if (WiFi.SSID().length() > 0 &&
        now - g_last_wifi_reconnect_attempt > 10000) {
      g_last_wifi_reconnect_attempt = now;
      WEBSCREEN_DEBUG_PRINTLN("Wi-Fi disconnected, attempting reconnection...");
      WiFi.reconnect();  // Previously this only logged — never reconnected
    }
    return;
  }

  // WiFi is connected - initialize NTP if this is a new connection
  if (!g_was_wifi_connected) {
    g_was_wifi_connected = true;
    if (!webscreen_ntp_is_synced()) {
      WEBSCREEN_DEBUG_PRINTLN("WiFi connected - initializing NTP...");
      webscreen_ntp_setup_from_config();
    }
  }

  if (g_mqtt_enabled) {
    if (g_mqttClient.connected()) {
      g_mqttClient.loop();
    } else {
      // Broker dropped — retry with the credentials from the last successful
      // mqtt_connect() (elk_mqtt_try_reconnect in lvgl_elk.h). Each attempt
      // can block this task (the LVGL owner) up to the ~5s socket timeout,
      // so back off exponentially while the broker stays down.
      static unsigned long s_mqtt_backoff_ms = 5000;
      unsigned long now = WEBSCREEN_MILLIS();
      if (now - g_last_mqtt_reconnect_attempt > s_mqtt_backoff_ms) {
        g_last_mqtt_reconnect_attempt = now;
        if (elk_mqtt_try_reconnect()) {
          s_mqtt_backoff_ms = 5000;
        } else if (s_mqtt_backoff_ms < 60000) {
          s_mqtt_backoff_ms *= 2;
        }
      }
    }
    // Messages are queued by onMqttMessage callback.
    // JS polls via mqtt_has_message() from its timer — no js_eval from C++.
    extern void processPendingMqttMessage();
    processPendingMqttMessage();
  }
}
extern void init_lvgl_display();
extern void init_lv_fs();
extern void init_mem_fs();
extern void init_ram_images();
bool webscreen_runtime_init_sd_filesystem(void) {
  WEBSCREEN_DEBUG_PRINTLN("Initializing LVGL SD filesystem driver...");
  init_lv_fs();
  return true;
}
bool webscreen_runtime_init_memory_filesystem(void) {
  WEBSCREEN_DEBUG_PRINTLN("Initializing LVGL memory filesystem driver...");
  init_mem_fs();
  return true;
}
bool webscreen_runtime_init_ram_images(void) {
  WEBSCREEN_DEBUG_PRINTLN("Initializing RAM images storage...");
  init_ram_images();
  return true;
}