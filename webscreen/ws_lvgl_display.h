// ws_lvgl_display.h — fragment of the WebScreen Elk/LVGL bridge. Not a standalone
// header: included exactly once, in order, by lvgl_elk.h.

/******************************************************************************
 * B) LVGL + Display
 ******************************************************************************/
static lv_disp_draw_buf_t draw_buf;
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {  // Calculate width/height from the area
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  // Push the rendered data to the display
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);

  // Tell LVGL flush is done
  lv_disp_flush_ready(disp);
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
  static const uint32_t DRAW_BUF_LINES = 40;  // tweak later
  static lv_color_t draw_buf_int[EXAMPLE_LCD_H_RES * DRAW_BUF_LINES];

  // Initialize LVGL draw buffer
  lv_disp_draw_buf_init(&draw_buf, draw_buf_int, NULL, EXAMPLE_LCD_H_RES * DRAW_BUF_LINES);

  // Register the display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = EXAMPLE_LCD_H_RES;
  disp_drv.ver_res = EXAMPLE_LCD_V_RES;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  lv_obj_t *scr = lv_scr_act();

  // Set the screen background color directly on the screen object
  lv_obj_set_style_bg_color(scr, lv_color_hex(g_bg_color), 0);

  // Set the default text color for any labels created on the screen
  // (This will be inherited by children unless they have their own color set)
  lv_obj_set_style_text_color(scr, lv_color_hex(g_fg_color), 0);

  LOG("LVGL + Display initialized.");
}

