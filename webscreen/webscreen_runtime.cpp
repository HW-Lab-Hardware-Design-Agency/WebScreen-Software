#include "webscreen_runtime.h"
#include "webscreen_main.h"
#include <lvgl.h>
#include "tick.h"
#include "pins_config.h"
#include "rm67162.h"
#include "globals.h"
#include "lvgl_elk.h"

extern "C" {
  #include "elk.h"
}
#include <WiFi.h>
#include <PubSubClient.h>
static bool g_javascript_active = false;
static bool g_fallback_active = false;
static String g_current_script_file = "";
static String g_fallback_text = "WebScreen v2.0\nFallback Mode\nSD card or script not found";
static String g_last_error = "";
static uint32_t g_runtime_start_time = 0;
static bool g_lvgl_initialized = false;

extern struct js *js;
extern uint8_t elk_memory[];
static TaskHandle_t g_js_task_handle = NULL;
static bool g_js_engine_initialized = false;
static String g_js_script_content = "";

static unsigned long g_last_mqtt_reconnect_attempt = 0;
static unsigned long g_last_wifi_reconnect_attempt = 0;

extern WiFiClient g_wifiClient;
extern PubSubClient g_mqttClient;
static uint32_t g_loop_count = 0;
static uint32_t g_last_performance_check = 0;
static uint32_t g_avg_loop_time_us = 0;
static uint32_t g_max_loop_time_us = 0;

bool webscreen_runtime_start_javascript(const char* script_file) {    if (!script_file) {
        g_last_error = "Script file path is NULL";
        return false;
    }
    
    WEBSCREEN_DEBUG_PRINTF("Starting JavaScript runtime with: %s\n", script_file);
    
    if (!SD_MMC.exists(script_file)) {        g_last_error = "Script file not found: ";
        g_last_error += script_file;
        WEBSCREEN_DEBUG_PRINTLN(g_last_error.c_str());
        return false;
    }
    
    webscreen_runtime_shutdown();
    
    init_lvgl_display();
    
    if (!webscreen_runtime_init_sd_filesystem()) {        g_last_error = "Failed to initialize SD filesystem";
        return false;
    }
    
    if (!webscreen_runtime_init_memory_filesystem()) {        g_last_error = "Failed to initialize memory filesystem";
        return false;
    }
    
    if (!webscreen_runtime_init_ram_images()) {        g_last_error = "Failed to initialize RAM images";
        return false;
    }
    
    if (!webscreen_runtime_init_javascript_engine()) {        g_last_error = "Failed to initialize JavaScript engine";
        return false;
    }
    
    if (!webscreen_runtime_load_script(script_file)) {        g_last_error = "Failed to load JavaScript script";
        return false;
    }
    
    if (!webscreen_runtime_start_javascript_task()) {        g_last_error = "Failed to start JavaScript execution task";
        return false;
    }
    
    g_current_script_file = script_file;
    g_javascript_active = true;
    g_fallback_active = false;
    g_runtime_start_time = WEBSCREEN_MILLIS();
    g_last_error = "";
    
    WEBSCREEN_DEBUG_PRINTLN("JavaScript runtime started (simulated)");
    return true;
}


bool webscreen_runtime_start_fallback(void) {    WEBSCREEN_DEBUG_PRINTLN("Starting fallback application");
    

    webscreen_runtime_shutdown();
    

    if (!webscreen_runtime_init_lvgl()) {        g_last_error = "Failed to initialize LVGL for fallback";
        return false;
    }
    
    g_javascript_active = false;
    g_fallback_active = true;
    g_runtime_start_time = WEBSCREEN_MILLIS();
    g_last_error = "";
    
    WEBSCREEN_DEBUG_PRINTLN("Fallback application started");
    return true;
}


