#include "dynamic_js.h"
#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>
#include <WiFi.h>

#include "pins_config.h"
#include "rm67162.h"
#include "webscreen_runtime.h"
#include "webscreen_hardware.h"
#include "globals.h"
#include "serial_commands.h"

String g_script_filename = "/app.js";
bool dynamic_js_setup() {
  LOG("DYNAMIC_JS: Setting up Elk + script scenario...");

  WiFi.mode(WIFI_STA);
  SerialCommands::init();

  // Forward button presses to JS apps (on_button / get_button_event)
  webscreen_hardware_set_button_callback(webscreen_runtime_notify_button);

  if (!webscreen_runtime_start_javascript(g_script_filename.c_str())) {
    LOG("Failed to start JavaScript runtime");
    return false;
  }

  LOG("DYNAMIC_JS: setup done!");
  return true;
}
void dynamic_js_loop() {
  // Handle power button (short press = display toggle, long press = power off)
  webscreen_hardware_handle_button();

  // Handle serial commands
  String line;
  if (SerialCommands::readLine(line)) {

    // Check if it's a command (starts with /)
    if (line.startsWith("/")) {
      SerialCommands::processCommand(line);
    } else {
      // Regular text input - could be used by JavaScript app
      LOG("Serial input: " + line);
    }
  }

  webscreen_runtime_loop_javascript();
}
