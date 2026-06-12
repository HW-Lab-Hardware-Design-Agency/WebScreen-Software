// ws_lvgl_charts.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

/********************************************************************************
 * METER
 ********************************************************************************/
// Example calls:
//   lv_meter_create, lv_meter_add_scale, lv_meter_set_scale_ticks,
//   lv_meter_set_scale_major_ticks, lv_meter_set_scale_range
//   lv_meter_add_arc, lv_meter_add_scale_lines, lv_meter_add_needle_line,
//   lv_meter_add_needle_img
//   lv_meter_set_indicator_start_value, lv_meter_set_indicator_end_value, lv_meter_set_indicator_value

// Slot registries for meter scales and indicators (see g_chart_series note:
// these were raw pointers packed into doubles before — wild-pointer resets).
static const int MAX_METER_SCALES = 8;
static lv_meter_scale_t *g_meter_scales[MAX_METER_SCALES] = { nullptr };
static const int MAX_METER_INDICATORS = 16;
static lv_meter_indicator_t *g_meter_indicators[MAX_METER_INDICATORS] = { nullptr };

static int store_meter_scale(lv_meter_scale_t *sc) {
  for (int i = 0; i < MAX_METER_SCALES; i++) {
    if (!g_meter_scales[i]) {
      g_meter_scales[i] = sc;
      return i;
    }
  }
  return -1;
}
static lv_meter_scale_t *get_meter_scale(int handle) {
  if (handle < 0 || handle >= MAX_METER_SCALES) return nullptr;
  return g_meter_scales[handle];
}
static int store_meter_indicator(lv_meter_indicator_t *ind) {
  for (int i = 0; i < MAX_METER_INDICATORS; i++) {
    if (!g_meter_indicators[i]) {
      g_meter_indicators[i] = ind;
      return i;
    }
  }
  return -1;
}
static lv_meter_indicator_t *get_meter_indicator(int handle) {
  if (handle < 0 || handle >= MAX_METER_INDICATORS) return nullptr;
  return g_meter_indicators[handle];
}

static jsval_t js_lv_meter_create(struct js *js, jsval_t *args, int nargs) {  // no params
  lv_obj_t *m = lv_meter_create(lv_scr_act());
  int handle = store_lv_obj(m);
  return js_mknum(handle);
}

static jsval_t js_lv_meter_add_scale(struct js *js, jsval_t *args, int nargs) {  // (meterHandle) -> scale handle
  if (nargs < 1) return js_mknull();
  int mh = (int)js_getnum(args[0]);
  lv_obj_t *mt = get_lv_obj(mh);
  if (!mt) return js_mknull();

  lv_meter_scale_t *sc = lv_meter_add_scale(mt);
  if (!sc) return js_mknum(-1);
  int sh = store_meter_scale(sc);
  if (sh < 0) {
    // LVGL has no API to remove a scale; it stays in the meter (freed with it)
    // but is unreachable from JS.
    LOG("lv_meter_add_scale: no free scale slots");
    return js_mknum(-1);
  }
  return js_mknum(sh);
}

static jsval_t js_lv_meter_set_scale_ticks(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, cnt, width, length, color)
  if (nargs < 6) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int scH = (int)js_getnum(args[1]);
  int cnt = (int)js_getnum(args[2]);
  int width = (int)js_getnum(args[3]);
  int length = (int)js_getnum(args[4]);
  double col = js_getnum(args[5]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_set_scale_ticks: invalid scale handle %d\n", scH);
    return js_mknull();
  }
  lv_meter_set_scale_ticks(mt, sc, cnt, width, length, lv_color_hex((uint32_t)col));
  return js_mknull();
}

static jsval_t js_lv_meter_set_scale_major_ticks(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, freq, width, length, color, label_gap)
  if (nargs < 7) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int scH = (int)js_getnum(args[1]);
  int freq = (int)js_getnum(args[2]);
  int width = (int)js_getnum(args[3]);
  int length = (int)js_getnum(args[4]);
  double col = js_getnum(args[5]);
  int label_gap = (int)js_getnum(args[6]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();

  lv_meter_scale_t *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_set_scale_major_ticks: invalid scale handle %d\n", scH);
    return js_mknull();
  }
  lv_meter_set_scale_major_ticks(mt, sc, freq, width, length, lv_color_hex((uint32_t)col), label_gap);
  return js_mknull();
}

