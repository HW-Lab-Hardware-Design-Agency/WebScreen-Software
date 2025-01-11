// fallback.cpp
#include <Arduino.h>
#include "lvgl.h"
#include "rm67162.h"
#include "notification.h"  // Contains 'extern const lv_img_dsc_t notification;'

// External objects
extern lv_disp_draw_buf_t draw_buf; // or create local if you prefer
extern lv_color_t *buf;             // or create local
extern bool fallbackActive;         // Exposed globally for your main

// The GIF from notification.h
extern const lv_img_dsc_t notification;

// We'll keep these static to avoid collisions
static lv_obj_t *label = nullptr;  
static lv_obj_t *gif   = nullptr;

static void my_disp_flush_fallback(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  // Display flush provided by rm67162 or your custom driver
  // e.g.:
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd_PushColors(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

static void scroll_anim_cb(void *var, int32_t v) {
  lv_obj_set_y((lv_obj_t *)var, v);
}

static void create_scroll_animation(lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_values(&a, start, end);
  lv_anim_set_time(&a, duration);
  lv_anim_set_exec_cb(&a, scroll_anim_cb);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
  lv_anim_set_repeat_count(&a, 2); // Repeat twice

  // When animation finishes, hide label, show GIF
  lv_anim_set_ready_cb(&a, [](lv_anim_t *anim) {
    lv_obj_t *obj = static_cast<lv_obj_t *>(anim->var);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);  // Hide text
    lv_obj_clear_flag(gif, LV_OBJ_FLAG_HIDDEN);// Show GIF
  });
  lv_anim_start(&a);
}

// ------------------------------------------------------------------
// Fallback setup
// ------------------------------------------------------------------
void fallback_setup() {
  Serial.println("FALLBACK: Setting up LVGL + scrolling label + GIF...");

  // 1) Minimal LVGL init if not already done
  lv_init();
  // 2) Screen on
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  // 3) Init your AMOLED driver, set rotation
  rm67162_init();
  lcd_setRotation(1);

  // 4) Allocate buffer
  buf = (lv_color_t *)ps_malloc(sizeof(lv_color_t) * LVGL_LCD_BUF_SIZE);
  if(!buf) {
    Serial.println("FALLBACK: Buffer alloc failed");
    return;
  }

  // 5) Initialize draw buffer
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_LCD_BUF_SIZE);

  // 6) Register display driver
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 536; 
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = my_disp_flush_fallback;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // 7) Style
  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_text_font(&style, &lv_font_montserrat_40);
  lv_style_set_text_color(&style, lv_color_white());
  lv_style_set_bg_color(&style, lv_color_black());
  lv_style_set_pad_all(&style, 5);
  lv_style_set_text_align(&style, LV_TEXT_ALIGN_CENTER);

  // 8) Create label
  label = lv_label_create(lv_scr_act());
  lv_obj_add_style(label, &style, 0);
  lv_label_set_text(label,
    "This is fallback: a long text that will scroll automatically.\n"
    "Add more text to test scrolling.\n"
    "Eventually it shows a GIF afterwards."
  );
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, 525);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

  // 9) Create scroll animation (10 seconds for a full scroll)
  create_scroll_animation(label, 240, -lv_obj_get_height(label), 10000);

  // 10) Create the GIF
  gif = lv_gif_create(lv_scr_act());
  lv_gif_set_src(gif, &notification);
  lv_obj_align(gif, LV_ALIGN_CENTER, 0, 0);

  // Initially hide label, show GIF
  lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(gif, LV_OBJ_FLAG_HIDDEN);

  Serial.println("FALLBACK: Setup complete");
}

// ------------------------------------------------------------------
// Fallback loop
// ------------------------------------------------------------------
void fallback_loop() {
  lv_timer_handler();

  // If there's serial input, re-animate the label with new text
  if (Serial.available()) {
    String received = Serial.readStringUntil('\n');
    lv_label_set_text(label, received.c_str());
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(gif, LV_OBJ_FLAG_HIDDEN);

    create_scroll_animation(label, 240, -lv_obj_get_height(label), 10000);
  }
}
