// ws_lvgl_display.h — fragment of the WebScreen Elk/LVGL bridge. Not a standalone
// header: included exactly once, in order, by lvgl_elk.h.

/******************************************************************************
 * B) LVGL + Display
 ******************************************************************************/
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  // Push the rendered data to the display (panel wants RGB565 byte-swapped;
  // the display color format is RGB565_SWAPPED so LVGL renders it that way)
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)px_map);

  // Tell LVGL flush is done
  lv_display_flush_ready(disp);
}
void init_lvgl_display() {
  // Once per boot: the in-place JS app restart relies on this guard to reuse the live display.
  static bool s_display_initialized = false;
  if (s_display_initialized) {
    LOG("Display already initialized — skipping re-init");
    return;
  }
  s_display_initialized = true;

  LOG("Initializing display...");

  // Turn on backlight / screen power
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  // Init the AMOLED driver & set rotation
  rm67162_init();
  lcd_setRotation(1);

  // Apply configured brightness (default 0xD0 is set by rm67162_init)
  if (g_webscreen_config.display.brightness > 0) {
    lcd_brightness(g_webscreen_config.display.brightness);
    LOG("Display brightness set to configured value: " + String(g_webscreen_config.display.brightness));
  }

  // Init LVGL
  lv_init();
  start_lvgl_tick();

  // Single DRAM draw buffer: the flush is synchronous, so a second (PSRAM) buffer buys nothing.
  // Sized in bytes: v9's lv_color_t is 3 bytes, but the RGB565 display renders 2 bytes/px.
  static const uint32_t DRAW_BUF_LINES = 40;  // tweak later
  static uint8_t draw_buf_int[EXAMPLE_LCD_H_RES * DRAW_BUF_LINES * 2] __attribute__((aligned(4)));

  lv_display_t *disp = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
  // Panel expects byte-swapped RGB565 (was LV_COLOR_16_SWAP in LVGL 8)
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, draw_buf_int, NULL, sizeof(draw_buf_int),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_obj_t *scr = lv_scr_act();

  // Set the screen background color directly on the screen object
  lv_obj_set_style_bg_color(scr, lv_color_hex(g_bg_color), 0);

  // Set the default text color for any labels created on the screen
  // (This will be inherited by children unless they have their own color set)
  lv_obj_set_style_text_color(scr, lv_color_hex(g_fg_color), 0);

  LOG("LVGL + Display initialized.");
}

