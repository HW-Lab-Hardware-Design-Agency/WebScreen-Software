// ws_elk_runtime.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

/******************************************************************************
 * I) In-place app teardown helpers
 *
 * Called by webscreen_runtime.cpp (the only TU that includes this header)
 * from the JS task's own loop — the task that owns LVGL — so no cross-task
 * locking is needed. Order matters: UI first (deletes lv_img widgets that
 * reference RAM-image descriptors), then media buffers, then comm state.
 ******************************************************************************/

// 1) Timers, animations, widgets, object handles, styles
static void elk_teardown_ui() {
  delete_all_elk_timers();
  lv_anim_del_all();
  lv_obj_clean(lv_scr_act());
  {
    std::lock_guard<std::mutex> lock(g_obj_mtx);
    g_objects.clear();
  }
  for (int i = 0; i < MAX_STYLES; i++) {
    if (g_style_map[i] != nullptr) {
      lv_style_reset(g_style_map[i]);
      delete g_style_map[i];
      g_style_map[i] = nullptr;
    }
  }
  // Sub-object registries hold pointers into widgets lv_obj_clean just
  // deleted; null them so slots become reusable and are never served stale.
  for (int i = 0; i < MAX_CHART_SERIES; i++) g_chart_series[i] = nullptr;
  for (int i = 0; i < MAX_METER_SCALES; i++) g_meter_scales[i] = nullptr;
  for (int i = 0; i < MAX_METER_INDICATORS; i++) g_meter_indicators[i] = nullptr;
  for (int i = 0; i < MAX_SPANS; i++) g_spans[i] = nullptr;
  g_js_error_streak = 0;
}

// 2) PSRAM media buffers (widgets referencing them are already gone)
static void elk_teardown_media() {
  for (int i = 0; i < MAX_RAM_IMAGES; i++) {
    if (g_ram_images[i].buffer != NULL) {
      free(g_ram_images[i].buffer);
    }
    g_ram_images[i].used = false;
    g_ram_images[i].buffer = NULL;
    g_ram_images[i].size = 0;
  }
  if (g_gifBuffer != NULL) {
    free(g_gifBuffer);
    g_gifBuffer = NULL;
  }
  g_gifSize = 0;
}

// 3) Communication state owned by the old script
static void elk_teardown_comm() {
  if (g_mqttClient.connected()) {
    g_mqttClient.disconnect();
  }
  g_mqttCallbackName[0] = '\0';
  g_mqttMsgPending = false;
  g_mqttMsgReady = false;
  g_http_headers.clear();
  g_js_gc_requested = false;
}

/******************************************************************************
 * J) Register All JS Functions
 ******************************************************************************/

