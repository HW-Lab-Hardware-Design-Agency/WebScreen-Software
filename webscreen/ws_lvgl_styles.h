// ws_lvgl_styles.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

/******************************************************************************
 * H) Style Handles + Full Style Setters
 ******************************************************************************/
static const int MAX_STYLES = 32;
static lv_style_t *g_style_map[MAX_STYLES] = { nullptr };

static lv_style_t *get_lv_style(int handle) {
  if (handle < 0 || handle >= MAX_STYLES) return nullptr;
  return g_style_map[handle];
}

// Allocates a registry-tracked lv_style_t and returns its handle, or -1 when
// all slots are taken. Tracked styles are freed by elk_teardown_ui().
static int alloc_tracked_style(void) {
  for (int i = 0; i < MAX_STYLES; i++) {
    if (!g_style_map[i]) {
      lv_style_t *st = new lv_style_t;
      lv_style_init(st);
      g_style_map[i] = st;
      return i;
    }
  }
  return -1;
}
static jsval_t js_create_label(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknum(-1);  // need x,y
  int x = (int)js_getnum(args[0]);
  int y = (int)js_getnum(args[1]);

  lv_obj_t *label = lv_label_create(lv_scr_act());
  lv_obj_set_pos(label, x, y);

  int handle = store_lv_obj(label);
  return js_mknum(handle);
}

static jsval_t js_label_set_text(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int lblHandle = (int)js_getnum(args[0]);
  const char *rawText = js_str(js, args[1]);
  if (!rawText) {
    LOG("label_set_text: invalid text argument");
    return js_mknull();
  }

  // Check memory before doing anything - fail early if critically low
  size_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < 8000) {
    LOGF("label_set_text: CRITICAL - memory too low (%u bytes), skipping\n", freeHeap);
    return js_mknull();
  }

  // Retrieve the lv_obj_t* from the handle
  lv_obj_t *label = get_lv_obj(lblHandle);
  if (!label) {
    LOGF("label_set_text: invalid handle %d\n", lblHandle);
    return js_mknull();
  }

  // Use static buffer to avoid heap allocation - max 256 chars
  static char textBuffer[256];
  size_t rawLen = strlen(rawText);

  // Strip surrounding quotes if present, using C-style operations
  const char *start = rawText;
  size_t len = rawLen;

  if (rawLen >= 2 && rawText[0] == '"' && rawText[rawLen - 1] == '"') {
    start = rawText + 1;
    len = rawLen - 2;
  }

  // Clamp to buffer size
  if (len >= sizeof(textBuffer)) {
    len = sizeof(textBuffer) - 1;
  }

  memcpy(textBuffer, start, len);
  textBuffer[len] = '\0';

  // Set the text
  lv_label_set_text(label, textBuffer);
  return js_mknull();
}

// style_set_text_font(styleHandle, fontSize)
static jsval_t js_style_set_text_font(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int styleH = (int)js_getnum(args[0]);
  int fontSize = (int)js_getnum(args[1]);

  // Convert the style handle to an lv_style_t*
  lv_style_t *st = get_lv_style(styleH);
  if (!st) return js_mknull();

  // Pick a built-in font
  const lv_font_t *font = get_font_for_size(fontSize);

  // Apply it
  lv_style_set_text_font(st, font);

  return js_mknull();
}

// create_style()
static jsval_t js_create_style(struct js *js, jsval_t *args, int nargs) {
  int handle = alloc_tracked_style();
  if (handle < 0) LOG("create_style => no free style slots");
  return js_mknum(handle);
}

// obj_add_style(objHandle, styleHandle, partOrState)
static jsval_t js_obj_add_style(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int objHandle = (int)js_getnum(args[0]);
  int styleHandle = (int)js_getnum(args[1]);
  int partState = 0;
  if (nargs >= 3) partState = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(objHandle);
  lv_style_t *st = get_lv_style(styleHandle);
  if (!obj || !st) {
    LOG("obj_add_style => invalid handle");
    return js_mknull();
  }
  lv_obj_add_style(obj, st, partState);
  return js_mknull();
}

// ***Full style property setters***
//
// All mechanical js_style_set_* bindings share one shape:
// (styleHandle, value) -> look up the style, apply one lv_style_set_* call,
// return null. They are generated from the table below; only
// js_style_set_text_font stays hand-written (size -> font pointer lookup,
// defined above).
//
// argtype is the intermediate C type the JS number is parsed into; cast is
// the type used at the LVGL call site. Both are kept per property so the
// generated code converts exactly like the former hand-written functions.
#define WS_STYLE_SETTER_NUM(fn, lv_setter, argtype, cast)        \
  static jsval_t fn(struct js *js, jsval_t *args, int nargs) {   \
    if (nargs < 2) return js_mknull();                           \
    int styleH = (int)js_getnum(args[0]);                        \
    argtype val = (argtype)js_getnum(args[1]);                   \
    lv_style_t *st = get_lv_style(styleH);                       \
    if (!st) return js_mknull();                                 \
    lv_setter(st, (cast)val);                                    \
    return js_mknull();                                          \
  }

