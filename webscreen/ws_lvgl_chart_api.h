// Native LVGL 8 chart bindings; included by ws_lvgl_styles.h.
static bool valid_chart_axis(int axis) {
  return axis == LV_CHART_AXIS_PRIMARY_Y || axis == LV_CHART_AXIS_SECONDARY_Y ||
         axis == LV_CHART_AXIS_PRIMARY_X || axis == LV_CHART_AXIS_SECONDARY_X;
}

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
  // LVGL 8.3's destructor omits scatter X arrays. Leaving scatter mode releases
  // them through the native API before the destructor frees the series.
  lv_obj_add_event_cb(chart, [](lv_event_t *event) {
    lv_obj_t *object = lv_event_get_target(event);
    if (lv_chart_get_type(object) == LV_CHART_TYPE_SCATTER) {
      lv_chart_set_type(object, LV_CHART_TYPE_NONE);
    }
  }, LV_EVENT_DELETE, nullptr);
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
  if (!obj || !lv_obj_check_type(obj, &lv_chart_class)) return js_mknull();

  if (t < LV_CHART_TYPE_NONE || t > LV_CHART_TYPE_SCATTER) return js_mknull();
  if (lv_chart_get_type(obj) == t) return js_mknull();
  lv_chart_set_type(obj, (lv_chart_type_t)t);
  if (lv_chart_get_type(obj) == LV_CHART_TYPE_SCATTER) {
    for (auto *ser = lv_chart_get_series_next(obj, nullptr); ser; ser = lv_chart_get_series_next(obj, ser)) {
      ser->x_ext_buf_assigned = false;
      for (uint32_t i = 0; i < lv_chart_get_point_count(obj); i++) ser->x_points[i] = LV_CHART_POINT_NONE;
    }
  }
  return js_mknull();
}

static jsval_t js_lv_chart_set_div_line_count(struct js *js, jsval_t *args, int nargs) {  // (handle, y_div, x_div)
  if (nargs < 3) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int y_div = (int)js_getnum(args[1]);
  int x_div = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj || !lv_obj_check_type(obj, &lv_chart_class)) return js_mknull();

  lv_chart_set_div_line_count(obj, y_div, x_div);
  return js_mknull();
}

static jsval_t js_lv_chart_set_update_mode(struct js *js, jsval_t *args, int nargs) {  // (handle, mode)
  // e.g. mode = LV_CHART_UPDATE_MODE_SHIFT, LV_CHART_UPDATE_MODE_CIRCULAR
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int mode = (int)js_getnum(args[1]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj || !lv_obj_check_type(obj, &lv_chart_class)) return js_mknull();

  if (mode != LV_CHART_UPDATE_MODE_SHIFT && mode != LV_CHART_UPDATE_MODE_CIRCULAR) return js_mknull();
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
  if (!obj || !lv_obj_check_type(obj, &lv_chart_class)) return js_mknull();

  if (mx <= mn || mn < LV_COORD_MIN || mx > LV_COORD_MAX || !valid_chart_axis(axis)) return js_mknull();
  lv_chart_set_range(obj, (lv_chart_axis_t)axis, mn, mx);
  return js_mknull();
}

static jsval_t js_lv_chart_set_point_count(struct js *js, jsval_t *args, int nargs) {  // (handle, count)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int c = (int)js_getnum(args[1]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj || !lv_obj_check_type(obj, &lv_chart_class)) return js_mknull();

  if (c < 1 || c > 4096) return js_mknull();
  lv_chart_set_point_count(obj, c);
  return js_mknull();
}

static jsval_t js_lv_chart_refresh(struct js *js, jsval_t *args, int nargs) {  // (handle)
  if (nargs < 1) return js_mknull();
  int h = (int)js_getnum(args[0]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj || !lv_obj_check_type(obj, &lv_chart_class)) return js_mknull();

  lv_chart_refresh(obj);
  return js_mknull();
}

