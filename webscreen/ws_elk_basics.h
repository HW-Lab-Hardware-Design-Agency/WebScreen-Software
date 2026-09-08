// ws_elk_basics.h — fragment of the WebScreen Elk/LVGL bridge; included once, in order, by lvgl_elk.h (not standalone).

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

  return js_mknum(freeHeap);
}

// mem_info() => JSON string of heap/PSRAM/JS-arena stats
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

// gc() => request a collection; never collect here (bindings run under F_CALL,
// where GC dangles do_call_op's saved code pointer) — safe points collect instead.
static volatile bool g_js_gc_requested = false;
static jsval_t js_request_gc(struct js *js, jsval_t *args, int nargs) {
  g_js_gc_requested = true;
  return js_mknum((double)(js_total(js) - js_usage(js)));  // free bytes (pre-GC)
}

static jsval_t js_wifi_connect(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 2) return js_mkfalse();
  String ssid = js_arg_str(js, args[0]);
  String pass = js_arg_str(js, args[1]);
  if (ssid.isEmpty()) return js_mkfalse();
  // No isEmpty() bail for pass: an empty password is valid (open AP).

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

#include "ws_elk_timers.h"

static jsval_t js_sd_read_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  String path = js_arg_str(js, args[0]);
  if (path.isEmpty()) return js_mknull();

  File file = SD_MMC.open(path);
  if (!file) {
    LOGF("Failed to open file: %s\n", path.c_str());
    return js_mknull();
  }
  // Content briefly exists twice (heap String + arena copy); refuse files > 256KB.
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

static jsval_t js_sd_list_dir(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mknull();
  String path = js_arg_str(js, args[0]);
  if (path.isEmpty()) return js_mknull();

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

static jsval_t js_to_number(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) {
    return js_mknum(0);  // Return 0 if no argument
  }

  if (js_type(args[0]) == JS_NUM) {
    return args[0];
  }

  size_t len;
  const char *str = js_getstr(js, args[0], &len);
  if (!str) {
    return js_mknum(0);  // Return 0 if not a valid string
  }

  return js_mknum(atof(str));
}

static jsval_t js_number_to_string(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) {
    return js_mkstr(js, "", 0);
  }

  uint8_t type = js_type(args[0]);

  if (type == JS_NUM) {
    char buf[32];
    double num = js_getnum(args[0]);
    snprintf(buf, sizeof(buf), "%.17g", num);
    return js_mkstr(js, buf, strlen(buf));
  } else if (type == JS_STR) {  // If it's already a string (like from parse_json_value), just return it
    return args[0];
  }

  return js_mkstr(js, "", 0);
}

/******************************************************************************
 * E2) String / number / random helpers
 ******************************************************************************/

// random() => [0,1); random(max) => int in [0,max); random(min,max) => int in [min,max)
static jsval_t js_random(struct js *js, jsval_t *args, int nargs) {
  double r = (double)esp_random() / 4294967296.0;  // hardware RNG, [0, 1)
  if (nargs == 0) return js_mknum(r);
  double lo = (nargs >= 2) ? js_getnum(args[0]) : 0.0;
  double hi = (nargs >= 2) ? js_getnum(args[1]) : js_getnum(args[0]);
  if (hi <= lo) return js_mknum(lo);
  return js_mknum(lo + floor(r * (hi - lo)));
}

// str_split(str, sep, idx) => idx-th field (0-based) or null; sep may be multi-char.
static jsval_t js_str_split(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) return js_mknull();
  String s = js_arg_str(js, args[0]);
  String sep = js_arg_str(js, args[1]);
  int idx = (int)js_getnum(args[2]);
  if (sep.length() == 0 || idx < 0) return js_mknull();
  int start = 0;
  for (int i = 0; i < idx; i++) {
    int p = s.indexOf(sep, start);
    if (p < 0) return js_mknull();
    start = p + sep.length();
  }
  int end = s.indexOf(sep, start);
  String field = (end < 0) ? s.substring(start) : s.substring(start, end);
  return js_mkstr(js, field.c_str(), field.length());
}