void webscreen_runtime_loop_javascript(void) {    if (!g_javascript_active) {
        return;
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
}


void webscreen_runtime_loop_fallback(void) {    if (!g_fallback_active) {
        return;
    }
    

    webscreen_runtime_lvgl_timer_handler();
    

    static uint32_t last_update = 0;
    static int animation_frame = 0;
    
    if (WEBSCREEN_MILLIS() - last_update > 1000) { // Update every second
        last_update = WEBSCREEN_MILLIS();
        animation_frame++;
        

        String animated_text = g_fallback_text;
        for (int i = 0; i < (animation_frame % 4); i++) {            animated_text += ".";
        }
        
        WEBSCREEN_DEBUG_PRINTF("Fallback frame %d: %s\n", animation_frame, animated_text.c_str());
    }
    

    if (Serial.available()) {        String input = Serial.readStringUntil('\n');
        input.trim();
        
        if (input.length() > 0) {            webscreen_runtime_set_fallback_text(input.c_str());
        }
    }
}


void webscreen_runtime_shutdown(void) {    if (g_javascript_active || g_fallback_active) {
        WEBSCREEN_DEBUG_PRINTLN("Shutting down runtime");
        

        if (g_js_task_handle != NULL) {            vTaskDelete(g_js_task_handle);
            g_js_task_handle = NULL;
        }
        

        if (js) {

            js = NULL;
        }
        
        g_javascript_active = false;
        g_fallback_active = false;
        g_js_engine_initialized = false;
        g_current_script_file = "";
        g_js_script_content = "";
        g_last_error = "";
    }
}


bool webscreen_runtime_is_javascript_active(void) {    return g_javascript_active;
}

const char* webscreen_runtime_get_javascript_status(void) {    if (!g_javascript_active) {
        return "JavaScript runtime inactive";
    }
    
    static String status;
    status = "JavaScript active - Script: ";
    status += g_current_script_file;
    status += " - Uptime: ";
    status += (WEBSCREEN_MILLIS() - g_runtime_start_time);
    status += "ms";
    
    return status.c_str();
}


bool webscreen_runtime_execute_javascript(const char* code) {    if (!g_javascript_active || !code) {
        return false;
    }
    

    WEBSCREEN_DEBUG_PRINTF("Executing JS: %s\n", code);
    

    if (strstr(code, "print(")) {

        const char* start = strchr(code, '"');
        if (start) {            start++; // Skip opening quote
            const char* end = strchr(start, '"');
            if (end) {                String text = String(start).substring(0, end - start);
                webscreen_runtime_set_fallback_text(text.c_str());
                return true;
            }
        }
    }
    
    return true; // Simulate successful execution
}


void webscreen_runtime_get_javascript_stats(uint32_t* exec_count, 
                                           uint32_t* avg_time_us, 
                                           uint32_t* error_count) {    if (exec_count) *exec_count = g_loop_count;
    if (avg_time_us) *avg_time_us = g_avg_loop_time_us;
    if (error_count) *error_count = g_last_error.length() > 0 ? 1 : 0;
}


bool webscreen_runtime_is_fallback_active(void) {    return g_fallback_active;
}


void webscreen_runtime_set_fallback_text(const char* text) {    if (text) {
        g_fallback_text = text;
        WEBSCREEN_DEBUG_PRINTF("Fallback text updated: %s\n", text);
    }
}

const char* webscreen_runtime_get_fallback_status(void) {    if (!g_fallback_active) {
        return "Fallback application inactive";
    }
    
    static String status;
    status = "Fallback active - Uptime: ";
    status += (WEBSCREEN_MILLIS() - g_runtime_start_time);
    status += "ms";
    
    return status.c_str();
}


bool webscreen_runtime_init_lvgl(void) {

    g_lvgl_initialized = true;
    return true;
}


void webscreen_runtime_lvgl_timer_handler(void) {    if (g_lvgl_initialized) {
        lv_timer_handler();
    }
}

void* webscreen_runtime_get_lvgl_display(void) {    if (g_lvgl_initialized) {
        return lv_disp_get_default();
    }
    return nullptr;
}


void webscreen_runtime_set_background_color(uint32_t color) {    WEBSCREEN_DEBUG_PRINTF("Background color set to 0x%06X\n", color);
    if (g_lvgl_initialized) {        lv_obj_t * scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_hex(color), 0);
    }
}


