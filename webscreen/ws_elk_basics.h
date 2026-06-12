// ws_elk_basics.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

/******************************************************************************
 * E) Elk-Facing Functions (print, Wi-Fi, SD ops, etc.)
 ******************************************************************************/
static jsval_t js_print(struct js *js, jsval_t *args, int nargs) {
  for (int i = 0; i < nargs; i++) {
    const char *str = js_str(js, args[i]);
    if (str) LOG(str);
    else LOG("print: argument is not a string");
  }
  return js_mknull();
}

// Memory stats - useful for debugging memory issues
static jsval_t js_mem_stats(struct js *js, jsval_t *args, int nargs) {
  size_t freeHeap = ESP.getFreeHeap();
  size_t minFreeHeap = ESP.getMinFreeHeap();
  size_t heapSize = ESP.getHeapSize();
  size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

  LOGF("=== Memory Stats ===\n");
  LOGF("ESP32 Heap: %u / %u bytes (min free: %u, largest block: %u)\n",
       freeHeap, heapSize, minFreeHeap, largestBlock);
  LOGF("PSRAM: %u / %u bytes free\n", ESP.getFreePsram(), ESP.getPsramSize());
  if (js) {
    LOGF("JS arena: %u / %u bytes used\n",
         (unsigned)js_usage(js), (unsigned)js_total(js));
  }
  LOGF("====================\n");

  // Return free heap as a number for JS to use
  return js_mknum(freeHeap);
}

// mem_info() => JSON string with all memory pools, for apps that want to
// display or report their own footprint.
static jsval_t js_mem_info(struct js *js, jsval_t *args, int nargs) {
  char buf[224];
  snprintf(buf, sizeof(buf),
           "{\"heap_free\":%u,\"heap_min\":%u,\"heap_largest\":%u,"
           "\"psram_free\":%u,\"js_used\":%u,\"js_total\":%u}",
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned)ESP.getFreePsram(),
           (unsigned)(js ? js_usage(js) : 0), (unsigned)(js ? js_total(js) : 0));
  return js_mkstr(js, buf, strlen(buf));
}

// gc() => request a garbage collection. The collection itself must NOT run
// here: a binding executes inside a function call (F_CALL), where Elk's GC
// would dangle the saved code pointer in do_call_op(). The timer bridge and
// the JS task loop run it at the next safe point instead.
static volatile bool g_js_gc_requested = false;
static jsval_t js_request_gc(struct js *js, jsval_t *args, int nargs) {
  g_js_gc_requested = true;
  return js_mknum((double)(js_total(js) - js_usage(js)));  // free bytes (pre-GC)
}