// Color variant: numeric hex argument goes through lv_color_hex().
#define WS_STYLE_SETTER_COLOR(fn, lv_setter)                     \
  static jsval_t fn(struct js *js, jsval_t *args, int nargs) {   \
    if (nargs < 2) return js_mknull();                           \
    int styleH = (int)js_getnum(args[0]);                        \
    double color = js_getnum(args[1]);                           \
    lv_style_t *st = get_lv_style(styleH);                       \
    if (!st) return js_mknull();                                 \
    lv_setter(st, lv_color_hex((uint32_t)color));                \
    return js_mknull();                                          \
  }

// Background / border
WS_STYLE_SETTER_NUM(js_style_set_radius, lv_style_set_radius, int, lv_coord_t)
WS_STYLE_SETTER_NUM(js_style_set_bg_opa, lv_style_set_bg_opa, int, lv_opa_t)
WS_STYLE_SETTER_COLOR(js_style_set_bg_color, lv_style_set_bg_color)
WS_STYLE_SETTER_COLOR(js_style_set_border_color, lv_style_set_border_color)
WS_STYLE_SETTER_NUM(js_style_set_border_width, lv_style_set_border_width, int, int)
WS_STYLE_SETTER_NUM(js_style_set_border_opa, lv_style_set_border_opa, int, lv_opa_t)
WS_STYLE_SETTER_NUM(js_style_set_border_side, lv_style_set_border_side, int, lv_border_side_t)  // e.g. LV_BORDER_SIDE_BOTTOM|LV_BORDER_SIDE_RIGHT

// Outline
WS_STYLE_SETTER_NUM(js_style_set_outline_width, lv_style_set_outline_width, int, int)
WS_STYLE_SETTER_COLOR(js_style_set_outline_color, lv_style_set_outline_color)
WS_STYLE_SETTER_NUM(js_style_set_outline_pad, lv_style_set_outline_pad, int, int)

// Shadow
WS_STYLE_SETTER_NUM(js_style_set_shadow_width, lv_style_set_shadow_width, int, int)
WS_STYLE_SETTER_COLOR(js_style_set_shadow_color, lv_style_set_shadow_color)
WS_STYLE_SETTER_NUM(js_style_set_shadow_ofs_x, lv_style_set_shadow_ofs_x, int, int)
WS_STYLE_SETTER_NUM(js_style_set_shadow_ofs_y, lv_style_set_shadow_ofs_y, int, int)

// Image recolor, transform
WS_STYLE_SETTER_COLOR(js_style_set_img_recolor, lv_style_set_img_recolor)
WS_STYLE_SETTER_NUM(js_style_set_img_recolor_opa, lv_style_set_img_recolor_opa, int, lv_opa_t)
WS_STYLE_SETTER_NUM(js_style_set_transform_angle, lv_style_set_transform_angle, int, lv_coord_t)

// Text (text_font is hand-written above)
WS_STYLE_SETTER_NUM(js_style_set_text_align, lv_style_set_text_align, int, lv_text_align_t)  // LV_TEXT_ALIGN_LEFT=0, _CENTER=1, _RIGHT=2, _AUTO=3
WS_STYLE_SETTER_COLOR(js_style_set_text_color, lv_style_set_text_color)
WS_STYLE_SETTER_NUM(js_style_set_text_letter_space, lv_style_set_text_letter_space, int, int)
WS_STYLE_SETTER_NUM(js_style_set_text_line_space, lv_style_set_text_line_space, int, int)
WS_STYLE_SETTER_NUM(js_style_set_text_decor, lv_style_set_text_decor, int, lv_text_decor_t)  // e.g. LV_TEXT_DECOR_UNDERLINE

// Line
WS_STYLE_SETTER_COLOR(js_style_set_line_color, lv_style_set_line_color)
WS_STYLE_SETTER_NUM(js_style_set_line_width, lv_style_set_line_width, int, int)
WS_STYLE_SETTER_NUM(js_style_set_line_rounded, lv_style_set_line_rounded, bool, bool)

