// lvgl_elk.h — WebScreen's Elk JS <-> LVGL/network bridge.
//
// Split into fragment headers (ws_*.h), included below IN ORDER —
// the concatenation is token-identical to the former monolith, so
// the fragments are order-dependent and not individually includable.
// This header itself is included exactly once, by webscreen_runtime.cpp.
#pragma once

#include "ws_elk_core.h"
#include "ws_elk_util.h"
#include "ws_lvgl_fs.h"
#include "ws_lvgl_display.h"
#include "ws_elk_basics.h"
#include "ws_elk_media.h"
#include "ws_lvgl_widgets.h"
#include "ws_lvgl_styles.h"
#include "ws_lvgl_charts.h"
#include "ws_elk_http.h"
#include "ws_elk_sd_ext.h"
#include "ws_elk_ble.h"
#include "ws_elk_mqtt.h"
#include "ws_elk_time.h"
#include "ws_elk_runtime.h"
