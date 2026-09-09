// ws_elk_sd_ext.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

//~~~~~~~~~~~~~~~~~~~~~~~~~~~ 4) Extended SD ops ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// We already have sd_list_dir, sd_read_file, sd_write_file. Add file delete:
static jsval_t js_sd_delete_file(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  const char *path = js_str(js, args[0]);
  if (!path) return js_mkfalse();

  String fullPath = String(path);
  if (SD_MMC.exists(fullPath)) {
    bool ok = SD_MMC.remove(fullPath);
    return ok ? js_mktrue() : js_mkfalse();
  }
  return js_mkfalse();
}
