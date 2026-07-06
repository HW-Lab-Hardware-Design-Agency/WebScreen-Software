// ws_lvgl_charts.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

/********************************************************************************
 * METER (LVGL 9: implemented on lv_scale — lv_meter was removed upstream)
 ********************************************************************************/
// The JS API keeps the LVGL 8 lv_meter_* names and signatures. A "meter" is a
// round lv_scale; indicators are children (needle line/image, arc) or scale
// sections. Records live in static pools so the registry sweep can null slots
// without leaking heap.

struct ws_meter_scale {
  lv_obj_t *scale;               // lv_scale widget (meter obj or extra child)
  int32_t min, max;
  int32_t angle_range, rotation;
};

enum : uint8_t { WS_MIND_NEEDLE_LINE, WS_MIND_NEEDLE_IMG, WS_MIND_ARC, WS_MIND_LINES };

struct ws_meter_indicator {
  uint8_t type;
  ws_meter_scale *sc;            // swept together with the scale slot (same owner)
  lv_obj_t *obj;                 // needle line/img or arc child (owned by the widget tree)
  lv_scale_section_t *section;   // WS_MIND_LINES only
  lv_style_t style_items;        // section styles; static storage, reset on slot reuse
  lv_style_t style_ind;
  bool styles_inited;
  int32_t r_mod;
  int32_t start, end;
};

static const int MAX_METER_SCALES = 8;
static ws_meter_scale g_meter_scale_pool[MAX_METER_SCALES];
static ws_meter_scale *g_meter_scales[MAX_METER_SCALES] = { nullptr };
static lv_obj_t *g_meter_scales_owner[MAX_METER_SCALES] = { nullptr };

static const int MAX_METER_INDICATORS = 16;
static ws_meter_indicator g_meter_indicator_pool[MAX_METER_INDICATORS];
static ws_meter_indicator *g_meter_indicators[MAX_METER_INDICATORS] = { nullptr };
static lv_obj_t *g_meter_indicators_owner[MAX_METER_INDICATORS] = { nullptr };

static int store_meter_scale(lv_obj_t *scale_widget, lv_obj_t *owner) {
  for (int i = 0; i < MAX_METER_SCALES; i++) {
    if (!g_meter_scales[i]) {
      ws_meter_scale *rec = &g_meter_scale_pool[i];
      rec->scale = scale_widget;
      rec->min = 0; rec->max = 100;
      rec->angle_range = 270; rec->rotation = 135;
      g_meter_scales[i] = rec;
      g_meter_scales_owner[i] = owner;
      return i;
    }
  }
  return -1;
}
static ws_meter_scale *get_meter_scale(int handle) {
  if (handle < 0 || handle >= MAX_METER_SCALES) return nullptr;
  return g_meter_scales[handle];
}
static int store_meter_indicator(uint8_t type, ws_meter_scale *sc, lv_obj_t *owner) {
  for (int i = 0; i < MAX_METER_INDICATORS; i++) {
    if (!g_meter_indicators[i]) {
      ws_meter_indicator *rec = &g_meter_indicator_pool[i];
      if (rec->styles_inited) {
        lv_style_reset(&rec->style_items);
        lv_style_reset(&rec->style_ind);
        rec->styles_inited = false;
      }
      rec->type = type;
      rec->sc = sc;
      rec->obj = nullptr;
      rec->section = nullptr;
      rec->r_mod = 0;
      rec->start = sc->min;
      rec->end = sc->min;
      g_meter_indicators[i] = rec;
      g_meter_indicators_owner[i] = owner;
      return i;
    }
  }
  return -1;
}
static ws_meter_indicator *get_meter_indicator(int handle) {
  if (handle < 0 || handle >= MAX_METER_INDICATORS) return nullptr;
  return g_meter_indicators[handle];
}

// Map a scale value to an absolute arc angle (degrees).
static int32_t ws_meter_value_to_angle(const ws_meter_scale *sc, int32_t v) {
  int32_t span = sc->max - sc->min;
  if (span == 0) span = 1;
  if (v < sc->min) v = sc->min;
  if (v > sc->max) v = sc->max;
  return sc->rotation + (int32_t)((int64_t)(v - sc->min) * sc->angle_range / span);
}

