// ws_lvgl_widgets.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

/******************************************************************************
 * G) Basic draw_label, draw_rect, show_image from SD
 ******************************************************************************/
static const lv_font_t *get_font_for_size(int size) {  // Map the integer size to specific built-in Montserrat fonts
  if (size == 20) return &lv_font_montserrat_20;
  if (size == 28) return &lv_font_montserrat_28;
  if (size == 34) return &lv_font_montserrat_34;
  if (size == 40) return &lv_font_montserrat_40;
  if (size == 44) return &lv_font_montserrat_44;
  if (size == 48) return &lv_font_montserrat_48;
  return &lv_font_montserrat_14;
}

// Forward declaration for store_lv_obj (defined later in the file)
static int store_lv_obj(lv_obj_t *obj);

// Forward declarations for the tracked style registry (defined later in the
// file) so draw_rect can allocate through it instead of leaking a bare `new`.
static int alloc_tracked_style(void);
static lv_style_t *get_lv_style(int handle);

static jsval_t js_lvgl_draw_label(struct js *js, jsval_t *args, int nargs) {  // We expect at least 3 args: text, x, y. 4th arg is optional fontSize
  if (nargs < 3) {
    LOG("draw_label: expects text, x, y, [fontSize]");
    return js_mknull();
  }

  const char *rawText = js_str(js, args[0]);
  if (!rawText) return js_mknull();
  String txt(rawText);
  if (txt.startsWith("\"") && txt.endsWith("\"")) {
    txt.remove(0, 1);
    txt.remove(txt.length() - 1, 1);
  }

  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_label_set_text(label, txt.c_str());
  lv_obj_set_pos(label, x, y);

  if (nargs >= 4) {
    int fontSize = (int)js_getnum(args[3]);
    const lv_font_t *font = get_font_for_size(fontSize);
    lv_obj_set_style_text_font(label, font, 0);
  }

  return js_mknull();
}

static jsval_t js_lvgl_draw_rect(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 4) {
    LOG("draw_rect: expects x, y, w, h [, color]");
    return js_mknum(-1);
  }
  int x = (int)js_getnum(args[0]);
  int y = (int)js_getnum(args[1]);
  int w = (int)js_getnum(args[2]);
  int h = (int)js_getnum(args[3]);

  // Optional color parameter (default: green 0x00ff00)
  uint32_t color = 0x00ff00;
  if (nargs >= 5) {
    color = (uint32_t)js_getnum(args[4]);
  }

  lv_obj_t *rect = lv_obj_create(lv_scr_act());
  lv_obj_set_size(rect, w, h);
  lv_obj_set_pos(rect, x, y);

  // Per-rect style allocated via the tracked registry so elk_teardown_ui()
  // frees it (a bare `new lv_style_t` here used to leak, invisible to
  // teardown). Registry full => the rect is still drawn, just unstyled.
  lv_style_t *styleRect = get_lv_style(alloc_tracked_style());
  if (styleRect) {
    lv_style_set_bg_color(styleRect, lv_color_hex(color));
    lv_style_set_radius(styleRect, 5);
    lv_obj_add_style(rect, styleRect, 0);
  } else {
    LOG("draw_rect: no free style slots, rect left unstyled");
  }

  int handle = store_lv_obj(rect);
  LOGF("draw_rect: at (%d,%d), size(%d,%d), color=0x%06X => handle %d\n", x, y, w, h, color, handle);
  return js_mknum(handle);
}

static jsval_t js_lvgl_show_image(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) {
    LOG("show_image: expects path,x,y");
    return js_mknull();
  }
  const char *rawPath = js_str(js, args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  if (!rawPath) {
    LOG("show_image: invalid path");
    return js_mknull();
  }
  // Build "S:/filename"
  String path(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }
  String lvglPath = "S:" + path;

  lv_obj_t *img = lv_img_create(lv_scr_act());
  lv_img_set_src(img, lvglPath.c_str());
  lv_obj_set_pos(img, x, y);

  LOGF("show_image: '%s' at (%d,%d)\n", lvglPath.c_str(), x, y);
  return js_mknull();
}

/******************************************************************************
 * G2) create_image, rotate_obj, move_obj, animate_obj (Object Handle Approach)
 ******************************************************************************/