// Padding
WS_STYLE_SETTER_NUM(js_style_set_pad_all, lv_style_set_pad_all, int, int)
WS_STYLE_SETTER_NUM(js_style_set_pad_left, lv_style_set_pad_left, int, int)
WS_STYLE_SETTER_NUM(js_style_set_pad_right, lv_style_set_pad_right, int, int)
WS_STYLE_SETTER_NUM(js_style_set_pad_top, lv_style_set_pad_top, int, int)
WS_STYLE_SETTER_NUM(js_style_set_pad_bottom, lv_style_set_pad_bottom, int, int)
WS_STYLE_SETTER_NUM(js_style_set_pad_ver, lv_style_set_pad_ver, int, int)
WS_STYLE_SETTER_NUM(js_style_set_pad_hor, lv_style_set_pad_hor, int, int)

// Some dimension-related style props
WS_STYLE_SETTER_NUM(js_style_set_width, lv_style_set_width, int, lv_coord_t)
WS_STYLE_SETTER_NUM(js_style_set_height, lv_style_set_height, int, lv_coord_t)
WS_STYLE_SETTER_NUM(js_style_set_x, lv_style_set_x, double, lv_coord_t)
WS_STYLE_SETTER_NUM(js_style_set_y, lv_style_set_y, double, lv_coord_t)

/******************************************************************************
 * H2) Additional object property functions
 ******************************************************************************/
static jsval_t js_obj_set_size(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int w = (int)js_getnum(args[1]);
  int h = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) {
    LOGF("obj_set_size => invalid handle %d\n", handle);
    return js_mknull();
  }
  lv_obj_set_size(obj, w, h);
  return js_mknull();
}

// obj_align(objHandle, alignConst, xOfs, yOfs)
static jsval_t js_obj_align(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 4) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int alignVal = (int)js_getnum(args[1]);
  int xOfs = (int)js_getnum(args[2]);
  int yOfs = (int)js_getnum(args[3]);

  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) {
    LOGF("obj_align => invalid handle %d\n", handle);
    return js_mknull();
  }
  lv_obj_align(obj, (lv_align_t)alignVal, xOfs, yOfs);
  return js_mknull();
}

/******************************************************************************
 * ***ADDED FOR NEW EXAMPLES***
 * For scrolling, flex, flags, etc.
 ******************************************************************************/
static jsval_t js_obj_set_scroll_snap_x(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int snap_mode = (int)js_getnum(args[1]);  // numeric for LV_SCROLL_SNAP_x
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_scroll_snap_x(obj, (lv_scroll_snap_t)snap_mode);
  return js_mknull();
}

