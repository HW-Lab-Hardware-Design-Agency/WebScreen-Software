// LVGL Lab: offline, no assets. Upload, then /load lvgl95_showcase.js.
// Requires this branch's new arc-label bindings: rebuild/upload firmware first.
// Short press cycles CURVE -> BAR -> PAUSED. Long press still powers off.
let ink = 0xe9f1fa;
let muted = 0x92a7bd;
let mint = 0x36e4ae;
let blue = 0x52a9ff;
let card = 0x142235;
let arc_text = -1;
let dial = -1;
let dial_needle = -1;
let graph = -1;
let series = -1;
let value_label = -1;
let mode_label = -1;
let mode = 0;
let value = 0;
let direction = 1;
let turns = 0;

let label_style = function(size, color) {
  let style = create_style();
  style_set_text_font(style, size);
  style_set_text_color(style, color);
  return style;
};

let text_at = function(text, x, y, style) {
  let label = create_label(x, y);
  obj_add_style(label, style, 0);
  label_set_text(label, text);
  return label;
};

let small = label_style(14, muted);
let heading = label_style(20, ink);
let accent = label_style(14, mint);
let reading = label_style(34, ink);
let flat = create_style();
style_set_radius(flat, 0);
style_set_border_width(flat, 0);
style_set_pad_all(flat, 0);

let background = draw_rect(0, 0, 536, 240, 0x0b1320);
obj_add_style(background, flat, 0);
text_at("LVGL LAB", 12, 6, heading);
text_at("9.5 / SOFTWARE RENDERING", 308, 10, small);

let lab_tick = function() {
  if (mode < 2) {
    value = value + direction * 2;
    if (value >= 100) direction = -1;
    if (value <= 0) direction = 1;
    turns = (turns + 1) % 360;
    lv_meter_set_indicator_value(dial, dial_needle, value);
    // Smoothstep makes the curve visible without requiring a Math library.
    let t = value / 100;
    lv_chart_set_next_value(graph, series, 100 * t * t * (3 - 2 * t));
    arc_label_set_angles(arc_text, (210 + turns) % 360, 120);
    label_set_text(value_label, numberToString(value));
  }
  return 0;
};

let lab_button = function() {
  mode = (mode + 1) % 3;
  if (mode === 0) {
    lv_chart_set_type(graph, 4);
    label_set_text(mode_label, "LIVE CHART / SMOOTH CURVE");
    arc_label_set_text(arc_text, "WEBSCREEN / LVGL 9.5");
    arc_label_set_direction(arc_text, 0);
  }
  if (mode === 1) {
    lv_chart_set_type(graph, 2);
    label_set_text(mode_label, "LIVE CHART / BAR");
    arc_label_set_text(arc_text, "CURVED TEXT / REVERSE");
    arc_label_set_direction(arc_text, 1);
  }
  if (mode === 2) {
    label_set_text(mode_label, "PAUSED / TAKE A SCREENSHOT");
  }
  return 0;
};

let start_lab = function() {
  let panel = draw_rect(236, 40, 288, 165, card);
  let panel_style = create_style();
  style_set_radius(panel_style, 12);
  style_set_border_width(panel_style, 0);
  obj_add_style(panel, panel_style, 0);

  dial = lv_meter_create();
  obj_set_size(dial, 152, 152);
  move_obj(dial, 36, 44);
  let dial_style = create_style();
  style_set_bg_opa(dial_style, 0);
  style_set_border_width(dial_style, 0);
  style_set_pad_all(dial_style, 0);
  style_set_text_color(dial_style, muted);
  obj_add_style(dial, dial_style, 0);
  let scale = lv_meter_add_scale(dial);
  lv_meter_set_scale_ticks(dial, scale, 21, 1, 5, muted);
  lv_meter_set_scale_major_ticks(dial, scale, 5, 2, 9, ink, 8);
  lv_meter_set_scale_range(dial, scale, 0, 100, 240, 150);
  let band = lv_meter_add_arc(dial, scale, 3, mint, -3);
  lv_meter_set_indicator_start_value(dial, band, 0);
  lv_meter_set_indicator_end_value(dial, band, 100);
  dial_needle = lv_meter_add_needle_line(dial, scale, 3, blue, -22);
  lv_meter_set_indicator_value(dial, dial_needle, value);

  arc_text = create_arc_label("WEBSCREEN / LVGL 9.5", 10, 18, 86);
  obj_add_style(arc_text, accent, 0);
  value_label = text_at("0", 77, 147, reading);
  obj_set_size(value_label, 72, 42);
  style_set_text_align(reading, 2);
  text_at("SCALE GAUGE", 65, 193, small);

  mode_label = text_at("LIVE CHART / SMOOTH CURVE", 248, 50, small);
  graph = lv_chart_create();
  obj_set_size(graph, 258, 112);
  obj_align(graph, 1, 249, 78);
  let graph_style = create_style();
  style_set_bg_color(graph_style, card);
  style_set_border_width(graph_style, 0);
  style_set_pad_all(graph_style, 4);
  obj_add_style(graph, graph_style, 0);
  lv_chart_set_type(graph, 4);
  lv_chart_set_point_count(graph, 24);
  lv_chart_set_div_line_count(graph, 3, 5);
  lv_chart_set_range(graph, 0, 0, 100);
  series = lv_chart_add_series(graph, mint, 0);
  for (let i = 0; i < 24; i++) lv_chart_set_next_value(graph, series, 0);

  let red = draw_rect(12, 223, 14, 5, 0xff0000);
  obj_add_style(red, flat, 0);
  let green = draw_rect(30, 223, 14, 5, 0x00ff00);
  obj_add_style(green, flat, 0);
  let blue_check = draw_rect(48, 223, 14, 5, 0x0000ff);
  obj_add_style(blue_check, flat, 0);
  text_at("PRESS: CURVE / BAR / PAUSE", 268, 218, small);
  on_button("lab_button");
  create_timer("lab_tick", 80);
  print("LVGL Lab ready. Press the button to cycle curve, bar, and pause.");
  return 0;
};

start_lab();