// Wi-Fi connect
static jsval_t js_wifi_connect(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) return js_mkfalse();
  const char *ssidQ = js_str(js, args[0]);
  const char *passQ = js_str(js, args[1]);
  if (!ssidQ || !passQ) return js_mkfalse();

  // Strip quotes if any
  String ssid(ssidQ);
  String pass(passQ);
  if (ssid.startsWith("\"") && ssid.endsWith("\"")) {
    ssid = ssid.substring(1, ssid.length() - 1);
  }
  if (pass.startsWith("\"") && pass.endsWith("\"")) {
    pass = pass.substring(1, pass.length() - 1);
  }

  LOGF("Connecting to Wi-Fi SSID: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());

  for (uint32_t i = 0; i < 20 && WiFi.status() != WL_CONNECTED; ++i) {
    vTaskDelay(pdMS_TO_TICKS(250));
    LOG(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    LOG("Wi-Fi connected");
    return js_mktrue();
  } else {
    LOG("Failed to connect to Wi-Fi");
    return js_mkfalse();
  }
}

static jsval_t js_wifi_status(struct js *js, jsval_t *args, int nargs) {
  return (WiFi.status() == WL_CONNECTED) ? js_mktrue() : js_mkfalse();
}

static jsval_t js_wifi_get_ip(struct js *js, jsval_t *args, int nargs) {
  if (WiFi.status() != WL_CONNECTED) {
    LOG("Not connected to Wi-Fi");
    return js_mknull();
  }
  IPAddress ip = WiFi.localIP();
  String ipStr = ip.toString();
  return js_mkstr(js, ipStr.c_str(), ipStr.length());
}

// Delay in JS: "delay(ms)"
static jsval_t js_delay(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  double ms = js_getnum(args[0]);
  vTaskDelay(pdMS_TO_TICKS((unsigned long)ms));
  return js_mknull();
}

static jsval_t js_get_millis(struct js *js, jsval_t *args, int nargs) {
  return js_mknum((double)millis());
}

static jsval_t js_str_length(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mknum(0);
  size_t len;
  char *s = js_getstr(js, args[0], &len);
  if (!s) return js_mknum(0);
  String str(s, len);
  if (str.startsWith("\"") && str.endsWith("\"") && str.length() >= 2) {
    return js_mknum((double)(str.length() - 2));
  }
  return js_mknum((double)str.length());
}

// LVGL Timer Bridging Functions

// Implemented in webscreen_runtime.cpp — asks the JS task to tear down and
// re-start the app in place. NEVER reboots the device. (extern "C" to match
// the declaration in webscreen_runtime.h.)
extern "C" void webscreen_runtime_request_restart(const char *reason);

// Consecutive failed JS timer evals before we give up on the current app
// state and do an in-place restart (engine re-created over the same arena,
// script re-evaluated). An OOM'd or wedged script recovers in ~1s instead of
// power-cycling the device.
static uint32_t g_js_error_streak = 0;
static const uint32_t JS_ERROR_STREAK_LIMIT = 10;

// This C++ function will be the callback for LVGL. It will execute a JS function.
static void elk_timer_cb(lv_timer_t *timer) {
  char *func_name = (char *)timer->user_data;
  if (func_name == NULL || js == NULL) return;

  // Internal-DRAM guard: TLS, lwip, MQTT and LVGL all allocate from internal
  // heap; when it is genuinely low the only safe move is to shed load for a
  // tick. js_gc() cannot help here — it compacts the PSRAM arena, a different
  // memory — and rebooting (what this firmware used to do) just turns memory
  // pressure into a crash loop.
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
  // Collect early when the arena is filling up, or when JS asked via gc().
  if (g_js_gc_requested || js_usage(js) > (js_total(js) / 4) * 3) {
    g_js_gc_requested = false;
    js_gc(js);
  }

  // Construct a snippet of JavaScript to call the function, e.g., "my_func();"
  char snippet[64];
  snprintf(snippet, sizeof(snippet), "%s();", func_name);

  // Use js_eval to execute the function call.
  jsval_t res = js_eval(js, snippet, strlen(snippet));
  if (js_type(res) == JS_ERR) {
    g_js_error_streak++;
    LOGF("[TIMER CB] Error in %s (streak %u/%u): %s | arena %u/%u, heap %u\n",
         snippet, g_js_error_streak, JS_ERROR_STREAK_LIMIT, js_str(js, res),
         (unsigned)js_usage(js), (unsigned)js_total(js), (unsigned)ESP.getFreeHeap());
    // An error often leaves garbage at a high-water mark; collect now so a
    // transient OOM can clear itself before the next tick.
    js_gc(js);
    if (g_js_error_streak >= JS_ERROR_STREAK_LIMIT) {
      g_js_error_streak = 0;
      webscreen_runtime_request_restart("repeated JS timer errors");
    }
  } else {
    g_js_error_streak = 0;
  }
}

// Delete every LVGL timer created through create_timer() and free the
// malloc'd function-name strings they carry. Used by timer_delete() and by
// the in-place app restart.
static void delete_all_elk_timers() {
  lv_timer_t *t = lv_timer_get_next(NULL);
  while (t != NULL) {
    lv_timer_t *next = lv_timer_get_next(t);
    if (t->timer_cb == elk_timer_cb) {
      free(t->user_data);
      lv_timer_del(t);
    }
    t = next;
  }
}

// timer_delete("fname") => stop the timer created with create_timer("fname", ms).
// Returns true if a timer was found and deleted.
static jsval_t js_timer_delete(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  size_t len = 0;
  char *name = js_getstr(js, args[0], &len);
  if (!name || len == 0) return js_mkfalse();

  lv_timer_t *t = lv_timer_get_next(NULL);
  while (t != NULL) {
    lv_timer_t *next = lv_timer_get_next(t);
    if (t->timer_cb == elk_timer_cb && t->user_data != NULL &&
        strlen((const char *)t->user_data) == len &&
        memcmp(t->user_data, name, len) == 0) {
      free(t->user_data);
      lv_timer_del(t);  // Safe even for the currently-running timer in LVGL 8
      return js_mktrue();
    }
    t = next;
  }
  return js_mkfalse();
}

// This is the function we will expose to JavaScript.
// It creates an LVGL timer that will call our C++ callback.
static jsval_t js_create_timer(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) {
    LOG("create_timer expects: function_name, period_ms");
    return js_mknull();
  }

  size_t func_name_len;
  char *func_name_str = js_getstr(js, args[0], &func_name_len);
  double period = js_getnum(args[1]);

  if (!func_name_str || func_name_len == 0) {
    return js_mknull();
  }

  char *name_for_timer = (char *)malloc(func_name_len + 1);
  if (!name_for_timer) {
    LOG("Failed to allocate memory for timer callback name");
    return js_mknull();
  }
  memcpy(name_for_timer, func_name_str, func_name_len);
  name_for_timer[func_name_len] = '\0';

  // Create the LVGL timer
  lv_timer_create(elk_timer_cb, (uint32_t)period, name_for_timer);

  LOGF("Created LVGL timer to call JS function '%s' every %dms\n", name_for_timer, (int)period);
  return js_mknull();
}

