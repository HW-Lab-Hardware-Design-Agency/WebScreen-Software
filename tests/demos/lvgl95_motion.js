// /load lvgl95_motion.js — short press pauses/resumes the moving lines.
// Two independent point buffers; all objects/styles are allocated once.
let demo_name = "MOTION LINES";
let demo_frame = 0;
let demo_mode = 0;
let demo_modes = 2;
let demo_paused = false;
let travel = 0;
let step = 3;
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
text("MOTION LINES", 14, 10, white);
text("05 / POINT BUFFERS", 357, 15, small);
let trace_a = lv_line_create();
let trace_b = lv_line_create();
obj_align(trace_a, 1, 22, 70);
obj_align(trace_b, 1, 22, 70);
let mint = create_style();
style_set_line_width(mint, 3);
style_set_line_color(mint, 0x3de1b4);
style_set_line_rounded(mint, 1);
let gold = create_style();
style_set_line_width(gold, 2);
style_set_line_color(gold, 0xffce75);
style_set_line_rounded(gold, 1);
obj_add_style(trace_a, mint, 0);
obj_add_style(trace_b, gold, 0);
let marker = draw_rect(22, 188, 18, 8, 0x61a9ff);
obj_add_style(marker, clean, 0);
let status = text("LIVE / PRESS TO PAUSE", 14, 219, small);
text("TWO INDEPENDENT TRACES", 321, 219, small);
let demo_button = function() {
  demo_mode = (demo_mode + 1) % demo_modes;
  demo_paused = demo_mode === 1;
  if (demo_paused) label_set_text(status, "PAUSED / PRESS TO RESUME");
  else label_set_text(status, "LIVE / PRESS TO PAUSE");
  return 0;
};
let demo_tick = function() {
  if (!demo_paused) {
    demo_frame = demo_frame + 1;
    travel = travel + step;
    if (travel >= 96) step = -3;
    if (travel <= 0) step = 3;
    lv_line_set_points(trace_a, 0, 48, 80, travel, 160, 96 - travel, 240, travel, 320, 96 - travel, 400, travel, 480, 48);
    lv_line_set_points(trace_b, 0, 48, 80, 96 - travel, 160, travel, 240, 96 - travel, 320, travel, 400, 96 - travel, 480, 48);
    // obj_align avoids the per-frame serial logging of move_obj.
    obj_align(marker, 1, 22 + travel * 4, 188);
  }
  return 0;
};
demo_tick();
on_button("demo_button");
create_timer("demo_tick", 120);
print("DEMO READY: MOTION LINES");