static void ws_meter_indicator_apply(ws_meter_indicator *rec) {
  switch (rec->type) {
    case WS_MIND_NEEDLE_LINE: {
      int32_t len = lv_obj_get_width(rec->sc->scale) / 2 + rec->r_mod;
      if (len < 1) len = 1;
      lv_scale_set_line_needle_value(rec->sc->scale, rec->obj, len, rec->end);
      break;
    }
    case WS_MIND_NEEDLE_IMG:
      lv_scale_set_image_needle_value(rec->sc->scale, rec->obj, rec->end);
      break;
    case WS_MIND_ARC:
      lv_arc_set_start_angle(rec->obj, ws_meter_value_to_angle(rec->sc, rec->start));
      lv_arc_set_end_angle(rec->obj, ws_meter_value_to_angle(rec->sc, rec->end));
      break;
    case WS_MIND_LINES:
      lv_scale_section_set_range(rec->section, rec->start, rec->end);
      lv_obj_invalidate(rec->sc->scale);
      break;
  }
}

static jsval_t js_lv_meter_create(struct js *js, jsval_t *args, int nargs) {  // no params
  lv_obj_t *m = lv_scale_create(lv_scr_act());
  lv_scale_set_mode(m, LV_SCALE_MODE_ROUND_INNER);
  lv_obj_set_size(m, 200, 200);              // lv_meter's old default face size
  lv_scale_set_angle_range(m, 270);          // lv_meter defaults
  lv_scale_set_rotation(m, 135);
  int handle = store_lv_obj(m);
  return js_mknum(handle);
}

static jsval_t js_lv_meter_add_scale(struct js *js, jsval_t *args, int nargs) {  // (meterHandle) -> scale handle
  if (nargs < 1) return js_mknull();
  int mh = (int)js_getnum(args[0]);
  lv_obj_t *mt = get_lv_obj(mh);
  if (!mt) return js_mknull();

  // First scale binds the meter widget itself; extra scales become
  // full-size child lv_scales stacked on top (multi-scale meters).
  lv_obj_t *scale_widget = mt;
  for (int i = 0; i < MAX_METER_SCALES; i++) {
    if (g_meter_scales[i] && g_meter_scales[i]->scale == mt) {
      scale_widget = lv_scale_create(mt);
      lv_scale_set_mode(scale_widget, LV_SCALE_MODE_ROUND_INNER);
      lv_obj_set_size(scale_widget, lv_pct(100), lv_pct(100));
      lv_obj_center(scale_widget);
      lv_scale_set_angle_range(scale_widget, 270);
      lv_scale_set_rotation(scale_widget, 135);
      break;
    }
  }
  int sh = store_meter_scale(scale_widget, mt);
  if (sh < 0) {
    LOG("lv_meter_add_scale: no free scale slots");
    if (scale_widget != mt) lv_obj_del(scale_widget);
    return js_mknum(-1);
  }
  return js_mknum(sh);
}

static jsval_t js_lv_meter_set_scale_ticks(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, cnt, width, length, color)
  if (nargs < 6) return js_mknull();
  int scH = (int)js_getnum(args[1]);
  int cnt = (int)js_getnum(args[2]);
  int width = (int)js_getnum(args[3]);
  int length = (int)js_getnum(args[4]);
  double col = js_getnum(args[5]);

  ws_meter_scale *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_set_scale_ticks: invalid scale handle %d\n", scH);
    return js_mknull();
  }
  lv_scale_set_total_tick_count(sc->scale, cnt);
  lv_obj_set_style_line_width(sc->scale, width, LV_PART_ITEMS);
  lv_obj_set_style_length(sc->scale, length, LV_PART_ITEMS);
  lv_obj_set_style_line_color(sc->scale, lv_color_hex((uint32_t)col), LV_PART_ITEMS);
  return js_mknull();
}

static jsval_t js_lv_meter_set_scale_major_ticks(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, freq, width, length, color, label_gap)
  if (nargs < 7) return js_mknull();
  int scH = (int)js_getnum(args[1]);
  int freq = (int)js_getnum(args[2]);
  int width = (int)js_getnum(args[3]);
  int length = (int)js_getnum(args[4]);
  double col = js_getnum(args[5]);
  int label_gap = (int)js_getnum(args[6]);
  (void)label_gap;  // no direct LVGL 9 equivalent; labels follow the major ticks

  ws_meter_scale *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_set_scale_major_ticks: invalid scale handle %d\n", scH);
    return js_mknull();
  }
  lv_scale_set_major_tick_every(sc->scale, freq);
  lv_scale_set_label_show(sc->scale, true);
  lv_obj_set_style_line_width(sc->scale, width, LV_PART_INDICATOR);
  lv_obj_set_style_length(sc->scale, length, LV_PART_INDICATOR);
  lv_obj_set_style_line_color(sc->scale, lv_color_hex((uint32_t)col), LV_PART_INDICATOR);
  return js_mknull();
}