// std::vector‑based registry ----
#include <vector>
#include <mutex>
static std::vector<lv_obj_t *> g_objects;
static std::mutex g_obj_mtx;

static int store_lv_obj(lv_obj_t *obj) {
  std::lock_guard<std::mutex> lock(g_obj_mtx);
  for (size_t i = 0; i < g_objects.size(); ++i)
    if (!g_objects[i]) {
      g_objects[i] = obj;
      return (int)i;
    }
  g_objects.push_back(obj);
  return (int)(g_objects.size() - 1);
}

static lv_obj_t *get_lv_obj(int h) {
  std::lock_guard<std::mutex> lock(g_obj_mtx);
  return (h >= 0 && h < (int)g_objects.size()) ? g_objects[h] : nullptr;
}

static void release_lv_obj(int h) {
  std::lock_guard<std::mutex> lock(g_obj_mtx);
  if (h >= 0 && h < (int)g_objects.size()) g_objects[h] = nullptr;
}

// Helper functions to extract RGB components from lv_color_t

uint8_t get_red(lv_color_t color) {
  return (color.full >> 11) & 0x1F;  // 5 bits
}
uint8_t get_green(lv_color_t color) {
  return (color.full >> 5) & 0x3F;  // 6 bits
}
uint8_t get_blue(lv_color_t color) {
  return color.full & 0x1F;  // 5 bits
}

// create_image("/messi.png", x,y) => returns handle
static jsval_t js_create_image(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) {
    LOG("create_image: expects path,x,y");
    return js_mknum(-1);
  }
  const char *rawPath = js_str(js, args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);
  if (!rawPath) return js_mknum(-1);

  String path(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }
  String fullPath = "S:" + path;

  lv_obj_t *img = lv_img_create(lv_scr_act());
  lv_img_set_src(img, fullPath.c_str());
  lv_obj_set_pos(img, x, y);

  int handle = store_lv_obj(img);
  LOGF("create_image: '%s' => handle %d\n", fullPath.c_str(), handle);
  return js_mknum(handle);
}

// create_image_from_ram("/somefile.bin", x, y)
static jsval_t js_create_image_from_ram(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) {
    LOG("create_image_from_ram: expects path, x, y");
    return js_mknum(-1);
  }

  const char *rawPath = js_str(js, args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);
  if (!rawPath) return js_mknum(-1);

  int slot = -1;
  for (int i = 0; i < MAX_RAM_IMAGES; i++) {
    if (!g_ram_images[i].used) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    LOG("No free RamImage slots!");
    return js_mknum(-1);
  }
  RamImage *ri = &g_ram_images[slot];

  String path = String(rawPath);
  if (path.startsWith("\"") && path.endsWith("\"")) {
    path = path.substring(1, path.length() - 1);
  }

  if (!load_image_file_into_ram(path.c_str(), ri)) {
    LOG("Could not load image into RAM");
    return js_mknum(-1);
  }

  lv_obj_t *img = lv_img_create(lv_scr_act());
  lv_img_set_src(img, &ri->dsc);  // <--- the magic

  lv_obj_set_pos(img, x, y);

  int handle = store_lv_obj(img);
  LOGF("create_image_from_ram: '%s' => ram slot=%d => handle %d\n",
       path.c_str(), slot, handle);
  return js_mknum(handle);
}

// ram_image_free(slot) => free a RamImage slot's PSRAM buffer and mark it
// reusable. Returns true on success, false for invalid/unused slots.
// CALLER CONTRACT: if a live lv_img widget still shows this slot, the app
// must delete that widget first (obj_delete) — LVGL keeps a raw pointer to
// the descriptor/buffer and would render freed PSRAM.
static jsval_t js_ram_image_free(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  int slot = (int)js_getnum(args[0]);
  if (slot < 0 || slot >= MAX_RAM_IMAGES || !g_ram_images[slot].used) {
    return js_mkfalse();
  }
  RamImage *ri = &g_ram_images[slot];
  if (ri->buffer != NULL) free(ri->buffer);
  ri->used = false;
  ri->buffer = NULL;
  ri->size = 0;
  memset(&ri->dsc, 0, sizeof(ri->dsc));
  LOGF("ram_image_free: slot %d released\n", slot);
  return js_mktrue();
}

