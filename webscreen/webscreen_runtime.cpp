/**
 * @file webscreen_runtime.cpp
 * @brief Runtime environment implementation for Arduino
 * 
 * Simplified runtime implementation following Arduino best practices.
 */

#include "webscreen_runtime.h"
#include "webscreen_main.h"

// Runtime state
static bool g_javascript_active = false;
static bool g_fallback_active = false;
static String g_current_script_file = "";
static String g_fallback_text = "WebScreen v2.0\nFallback Mode\nSD card or script not found";
static String g_last_error = "";
static uint32_t g_runtime_start_time = 0;

// Performance monitoring
static uint32_t g_loop_count = 0;
static uint32_t g_last_performance_check = 0;
static uint32_t g_avg_loop_time_us = 0;
static uint32_t g_max_loop_time_us = 0;

// ============================================================================
// RUNTIME MANAGEMENT
// ============================================================================

bool webscreen_runtime_start_javascript(const char* script_file) {
    if (!script_file) {
        g_last_error = "Script file path is NULL";
        return false;
    }
    
    WEBSCREEN_DEBUG_PRINTF("Starting JavaScript runtime with: %s\n", script_file);
    
    // Check if file exists on SD card
    if (!SD_MMC.exists(script_file)) {
        g_last_error = "Script file not found: ";
        g_last_error += script_file;
        WEBSCREEN_DEBUG_PRINTLN(g_last_error.c_str());
        return false;
    }
    
    // Stop any existing runtime
    webscreen_runtime_shutdown();
    
    // Initialize LVGL if not already done
    if (!webscreen_runtime_init_lvgl()) {
        g_last_error = "Failed to initialize LVGL";
        return false;
    }
    
    // In a full implementation, this would initialize the Elk JavaScript engine
    // For this Arduino example, we'll simulate JavaScript execution
    
    g_current_script_file = script_file;
    g_javascript_active = true;
    g_fallback_active = false;
    g_runtime_start_time = WEBSCREEN_MILLIS();
    g_last_error = "";
    
    WEBSCREEN_DEBUG_PRINTLN("JavaScript runtime started (simulated)");
    return true;
}

bool webscreen_runtime_start_fallback(void) {
    WEBSCREEN_DEBUG_PRINTLN("Starting fallback application");
    
    // Stop any existing runtime
    webscreen_runtime_shutdown();
    
    // Initialize LVGL if not already done
    if (!webscreen_runtime_init_lvgl()) {
        g_last_error = "Failed to initialize LVGL for fallback";
        return false;
    }
    
    g_javascript_active = false;
    g_fallback_active = true;
    g_runtime_start_time = WEBSCREEN_MILLIS();
    g_last_error = "";
    
    WEBSCREEN_DEBUG_PRINTLN("Fallback application started");
    return true;
}

void webscreen_runtime_loop_javascript(void) {
    if (!g_javascript_active) {
        return;
    }
    
    uint32_t loop_start = micros();
    
    // Simulate JavaScript execution
    // In a real implementation, this would run the Elk JavaScript engine
    
    // Handle LVGL timer
    webscreen_runtime_lvgl_timer_handler();
    
    // Handle serial input for JavaScript console
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        
        if (input.length() > 0) {
            WEBSCREEN_DEBUG_PRINTF("JS Console: %s\n", input.c_str());
            
            // Simple command processing
            if (input == "status") {
                WEBSCREEN_DEBUG_PRINTF("JavaScript runtime active for %lu ms\n", 
                                       WEBSCREEN_MILLIS() - g_runtime_start_time);
            } else if (input.startsWith("print ")) {
                String message = input.substring(6);
                webscreen_runtime_set_fallback_text(message.c_str());
            }
        }
    }
    
    // Update performance statistics
    uint32_t loop_time = micros() - loop_start;
    g_loop_count++;
    
    if (loop_time > g_max_loop_time_us) {
        g_max_loop_time_us = loop_time;
    }
    
    // Calculate average every 1000 loops
    if (g_loop_count % 1000 == 0) {
        g_avg_loop_time_us = (g_avg_loop_time_us + loop_time) / 2;
    }
}