// str_split_count(str, sep) => number of fields ("" => 0, no sep => 1)
static jsval_t js_str_split_count(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknum(0);
  String s = js_arg_str(js, args[0]);
  String sep = js_arg_str(js, args[1]);
  if (s.length() == 0 || sep.length() == 0) return js_mknum(0);
  int count = 1, start = 0, p;
  while ((p = s.indexOf(sep, start)) >= 0) {
    count++;
    start = p + sep.length();
  }
  return js_mknum((double)count);
}

// format_number(value, decimals) => fixed-point string, e.g. (3.14159, 2) => "3.14"
static jsval_t js_format_number(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkstr(js, "", 0);
  double v = js_getnum(args[0]);
  int dec = (nargs >= 2) ? (int)js_getnum(args[1]) : 0;
  if (dec < 0) dec = 0;
  if (dec > 9) dec = 9;
  char buf[40];
  snprintf(buf, sizeof(buf), "%.*f", dec, v);
  return js_mkstr(js, buf, strlen(buf));
}

// pad_number(value, width) => zero-padded integer string, e.g. (7, 2) => "07"
static jsval_t js_pad_number(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mkstr(js, "", 0);
  long v = (long)js_getnum(args[0]);
  int w = (int)js_getnum(args[1]);
  if (w < 1) w = 1;
  if (w > 16) w = 16;
  char buf[24];
  snprintf(buf, sizeof(buf), "%0*ld", w, v);
  return js_mkstr(js, buf, strlen(buf));
}

/******************************************************************************
 * E3) Button events — short presses cross loopTask -> JS task via two single-writer counters (lock-free); long press = power off, never reaches JS.
 ******************************************************************************/
static volatile uint32_t g_button_evt_produced = 0;  // written by loopTask only
static volatile uint32_t g_button_evt_consumed = 0;  // written by JS task only
static char g_button_cb_name[56] = "";

extern "C" void webscreen_hardware_set_button_toggle(bool enabled);

// on_button("fn") => short presses call fn(1); on_button("") restores the default toggle.
static jsval_t js_on_button(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  String name = js_arg_str(js, args[0]);
  if (name.length() >= sizeof(g_button_cb_name)) {
    LOGF("on_button: function name too long (max %u chars)\n",
         (unsigned)(sizeof(g_button_cb_name) - 1));
    return js_mkfalse();
  }
  strlcpy(g_button_cb_name, name.c_str(), sizeof(g_button_cb_name));
  webscreen_hardware_set_button_toggle(g_button_cb_name[0] == '\0');
  return js_mktrue();
}

// get_button_event() => 1 if a short press was pending (consumes it), else 0.
static jsval_t js_get_button_event(struct js *js, jsval_t *args, int nargs) {
  if (g_button_evt_produced != g_button_evt_consumed) {
    g_button_evt_consumed++;
    return js_mknum(1);
  }
  return js_mknum(0);
}

// button_set_toggle(bool) => keep/suppress the default display on/off toggle when polling.
static jsval_t js_button_set_toggle(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  webscreen_hardware_set_button_toggle(js_truthy(js, args[0]));
  return js_mktrue();
}

// JS-task safe point: dispatch one pending press by name (Elk cannot hold function values across C callbacks).
static void elk_dispatch_button_event(void) {
  if (js == NULL || g_button_cb_name[0] == '\0') return;
  if (g_button_evt_produced == g_button_evt_consumed) return;
  g_button_evt_consumed++;
  char snippet[72];
  snprintf(snippet, sizeof(snippet), "%s(1);", g_button_cb_name);
  jsval_t res = js_eval(js, snippet, strlen(snippet));
  if (js_type(res) == JS_ERR) {
    const char *errstr = js_str(js, res);
    LOGF("[BUTTON CB] Error in %s: %s\n", snippet, errstr);
    char rec[128];
    snprintf(rec, sizeof(rec), "button %s: %s", g_button_cb_name, errstr ? errstr : "?");
    webscreen_runtime_note_js_error(rec);
  }
}
