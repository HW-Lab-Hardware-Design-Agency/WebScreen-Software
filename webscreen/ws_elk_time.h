// ws_elk_time.h — fragment of the WebScreen Elk/LVGL bridge; included once, in order, by lvgl_elk.h (not standalone).

/******************************************************************************
 * H2) Display Brightness API
 ******************************************************************************/

static jsval_t js_set_brightness(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mknum(-1);
  int val = (int)js_getnum(args[0]);
  if (val < 0) val = 0;
  if (val > 255) val = 255;
  lcd_brightness((uint8_t)val);
  webscreen_hardware_sync_brightness((uint8_t)val);
  return js_mknum(val);
}

static jsval_t js_get_brightness(struct js *js, jsval_t *args, int nargs) {
  return js_mknum((double)webscreen_display_get_brightness());
}

/******************************************************************************
 * H3) NTP Time API
 ******************************************************************************/

static bool webscreen_get_local_time(struct tm *result) {
  time_t now = time(nullptr);
  return now >= 1609459200 && localtime_r(&now, result) != nullptr;
}

static jsval_t js_get_hours(struct js *js, jsval_t *args, int nargs) {
  struct tm timeinfo;
  if (!webscreen_get_local_time(&timeinfo)) return js_mknum(-1);
  return js_mknum((double)timeinfo.tm_hour);
}

static jsval_t js_get_minutes(struct js *js, jsval_t *args, int nargs) {
  struct tm timeinfo;
  if (!webscreen_get_local_time(&timeinfo)) return js_mknum(-1);
  return js_mknum((double)timeinfo.tm_min);
}

static jsval_t js_get_seconds(struct js *js, jsval_t *args, int nargs) {
  struct tm timeinfo;
  if (!webscreen_get_local_time(&timeinfo)) return js_mknum(-1);
  return js_mknum((double)timeinfo.tm_sec);
}

static jsval_t js_get_year(struct js *js, jsval_t *args, int nargs) {
  struct tm timeinfo;
  if (!webscreen_get_local_time(&timeinfo)) return js_mknum(-1);
  return js_mknum((double)(timeinfo.tm_year + 1900));
}

static jsval_t js_get_month(struct js *js, jsval_t *args, int nargs) {
  struct tm timeinfo;
  if (!webscreen_get_local_time(&timeinfo)) return js_mknum(-1);
  return js_mknum((double)(timeinfo.tm_mon + 1));  // 1-12
}

static jsval_t js_get_day(struct js *js, jsval_t *args, int nargs) {
  struct tm timeinfo;
  if (!webscreen_get_local_time(&timeinfo)) return js_mknum(-1);
  return js_mknum((double)timeinfo.tm_mday);
}

static jsval_t js_get_weekday(struct js *js, jsval_t *args, int nargs) {
  struct tm timeinfo;
  if (!webscreen_get_local_time(&timeinfo)) return js_mknum(-1);
  return js_mknum((double)timeinfo.tm_wday);  // 0=Sunday, 6=Saturday
}

static jsval_t js_get_epoch(struct js *js, jsval_t *args, int nargs) {
  time_t now;
  time(&now);
  if (now < 1609459200) return js_mknum(-1);  // Before 2021-01-01
  return js_mknum((double)now);
}

static jsval_t js_ntp_synced(struct js *js, jsval_t *args, int nargs) {
  struct tm timeinfo;
  if (webscreen_get_local_time(&timeinfo) && timeinfo.tm_year > (2020 - 1900)) {
    return js_mktrue();
  }
  return js_mkfalse();
}

// format_time(fmt[, epoch]) => strftime of local time, e.g. ("%H:%M:%S") => "14:05:09"
static jsval_t js_format_time(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkstr(js, "", 0);
  String fmt = js_arg_str(js, args[0]);
  if (fmt.length() == 0) return js_mkstr(js, "", 0);
  time_t t;
  if (nargs >= 2) {
    double epoch = js_getnum(args[1]);
    if (js_type(args[1]) != JS_NUM || !isfinite(epoch) || epoch < 0 || epoch > 253402300799.0) {
      return js_mkerr(js, "epoch must be between 1970 and 9999");
    }
    t = (time_t)epoch;
  } else {
    time(&t);
  }
  struct tm timeinfo;
  if (!localtime_r(&t, &timeinfo)) return js_mkstr(js, "", 0);
  char buf[64];
  size_t n = strftime(buf, sizeof(buf), fmt.c_str(), &timeinfo);
  return js_mkstr(js, buf, n);
}
