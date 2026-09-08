// Copy to the SD card and run /load migration_smoke.js.
let title = create_label(8, 4);
label_set_text(title, "LVGL 9.5 migration smoke test");

let chart = lv_chart_create();
obj_set_size(chart, 250, 160);
move_obj(chart, 8, 50);
lv_chart_set_type(chart, 2); // Existing JS API: 2 is BAR.
lv_chart_set_point_count(chart, 12);
lv_chart_set_range(chart, 0, 0, 100);
let series = lv_chart_add_series(chart, 0x00ff88, 0);

let meter = lv_meter_create();
obj_set_size(meter, 180, 180);
move_obj(meter, 320, 45);
let scale = lv_meter_add_scale(meter);
lv_meter_set_scale_ticks(meter, scale, 21, 2, 8, 0x888888);
lv_meter_set_scale_major_ticks(meter, scale, 5, 3, 12, 0xffffff, 10);
lv_meter_set_scale_range(meter, scale, 0, 100, 270, 135);
let needle = lv_meter_add_needle_line(meter, scale, 3, 0xff4444, -20);

let first_line = lv_line_create();
lv_line_set_points(first_line, 10, 225, 100, 225);
let second_line = lv_line_create();
lv_line_set_points(second_line, 120, 225, 210, 225);

let value = 0;
let update = function() {
  value = (value + 5) % 101;
  lv_chart_set_next_value(chart, series, value);
  lv_meter_set_indicator_value(meter, needle, value);
};
create_timer("update", 100);

let once = function() {
  timer_delete("once");
  print("Self-deleting timer passed");
};
create_timer("once", 250);
