#include "webscreen_js_args.h"

// ws_elk_runtime.h — fragment of the WebScreen Elk/LVGL bridge; included once, in order, by lvgl_elk.h (not standalone).

/******************************************************************************
 * I) In-place app teardown — JS task only (owns LVGL, no locking); order matters: UI, then media buffers, then comm state.
 ******************************************************************************/

static void elk_teardown_ui() {
  delete_all_elk_timers();
  lv_anim_del_all();
  lv_obj_clean(lv_scr_act());
  for (int i = 0; i < MAX_STYLES; i++) {
    if (g_style_map[i] != nullptr) {
      lv_style_reset(g_style_map[i]);
      delete g_style_map[i];
      g_style_map[i] = nullptr;
    }
  }
  // Sub-object registries point into widgets just deleted; null them so slots are never served stale.
  for (int i = 0; i < MAX_CHART_SERIES; i++) g_chart_series[i] = nullptr;
  for (int i = 0; i < MAX_METER_SCALES; i++) g_meter_scales[i] = nullptr;
  for (int i = 0; i < MAX_METER_INDICATORS; i++) g_meter_indicators[i] = nullptr;
  for (int i = 0; i < MAX_SPANS; i++) g_spans[i] = nullptr;
  g_js_error_streak = 0;
}

static void elk_teardown_media() {
  // Reloads must see media replaced under the same SD-card path.
  lv_img_cache_invalidate_src(NULL);
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

static void elk_teardown_comm() {
  if (g_mqttClient.connected()) {
    g_mqttClient.disconnect();
  }
  g_mqttCallbackName[0] = '\0';
  g_mqttMessages.clear();
  // Disarm auto-reconnect: the maintain loop must not resurrect the old script's broker session.
  g_mqttHaveCreds = false;
  g_mqttSubscriptions.clear();
  g_http_headers.clear();
  g_js_gc_requested = false;
  // Release the button handler, restore the default display toggle, drain queued presses.
  g_button_cb_name[0] = '\0';
  webscreen_hardware_set_button_toggle(true);
  g_button_evt_consumed.store(g_button_evt_produced.load());
}

/******************************************************************************
 * J) Register All JS Functions
 ******************************************************************************/

void register_js_functions() {
  jsval_t global = js_glob(js);

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
  js_set(js, global, "set_brightness", js_mkfun(webscreen_checked_binding<js_set_brightness>));
  js_set(js, global, "get_brightness", js_mkfun(js_get_brightness));

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

  js_set(js, global, "str_index_of", js_mkfun(js_str_index_of));
  js_set(js, global, "str_substring", js_mkfun(js_str_substring));

  js_set(js, global, "random", js_mkfun(webscreen_checked_binding<js_random>));
  js_set(js, global, "str_split", js_mkfun(webscreen_checked_binding<js_str_split, 4>));
  js_set(js, global, "str_split_count", js_mkfun(js_str_split_count));
  js_set(js, global, "format_number", js_mkfun(webscreen_checked_binding<js_format_number, 2>));
  js_set(js, global, "pad_number", js_mkfun(webscreen_checked_binding<js_pad_number>));
  js_set(js, global, "format_time", js_mkfun(js_format_time));

  js_set(js, global, "on_button", js_mkfun(js_on_button));
  js_set(js, global, "get_button_event", js_mkfun(js_get_button_event));
  js_set(js, global, "button_set_toggle", js_mkfun(js_button_set_toggle));

  js_set(js, global, "http_get", js_mkfun(js_http_get));
  js_set(js, global, "http_post", js_mkfun(js_http_post));
  js_set(js, global, "http_delete", js_mkfun(js_http_delete));
  js_set(js, global, "http_set_ca_cert_from_sd", js_mkfun(js_http_set_ca_cert_from_sd));
  js_set(js, global, "parse_json_value", js_mkfun(js_parse_json_value));
  js_set(js, global, "http_set_header", js_mkfun(js_http_set_header));
  js_set(js, global, "http_clear_headers", js_mkfun(js_http_clear_headers));

  js_set(js, global, "sd_read_file", js_mkfun(js_sd_read_file));
  js_set(js, global, "sd_write_file", js_mkfun(js_sd_write_file));
  js_set(js, global, "sd_list_dir", js_mkfun(js_sd_list_dir));
  js_set(js, global, "sd_delete_file", js_mkfun(js_sd_delete_file));

  js_set(js, global, "ble_init", js_mkfun(js_ble_init));
  js_set(js, global, "ble_is_connected", js_mkfun(js_ble_is_connected));
  js_set(js, global, "ble_write", js_mkfun(js_ble_write));

  js_set(js, global, "show_gif_from_sd", js_mkfun(webscreen_checked_binding<js_show_gif_from_sd, 6>));
  js_set(js, global, "gif_free", js_mkfun(webscreen_checked_binding<js_gif_free>));

  js_set(js, global, "draw_label", js_mkfun(webscreen_checked_binding<js_lvgl_draw_label, 14>));
  js_set(js, global, "draw_rect", js_mkfun(webscreen_checked_binding<js_lvgl_draw_rect>));
  js_set(js, global, "show_image", js_mkfun(webscreen_checked_binding<js_lvgl_show_image, 6>));
  js_set(js, global, "create_label", js_mkfun(webscreen_checked_binding<js_create_label>));
  js_set(js, global, "label_set_text", js_mkfun(webscreen_checked_binding<js_label_set_text, 1>));

  js_set(js, global, "create_image", js_mkfun(webscreen_checked_binding<js_create_image, 6>));
  js_set(js, global, "create_image_from_ram", js_mkfun(webscreen_checked_binding<js_create_image_from_ram, 30>));
  js_set(js, global, "ram_image_free", js_mkfun(webscreen_checked_binding<js_ram_image_free>));
  js_set(js, global, "rotate_obj", js_mkfun(webscreen_checked_binding<js_rotate_obj>));
  js_set(js, global, "move_obj", js_mkfun(webscreen_checked_binding<js_move_obj>));
  js_set(js, global, "animate_obj", js_mkfun(webscreen_checked_binding<js_animate_obj>));
  js_set(js, global, "obj_delete", js_mkfun(webscreen_checked_binding<js_obj_delete>));

  js_set(js, global, "create_style", js_mkfun(webscreen_checked_binding<js_create_style>));
  js_set(js, global, "obj_add_style", js_mkfun(webscreen_checked_binding<js_obj_add_style>));

  js_set(js, global, "style_set_radius", js_mkfun(webscreen_checked_binding<js_style_set_radius>));
  js_set(js, global, "style_set_bg_opa", js_mkfun(webscreen_checked_binding<js_style_set_bg_opa>));
  js_set(js, global, "style_set_bg_color", js_mkfun(webscreen_checked_binding<js_style_set_bg_color>));
  js_set(js, global, "style_set_border_color", js_mkfun(webscreen_checked_binding<js_style_set_border_color>));
  js_set(js, global, "style_set_border_width", js_mkfun(webscreen_checked_binding<js_style_set_border_width>));
  js_set(js, global, "style_set_border_opa", js_mkfun(webscreen_checked_binding<js_style_set_border_opa>));
  js_set(js, global, "style_set_border_side", js_mkfun(webscreen_checked_binding<js_style_set_border_side>));
  js_set(js, global, "style_set_outline_width", js_mkfun(webscreen_checked_binding<js_style_set_outline_width>));
  js_set(js, global, "style_set_outline_color", js_mkfun(webscreen_checked_binding<js_style_set_outline_color>));
  js_set(js, global, "style_set_outline_pad", js_mkfun(webscreen_checked_binding<js_style_set_outline_pad>));
  js_set(js, global, "style_set_shadow_width", js_mkfun(webscreen_checked_binding<js_style_set_shadow_width>));
  js_set(js, global, "style_set_shadow_color", js_mkfun(webscreen_checked_binding<js_style_set_shadow_color>));
  js_set(js, global, "style_set_shadow_ofs_x", js_mkfun(webscreen_checked_binding<js_style_set_shadow_ofs_x>));
  js_set(js, global, "style_set_shadow_ofs_y", js_mkfun(webscreen_checked_binding<js_style_set_shadow_ofs_y>));
  js_set(js, global, "style_set_img_recolor", js_mkfun(webscreen_checked_binding<js_style_set_img_recolor>));
  js_set(js, global, "style_set_img_recolor_opa", js_mkfun(webscreen_checked_binding<js_style_set_img_recolor_opa>));
  js_set(js, global, "style_set_transform_angle", js_mkfun(webscreen_checked_binding<js_style_set_transform_angle>));
  js_set(js, global, "style_set_text_color", js_mkfun(webscreen_checked_binding<js_style_set_text_color>));
  js_set(js, global, "style_set_text_letter_space", js_mkfun(webscreen_checked_binding<js_style_set_text_letter_space>));
  js_set(js, global, "style_set_text_line_space", js_mkfun(webscreen_checked_binding<js_style_set_text_line_space>));
  js_set(js, global, "style_set_text_font", js_mkfun(webscreen_checked_binding<js_style_set_text_font>));
  js_set(js, global, "style_set_text_align", js_mkfun(webscreen_checked_binding<js_style_set_text_align>));
  js_set(js, global, "style_set_text_decor", js_mkfun(webscreen_checked_binding<js_style_set_text_decor>));
  js_set(js, global, "style_set_line_color", js_mkfun(webscreen_checked_binding<js_style_set_line_color>));
  js_set(js, global, "style_set_line_width", js_mkfun(webscreen_checked_binding<js_style_set_line_width>));
  js_set(js, global, "style_set_line_rounded", js_mkfun(webscreen_checked_binding<js_style_set_line_rounded>));
  js_set(js, global, "style_set_pad_all", js_mkfun(webscreen_checked_binding<js_style_set_pad_all>));
  js_set(js, global, "style_set_pad_left", js_mkfun(webscreen_checked_binding<js_style_set_pad_left>));
  js_set(js, global, "style_set_pad_right", js_mkfun(webscreen_checked_binding<js_style_set_pad_right>));
  js_set(js, global, "style_set_pad_top", js_mkfun(webscreen_checked_binding<js_style_set_pad_top>));
  js_set(js, global, "style_set_pad_bottom", js_mkfun(webscreen_checked_binding<js_style_set_pad_bottom>));
  js_set(js, global, "style_set_pad_ver", js_mkfun(webscreen_checked_binding<js_style_set_pad_ver>));
  js_set(js, global, "style_set_pad_hor", js_mkfun(webscreen_checked_binding<js_style_set_pad_hor>));
  js_set(js, global, "style_set_width", js_mkfun(webscreen_checked_binding<js_style_set_width>));
  js_set(js, global, "style_set_height", js_mkfun(webscreen_checked_binding<js_style_set_height>));
  js_set(js, global, "style_set_x", js_mkfun(webscreen_checked_binding<js_style_set_x>));
  js_set(js, global, "style_set_y", js_mkfun(webscreen_checked_binding<js_style_set_y>));

  js_set(js, global, "obj_set_size", js_mkfun(webscreen_checked_binding<js_obj_set_size>));
  js_set(js, global, "obj_align", js_mkfun(webscreen_checked_binding<js_obj_align>));

  js_set(js, global, "obj_set_scroll_snap_x", js_mkfun(webscreen_checked_binding<js_obj_set_scroll_snap_x>));
  js_set(js, global, "obj_set_scroll_snap_y", js_mkfun(webscreen_checked_binding<js_obj_set_scroll_snap_y>));
  js_set(js, global, "obj_add_flag", js_mkfun(webscreen_checked_binding<js_obj_add_flag>));
  js_set(js, global, "obj_clear_flag", js_mkfun(webscreen_checked_binding<js_obj_clear_flag>));
  js_set(js, global, "obj_set_scroll_dir", js_mkfun(webscreen_checked_binding<js_obj_set_scroll_dir>));
  js_set(js, global, "obj_set_scrollbar_mode", js_mkfun(webscreen_checked_binding<js_obj_set_scrollbar_mode>));
  js_set(js, global, "obj_set_flex_flow", js_mkfun(webscreen_checked_binding<js_obj_set_flex_flow>));
  js_set(js, global, "obj_set_flex_align", js_mkfun(webscreen_checked_binding<js_obj_set_flex_align>));
  js_set(js, global, "obj_set_style_clip_corner", js_mkfun(webscreen_checked_binding<js_obj_set_style_clip_corner>));
  js_set(js, global, "obj_set_style_base_dir", js_mkfun(webscreen_checked_binding<js_obj_set_style_base_dir>));

  //==================== METER ============================
  js_set(js, global, "lv_meter_create", js_mkfun(webscreen_checked_binding<js_lv_meter_create>));
  js_set(js, global, "lv_meter_add_scale", js_mkfun(webscreen_checked_binding<js_lv_meter_add_scale>));
  js_set(js, global, "lv_meter_set_scale_ticks", js_mkfun(webscreen_checked_binding<js_lv_meter_set_scale_ticks>));
  js_set(js, global, "lv_meter_set_scale_major_ticks", js_mkfun(webscreen_checked_binding<js_lv_meter_set_scale_major_ticks>));
  js_set(js, global, "lv_meter_set_scale_range", js_mkfun(webscreen_checked_binding<js_lv_meter_set_scale_range>));
  js_set(js, global, "lv_meter_add_arc", js_mkfun(webscreen_checked_binding<js_lv_meter_add_arc>));
  js_set(js, global, "lv_meter_add_scale_lines", js_mkfun(webscreen_checked_binding<js_lv_meter_add_scale_lines>));
  js_set(js, global, "lv_meter_add_needle_line", js_mkfun(webscreen_checked_binding<js_lv_meter_add_needle_line>));
  js_set(js, global, "lv_meter_add_needle_img", js_mkfun(webscreen_checked_binding<js_lv_meter_add_needle_img>));
  js_set(js, global, "lv_meter_set_indicator_start_value", js_mkfun(webscreen_checked_binding<js_lv_meter_set_indicator_start_value>));
  js_set(js, global, "lv_meter_set_indicator_end_value", js_mkfun(webscreen_checked_binding<js_lv_meter_set_indicator_end_value>));
  js_set(js, global, "lv_meter_set_indicator_value", js_mkfun(webscreen_checked_binding<js_lv_meter_set_indicator_value>));

  //==================== CHART ===========================
  // (js_lv_chart_get_y_array is not registered: it returns a raw C pointer Elk cannot use.)
  js_set(js, global, "lv_chart_create", js_mkfun(webscreen_checked_binding<js_lv_chart_create>));
  js_set(js, global, "lv_chart_set_type", js_mkfun(webscreen_checked_binding<js_lv_chart_set_type>));
  js_set(js, global, "lv_chart_set_div_line_count", js_mkfun(webscreen_checked_binding<js_lv_chart_set_div_line_count>));
  js_set(js, global, "lv_chart_set_update_mode", js_mkfun(webscreen_checked_binding<js_lv_chart_set_update_mode>));
  js_set(js, global, "lv_chart_set_range", js_mkfun(webscreen_checked_binding<js_lv_chart_set_range>));
  js_set(js, global, "lv_chart_set_point_count", js_mkfun(webscreen_checked_binding<js_lv_chart_set_point_count>));
  js_set(js, global, "lv_chart_refresh", js_mkfun(webscreen_checked_binding<js_lv_chart_refresh>));
  js_set(js, global, "lv_chart_add_series", js_mkfun(webscreen_checked_binding<js_lv_chart_add_series>));
  js_set(js, global, "lv_chart_set_next_value", js_mkfun(webscreen_checked_binding<js_lv_chart_set_next_value>));
  js_set(js, global, "lv_chart_set_next_value2", js_mkfun(webscreen_checked_binding<js_lv_chart_set_next_value2>));
  js_set(js, global, "lv_chart_set_axis_tick", js_mkfun(webscreen_checked_binding<js_lv_chart_set_axis_tick>));
  js_set(js, global, "lv_chart_set_zoom_x", js_mkfun(webscreen_checked_binding<js_lv_chart_set_zoom_x>));
  js_set(js, global, "lv_chart_set_zoom_y", js_mkfun(webscreen_checked_binding<js_lv_chart_set_zoom_y>));

  //==================== SPAN ============================
  js_set(js, global, "lv_spangroup_create", js_mkfun(webscreen_checked_binding<js_lv_spangroup_create>));
  js_set(js, global, "lv_spangroup_set_align", js_mkfun(webscreen_checked_binding<js_lv_spangroup_set_align>));
  js_set(js, global, "lv_spangroup_set_overflow", js_mkfun(webscreen_checked_binding<js_lv_spangroup_set_overflow>));
  js_set(js, global, "lv_spangroup_set_indent", js_mkfun(webscreen_checked_binding<js_lv_spangroup_set_indent>));
  js_set(js, global, "lv_spangroup_set_mode", js_mkfun(webscreen_checked_binding<js_lv_spangroup_set_mode>));
  js_set(js, global, "lv_spangroup_new_span", js_mkfun(webscreen_checked_binding<js_lv_spangroup_new_span>));
  js_set(js, global, "lv_span_set_text", js_mkfun(webscreen_checked_binding<js_lv_span_set_text, 1>));
  js_set(js, global, "lv_span_set_text_static", js_mkfun(webscreen_checked_binding<js_lv_span_set_text_static, 1>));
  js_set(js, global, "lv_spangroup_refr_mode", js_mkfun(webscreen_checked_binding<js_lv_spangroup_refr_mode>));

  js_set(js, global, "lv_line_create", js_mkfun(webscreen_checked_binding<js_lv_line_create>));
  js_set(js, global, "lv_line_set_points", js_mkfun(webscreen_checked_binding<js_lv_line_set_points>));

  // MQTT bridging
  js_set(js, global, "mqtt_init", js_mkfun(js_mqtt_init));
  js_set(js, global, "mqtt_connect", js_mkfun(js_mqtt_connect));
  js_set(js, global, "mqtt_publish", js_mkfun(js_mqtt_publish));
  js_set(js, global, "mqtt_subscribe", js_mkfun(js_mqtt_subscribe));
  js_set(js, global, "mqtt_loop", js_mkfun(js_mqtt_loop));
  js_set(js, global, "mqtt_on_message", js_mkfun(js_mqtt_on_message));
  js_set(js, global, "mqtt_has_message", js_mkfun(js_mqtt_has_message));
  js_set(js, global, "mqtt_get_payload", js_mkfun(js_mqtt_get_payload));
  js_set(js, global, "mqtt_get_topic", js_mkfun(js_mqtt_get_topic));
  js_set(js, global, "mqtt_msg_clear", js_mkfun(js_mqtt_msg_clear));
  js_set(js, global, "mqtt_dropped", js_mkfun(js_mqtt_dropped));
}