static jsval_t js_obj_set_scroll_snap_y(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int snap_mode = (int)js_getnum(args[1]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_scroll_snap_y(obj, (lv_scroll_snap_t)snap_mode);
  return js_mknull();
}

static jsval_t js_obj_add_flag(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int flag = (int)js_getnum(args[1]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_add_flag(obj, (lv_obj_flag_t)flag);
  return js_mknull();
}

static jsval_t js_obj_clear_flag(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int flag = (int)js_getnum(args[1]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_clear_flag(obj, (lv_obj_flag_t)flag);
  return js_mknull();
}

static jsval_t js_obj_set_scroll_dir(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int dir = (int)js_getnum(args[1]);  // e.g. LV_DIR_VER or ...
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_scroll_dir(obj, (lv_dir_t)dir);
  return js_mknull();
}

static jsval_t js_obj_set_scrollbar_mode(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int mode = (int)js_getnum(args[1]);  // e.g. LV_SCROLLBAR_MODE_OFF
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_scrollbar_mode(obj, (lv_scrollbar_mode_t)mode);
  return js_mknull();
}

static jsval_t js_obj_set_flex_flow(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int flowEnum = (int)js_getnum(args[1]);  // e.g. LV_FLEX_FLOW_ROW_WRAP
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_flex_flow(obj, (lv_flex_flow_t)flowEnum);
  return js_mknull();
}

static jsval_t js_obj_set_flex_align(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 4) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int main_place = (int)js_getnum(args[1]);
  int cross_place = (int)js_getnum(args[2]);
  int track_place = (int)js_getnum(args[3]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_flex_align(obj, (lv_flex_align_t)main_place,
                        (lv_flex_align_t)cross_place,
                        (lv_flex_align_t)track_place);
  return js_mknull();
}

static jsval_t js_obj_set_style_clip_corner(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  bool en = (bool)js_getnum(args[1]);
  int part = (int)js_getnum(args[2]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_style_clip_corner(obj, en, part);
  return js_mknull();
}

static jsval_t js_obj_set_style_base_dir(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) return js_mknull();
  int handle = (int)js_getnum(args[0]);
  int base_dir = (int)js_getnum(args[1]);  // e.g. LV_BASE_DIR_RTL
  int part = (int)js_getnum(args[2]);
  lv_obj_t *obj = get_lv_obj(handle);
  if (!obj) return js_mknull();
  lv_obj_set_style_base_dir(obj, (lv_base_dir_t)base_dir, part);
  return js_mknull();
}

/*******************************************************
 * CHART BRIDGING
 *******************************************************/

// Slot registry for chart series (mirrors g_style_map). Series, meter scales,
// meter indicators and spans are not lv_obj_t, so they cannot live in
// g_objects; they used to be handed to JS as raw pointers packed into doubles
// and cast straight back — any wrong/stale/NaN number became a wild pointer
// dereference inside LVGL (LoadProhibited reset). JS now gets a small slot
// index; apps that just pass the returned number back in keep working.
static const int MAX_CHART_SERIES = 16;
static lv_chart_series_t *g_chart_series[MAX_CHART_SERIES] = { nullptr };
// Owning chart per slot: lv_obj_del frees a chart's series with it, so
// js_obj_delete (via release_subobjects_owned_by) uses this to null every
// slot owned by a deleted widget subtree. Only meaningful while the matching
// g_chart_series entry is non-null.
static lv_obj_t *g_chart_series_owner[MAX_CHART_SERIES] = { nullptr };

static int store_chart_series(lv_chart_series_t *ser, lv_obj_t *owner) {
  for (int i = 0; i < MAX_CHART_SERIES; i++) {
    if (!g_chart_series[i]) {
      g_chart_series[i] = ser;
      g_chart_series_owner[i] = owner;
      return i;
    }
  }
  return -1;
}
static lv_chart_series_t *get_chart_series(int handle) {
  if (handle < 0 || handle >= MAX_CHART_SERIES) return nullptr;
  return g_chart_series[handle];
}

static jsval_t js_lv_chart_create(struct js *js, jsval_t *args, int nargs) {  // Creates a chart object on the current screen
  lv_obj_t *chart = lv_chart_create(lv_scr_act());
  // Optionally set default size or alignment
  lv_obj_set_size(chart, 200, 150);
  lv_obj_center(chart);

  // Store in your handle-based system
  int handle = store_lv_obj(chart);
  LOGF("lv_chart_create => handle %d\n", handle);

  // Return handle to JS
  return js_mknum(handle);
}

static jsval_t js_lv_chart_set_type(struct js *js, jsval_t *args, int nargs) {  // (handle, lv_chart_type int)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int t = (int)js_getnum(args[1]);  // e.g. LV_CHART_TYPE_LINE, LV_CHART_TYPE_BAR, etc.

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_set_type(obj, (lv_chart_type_t)t);
  return js_mknull();
}

static jsval_t js_lv_chart_set_div_line_count(struct js *js, jsval_t *args, int nargs) {  // (handle, y_div, x_div)
  if (nargs < 3) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int y_div = (int)js_getnum(args[1]);
  int x_div = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_set_div_line_count(obj, y_div, x_div);
  return js_mknull();
}

static jsval_t js_lv_chart_set_update_mode(struct js *js, jsval_t *args, int nargs) {  // (handle, mode)
  // e.g. mode = LV_CHART_UPDATE_MODE_SHIFT, LV_CHART_UPDATE_MODE_CIRCULAR
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int mode = (int)js_getnum(args[1]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_set_update_mode(obj, (lv_chart_update_mode_t)mode);
  return js_mknull();
}

static jsval_t js_lv_chart_set_range(struct js *js, jsval_t *args, int nargs) {  // (handle, axis, min, max)
  // e.g. axis=LV_CHART_AXIS_PRIMARY_Y, min=0, max=100
  if (nargs < 4) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int axis = (int)js_getnum(args[1]);
  int mn = (int)js_getnum(args[2]);
  int mx = (int)js_getnum(args[3]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_set_range(obj, (lv_chart_axis_t)axis, mn, mx);
  return js_mknull();
}

static jsval_t js_lv_chart_set_point_count(struct js *js, jsval_t *args, int nargs) {  // (handle, count)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int c = (int)js_getnum(args[1]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_set_point_count(obj, c);
  return js_mknull();
}

static jsval_t js_lv_chart_refresh(struct js *js, jsval_t *args, int nargs) {  // (handle)
  if (nargs < 1) return js_mknull();
  int h = (int)js_getnum(args[0]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_refresh(obj);
  return js_mknull();
}

static jsval_t js_lv_chart_add_series(struct js *js, jsval_t *args, int nargs) {  // (handle, color, axis)
  if (nargs < 3) return js_mknull();
  int h = (int)js_getnum(args[0]);
  double col = js_getnum(args[1]);
  int axis = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj) return js_mknull();

  lv_chart_series_t *ser = lv_chart_add_series(obj, lv_color_hex((uint32_t)col), (lv_chart_axis_t)axis);
  if (!ser) return js_mknum(-1);
  int sh = store_chart_series(ser, obj);
  if (sh < 0) {
    // No free slot: remove the series again so it cannot leak unreferenced.
    LOG("lv_chart_add_series: no free series slots");
    lv_chart_remove_series(obj, ser);
    return js_mknum(-1);
  }
  return js_mknum(sh);
}

static jsval_t js_lv_chart_set_next_value(struct js *js, jsval_t *args, int nargs) {  // (chartHandle, seriesHandle, value)
  if (nargs < 3) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int sh = (int)js_getnum(args[1]);
  int val = (int)js_getnum(args[2]);

  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  lv_chart_series_t *ser = get_chart_series(sh);
  if (!ser) {
    LOGF("lv_chart_set_next_value: invalid series handle %d\n", sh);
    return js_mknull();
  }
  lv_chart_set_next_value(chart, ser, val);
  return js_mknull();
}

static jsval_t js_lv_chart_set_next_value2(struct js *js, jsval_t *args, int nargs) {  // (chartHandle, seriesHandle, xVal, yVal)
  if (nargs < 4) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int sh = (int)js_getnum(args[1]);
  int xval = (int)js_getnum(args[2]);
  int yval = (int)js_getnum(args[3]);

  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  lv_chart_series_t *ser = get_chart_series(sh);
  if (!ser) {
    LOGF("lv_chart_set_next_value2: invalid series handle %d\n", sh);
    return js_mknull();
  }
  lv_chart_set_next_value2(chart, ser, xval, yval);
  return js_mknull();
}

static jsval_t js_lv_chart_set_axis_tick(struct js *js, jsval_t *args, int nargs) {  // (chartH, axis, majorLen, minorLen, majorCnt, minorCnt, label_en, draw_size)
  if (nargs < 8) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int axis = (int)js_getnum(args[1]);
  int majorLen = (int)js_getnum(args[2]);
  int minorLen = (int)js_getnum(args[3]);
  int majorCnt = (int)js_getnum(args[4]);
  int minorCnt = (int)js_getnum(args[5]);
  bool label = (bool)js_getnum(args[6]);
  int drawSiz = (int)js_getnum(args[7]);

  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  // LVGL 9 removed chart axis ticks (use an lv_scale next to the chart);
  // kept as a no-op so LVGL 8 era scripts keep running.
  (void)chart; (void)axis; (void)majorLen; (void)minorLen;
  return js_mknull();
}

static jsval_t js_lv_chart_set_zoom_x(struct js *js, jsval_t *args, int nargs) {  // (chartH, zoom)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int zm = (int)js_getnum(args[1]);
  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  // LVGL 9 removed chart zoom; no-op for script compatibility.
  (void)chart; (void)zm;
  return js_mknull();
}

static jsval_t js_lv_chart_set_zoom_y(struct js *js, jsval_t *args, int nargs) {  // (chartH, zoom)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int zm = (int)js_getnum(args[1]);
  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  // LVGL 9 removed chart zoom; no-op for script compatibility.
  (void)chart; (void)zm;
  return js_mknull();
}

static jsval_t js_lv_chart_get_y_array(struct js *js, jsval_t *args, int nargs) {  // (chartH, seriesHandle) -> returns a pointer number to the array
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int sh = (int)js_getnum(args[1]);

  lv_obj_t *chart = get_lv_obj(h);
  if (!chart) return js_mknull();

  lv_chart_series_t *ser = get_chart_series(sh);
  if (!ser) {
    LOGF("lv_chart_get_y_array: invalid series handle %d\n", sh);
    return js_mknull();
  }
  lv_coord_t *arr = lv_chart_get_y_array(chart, ser);
  // Still returned as a numeric address for compatibility; JS has no way to
  // dereference it and no binding accepts raw pointers anymore, so it is inert.
  intptr_t ret = (intptr_t)arr;
  return js_mknum((double)ret);
}

// Similarly you can add bridging for lv_chart_set_ext_y_array, lv_chart_set_ext_x_array,
// lv_chart_get_x_array, lv_chart_get_pressed_point, lv_chart_set_cursor_point, etc.
// if your examples require them.
