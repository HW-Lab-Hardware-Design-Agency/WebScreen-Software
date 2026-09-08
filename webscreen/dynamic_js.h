#pragma once
// Returns false if the JavaScript runtime could not be started — the caller
// should fall back to the notification app instead of leaving a dead screen.
bool dynamic_js_setup();

void dynamic_js_loop();