void webscreen_runtime_loop_fallback(void) {
    if (!g_fallback_active) {
        return;
    }
    
    // Handle LVGL timer
    webscreen_runtime_lvgl_timer_handler();
    
    // Simple animation for fallback mode
    static uint32_t last_update = 0;
    static int animation_frame = 0;
    
    if (WEBSCREEN_MILLIS() - last_update > 1000) { // Update every second
        last_update = WEBSCREEN_MILLIS();
        animation_frame++;
        
        // Create animated text
        String animated_text = g_fallback_text;
        for (int i = 0; i < (animation_frame % 4); i++) {
            animated_text += ".";
        }
        
        WEBSCREEN_DEBUG_PRINTF("Fallback frame %d: %s\n", animation_frame, animated_text.c_str());
    }
    
    // Handle serial input for fallback mode
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        
        if (input.length() > 0) {
            webscreen_runtime_set_fallback_text(input.c_str());
        }
    }
}

void webscreen_runtime_shutdown(void) {
    if (g_javascript_active || g_fallback_active) {
        WEBSCREEN_DEBUG_PRINTLN("Shutting down runtime");
        
        g_javascript_active = false;
        g_fallback_active = false;
        g_current_script_file = "";
        g_last_error = "";
    }
}

// ============================================================================
// JAVASCRIPT ENGINE
// ============================================================================

bool webscreen_runtime_is_javascript_active(void) {
    return g_javascript_active;
}

