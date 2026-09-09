# Writing reliable JavaScript apps

This branch uses Elk and LVGL 8.3.11. Elk implements a small JavaScript subset;
use `let`, `let update = function() { ... };`, and `for` loops. Consult
[API.md](API.md) for the supported bindings.

## Keep callbacks short

Create widgets once, retain their returned handles, and update only values that
change. Return from startup after registering timers so firmware can render,
service MQTT, and process serial commands.

```javascript
let label = create_label(8, 8);
let previous = -1;
let update_clock = function() {
  let seconds = get_seconds();
  if (seconds !== previous) {
    label_set_text(label, numberToString(seconds));
    previous = seconds;
  }
};
create_timer("update_clock", 200);
```

There are 32 app timers. Registering the same name updates that timer instead
of allocating another. Delete unused timers with `timer_delete(name)`; a
callback may delete itself. Avoid `delay()` in animation callbacks because it
still postpones rendering even though it cooperates with cancellation.

Startup has a 10-second elapsed-time budget, callbacks 5 seconds, and `/eval`
1 second. Each evaluation also has a 2,000,000-step limit. Break large tasks
into bounded timer ticks. HTTP/MQTT calls are synchronous and can visibly pause
the display; keep requests infrequent and responses small.

## Manage resources explicitly

Treat widget handles as opaque numbers and check creation results for -1.
Delete unused widgets with `obj_delete(handle)`; their old handles never become
valid for replacement widgets. The registry holds 256 widgets. Style, chart
series, meter, and span slots have separate limits; retain them only while their
owners exist.

For raw RAM images, supply the actual dimensions and exactly two bytes per
pixel in RGB565_SWAP order. Delete all consumers before calling
`ram_image_free(slot)`. Referenced buffers cannot be freed. App reload releases
UI resources, media, timers, and MQTT session state together.

Use `mem_info()` or `/stats` during repeated reloads to watch memory trends.
`gc()` requests collection at a safe point; it does not free live app variables.

## Consume network messages predictably

Call `mqtt_init(broker, port)`, then `mqtt_connect(client_id[, user, password])`.
Enable polling with `mqtt_on_message("poll_messages")`. From a timer, inspect
`mqtt_has_message()`, read `mqtt_get_topic()` and `mqtt_get_payload()`, then call
`mqtt_msg_clear()` once per consumed message. The API guide contains an example.

Four messages can queue without overwriting unread data. Watch `mqtt_dropped()`
for overflow or oversized messages. Up to eight subscriptions are restored
after reconnect. Keep payloads within 1024 bytes and the configured MQTT packet
buffer, which also includes topic and protocol overhead.

## Recover and diagnose

Use `/errors` for recorded failures, `/eval` to inspect app variables, and
`/screenshot` to inspect the current UI. `/load app.js` and `/restart_app`
interrupt busy JavaScript and restart at a safe point. Persistent app errors
park the app in safe mode; a corrected `/load` gives it a fresh attempt.
