// ws_lvgl_display.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

/******************************************************************************
 * B) LVGL + Display
 ******************************************************************************/
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf = NULL;
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {  // Calculate width/height from the area
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  // Push the rendered data to the display
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);

  // Tell LVGL flush is done
  lv_disp_flush_ready(disp);
}
void init_lvgl_display() {
  // Once per boot. A second call used to re-run lv_init(), leak the previous
  // PSRAM flush buffer and register a duplicate display — which is exactly
  // what happened when the runtime was started twice. The in-place JS app
  // restart relies on this guard to reuse the live display.
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

  // Use double buffering: draw BUF in internal RAM (DMA capable),
  // flush BUF in PSRAM (big but non‑DMA).
  static const uint32_t DRAW_BUF_LINES = 40;  // tweak later
  static lv_color_t draw_buf_int[EXAMPLE_LCD_H_RES * DRAW_BUF_LINES];
  // The second buffer only needs DRAW_BUF_LINES as well — lv_disp_draw_buf_init
  // below is told size EXAMPLE_LCD_H_RES * DRAW_BUF_LINES, so allocating a
  // full-screen LVGL_LCD_BUF_SIZE here wasted ~214KB of PSRAM that LVGL could
  // never address.
  buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * EXAMPLE_LCD_H_RES * DRAW_BUF_LINES);  // PSRAM
  if (!buf) {
    LOG("Failed to allocate LVGL buffer in PSRAM");
    return;
  }

  // Initialize LVGL draw buffer
  lv_disp_draw_buf_init(&draw_buf, draw_buf_int, buf, EXAMPLE_LCD_H_RES * DRAW_BUF_LINES);

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

