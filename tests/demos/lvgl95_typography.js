// /load lvgl95_typography.js — short press switches dark/light theme.
// Static rendering baseline: no timers, network calls, or media files.
let demo_name = "TYPE AND COLOR";
let demo_frame = 0;
let demo_mode = 0;
let demo_modes = 2;
let demo_paused = true;
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
let small = font(14, 0x96abc4);
let medium = font(20, 0xeaf0fa);
let large = font(34, 0xeaf0fa);
let jumbo = font(48, 0x3de1b4);
let clean = create_style();
style_set_border_width(clean, 0);
style_set_pad_all(clean, 0);
// Text labels can also form a flat background; their style stays mutable.
let bg_style = create_style();
style_set_bg_opa(bg_style, 255);
style_set_bg_color(bg_style, 0x0b1423);
let bg = text("", 0, 0, bg_style);
obj_set_size(bg, 536, 240);
text("TYPE AND COLOR", 14, 10, medium);
text("04 / STATIC BASELINE", 360, 15, small);
text("Hello, WebScreen.", 14, 49, large);
text("48", 14, 97, jumbo);
text("34", 108, 111, large);
text("20", 185, 124, medium);
text("14 px", 244, 130, small);
let copy = text("Readable at a glance. Labels wrap inside their assigned width.", 326, 92, small);
obj_set_size(copy, 191, 68);
let red = draw_rect(14, 172, 156, 26, 0xff0000);
let green = draw_rect(189, 172, 156, 26, 0x00ff00);
let blue = draw_rect(364, 172, 156, 26, 0x0000ff);
obj_add_style(red, clean, 0);
obj_add_style(green, clean, 0);
obj_add_style(blue, clean, 0);
let status = text("DARK / PRESS FOR LIGHT", 14, 219, small);
text("RGB565: RED / GREEN / BLUE", 311, 219, small);
let demo_button = function() {
  demo_mode = (demo_mode + 1) % demo_modes;
  if (demo_mode === 0) {
    style_set_bg_color(bg_style, 0x0b1423);
    style_set_text_color(small, 0x96abc4);
    style_set_text_color(medium, 0xeaf0fa);
    style_set_text_color(large, 0xeaf0fa);
    style_set_text_color(jumbo, 0x3de1b4);
    label_set_text(status, "DARK / PRESS FOR LIGHT");
  } else {
    style_set_bg_color(bg_style, 0xf1f5fc);
    style_set_text_color(small, 0x42556d);
    style_set_text_color(medium, 0x142237);
    style_set_text_color(large, 0x142237);
    style_set_text_color(jumbo, 0x006e54);
    label_set_text(status, "LIGHT / PRESS FOR DARK");
  }
  // Reapplying the existing background style invalidates the full screen.
  obj_add_style(bg, bg_style, 0);
  return 0;
};
on_button("demo_button");
print("DEMO READY: TYPE AND COLOR");
