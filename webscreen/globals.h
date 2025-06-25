#pragma once
#include <Arduino.h>
#include "log.h"
#include <stdint.h>

// Declare the global script filename variable
extern String g_script_filename;

// Add this new global flag
extern bool g_mqtt_enabled;

// ADD these new global variables for screen colors
extern uint32_t g_bg_color;
extern uint32_t g_fg_color;