static jsval_t js_lv_meter_set_scale_range(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, min, max, angle_range, rotation)
  if (nargs < 6) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int scH = (int)js_getnum(args[1]);
  int minV = (int)js_getnum(args[2]);
  int maxV = (int)js_getnum(args[3]);
  int angleRange = (int)js_getnum(args[4]);
  int rotation = (int)js_getnum(args[5]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_set_scale_range: invalid scale handle %d\n", scH);
    return js_mknull();
  }
  lv_meter_set_scale_range(mt, sc, minV, maxV, angleRange, rotation);
  return js_mknull();
}

// meter indicator creation
static jsval_t js_lv_meter_add_arc(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, width, color, rMod)
  // returns indicator handle
  if (nargs < 5) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int scH = (int)js_getnum(args[1]);
  int width = (int)js_getnum(args[2]);
  double col = js_getnum(args[3]);
  int rMod = (int)js_getnum(args[4]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_add_arc: invalid scale handle %d\n", scH);
    return js_mknull();
  }

  lv_meter_indicator_t *ind = lv_meter_add_arc(mt, sc, width, lv_color_hex((uint32_t)col), rMod);
  if (!ind) return js_mknum(-1);
  int ih = store_meter_indicator(ind);
  if (ih < 0) LOG("lv_meter_add_arc: no free indicator slots");
  return js_mknum(ih);
}

static jsval_t js_lv_meter_add_scale_lines(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, color_main, color_grad, local, width_mod)
  // returns indicator handle
  if (nargs < 6) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int scH = (int)js_getnum(args[1]);
  double colorM = js_getnum(args[2]);
  double colorG = js_getnum(args[3]);
  bool local = (bool)js_getnum(args[4]);
  int widthMod = (int)js_getnum(args[5]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_add_scale_lines: invalid scale handle %d\n", scH);
    return js_mknull();
  }

  lv_meter_indicator_t *ind = lv_meter_add_scale_lines(mt, sc,
                                                       lv_color_hex((uint32_t)colorM),
                                                       lv_color_hex((uint32_t)colorG),
                                                       local, widthMod);
  if (!ind) return js_mknum(-1);
  int ih = store_meter_indicator(ind);
  if (ih < 0) LOG("lv_meter_add_scale_lines: no free indicator slots");
  return js_mknum(ih);
}

static jsval_t js_lv_meter_add_needle_line(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, width, color, rMod)
  if (nargs < 5) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int scH = (int)js_getnum(args[1]);
  int width = (int)js_getnum(args[2]);
  double col = js_getnum(args[3]);
  int rMod = (int)js_getnum(args[4]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_add_needle_line: invalid scale handle %d\n", scH);
    return js_mknull();
  }

  lv_meter_indicator_t *ind = lv_meter_add_needle_line(mt, sc, width, lv_color_hex((uint32_t)col), rMod);
  if (!ind) return js_mknum(-1);
  int ih = store_meter_indicator(ind);
  if (ih < 0) LOG("lv_meter_add_needle_line: no free indicator slots");
  return js_mknum(ih);
}

static jsval_t js_lv_meter_add_needle_img(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, ramImageSlot, pivot_x, pivot_y)
  // returns indicator handle
  if (nargs < 5) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int scH = (int)js_getnum(args[1]);
  // Historically this arg was a raw lv_img_dsc_t* cast from a double, but no
  // binding ever exposed such a pointer to JS, so every call dereferenced a
  // fabricated address. It is now a g_ram_images slot index (the descriptor
  // filled by load_image_file_into_ram).
  int imgSlot = (int)js_getnum(args[2]);
  int pivotX = (int)js_getnum(args[3]);
  int pivotY = (int)js_getnum(args[4]);

  if (imgSlot < 0 || imgSlot >= MAX_RAM_IMAGES || !g_ram_images[imgSlot].used) {
    LOGF("lv_meter_add_needle_img: invalid ram image slot %d\n", imgSlot);
    return js_mknull();
  }
  const lv_img_dsc_t *src_dsc = &g_ram_images[imgSlot].dsc;

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_scale_t *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_add_needle_img: invalid scale handle %d\n", scH);
    return js_mknull();
  }

  lv_meter_indicator_t *ind = lv_meter_add_needle_img(mt, sc, src_dsc, pivotX, pivotY);
  if (!ind) return js_mknum(-1);
  int ih = store_meter_indicator(ind);
  if (ih < 0) LOG("lv_meter_add_needle_img: no free indicator slots");
  return js_mknum(ih);
}