void register_js_functions() {
  jsval_t global = js_glob(js);

  // Basic
  js_set(js, global, "print", js_mkfun(js_print));
  js_set(js, global, "mem_stats", js_mkfun(js_mem_stats));
  js_set(js, global, "mem_info", js_mkfun(js_mem_info));
  js_set(js, global, "gc", js_mkfun(js_request_gc));
  js_set(js, global, "timer_delete", js_mkfun(js_timer_delete));
  js_set(js, global, "wifi_connect", js_mkfun(js_wifi_connect));
  js_set(js, global, "wifi_status", js_mkfun(js_wifi_status));
  js_set(js, global, "wifi_get_ip", js_mkfun(js_wifi_get_ip));
  js_set(js, global, "delay", js_mkfun(js_delay));
  js_set(js, global, "get_millis", js_mkfun(js_get_millis));
  js_set(js, global, "str_length", js_mkfun(js_str_length));
  js_set(js, global, "set_brightness", js_mkfun(js_set_brightness));
  js_set(js, global, "get_brightness", js_mkfun(js_get_brightness));

  // NTP Time functions
  js_set(js, global, "get_hours", js_mkfun(js_get_hours));
  js_set(js, global, "get_minutes", js_mkfun(js_get_minutes));
  js_set(js, global, "get_seconds", js_mkfun(js_get_seconds));
  js_set(js, global, "get_year", js_mkfun(js_get_year));
  js_set(js, global, "get_month", js_mkfun(js_get_month));
  js_set(js, global, "get_day", js_mkfun(js_get_day));
  js_set(js, global, "get_weekday", js_mkfun(js_get_weekday));
  js_set(js, global, "get_epoch", js_mkfun(js_get_epoch));
  js_set(js, global, "ntp_synced", js_mkfun(js_ntp_synced));

  js_set(js, global, "create_timer", js_mkfun(js_create_timer));
  js_set(js, global, "toNumber", js_mkfun(js_to_number));
  js_set(js, global, "numberToString", js_mkfun(js_number_to_string));

  // bridging for indexOf / substring
  js_set(js, global, "str_index_of", js_mkfun(js_str_index_of));
  js_set(js, global, "str_substring", js_mkfun(js_str_substring));

  js_set(js, global, "http_get", js_mkfun(js_http_get));
  js_set(js, global, "http_post", js_mkfun(js_http_post));
  js_set(js, global, "http_delete", js_mkfun(js_http_delete));
  js_set(js, global, "http_set_ca_cert_from_sd", js_mkfun(js_http_set_ca_cert_from_sd));
  js_set(js, global, "parse_json_value", js_mkfun(js_parse_json_value));
  js_set(js, global, "http_set_header", js_mkfun(js_http_set_header));
  js_set(js, global, "http_clear_headers", js_mkfun(js_http_clear_headers));

  // SD functions
  js_set(js, global, "sd_read_file", js_mkfun(js_sd_read_file));
  js_set(js, global, "sd_write_file", js_mkfun(js_sd_write_file));
  js_set(js, global, "sd_list_dir", js_mkfun(js_sd_list_dir));
  js_set(js, global, "sd_delete_file", js_mkfun(js_sd_delete_file));

  js_set(js, global, "ble_init", js_mkfun(js_ble_init));
  js_set(js, global, "ble_is_connected", js_mkfun(js_ble_is_connected));
  js_set(js, global, "ble_write", js_mkfun(js_ble_write));

  // GIF from memory
  js_set(js, global, "show_gif_from_sd", js_mkfun(js_show_gif_from_sd));
  js_set(js, global, "gif_free", js_mkfun(js_gif_free));

  // Basic shapes and labels.
  js_set(js, global, "draw_label", js_mkfun(js_lvgl_draw_label));
  js_set(js, global, "draw_rect", js_mkfun(js_lvgl_draw_rect));
  js_set(js, global, "show_image", js_mkfun(js_lvgl_show_image));
  js_set(js, global, "create_label", js_mkfun(js_create_label));
  js_set(js, global, "label_set_text", js_mkfun(js_label_set_text));

  // Handle-based image creation + transforms
  js_set(js, global, "create_image", js_mkfun(js_create_image));
  js_set(js, global, "create_image_from_ram", js_mkfun(js_create_image_from_ram));
  js_set(js, global, "ram_image_free", js_mkfun(js_ram_image_free));
  js_set(js, global, "rotate_obj", js_mkfun(js_rotate_obj));
  js_set(js, global, "move_obj", js_mkfun(js_move_obj));
  js_set(js, global, "animate_obj", js_mkfun(js_animate_obj));
  js_set(js, global, "obj_delete", js_mkfun(js_obj_delete));

  // Style creation + property setters
  js_set(js, global, "create_style", js_mkfun(js_create_style));
  js_set(js, global, "obj_add_style", js_mkfun(js_obj_add_style));

  js_set(js, global, "style_set_radius", js_mkfun(js_style_set_radius));
  js_set(js, global, "style_set_bg_opa", js_mkfun(js_style_set_bg_opa));
  js_set(js, global, "style_set_bg_color", js_mkfun(js_style_set_bg_color));
  js_set(js, global, "style_set_border_color", js_mkfun(js_style_set_border_color));
  js_set(js, global, "style_set_border_width", js_mkfun(js_style_set_border_width));
  js_set(js, global, "style_set_border_opa", js_mkfun(js_style_set_border_opa));
  js_set(js, global, "style_set_border_side", js_mkfun(js_style_set_border_side));
  js_set(js, global, "style_set_outline_width", js_mkfun(js_style_set_outline_width));
  js_set(js, global, "style_set_outline_color", js_mkfun(js_style_set_outline_color));
  js_set(js, global, "style_set_outline_pad", js_mkfun(js_style_set_outline_pad));
  js_set(js, global, "style_set_shadow_width", js_mkfun(js_style_set_shadow_width));
  js_set(js, global, "style_set_shadow_color", js_mkfun(js_style_set_shadow_color));
  js_set(js, global, "style_set_shadow_ofs_x", js_mkfun(js_style_set_shadow_ofs_x));
  js_set(js, global, "style_set_shadow_ofs_y", js_mkfun(js_style_set_shadow_ofs_y));
  js_set(js, global, "style_set_img_recolor", js_mkfun(js_style_set_img_recolor));
  js_set(js, global, "style_set_img_recolor_opa", js_mkfun(js_style_set_img_recolor_opa));
  js_set(js, global, "style_set_transform_angle", js_mkfun(js_style_set_transform_angle));
  js_set(js, global, "style_set_text_color", js_mkfun(js_style_set_text_color));
  js_set(js, global, "style_set_text_letter_space", js_mkfun(js_style_set_text_letter_space));
  js_set(js, global, "style_set_text_line_space", js_mkfun(js_style_set_text_line_space));
  js_set(js, global, "style_set_text_font", js_mkfun(js_style_set_text_font));
  js_set(js, global, "style_set_text_align", js_mkfun(js_style_set_text_align));
  js_set(js, global, "style_set_text_decor", js_mkfun(js_style_set_text_decor));
  js_set(js, global, "style_set_line_color", js_mkfun(js_style_set_line_color));
  js_set(js, global, "style_set_line_width", js_mkfun(js_style_set_line_width));
  js_set(js, global, "style_set_line_rounded", js_mkfun(js_style_set_line_rounded));
  js_set(js, global, "style_set_pad_all", js_mkfun(js_style_set_pad_all));
  js_set(js, global, "style_set_pad_left", js_mkfun(js_style_set_pad_left));
  js_set(js, global, "style_set_pad_right", js_mkfun(js_style_set_pad_right));
  js_set(js, global, "style_set_pad_top", js_mkfun(js_style_set_pad_top));
  js_set(js, global, "style_set_pad_bottom", js_mkfun(js_style_set_pad_bottom));
  js_set(js, global, "style_set_pad_ver", js_mkfun(js_style_set_pad_ver));
  js_set(js, global, "style_set_pad_hor", js_mkfun(js_style_set_pad_hor));
  js_set(js, global, "style_set_width", js_mkfun(js_style_set_width));
  js_set(js, global, "style_set_height", js_mkfun(js_style_set_height));
  js_set(js, global, "style_set_x", js_mkfun(js_style_set_x));
  js_set(js, global, "style_set_y", js_mkfun(js_style_set_y));

  // Object property setters
  js_set(js, global, "obj_set_size", js_mkfun(js_obj_set_size));
  js_set(js, global, "obj_align", js_mkfun(js_obj_align));

  // Scroll, flex, flags
  js_set(js, global, "obj_set_scroll_snap_x", js_mkfun(js_obj_set_scroll_snap_x));
  js_set(js, global, "obj_set_scroll_snap_y", js_mkfun(js_obj_set_scroll_snap_y));
  js_set(js, global, "obj_add_flag", js_mkfun(js_obj_add_flag));
  js_set(js, global, "obj_clear_flag", js_mkfun(js_obj_clear_flag));
  js_set(js, global, "obj_set_scroll_dir", js_mkfun(js_obj_set_scroll_dir));
  js_set(js, global, "obj_set_scrollbar_mode", js_mkfun(js_obj_set_scrollbar_mode));
  js_set(js, global, "obj_set_flex_flow", js_mkfun(js_obj_set_flex_flow));
  js_set(js, global, "obj_set_flex_align", js_mkfun(js_obj_set_flex_align));
  js_set(js, global, "obj_set_style_clip_corner", js_mkfun(js_obj_set_style_clip_corner));
  js_set(js, global, "obj_set_style_base_dir", js_mkfun(js_obj_set_style_base_dir));

  //==================== METER ============================
  js_set(js, global, "lv_meter_create", js_mkfun(js_lv_meter_create));
  js_set(js, global, "lv_meter_add_scale", js_mkfun(js_lv_meter_add_scale));
  js_set(js, global, "lv_meter_set_scale_ticks", js_mkfun(js_lv_meter_set_scale_ticks));
  js_set(js, global, "lv_meter_set_scale_major_ticks", js_mkfun(js_lv_meter_set_scale_major_ticks));
  js_set(js, global, "lv_meter_set_scale_range", js_mkfun(js_lv_meter_set_scale_range));
  js_set(js, global, "lv_meter_add_arc", js_mkfun(js_lv_meter_add_arc));
  js_set(js, global, "lv_meter_add_scale_lines", js_mkfun(js_lv_meter_add_scale_lines));
  js_set(js, global, "lv_meter_add_needle_line", js_mkfun(js_lv_meter_add_needle_line));
  js_set(js, global, "lv_meter_add_needle_img", js_mkfun(js_lv_meter_add_needle_img));
  js_set(js, global, "lv_meter_set_indicator_start_value", js_mkfun(js_lv_meter_set_indicator_start_value));
  js_set(js, global, "lv_meter_set_indicator_end_value", js_mkfun(js_lv_meter_set_indicator_end_value));
  js_set(js, global, "lv_meter_set_indicator_value", js_mkfun(js_lv_meter_set_indicator_value));

  //==================== SPAN ============================
  js_set(js, global, "lv_spangroup_create", js_mkfun(js_lv_spangroup_create));
  js_set(js, global, "lv_spangroup_set_align", js_mkfun(js_lv_spangroup_set_align));
  js_set(js, global, "lv_spangroup_set_overflow", js_mkfun(js_lv_spangroup_set_overflow));
  js_set(js, global, "lv_spangroup_set_indent", js_mkfun(js_lv_spangroup_set_indent));
  js_set(js, global, "lv_spangroup_set_mode", js_mkfun(js_lv_spangroup_set_mode));
  js_set(js, global, "lv_spangroup_new_span", js_mkfun(js_lv_spangroup_new_span));
  js_set(js, global, "lv_span_set_text", js_mkfun(js_lv_span_set_text));
  js_set(js, global, "lv_span_set_text_static", js_mkfun(js_lv_span_set_text_static));
  js_set(js, global, "lv_spangroup_refr_mode", js_mkfun(js_lv_spangroup_refr_mode));

  // ---------- LINE bridging
  js_set(js, global, "lv_line_create", js_mkfun(js_lv_line_create));
  js_set(js, global, "lv_line_set_points", js_mkfun(js_lv_line_set_points));

  // MQTT bridging
  js_set(js, global, "mqtt_init", js_mkfun(js_mqtt_init));
  js_set(js, global, "mqtt_connect", js_mkfun(js_mqtt_connect));
  js_set(js, global, "mqtt_publish", js_mkfun(js_mqtt_publish));
  js_set(js, global, "mqtt_subscribe", js_mkfun(js_mqtt_subscribe));
  js_set(js, global, "mqtt_loop", js_mkfun(js_mqtt_loop));
  js_set(js, global, "mqtt_on_message", js_mkfun(js_mqtt_on_message));
  js_set(js, global, "mqtt_has_message", js_mkfun(js_mqtt_has_message));
  js_set(js, global, "mqtt_get_payload", js_mkfun(js_mqtt_get_payload));
  js_set(js, global, "mqtt_msg_clear", js_mkfun(js_mqtt_msg_clear));
  js_set(js, global, "mqtt_dropped", js_mkfun(js_mqtt_dropped));
}