const char* webscreen_runtime_get_javascript_status(void) {
    if (!g_javascript_active) {
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

bool webscreen_runtime_execute_javascript(const char* code) {
    if (!g_javascript_active || !code) {
        return false;
    }
    
    // In a real implementation, this would execute JavaScript code via Elk
    WEBSCREEN_DEBUG_PRINTF("Executing JS: %s\n", code);
    
    // Simple command simulation
    if (strstr(code, "print(")) {
        // Extract and display text
        const char* start = strchr(code, '"');
        if (start) {
            start++; // Skip opening quote
            const char* end = strchr(start, '"');
            if (end) {
                String text = String(start).substring(0, end - start);
                webscreen_runtime_set_fallback_text(text.c_str());
                return true;
            }
        }
    }
    
    return true; // Simulate successful execution
}

void webscreen_runtime_get_javascript_stats(uint32_t* exec_count, 
                                           uint32_t* avg_time_us, 
                                           uint32_t* error_count) {
    if (exec_count) *exec_count = g_loop_count;
    if (avg_time_us) *avg_time_us = g_avg_loop_time_us;
    if (error_count) *error_count = g_last_error.length() > 0 ? 1 : 0;
}

// ============================================================================
// FALLBACK APPLICATION
// ============================================================================

bool webscreen_runtime_is_fallback_active(void) {
    return g_fallback_active;
}

void webscreen_runtime_set_fallback_text(const char* text) {
    if (text) {
        g_fallback_text = text;
        WEBSCREEN_DEBUG_PRINTF("Fallback text updated: %s\n", text);
    }
}

const char* webscreen_runtime_get_fallback_status(void) {
    if (!g_fallback_active) {
        return "Fallback application inactive";
    }
    
    static String status;
    status = "Fallback active - Uptime: ";
    status += (WEBSCREEN_MILLIS() - g_runtime_start_time);
    status += "ms";
    
    return status.c_str();
}

// ============================================================================
// LVGL INTEGRATION
// ============================================================================

bool webscreen_runtime_init_lvgl(void) {
    // In a real implementation, this would initialize LVGL with proper display drivers
    WEBSCREEN_DEBUG_PRINTLN("LVGL initialized (stub implementation)");
    return true;
}

void webscreen_runtime_lvgl_timer_handler(void) {
    // In a real implementation, this would call lv_timer_handler()
    // For now, we'll just simulate the timer processing
    static uint32_t last_timer = 0;
    if (WEBSCREEN_MILLIS() - last_timer > 30) { // 30ms refresh rate
        last_timer = WEBSCREEN_MILLIS();
        // LVGL timer processing would happen here
    }
}

void* webscreen_runtime_get_lvgl_display(void) {
    // In a real implementation, this would return the lv_disp_t* object
    return nullptr; // Stub implementation
}

void webscreen_runtime_set_background_color(uint32_t color) {
    WEBSCREEN_DEBUG_PRINTF("Background color set to 0x%06X\n", color);
}

void webscreen_runtime_set_foreground_color(uint32_t color) {
    WEBSCREEN_DEBUG_PRINTF("Foreground color set to 0x%06X\n", color);
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

void webscreen_runtime_get_memory_usage(uint32_t* js_heap_used,
                                       uint32_t* lvgl_memory_used,
                                       uint32_t* total_runtime_memory) {
    // Simulate memory usage reporting
    if (js_heap_used) *js_heap_used = g_javascript_active ? 50000 : 0; // 50KB
    if (lvgl_memory_used) *lvgl_memory_used = 100000; // 100KB
    if (total_runtime_memory) *total_runtime_memory = 150000; // 150KB total
}

bool webscreen_runtime_garbage_collect(void) {
    if (g_javascript_active) {
        WEBSCREEN_DEBUG_PRINTLN("JavaScript garbage collection triggered");
        return true;
    }
    return false;
}

// ============================================================================
// ERROR HANDLING
// ============================================================================

const char* webscreen_runtime_get_last_error(void) {
    return g_last_error.length() > 0 ? g_last_error.c_str() : nullptr;
}

void webscreen_runtime_clear_errors(void) {
    g_last_error = "";
}

bool webscreen_runtime_has_errors(void) {
    return g_last_error.length() > 0;
}

// ============================================================================
// PERFORMANCE MONITORING
// ============================================================================

void webscreen_runtime_set_performance_monitoring(bool enable) {
    WEBSCREEN_DEBUG_PRINTF("Performance monitoring: %s\n", enable ? "Enabled" : "Disabled");
    
    if (enable) {
        g_loop_count = 0;
        g_avg_loop_time_us = 0;
        g_max_loop_time_us = 0;
        g_last_performance_check = WEBSCREEN_MILLIS();
    }
}

void webscreen_runtime_get_performance_stats(uint32_t* avg_loop_time_us,
                                            uint32_t* max_loop_time_us,
                                            uint32_t* fps) {
    if (avg_loop_time_us) *avg_loop_time_us = g_avg_loop_time_us;
    if (max_loop_time_us) *max_loop_time_us = g_max_loop_time_us;
    if (fps) *fps = g_avg_loop_time_us > 0 ? (1000000 / g_avg_loop_time_us) : 0;
}

void webscreen_runtime_print_status(void) {
    WEBSCREEN_DEBUG_PRINTLN("\n=== RUNTIME STATUS ===");
    WEBSCREEN_DEBUG_PRINTF("JavaScript Active: %s\n", g_javascript_active ? "Yes" : "No");
    WEBSCREEN_DEBUG_PRINTF("Fallback Active: %s\n", g_fallback_active ? "Yes" : "No");
    
    if (g_javascript_active) {
        WEBSCREEN_DEBUG_PRINTF("Script File: %s\n", g_current_script_file.c_str());
        WEBSCREEN_DEBUG_PRINTF("Runtime Uptime: %lu ms\n", 
                               WEBSCREEN_MILLIS() - g_runtime_start_time);
    }
    
    if (g_fallback_active) {
        WEBSCREEN_DEBUG_PRINTF("Fallback Text: %s\n", g_fallback_text.c_str());
    }
    
    WEBSCREEN_DEBUG_PRINTF("Loop Count: %lu\n", g_loop_count);
    WEBSCREEN_DEBUG_PRINTF("Avg Loop Time: %lu us\n", g_avg_loop_time_us);
    WEBSCREEN_DEBUG_PRINTF("Max Loop Time: %lu us\n", g_max_loop_time_us);
    
    if (g_last_error.length() > 0) {
        WEBSCREEN_DEBUG_PRINTF("Last Error: %s\n", g_last_error.c_str());
    }
    
    WEBSCREEN_DEBUG_PRINTLN("======================\n");
}