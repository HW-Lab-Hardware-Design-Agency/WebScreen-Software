// /load lvgl95_charts.js — short press: curve / line / bar / scatter / pause.
// Uses synthetic data. Long press still powers off.
let demo_name = "CHART GALLERY";
let demo_frame = 0;
let demo_mode = 0;
let demo_modes = 5;
let demo_paused = false;
let phase = 0;
let y = 0;
let font = function(size, color) {
  let s = create_style();
  style_set_text_font(s, size);
  style_set_text_color(s, color);
  return s;
};
let text = function(value, x, y, style) {
  let o = create_label(x, y);
  obj_add_style(o, style, 0);
  label_set_text(o, value);
  return o;
};
let white = font(20, 0xeaf0fa);
let small = font(14, 0x96abc4);
let clean = create_style();
style_set_border_width(clean, 0);
style_set_pad_all(clean, 0);
let bg = draw_rect(0, 0, 536, 240, 0x0b1423);
obj_add_style(bg, clean, 0);
text("CHART GALLERY", 14, 10, white);
let status = text("CURVE", 410, 15, small);
let chart = lv_chart_create();
obj_set_size(chart, 502, 157);
obj_align(chart, 1, 17, 47);
let plot = create_style();
style_set_bg_color(plot, 0x132237);
style_set_border_width(plot, 0);
style_set_pad_all(plot, 8);
obj_add_style(chart, plot, 0);
let series_style = create_style();
style_set_line_width(series_style, 2);
obj_add_style(chart, series_style, 0x050000); // LV_PART_ITEMS: series lines.
lv_chart_set_type(chart, 4);
lv_chart_set_point_count(chart, 32);
lv_chart_set_div_line_count(chart, 5, 9);
lv_chart_set_range(chart, 0, 0, 100);
lv_chart_set_range(chart, 2, 0, 31);
let a = lv_chart_add_series(chart, 0x3de1b4, 0);
let b = lv_chart_add_series(chart, 0xffce75, 0);
text("MINT: SIGNAL   GOLD: INVERSE", 14, 219, small);
text("PRESS: NEXT TYPE", 369, 219, small);

let append_sample = function() {
  let p = phase;
  if (p > 16) p = 32 - p;
  let t = p / 16;
  y = 100 * t * t * (3 - 2 * t);
  if (demo_mode === 3 || demo_mode === 4) {
    lv_chart_set_next_value2(chart, a, phase, y);
    lv_chart_set_next_value2(chart, b, phase, 100 - y);
  } else {
    lv_chart_set_next_value(chart, a, y);
    lv_chart_set_next_value(chart, b, 100 - y);
  }
  phase = (phase + 1) % 32;
  return 0;
};
let fill_plot = function() {
  for (let i = 0; i < 32; i++) append_sample();
  return 0;
};
let demo_button = function() {
  demo_mode = (demo_mode + 1) % demo_modes;
  demo_paused = demo_mode === 4;
  if (demo_mode === 0) { lv_chart_set_type(chart, 4); label_set_text(status, "CURVE"); }
  if (demo_mode === 1) { lv_chart_set_type(chart, 1); label_set_text(status, "LINE"); }
  if (demo_mode === 2) { lv_chart_set_type(chart, 2); label_set_text(status, "BAR"); }
  if (demo_mode === 3) { lv_chart_set_type(chart, 3); label_set_text(status, "SCATTER"); }
  if (demo_paused) label_set_text(status, "PAUSED");
  // Scatter shows unconnected points, including where the X values wrap.
  if (demo_mode >= 3) style_set_line_width(series_style, 0);
  else style_set_line_width(series_style, 2);
  obj_add_style(chart, series_style, 0x050000);
  fill_plot();
  return 0;
};
let demo_tick = function() {
  if (!demo_paused) {
    demo_frame = demo_frame + 1;
    // Only two native data updates per tick; seed the full plot on mode changes.
    append_sample();
  }
  return 0;
};
fill_plot();
on_button("demo_button");
create_timer("demo_tick", 250);
print("DEMO READY: CHART GALLERY");