static jsval_t js_lv_meter_set_scale_range(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, min, max, angle_range, rotation)
  if (nargs < 6) return js_mknull();
  int scH = (int)js_getnum(args[1]);
  int minV = (int)js_getnum(args[2]);
  int maxV = (int)js_getnum(args[3]);
  int angleRange = (int)js_getnum(args[4]);
  int rotation = (int)js_getnum(args[5]);

  ws_meter_scale *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_set_scale_range: invalid scale handle %d\n", scH);
    return js_mknull();
  }
  lv_scale_set_range(sc->scale, minV, maxV);
  lv_scale_set_angle_range(sc->scale, angleRange);
  lv_scale_set_rotation(sc->scale, rotation);
  sc->min = minV; sc->max = maxV;
  sc->angle_range = angleRange; sc->rotation = rotation;
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
  ws_meter_scale *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_add_arc: invalid scale handle %d\n", scH);
    return js_mknull();
  }

  int ih = store_meter_indicator(WS_MIND_ARC, sc, mt);
  if (ih < 0) { LOG("lv_meter_add_arc: no free indicator slots"); return js_mknum(-1); }
  ws_meter_indicator *rec = g_meter_indicators[ih];

  lv_obj_t *arc = lv_arc_create(sc->scale);
  lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
  lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);           // hide track
  lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, lv_color_hex((uint32_t)col), LV_PART_INDICATOR);
  int32_t grow = 2 * rMod;   // v8 r_mod grows/shrinks the indicator radius
  lv_obj_set_size(arc, lv_pct(100), lv_pct(100));
  if (grow != 0) {
    lv_obj_set_style_pad_all(arc, grow < 0 ? -grow : 0, LV_PART_MAIN);
  }
  lv_obj_center(arc);
  rec->obj = arc;
  rec->r_mod = rMod;
  ws_meter_indicator_apply(rec);
  return js_mknum(ih);
}

static jsval_t js_lv_meter_add_scale_lines(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, color_main, color_grad, local, width_mod)
  // returns indicator handle
  if (nargs < 6) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int scH = (int)js_getnum(args[1]);
  double colorM = js_getnum(args[2]);
  double colorG = js_getnum(args[3]);
  (void)colorG;  // tick-gradient has no LVGL 9 section equivalent
  bool local = (bool)js_getnum(args[4]);
  (void)local;
  int widthMod = (int)js_getnum(args[5]);
  (void)widthMod;

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  ws_meter_scale *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_add_scale_lines: invalid scale handle %d\n", scH);
    return js_mknull();
  }

  int ih = store_meter_indicator(WS_MIND_LINES, sc, mt);
  if (ih < 0) { LOG("lv_meter_add_scale_lines: no free indicator slots"); return js_mknum(-1); }
  ws_meter_indicator *rec = g_meter_indicators[ih];

  lv_style_init(&rec->style_items);
  lv_style_init(&rec->style_ind);
  rec->styles_inited = true;
  lv_style_set_line_color(&rec->style_items, lv_color_hex((uint32_t)colorM));
  lv_style_set_line_color(&rec->style_ind, lv_color_hex((uint32_t)colorM));

  rec->section = lv_scale_add_section(sc->scale);
  if (!rec->section) { g_meter_indicators[ih] = nullptr; return js_mknum(-1); }
  lv_scale_section_set_style(rec->section, LV_PART_ITEMS, &rec->style_items);
  lv_scale_section_set_style(rec->section, LV_PART_INDICATOR, &rec->style_ind);
  ws_meter_indicator_apply(rec);
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
  ws_meter_scale *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_add_needle_line: invalid scale handle %d\n", scH);
    return js_mknull();
  }

  int ih = store_meter_indicator(WS_MIND_NEEDLE_LINE, sc, mt);
  if (ih < 0) { LOG("lv_meter_add_needle_line: no free indicator slots"); return js_mknum(-1); }
  ws_meter_indicator *rec = g_meter_indicators[ih];

  lv_obj_t *needle = lv_line_create(sc->scale);
  lv_obj_set_style_line_width(needle, width, 0);
  lv_obj_set_style_line_color(needle, lv_color_hex((uint32_t)col), 0);
  lv_obj_set_style_line_rounded(needle, true, 0);
  rec->obj = needle;
  rec->r_mod = rMod;
  ws_meter_indicator_apply(rec);
  return js_mknum(ih);
}

