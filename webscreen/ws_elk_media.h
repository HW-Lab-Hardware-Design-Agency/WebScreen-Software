// ws_elk_media.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

/******************************************************************************
 * F) Load GIF from SD => g_gifBuffer => "M:mygif"
 ******************************************************************************/

// The one GIF widget streaming from the 'M:' driver. Cleared via the
// LV_EVENT_DELETE callback so any deletion path (gif_free, obj_delete,
// lv_obj_clean during app teardown) leaves no dangling pointer here.
static lv_obj_t *g_gif_widget = NULL;

static void gif_widget_delete_cb(lv_event_t *e) {
  if (lv_event_get_target(e) == g_gif_widget) g_gif_widget = NULL;
}

bool load_gif_into_ram(const char *path) {
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    LOGF("Failed to open %s\n", path);
    return false;
  }
  size_t fileSize = f.size();
  LOGF("File %s is %u bytes\n", path, (unsigned)fileSize);

  uint8_t *tmp = (uint8_t *)ps_malloc(fileSize);
  if (!tmp) {
    LOGF("Failed to allocate %u bytes in PSRAM\n", (unsigned)fileSize);
    f.close();
    return false;
  }
  size_t bytesRead = f.read(tmp, fileSize);
  f.close();
  if (bytesRead < fileSize) {
    LOGF("Failed to read full file: only %u of %u\n",
         (unsigned)bytesRead, (unsigned)fileSize);
    free(tmp);
    return false;
  }
  // Replace only after a successful read. ORDER CONSTRAINT: any widget
  // streaming the old buffer through 'M:' must already be deleted (the
  // callers do this) — LVGL readers hold raw pointers into g_gifBuffer.
  if (g_gifBuffer != NULL) free(g_gifBuffer);
  g_gifBuffer = tmp;
  g_gifSize = fileSize;
  LOG("GIF loaded into PSRAM successfully");
  return true;
}

static jsval_t js_show_gif_from_sd(struct js *js, jsval_t *args, int nargs) {  // Check if we have enough arguments (path, x, y)
  if (nargs < 3) {
    LOG("show_gif_from_sd: expects path, x, y");
    return js_mknull();
  }

  // Argument 0: Get the path string
  String path = js_arg_str(js, args[0]);
  if (path.isEmpty()) return js_mknull();

  // Argument 1 & 2: Get the x and y coordinates
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  // Delete the previous GIF widget BEFORE load_gif_into_ram frees the buffer
  // it streams from; a live reader on a freed/replaced buffer reads garbage.
  if (g_gif_widget != NULL) {
    lv_obj_del(g_gif_widget);  // delete cb clears g_gif_widget
  }

  // Load the specified GIF file into RAM
  if (!load_gif_into_ram(path.c_str())) {
    LOG("Could not load GIF into RAM");
    return js_mknull();
  }

  // Create the LVGL gif object
  lv_obj_t *gif = lv_gif_create(lv_scr_act());
  // Set the source to the in-memory driver 'M'
  lv_gif_set_src(gif, "M:mygif");

  // Set the position using the x and y coordinates from JavaScript
  lv_obj_set_pos(gif, x, y);

  g_gif_widget = gif;
  lv_obj_add_event_cb(gif, gif_widget_delete_cb, LV_EVENT_DELETE, NULL);

  // Update the log to show the new coordinates
  LOGF("Showing GIF from memory driver (file was %s) at (%d,%d)\n", path.c_str(), x, y);
  return js_mknull();
}

// gif_free() => delete the tracked GIF widget, then free g_gifBuffer.
// Widget first: it streams from the buffer via 'M:'. Returns true if
// anything was freed.
static jsval_t js_gif_free(struct js *js, jsval_t *args, int nargs) {
  bool freed = false;
  if (g_gif_widget != NULL) {
    lv_obj_del(g_gif_widget);  // delete cb clears g_gif_widget
    freed = true;
  }
  if (g_gifBuffer != NULL) {
    free(g_gifBuffer);
    g_gifBuffer = NULL;
    freed = true;
  }
  g_gifSize = 0;
  return freed ? js_mktrue() : js_mkfalse();
}

/******************************************************************************
 * F2) Load image file from SD into a RamImage slot
 ******************************************************************************/

bool load_image_file_into_ram(const char *path, RamImage *outImg) {
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    LOGF("Failed to open %s\n", path);
    return false;
  }
  size_t fileSize = f.size();
  LOGF("File %s is %u bytes\n", path, (unsigned)fileSize);

  uint8_t *buf = (uint8_t *)ps_malloc(fileSize);
  if (!buf) {
    LOGF("Failed to allocate %u bytes in PSRAM\n", (unsigned)fileSize);
    f.close();
    return false;
  }

  size_t bytesRead = f.read(buf, fileSize);
  f.close();
  if (bytesRead < fileSize) {
    LOGF("Failed to read full file: only %u of %u\n",
         (unsigned)bytesRead, (unsigned)fileSize);
    free(buf);
    return false;
  }

  outImg->used = true;
  outImg->buffer = buf;
  outImg->size = fileSize;

  lv_img_dsc_t *d = &outImg->dsc;
  memset(d, 0, sizeof(*d));

  // Basic mandatory fields:
  d->data_size = fileSize;
  d->data = buf;
  d->header.magic = LV_IMAGE_HEADER_MAGIC;
  d->header.w = 200;
  d->header.h = 200;
  // Raw file bytes (PNG/JPG/GIF): the matching LVGL decoder parses the real
  // header, so w/h above are placeholders.
  d->header.cf = LV_COLOR_FORMAT_RAW;

  // If you can't know width/height from file alone, you may just guess or parse
  // For a PNG/JPG you'd typically use an external decoder to fill w,h

  LOG("Image loaded into PSRAM successfully");
  return true;
}