// rotate_obj(handle, angle)
static jsval_t js_rotate_obj(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) {
    LOG("rotate_obj: expects handle, angle");
    return js_mknull();
  }
  int handle = (int)js_getnum(args[0]);
  int angle = (int)js_getnum(args[1]);  // 0..3600 => 0..360 deg

  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) {
    LOG("rotate_obj: invalid handle");
    return js_mknull();
  }
  // For lv_img in LVGL => set angle
  lv_img_set_angle(obj, angle);
  LOGF("rotate_obj: handle=%d angle=%d\n", handle, angle);
  return js_mknull();
}

// move_obj(handle, x, y)
static jsval_t js_move_obj(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) {
    LOG("move_obj: expects handle,x,y");
    return js_mknull();
  }
  int handle = (int)js_getnum(args[0]);
  int x = (int)js_getnum(args[1]);
  int y = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) {
    LOG("move_obj: invalid handle");
    return js_mknull();
  }
  lv_obj_set_pos(obj, x, y);
  LOGF("move_obj: handle=%d => pos(%d,%d)\n", handle, x, y);
  return js_mknull();
}

// We'll animate X + Y with two separate anims
static void anim_x_cb(void *var, int32_t v) {
  lv_obj_t *obj = (lv_obj_t *)var;
  lv_obj_set_x(obj, v);
}
static void anim_y_cb(void *var, int32_t v) {
  lv_obj_t *obj = (lv_obj_t *)var;
  lv_obj_set_y(obj, v);
}

// animate_obj(handle, x0,y0, x1,y1, duration)
static jsval_t js_animate_obj(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 5) {
    LOG("animate_obj: expects handle,x0,y0,x1,y1,[duration]");
    return js_mknull();
  }
  int handle = (int)js_getnum(args[0]);
  int x0 = (int)js_getnum(args[1]);
  int y0 = (int)js_getnum(args[2]);
  int x1 = (int)js_getnum(args[3]);
  int y1 = (int)js_getnum(args[4]);
  int duration = (nargs >= 6) ? (int)js_getnum(args[5]) : 1000;

  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) {
    LOG("animate_obj: invalid handle");
    return js_mknull();
  }
  // Start pos
  lv_obj_set_pos(obj, x0, y0);

  // Animate X
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, x0, x1);
  lv_anim_set_time(&a, duration);
  lv_anim_set_exec_cb(&a, anim_x_cb);
  lv_anim_start(&a);

  // Animate Y
  lv_anim_t a2;
  lv_anim_init(&a2);
  lv_anim_set_var(&a2, obj);
  lv_anim_set_values(&a2, y0, y1);
  lv_anim_set_time(&a2, duration);
  lv_anim_set_exec_cb(&a2, anim_y_cb);
  lv_anim_start(&a2);

  LOGF("animate_obj: handle=%d from(%d,%d) to(%d,%d), dur=%d\n",
       handle, x0, y0, x1, y1, duration);
  return js_mknull();
}

// obj_delete(handle) => deletes the LVGL object and frees its registry slot.
static jsval_t js_obj_delete(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) {
    LOG("obj_delete: expects handle");
    return js_mknull();
  }
  int handle = (int)js_getnum(args[0]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) {
    LOGF("obj_delete: invalid handle %d\n", handle);
    return js_mknull();
  }
  // lv_obj_del also deletes every descendant, so before deleting we sweep the
  // registry and null every handle whose object is the target or sits below
  // it in the widget tree (parent chains are still valid at this point).
  // This keeps child handles from dangling; objects re-parented OUT of this
  // subtree are unaffected. Must run before lv_obj_del.
  {
    std::lock_guard<std::mutex> lock(g_obj_mtx);
    for (size_t i = 0; i < g_objects.size(); ++i) {
      for (lv_obj_t *p = g_objects[i]; p != nullptr; p = lv_obj_get_parent(p)) {
        if (p == obj) {
          g_objects[i] = nullptr;
          break;
        }
      }
    }
  }
  lv_obj_del(obj);
  LOGF("obj_delete: handle %d deleted\n", handle);
  return js_mknull();
}