static jsval_t js_lv_meter_add_needle_img(struct js *js, jsval_t *args, int nargs) {  // (meterH, scaleHandle, ramImageSlot, pivot_x, pivot_y)
  // returns indicator handle
  if (nargs < 5) return js_mknull();
  int mH = (int)js_getnum(args[0]);
  int scH = (int)js_getnum(args[1]);
  int imgSlot = (int)js_getnum(args[2]);   // g_ram_images slot (see load_image_file_into_ram)
  int pivotX = (int)js_getnum(args[3]);
  int pivotY = (int)js_getnum(args[4]);

  if (imgSlot < 0 || imgSlot >= MAX_RAM_IMAGES || !g_ram_images[imgSlot].used) {
    LOGF("lv_meter_add_needle_img: invalid ram image slot %d\n", imgSlot);
    return js_mknull();
  }
  const lv_img_dsc_t *src_dsc = &g_ram_images[imgSlot].dsc;

  lv_obj_t *mt = get_lv_obj(mH);
  if (!mt) return js_mknull();
  ws_meter_scale *sc = get_meter_scale(scH);
  if (!sc) {
    LOGF("lv_meter_add_needle_img: invalid scale handle %d\n", scH);
    return js_mknull();
  }

  int ih = store_meter_indicator(WS_MIND_NEEDLE_IMG, sc, mt);
  if (ih < 0) { LOG("lv_meter_add_needle_img: no free indicator slots"); return js_mknum(-1); }
  ws_meter_indicator *rec = g_meter_indicators[ih];

  lv_obj_t *img = lv_img_create(sc->scale);
  lv_img_set_src(img, src_dsc);
  lv_image_set_pivot(img, pivotX, pivotY);
  rec->obj = img;
  ws_meter_indicator_apply(rec);
  return js_mknum(ih);
}

// meter set indicator
static jsval_t js_lv_meter_set_indicator_start_value(struct js *js, jsval_t *args, int nargs) {  // (meterH, indicatorHandle, startVal)
  if (nargs < 3) return js_mknull();
  int indH = (int)js_getnum(args[1]);
  int stVal = (int)js_getnum(args[2]);

  ws_meter_indicator *ind = get_meter_indicator(indH);
  if (!ind) {
    LOGF("lv_meter_set_indicator_start_value: invalid indicator handle %d\n", indH);
    return js_mknull();
  }
  ind->start = stVal;
  ws_meter_indicator_apply(ind);
  return js_mknull();
}

static jsval_t js_lv_meter_set_indicator_end_value(struct js *js, jsval_t *args, int nargs) {  // (meterH, indicatorHandle, endVal)
  if (nargs < 3) return js_mknull();
  int indH = (int)js_getnum(args[1]);
  int endVal = (int)js_getnum(args[2]);

  ws_meter_indicator *ind = get_meter_indicator(indH);
  if (!ind) {
    LOGF("lv_meter_set_indicator_end_value: invalid indicator handle %d\n", indH);
    return js_mknull();
  }
  ind->end = endVal;
  ws_meter_indicator_apply(ind);
  return js_mknull();
}

static jsval_t js_lv_meter_set_indicator_value(struct js *js, jsval_t *args, int nargs) {  // (meterH, indicatorHandle, val)
  if (nargs < 3) return js_mknull();
  int indH = (int)js_getnum(args[1]);
  int val = (int)js_getnum(args[2]);

  ws_meter_indicator *ind = get_meter_indicator(indH);
  if (!ind) {
    LOGF("lv_meter_set_indicator_value: invalid indicator handle %d\n", indH);
    return js_mknull();
  }
  ind->start = val;   // lv_meter semantics: plain value sets both ends
  ind->end = val;
  ws_meter_indicator_apply(ind);
  return js_mknull();
}