static jsval_t js_lv_chart_add_series(struct js *js, jsval_t *args, int nargs) {  // (handle, color, axis)
  if (nargs < 3) return js_mknull();
  int h = (int)js_getnum(args[0]);
  double col = js_getnum(args[1]);
  int axis = (int)js_getnum(args[2]);

  lv_obj_t *obj = get_lv_obj(h);
  if (!obj || !lv_obj_check_type(obj, &lv_chart_class)) return js_mknull();

  if (!valid_chart_axis(axis)) return js_mknull();
  bool available = false;
  for (auto *series : g_chart_series) if (!series) available = true;
  if (!available) return js_mknum(-1);
  lv_chart_series_t *ser = lv_chart_add_series(obj, lv_color_hex((uint32_t)col), (lv_chart_axis_t)axis);
  if (!ser) return js_mknum(-1);
  // LVGL 8.3 does not initialize the X-buffer ownership flag or its values.
  ser->x_ext_buf_assigned = false;
  if (lv_chart_get_type(obj) == LV_CHART_TYPE_SCATTER) {
    if (!ser->x_points) {
      lv_chart_remove_series(obj, ser);
      return js_mknum(-1);
    }
    for (uint32_t i = 0; i < lv_chart_get_point_count(obj); i++) ser->x_points[i] = LV_CHART_POINT_NONE;
  } else {
    ser->x_points = nullptr;
  }
  int sh = store_chart_series(ser, obj);
  if (sh < 0) {
    // No free slot: remove the series again so it cannot leak unreferenced.
    LOG("lv_chart_add_series: no free series slots");
    if (ser->x_points) lv_mem_free(ser->x_points);
    ser->x_points = nullptr;
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
  if (!chart || !lv_obj_check_type(chart, &lv_chart_class)) return js_mknull();

  lv_chart_series_t *ser = get_chart_series(sh);
  if (!ser || g_chart_series_owner[sh] != chart) {
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
  if (!chart || !lv_obj_check_type(chart, &lv_chart_class)) return js_mknull();

  lv_chart_series_t *ser = get_chart_series(sh);
  if (!ser || g_chart_series_owner[sh] != chart) {
    LOGF("lv_chart_set_next_value2: invalid series handle %d\n", sh);
    return js_mknull();
  }
  if (lv_chart_get_type(chart) != LV_CHART_TYPE_SCATTER) return js_mknull();
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
  if (!chart || !lv_obj_check_type(chart, &lv_chart_class)) return js_mknull();

  if (!valid_chart_axis(axis) || majorCnt < 2 || majorCnt > 1000 || minorCnt < 1 || minorCnt > 100) return js_mknull();
  lv_chart_set_axis_tick(chart, (lv_chart_axis_t)axis, majorLen, minorLen,
                         majorCnt, minorCnt, label, drawSiz);
  return js_mknull();
}

static jsval_t js_lv_chart_set_zoom_x(struct js *js, jsval_t *args, int nargs) {  // (chartH, zoom)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int zm = (int)js_getnum(args[1]);
  lv_obj_t *chart = get_lv_obj(h);
  if (!chart || !lv_obj_check_type(chart, &lv_chart_class)) return js_mknull();

  if (zm < 256 || zm > 4096) return js_mknull();
  lv_chart_set_zoom_x(chart, zm);
  return js_mknull();
}

static jsval_t js_lv_chart_set_zoom_y(struct js *js, jsval_t *args, int nargs) {  // (chartH, zoom)
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int zm = (int)js_getnum(args[1]);
  lv_obj_t *chart = get_lv_obj(h);
  if (!chart || !lv_obj_check_type(chart, &lv_chart_class)) return js_mknull();

  if (zm < 256 || zm > 4096) return js_mknull();
  lv_chart_set_zoom_y(chart, zm);
  return js_mknull();
}

static jsval_t js_lv_chart_get_y_array(struct js *js, jsval_t *args, int nargs) {  // (chartH, seriesHandle) -> returns a pointer number to the array
  if (nargs < 2) return js_mknull();
  int h = (int)js_getnum(args[0]);
  int sh = (int)js_getnum(args[1]);

  lv_obj_t *chart = get_lv_obj(h);
  if (!chart || !lv_obj_check_type(chart, &lv_chart_class)) return js_mknull();

  lv_chart_series_t *ser = get_chart_series(sh);
  if (!ser || g_chart_series_owner[sh] != chart) {
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