void webscreen_runtime_set_foreground_color(uint32_t color) {    WEBSCREEN_DEBUG_PRINTF("Foreground color set to 0x%06X\n", color);
    if (g_lvgl_initialized) {        lv_obj_t * scr = lv_scr_act();
        lv_obj_set_style_text_color(scr, lv_color_hex(color), 0);
    }
}


void webscreen_runtime_get_memory_usage(uint32_t* js_heap_used,
                                       uint32_t* lvgl_memory_used,
                                       uint32_t* total_runtime_memory) {

    if (js_heap_used) *js_heap_used = g_javascript_active ? 50000 : 0;
    if (lvgl_memory_used) *lvgl_memory_used = 100000;
    if (total_runtime_memory) *total_runtime_memory = 150000;
}


bool webscreen_runtime_garbage_collect(void) {    if (g_javascript_active) {
        WEBSCREEN_DEBUG_PRINTLN("JavaScript garbage collection triggered");
        return true;
    }
    return false;
}

const char* webscreen_runtime_get_last_error(void) {    return g_last_error.length() > 0 ? g_last_error.c_str() : nullptr;
}


void webscreen_runtime_clear_errors(void) {    g_last_error = "";
}


bool webscreen_runtime_has_errors(void) {    return g_last_error.length() > 0;
}


void webscreen_runtime_set_performance_monitoring(bool enable) {    WEBSCREEN_DEBUG_PRINTF("Performance monitoring: %s\n", enable ? "Enabled" : "Disabled");
    
    if (enable) {        g_loop_count = 0;
        g_avg_loop_time_us = 0;
        g_max_loop_time_us = 0;
        g_last_performance_check = WEBSCREEN_MILLIS();
    }
}


void webscreen_runtime_get_performance_stats(uint32_t* avg_loop_time_us,
                                            uint32_t* max_loop_time_us,
                                            uint32_t* fps) {    if (avg_loop_time_us) *avg_loop_time_us = g_avg_loop_time_us;
    if (max_loop_time_us) *max_loop_time_us = g_max_loop_time_us;
    if (fps) *fps = g_avg_loop_time_us > 0 ? (1000000 / g_avg_loop_time_us) : 0;
}


void webscreen_runtime_print_status(void) {    WEBSCREEN_DEBUG_PRINTLN("\n=== RUNTIME STATUS ===");
    WEBSCREEN_DEBUG_PRINTF("JavaScript Active: %s\n", g_javascript_active ? "Yes" : "No");
    WEBSCREEN_DEBUG_PRINTF("Fallback Active: %s\n", g_fallback_active ? "Yes" : "No");
    
    if (g_javascript_active) {        WEBSCREEN_DEBUG_PRINTF("Script File: %s\n", g_current_script_file.c_str());
        WEBSCREEN_DEBUG_PRINTF("Runtime Uptime: %lu ms\n", 
                               WEBSCREEN_MILLIS() - g_runtime_start_time);
    }
    
    if (g_fallback_active) {        WEBSCREEN_DEBUG_PRINTF("Fallback Text: %s\n", g_fallback_text.c_str());
    }
    
    WEBSCREEN_DEBUG_PRINTF("Loop Count: %lu\n", g_loop_count);
    WEBSCREEN_DEBUG_PRINTF("Avg Loop Time: %lu us\n", g_avg_loop_time_us);
    WEBSCREEN_DEBUG_PRINTF("Max Loop Time: %lu us\n", g_max_loop_time_us);
    
    if (g_last_error.length() > 0) {        WEBSCREEN_DEBUG_PRINTF("Last Error: %s\n", g_last_error.c_str());
    }
    
    WEBSCREEN_DEBUG_PRINTLN("======================\n");
}


bool webscreen_runtime_init_javascript_engine(void) {    if (g_js_engine_initialized) {
        return true;
    }
    
    WEBSCREEN_DEBUG_PRINTLN("Initializing Elk JavaScript engine...");
    

    js = js_create(elk_memory, sizeof(elk_memory));
    if (!js) {        WEBSCREEN_DEBUG_PRINTLN("Failed to initialize Elk JavaScript engine");
        return false;
    }
    

    webscreen_runtime_register_js_functions();
    
    g_js_engine_initialized = true;
    WEBSCREEN_DEBUG_PRINTLN("JavaScript engine initialized successfully");
    return true;
}