/********************************************************************************
 * SPAN
 ********************************************************************************/
// Slot registry for spans (see g_chart_series note: raw pointers packed into
// doubles before — wild-pointer resets).
static const int MAX_SPANS = 16;
static lv_span_t *g_spans[MAX_SPANS] = { nullptr };
// Owning spangroup per slot (see g_chart_series_owner): lets js_obj_delete
// null span slots when the spangroup (or an ancestor) is deleted. Only
// meaningful while the matching g_spans entry is non-null.
static lv_obj_t *g_spans_owner[MAX_SPANS] = { nullptr };

static int store_span(lv_span_t *sp, lv_obj_t *owner) {
  for (int i = 0; i < MAX_SPANS; i++) {
    if (!g_spans[i]) {
      g_spans[i] = sp;
      g_spans_owner[i] = owner;
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
// The temp is heap-sized to the actual text (a fixed 256-byte buffer used to
// silently truncate longer spans); lv_span_set_text copies it internally, so
// the temp is freed right after.
static void span_set_text_copy(lv_span_t *sp, const char *raw) {
  size_t len = strlen(raw);
  const char *start = raw;
  if (len >= 2 && raw[0] == '"' && raw[len - 1] == '"') {
    start = raw + 1;
    len -= 2;
  }
  char *tmp = (char *)malloc(len + 1);
  if (!tmp) {
    LOG("span_set_text: out of memory, span text unchanged");
    return;
  }
  memcpy(tmp, start, len);
  tmp[len] = '\0';
  lv_span_set_text(sp, tmp);
  free(tmp);
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
  int sh = store_span(sp, spg);
  if (sh < 0) {
    // No free slot: delete the span again so it cannot leak unreferenced.
    LOG("lv_spangroup_new_span: no free span slots");
    lv_spangroup_delete_span(spg, sp);
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

  static lv_point_precise_t points[32];  // up to 16 points
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

/*******************************************************
 * SUB-OBJECT REGISTRY SWEEP
 *******************************************************/
// Releases every chart-series / meter-scale / meter-indicator / span slot
// whose owning widget is `root` or a descendant of it. Forward-declared in
// ws_lvgl_widgets.h and called by js_obj_delete BEFORE lv_obj_del: LVGL 8.3
// destructors free these sub-objects together with their widget, so a slot
// left behind would let a stale JS handle pass get_*() validation and reach
// freed memory. Walks each owner's parent chain (still valid at this point),
// same technique as the g_objects sweep. Slots whose registry entry is
// already null are skipped — their owner pointer may be stale (e.g. after
// elk_teardown_ui) and must not be dereferenced. Defined at the end of this
// fragment so every registry it sweeps is visible.
static void release_subobjects_owned_by(lv_obj_t *root) {
  for (int i = 0; i < MAX_CHART_SERIES; i++) {
    if (!g_chart_series[i]) continue;
    for (lv_obj_t *p = g_chart_series_owner[i]; p != nullptr; p = lv_obj_get_parent(p)) {
      if (p == root) {
        g_chart_series[i] = nullptr;
        g_chart_series_owner[i] = nullptr;
        break;
      }
    }
  }
  for (int i = 0; i < MAX_METER_SCALES; i++) {
    if (!g_meter_scales[i]) continue;
    for (lv_obj_t *p = g_meter_scales_owner[i]; p != nullptr; p = lv_obj_get_parent(p)) {
      if (p == root) {
        g_meter_scales[i] = nullptr;
        g_meter_scales_owner[i] = nullptr;
        break;
      }
    }
  }
  for (int i = 0; i < MAX_METER_INDICATORS; i++) {
    if (!g_meter_indicators[i]) continue;
    for (lv_obj_t *p = g_meter_indicators_owner[i]; p != nullptr; p = lv_obj_get_parent(p)) {
      if (p == root) {
        g_meter_indicators[i] = nullptr;
        g_meter_indicators_owner[i] = nullptr;
        break;
      }
    }
  }
  for (int i = 0; i < MAX_SPANS; i++) {
    if (!g_spans[i]) continue;
    for (lv_obj_t *p = g_spans_owner[i]; p != nullptr; p = lv_obj_get_parent(p)) {
      if (p == root) {
        g_spans[i] = nullptr;
        g_spans_owner[i] = nullptr;
        break;
      }
    }
  }
}