// meter set indicator
static jsval_t js_lv_meter_set_indicator_start_value(struct js *js, jsval_t *args, int nargs) {  // (meterH, indicatorHandle, startVal)
  if (nargs < 3) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int indH = (int)js_getnum(args[1]);
  int stVal = (int)js_getnum(args[2]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_indicator_t *ind = get_meter_indicator(indH);
  if (!ind) {
    LOGF("lv_meter_set_indicator_start_value: invalid indicator handle %d\n", indH);
    return js_mknull();
  }

  lv_meter_set_indicator_start_value(mt, ind, stVal);
  return js_mknull();
}

static jsval_t js_lv_meter_set_indicator_end_value(struct js *js, jsval_t *args, int nargs) {  // (meterH, indicatorHandle, endVal)
  if (nargs < 3) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int indH = (int)js_getnum(args[1]);
  int endVal = (int)js_getnum(args[2]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_indicator_t *ind = get_meter_indicator(indH);
  if (!ind) {
    LOGF("lv_meter_set_indicator_end_value: invalid indicator handle %d\n", indH);
    return js_mknull();
  }

  lv_meter_set_indicator_end_value(mt, ind, endVal);
  return js_mknull();
}

static jsval_t js_lv_meter_set_indicator_value(struct js *js, jsval_t *args, int nargs) {  // (meterH, indicatorHandle, val)
  if (nargs < 3) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int indH = (int)js_getnum(args[1]);
  int val = (int)js_getnum(args[2]);

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  lv_meter_indicator_t *ind = get_meter_indicator(indH);
  if (!ind) {
    LOGF("lv_meter_set_indicator_value: invalid indicator handle %d\n", indH);
    return js_mknull();
  }

  lv_meter_set_indicator_value(mt, ind, val);
  return js_mknull();
}

/********************************************************************************
 * SPAN
 ********************************************************************************/
// Slot registry for spans (see g_chart_series note: raw pointers packed into
// doubles before — wild-pointer resets).
static const int MAX_SPANS = 16;
static lv_span_t *g_spans[MAX_SPANS] = { nullptr };

static int store_span(lv_span_t *sp) {
  for (int i = 0; i < MAX_SPANS; i++) {
    if (!g_spans[i]) {
      g_spans[i] = sp;
      return i;
    }
  }
  return -1;
}
static lv_span_t *get_span(int handle) {
  if (handle < 0 || handle >= MAX_SPANS) return nullptr;
  return g_spans[handle];
}

// Copies span text into LVGL-owned storage, stripping the surrounding quotes
// js_str() leaves on string values (same treatment as label_set_text).
static void span_set_text_copy(lv_span_t *sp, const char *raw) {
  static char buf[256];  // single-task only: all JS runs on the WebScreenJS task
  size_t len = strlen(raw);
  const char *start = raw;
  if (len >= 2 && raw[0] == '"' && raw[len - 1] == '"') {
    start = raw + 1;
    len -= 2;
  }
  if (len >= sizeof(buf)) len = sizeof(buf) - 1;
  memcpy(buf, start, len);
  buf[len] = '\0';
  lv_span_set_text(sp, buf);
}

static jsval_t js_lv_spangroup_create(struct js *js, jsval_t *args, int nargs) {
  lv_obj_t *spg = lv_spangroup_create(lv_scr_act());
  int handle = store_lv_obj(spg);
  return js_mknum(handle);
}

static jsval_t js_lv_spangroup_set_align(struct js *js, jsval_t *args, int nargs) {  // (spangroupH, alignEnum=LV_TEXT_ALIGN_LEFT/CENTER/RIGHT/AUTO)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int alg = (int)js_getnum(args[1]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_spangroup_set_align(spg, (lv_text_align_t)alg);
  return js_mknull();
}

static jsval_t js_lv_spangroup_set_overflow(struct js *js, jsval_t *args, int nargs) {  // (spangroupH, overflowEnum=LV_SPAN_OVERFLOW_CLIP/ELLIPSIS)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int ovf = (int)js_getnum(args[1]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_spangroup_set_overflow(spg, (lv_span_overflow_t)ovf);
  return js_mknull();
}

static jsval_t js_lv_spangroup_set_indent(struct js *js, jsval_t *args, int nargs) {  // (spangroupH, indentPX)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int indent = (int)js_getnum(args[1]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_spangroup_set_indent(spg, indent);
  return js_mknull();
}

static jsval_t js_lv_spangroup_set_mode(struct js *js, jsval_t *args, int nargs) {  // (spangroupH, mode=LV_SPAN_MODE_FIXED/NOWRAP/BREAK)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int md = (int)js_getnum(args[1]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_spangroup_set_mode(spg, (lv_span_mode_t)md);
  return js_mknull();
}

static jsval_t js_lv_spangroup_new_span(struct js *js, jsval_t *args, int nargs) {  // (spangroupH) -> span handle
  if (nargs < 1) return js_mknull();
  int h = (int)js_getnum(args[0]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_span_t *sp = lv_spangroup_new_span(spg);
  if (!sp) return js_mknum(-1);
  int sh = store_span(sp);
  if (sh < 0) {
    // No free slot: delete the span again so it cannot leak unreferenced.
    LOG("lv_spangroup_new_span: no free span slots");
    lv_spangroup_del_span(spg, sp);
    return js_mknum(-1);
  }
  return js_mknum(sh);
}

static jsval_t js_lv_span_set_text(struct js *js, jsval_t *args, int nargs) {  // (spanHandle, text)
  if (nargs < 2) return js_mknull();
  int spanH = (int)js_getnum(args[0]);
  const char *txt = js_str(js, args[1]);
  if (!txt) return js_mknull();

  lv_span_t *sp = get_span(spanH);
  if (!sp) {
    LOGF("lv_span_set_text: invalid span handle %d\n", spanH);
    return js_mknull();
  }
  span_set_text_copy(sp, txt);
  return js_mknull();
}

static jsval_t js_lv_span_set_text_static(struct js *js, jsval_t *args, int nargs) {  // (spanHandle, text)
  // Name kept for API compatibility, but it now COPIES via lv_span_set_text():
  // js_str() returns a pointer into the Elk arena, which the mark-compact GC
  // moves/reclaims, so a "static" (non-copying) reference renders garbage
  // after the next js_gc().
  if (nargs < 2) return js_mknull();
  int spanH = (int)js_getnum(args[0]);
  const char *txt = js_str(js, args[1]);
  if (!txt) return js_mknull();

  lv_span_t *sp = get_span(spanH);
  if (!sp) {
    LOGF("lv_span_set_text_static: invalid span handle %d\n", spanH);
    return js_mknull();
  }
  span_set_text_copy(sp, txt);
  return js_mknull();
}

static jsval_t js_lv_spangroup_refr_mode(struct js *js, jsval_t *args, int nargs) {  // (spangroupH)
  if (nargs < 1) return js_mknull();
  int h = (int)js_getnum(args[0]);
  lv_obj_t *spg = get_lv_obj(h);
  if (!spg) return js_mknull();

  lv_spangroup_refr_mode(spg);
  return js_mknull();
}

/*******************************************************
 * LINE BRIDGING
 *******************************************************/

static jsval_t js_lv_line_create(struct js *js, jsval_t *args, int nargs) {
  lv_obj_t *line = lv_line_create(lv_scr_act());
  int handle = store_lv_obj(line);
  LOGF("lv_line_create => handle %d\n", handle);
  return js_mknum(handle);
}

// Setting points requires us to parse an array of {x,y} from JS or something
// For simplicity, here's a bridging that receives e.g. (lineH, x0, y0, x1, y1, x2, y2, ...)
static jsval_t js_lv_line_set_points(struct js *js, jsval_t *args, int nargs) {  // Must have at least (lineH, x0, y0)
  if (nargs < 3) return js_mknull();
  int h = (int)js_getnum(args[0]);

  // The rest are coordinate pairs
  int pairCount = (nargs - 1) / 2;  // minus 1 for the handle, then each 2 = one point
  if (pairCount < 1) return js_mknull();

  lv_obj_t *line = get_lv_obj(h);
  if (!line) return js_mknull();

  static lv_point_t points[32];        // up to 16 points
  if (pairCount > 16) pairCount = 16;  // clamp

  int idx = 1;  // start reading from arg[1]
  for (int i = 0; i < pairCount; i++) {
    int x = (int)js_getnum(args[idx++]);
    int y = (int)js_getnum(args[idx++]);
    points[i].x = x;
    points[i].y = y;
  }

  lv_line_set_points(line, points, pairCount);
  return js_mknull();
}