// sd_read_file(path)
static jsval_t js_sd_read_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  const char *rawPath = js_str(js, args[0]);
  if (!rawPath) return js_mknull();

  // Strip quotes if present
  String path(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path.remove(0, 1);
    path.remove(path.length() - 1, 1);
  }

  File file = SD_MMC.open(path);
  if (!file) {
    LOGF("Failed to open file: %s\n", path.c_str());
    return js_mknull();
  }
  // The content briefly exists twice (heap String + Elk arena copy); a
  // multi-MB file would exhaust both, so refuse anything over 256KB.
  size_t fileSize = file.size();
  if (fileSize > 256 * 1024) {
    LOGF("sd_read_file: %s is %u bytes, refusing files > 256KB\n",
         path.c_str(), (unsigned)fileSize);
    file.close();
    return js_mknull();
  }
  String content = file.readString();
  file.close();
  return js_mkstr(js, content.c_str(), content.length());
}

// sd_write_file(path, data)
static jsval_t js_sd_write_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) return js_mkfalse();
  const char *path = js_str(js, args[0]);
  const char *data = js_str(js, args[1]);
  if (!path || !data) return js_mkfalse();

  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    LOGF("Failed to open for writing: %s\n", path);
    return js_mkfalse();
  }
  f.write((const uint8_t *)data, strlen(data));
  f.close();
  return js_mktrue();
}

// sd_list_dir(path)
static jsval_t js_sd_list_dir(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  const char *pathQ = js_str(js, args[0]);
  if (!pathQ) return js_mknull();

  // Strip quotes
  String path(pathQ);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }

  File root = SD_MMC.open(path);
  if (!root) {
    LOGF("Failed to open directory: %s\n", path.c_str());
    return js_mknull();
  }
  if (!root.isDirectory()) {
    LOG("Not a directory");
    root.close();
    return js_mknull();
  }

  // Collect listing
  char fileList[512];
  int fileListLen = 0;

  File f = root.openNextFile();
  while (f) {
    const char *type = f.isDirectory() ? "DIR: " : "FILE: ";
    const char *name = f.name();
    int len = snprintf(fileList + fileListLen, sizeof(fileList) - fileListLen,
                       "%s%s\n", type, name);
    if (len < 0 || len >= (int)(sizeof(fileList) - fileListLen)) break;
    fileListLen += len;
    f = root.openNextFile();
  }
  root.close();
  return js_mkstr(js, fileList, fileListLen);
}

// Helper function to convert a JS string to a JS number
static jsval_t js_to_number(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) {
    return js_mknum(0);  // Return 0 if no argument
  }

  // If it's already a number, just return it.
  if (js_type(args[0]) == JS_NUM) {
    return args[0];
  }

  // Get the string value from the JS argument
  size_t len;
  const char *str = js_getstr(js, args[0], &len);
  if (!str) {
    return js_mknum(0);  // Return 0 if not a valid string
  }

  // Convert the C-string to a double and return as a JS number
  return js_mknum(atof(str));
}

// Helper function to convert a JS number to a JS string
static jsval_t js_number_to_string(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) {
    return js_mkstr(js, "", 0);
  }

  uint8_t type = js_type(args[0]);

  if (type == JS_NUM) {
    char buf[32];
    double num = js_getnum(args[0]);
    // Using "%.17g" is how the Elk engine itself formats numbers
    snprintf(buf, sizeof(buf), "%.17g", num);
    return js_mkstr(js, buf, strlen(buf));
  } else if (type == JS_STR) {  // If it's already a string (like from parse_json_value), just return it
    return args[0];
  }

  return js_mkstr(js, "", 0);
}

