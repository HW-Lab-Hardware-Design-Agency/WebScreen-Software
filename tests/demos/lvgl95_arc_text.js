// /load lvgl95_arc_text.js — requires the arc-label migration firmware.
// Short press: clockwise / counterclockwise / pause. Long press powers off.
let demo_name = "ARC TEXT";
let demo_frame = 0;
let demo_mode = 0;
let demo_modes = 3;
let demo_paused = false;
let angle = 0;

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
let mint = font(20, 0x3de1b4);
let gold = font(14, 0xffce75);
let clean = create_style();
style_set_border_width(clean, 0);
style_set_pad_all(clean, 0);
let bg = draw_rect(0, 0, 536, 240, 0x0b1423);
obj_add_style(bg, clean, 0);
text("ARC TEXT", 14, 10, white);
text("01 / CURVED LABELS", 344, 14, small);
let outer = create_arc_label("WEBSCREEN / EXPLORE", 13, 27, 77);
let inner = create_arc_label("LVGL 9.5", 42, 56, 48);
obj_add_style(outer, mint, 0);
obj_add_style(inner, gold, 0);
arc_label_set_angles(outer, 190, 220);
arc_label_set_angles(inner, 10, 170);
arc_label_set_direction(inner, 1);
text("Hello", 82, 108, white);
text("Text follows a circular path.", 235, 77, white);
text("Two radii, fonts, and directions.", 235, 113, small);
text("Press to reverse, then pause.", 235, 145, small);
let status = text("CLOCKWISE", 235, 179, gold);
text("PRESS: DIRECTION / PAUSE", 14, 218, small);

let demo_button = function() {
  demo_mode = (demo_mode + 1) % demo_modes;
  demo_paused = demo_mode === 2;
  if (demo_mode === 0) {
    arc_label_set_direction(outer, 0);
    arc_label_set_direction(inner, 1);
    label_set_text(status, "CLOCKWISE");
  }
  if (demo_mode === 1) {
    arc_label_set_direction(outer, 1);
    arc_label_set_direction(inner, 0);
    label_set_text(status, "COUNTERCLOCKWISE");
  }
  if (demo_paused) label_set_text(status, "PAUSED / SCREENSHOT READY");
  return 0;
};
let demo_tick = function() {
  if (!demo_paused) {
    demo_frame = demo_frame + 1;
    angle = (angle + 2) % 360;
    arc_label_set_angles(outer, (190 + angle) % 360, 220);
    arc_label_set_angles(inner, (370 - angle) % 360, 170);
  }
  return 0;
};
on_button("demo_button");
create_timer("demo_tick", 150);
print("DEMO READY: ARC TEXT");