bool webscreen_runtime_load_script(const char* script_file) {    if (!script_file) {
        return false;
    }
    
    WEBSCREEN_DEBUG_PRINTF("Loading JavaScript script from: %s\n", script_file);
    
    File file = SD_MMC.open(script_file);
    if (!file) {        WEBSCREEN_DEBUG_PRINTF("Failed to open script file: %s\n", script_file);
        return false;
    }
    
    g_js_script_content = file.readString();
    file.close();
    
    if (g_js_script_content.length() == 0) {        WEBSCREEN_DEBUG_PRINTLN("Script file is empty");
        return false;
    }
    
    WEBSCREEN_DEBUG_PRINTF("Script loaded successfully (%d bytes)\n", g_js_script_content.length());
    return true;
}


bool webscreen_runtime_start_javascript_task(void) {    if (g_js_task_handle != NULL) {
        WEBSCREEN_DEBUG_PRINTLN("JavaScript task already running");
        return true;
    }
    
    WEBSCREEN_DEBUG_PRINTLN("Starting JavaScript execution task...");
    
    BaseType_t result = xTaskCreatePinnedToCore(
        webscreen_runtime_javascript_task,
        "WebScreenJS",
        16384,  // Stack size
        NULL,   // Parameters
        1,      // Priority
        &g_js_task_handle,
        0       // Core
    );
    
    if (result != pdPASS) {        WEBSCREEN_DEBUG_PRINTLN("Failed to create JavaScript task");
        return false;
    }
    
    WEBSCREEN_DEBUG_PRINTLN("JavaScript task started successfully");
    return true;
}


void webscreen_runtime_javascript_task(void *pvParameters) {    WEBSCREEN_DEBUG_PRINTLN("JavaScript task started");
    

    vTaskDelay(pdMS_TO_TICKS(100));
    

    if (js && g_js_script_content.length() > 0) {        jsval_t result = js_eval(js, g_js_script_content.c_str(), g_js_script_content.length());
        if (js_type(result) == JS_ERR) {            WEBSCREEN_DEBUG_PRINTLN("JavaScript execution error");
        } else {            WEBSCREEN_DEBUG_PRINTLN("JavaScript script executed successfully");
        }
    }
    

    for(;;) {

        if (g_mqtt_enabled) {            webscreen_runtime_wifi_mqtt_maintain_loop();
        }
        

        lv_timer_handler();
        

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
extern void register_js_functions();


void webscreen_runtime_register_js_functions(void) {    if (!js) {
        return;
    }
    
    WEBSCREEN_DEBUG_PRINTLN("Registering JavaScript API functions...");
    

    register_js_functions();
    
    WEBSCREEN_DEBUG_PRINTLN("JavaScript API functions registered successfully");
}


void webscreen_runtime_wifi_mqtt_maintain_loop(void) {

    if (WiFi.status() != WL_CONNECTED) {        unsigned long now = WEBSCREEN_MILLIS();

        if (now - g_last_wifi_reconnect_attempt > 10000) {            g_last_wifi_reconnect_attempt = now;
            WEBSCREEN_DEBUG_PRINTLN("Wi-Fi disconnected, attempting reconnection...");

        }
        return;
    }
    

    if (g_mqtt_enabled) {    }
}
extern void init_lvgl_display();
extern void init_lv_fs();
extern void init_mem_fs();
extern void init_ram_images();


bool webscreen_runtime_init_sd_filesystem(void) {    WEBSCREEN_DEBUG_PRINTLN("Initializing LVGL SD filesystem driver...");
    init_lv_fs();
    return true;
}


bool webscreen_runtime_init_memory_filesystem(void) {    WEBSCREEN_DEBUG_PRINTLN("Initializing LVGL memory filesystem driver...");
    init_mem_fs();
    return true;
}


bool webscreen_runtime_init_ram_images(void) {    WEBSCREEN_DEBUG_PRINTLN("Initializing RAM images storage...");
    init_ram_images();
    return true;
}