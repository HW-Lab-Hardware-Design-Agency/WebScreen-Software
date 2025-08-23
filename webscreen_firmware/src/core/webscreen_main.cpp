/**
 * @file webscreen_main.cpp
 * @brief Main application logic for WebScreen
 * 
 * Refactored main application with improved error handling, modular design,
 * and better separation of concerns.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// Core components
#include "config_manager.h"
#include "error_handler.h"
#include "../utils/logging/logger.h"
#include "../utils/memory/memory_manager.h"

// Hardware components
#include "../hardware/storage/sd_manager.h"
#include "../hardware/display/display_manager.h" 
#include "../hardware/power/power_manager.h"

// Runtime components
#include "../runtime/js/js_runtime.h"
#include "../runtime/fallback/fallback_app.h"

// Network components
#include "../network/wifi/wifi_manager.h"

// Global application state
typedef enum {
    WEBSCREEN_STATE_INITIALIZING,
    WEBSCREEN_STATE_RUNNING_JS,
    WEBSCREEN_STATE_RUNNING_FALLBACK,
    WEBSCREEN_STATE_ERROR,
    WEBSCREEN_STATE_SHUTDOWN
} webscreen_app_state_t;

static webscreen_app_state_t g_app_state = WEBSCREEN_STATE_INITIALIZING;
static bool g_use_fallback = false;

/**
 * @brief Initialize all core systems
 * @return true if all systems initialized successfully
 */
static bool initialize_core_systems(void) {
    WEBSCREEN_LOG_INFO("Main", "Initializing WebScreen v2.0...");
    
    // Initialize error handling first
    if (!webscreen_error_handler_init()) {
        Serial.println("FATAL: Failed to initialize error handler");
        return false;
    }
    
    // Initialize logging system
    webscreen_logger_config_t log_config = {
        .min_level = WEBSCREEN_LOG_LEVEL_INFO,
        .output_mask = WEBSCREEN_LOG_OUTPUT_SERIAL | WEBSCREEN_LOG_OUTPUT_SD,
        .include_timestamp = true,
        .include_level = true,
        .include_module = true,
        .colored_output = true,
        .sd_log_file = "/webscreen.log",
        .max_sd_file_size = 512 * 1024  // 512KB
    };
    
    if (!webscreen_logger_init(&log_config)) {
        WEBSCREEN_ERROR_REPORT_FATAL(WEBSCREEN_ERROR_UNKNOWN, "Failed to initialize logger");
        return false;
    }
    
    // Initialize memory management
    if (!memory_manager_init()) {
        WEBSCREEN_ERROR_REPORT_FATAL(WEBSCREEN_ERROR_MEMORY_ALLOCATION_FAILED, 
                                     "Failed to initialize memory manager");
        return false;
    }
    
    // Log system information
    webscreen_logger_log_system_info();
    
    return true;
}

/**
 * @brief Initialize hardware systems
 * @return true if all hardware initialized successfully
 */
static bool initialize_hardware(void) {
    WEBSCREEN_LOG_INFO("Main", "Initializing hardware systems...");
    
    // Initialize power management first
    if (!power_manager_init()) {
        WEBSCREEN_ERROR_REPORT_ERROR(WEBSCREEN_ERROR_UNKNOWN, "Power manager init failed");
        return false;
    }
    
    // Initialize SD card storage
    if (!sd_manager_init()) {
        WEBSCREEN_ERROR_REPORT_WARNING(WEBSCREEN_ERROR_SD_INIT_FAILED, 
                                       "SD card initialization failed");
        g_use_fallback = true;
    }
    
    // Initialize display
    display_config_t display_config = {
        .width = 536,
        .height = 240,
        .buffer_size = 536 * 240,  // Full screen buffer
        .rotation = DISPLAY_ROTATION_90,
        .brightness = 200,
        .use_double_buffer = memory_psram_available(),  // Use double buffer if PSRAM available
        .use_dma = true
    };
    
    if (!display_manager_init(&display_config)) {
        WEBSCREEN_ERROR_REPORT_FATAL(WEBSCREEN_ERROR_DISPLAY_INIT_FAILED, 
                                     "Display initialization failed");
        return false;
    }
    
    WEBSCREEN_LOG_INFO("Main", "Hardware initialization complete");
    return true;
}

/**
 * @brief Load and parse configuration
 * @return true if configuration loaded successfully
 */
static bool load_configuration(void) {
    WEBSCREEN_LOG_INFO("Main", "Loading configuration...");
    
    if (!config_manager_init()) {
        WEBSCREEN_ERROR_REPORT_ERROR(WEBSCREEN_ERROR_CONFIG_FILE_NOT_FOUND,
                                     "Failed to initialize config manager");
        g_use_fallback = true;
        return false;
    }
    
    if (!config_manager_load("/webscreen.json")) {
        WEBSCREEN_ERROR_REPORT_WARNING(WEBSCREEN_ERROR_CONFIG_PARSE_FAILED,
                                       "Failed to load configuration file");
        g_use_fallback = true;
        return false;
    }
    
    // Apply display settings from config
    webscreen_config_t* config = config_manager_get_config();
    if (config) {
        display_set_brightness(config->display.brightness);
        // Apply other display settings...
    }
    
    return true;
}

/**
 * @brief Initialize network systems
 * @return true if network initialized successfully
 */
static bool initialize_network(void) {
    webscreen_config_t* config = config_manager_get_config();
    if (!config || !config->wifi.enabled) {
        WEBSCREEN_LOG_INFO("Main", "WiFi disabled in configuration");
        g_use_fallback = true;
        return false;
    }
    
    WEBSCREEN_LOG_INFO("Main", "Initializing network...");
    
    wifi_manager_config_t wifi_config = {
        .ssid = config->wifi.ssid,
        .password = config->wifi.password,
        .connection_timeout_ms = 15000,
        .auto_reconnect = true,
        .power_save_mode = true
    };
    
    if (!wifi_manager_init(&wifi_config)) {
        WEBSCREEN_ERROR_REPORT_WARNING(WEBSCREEN_ERROR_WIFI_CONNECT_FAILED,
                                       "WiFi initialization failed");
        g_use_fallback = true;
        return false;
    }
    
    if (!wifi_manager_connect()) {
        WEBSCREEN_ERROR_REPORT_WARNING(WEBSCREEN_ERROR_WIFI_TIMEOUT,
                                       "WiFi connection failed");
        g_use_fallback = true;
        return false;
    }
    
    WEBSCREEN_LOG_INFO("Main", "Connected to WiFi: %s", WiFi.localIP().toString().c_str());
    return true;
}

/**
 * @brief Start the appropriate runtime (JS or fallback)
 * @return true if runtime started successfully
 */
static bool start_runtime(void) {
    if (g_use_fallback) {
        WEBSCREEN_LOG_INFO("Main", "Starting fallback application...");
        
        if (!fallback_app_init()) {
            WEBSCREEN_ERROR_REPORT_FATAL(WEBSCREEN_ERROR_UNKNOWN,
                                         "Failed to start fallback application");
            return false;
        }
        
        g_app_state = WEBSCREEN_STATE_RUNNING_FALLBACK;
        return true;
    }
    
    WEBSCREEN_LOG_INFO("Main", "Starting JavaScript runtime...");
    
    // Check if script file exists
    webscreen_config_t* config = config_manager_get_config();
    if (!config || !sd_manager_file_exists(config->script_file)) {
        WEBSCREEN_ERROR_REPORT_WARNING(WEBSCREEN_ERROR_SCRIPT_FILE_NOT_FOUND,
                                       "Script file not found, falling back");
        g_use_fallback = true;
        return start_runtime();  // Recursively try fallback
    }
    
    js_runtime_config_t js_config = {
        .script_file = config->script_file,
        .heap_size = memory_psram_available() ? 512 * 1024 : 128 * 1024,  // 512KB or 128KB
        .max_execution_time_ms = 100,  // 100ms max per script execution
        .enable_network_api = !g_use_fallback,
        .enable_storage_api = true,
        .enable_display_api = true
    };
    
    if (!js_runtime_init(&js_config)) {
        WEBSCREEN_ERROR_REPORT_ERROR(WEBSCREEN_ERROR_JS_RUNTIME_FAILED,
                                     "JavaScript runtime failed, falling back");
        g_use_fallback = true;
        return start_runtime();  // Recursively try fallback
    }
    
    g_app_state = WEBSCREEN_STATE_RUNNING_JS;
    return true;
}

/**
 * @brief Main application loop
 */
static void run_main_loop(void) {
    static uint32_t last_stats_print = 0;
    static uint32_t last_health_check = 0;
    
    // Handle power button
    power_manager_handle_button();
    
    // Run appropriate runtime
    switch (g_app_state) {
        case WEBSCREEN_STATE_RUNNING_JS:
            js_runtime_loop();
            break;
            
        case WEBSCREEN_STATE_RUNNING_FALLBACK:
            fallback_app_loop();
            break;
            
        case WEBSCREEN_STATE_ERROR:
            // Handle error state - maybe show error screen
            delay(1000);
            break;
            
        default:
            WEBSCREEN_LOG_ERROR("Main", "Invalid application state: %d", g_app_state);
            g_app_state = WEBSCREEN_STATE_ERROR;
            break;
    }
    
    // Network maintenance
    if (!g_use_fallback) {
        wifi_manager_loop();
    }
    
    // Periodic system health checks
    uint32_t now = millis();
    if (now - last_health_check > 30000) {  // Every 30 seconds
        last_health_check = now;
        
        if (!webscreen_error_system_healthy()) {
            WEBSCREEN_LOG_WARN("Main", "System health degraded");
        }
        
        // Print stats every 5 minutes
        if (now - last_stats_print > 300000) {
            last_stats_print = now;
            memory_print_report();
            display_print_status();
            webscreen_error_print_report();
        }
    }
}

/**
 * @brief Emergency shutdown procedure
 */
static void emergency_shutdown(void) {
    WEBSCREEN_LOG_FATAL("Main", "Emergency shutdown initiated");
    
    // Try to save any critical data
    config_manager_save();
    
    // Shutdown systems in reverse order
    js_runtime_shutdown();
    fallback_app_shutdown();
    wifi_manager_shutdown();
    display_manager_shutdown();
    sd_manager_shutdown();
    power_manager_shutdown();
    
    // Final log
    WEBSCREEN_LOG_FATAL("Main", "System halted");
    webscreen_logger_flush();
    
    // Halt system
    while (true) {
        delay(1000);
    }
}

// Public interface for the Arduino environment

void webscreen_setup(void) {
    Serial.begin(115200);
    
    // Initialize core systems
    if (!initialize_core_systems()) {
        Serial.println("FATAL: Core system initialization failed");
        emergency_shutdown();
        return;
    }
    
    // Initialize hardware
    if (!initialize_hardware()) {
        WEBSCREEN_LOG_FATAL("Main", "Hardware initialization failed");
        emergency_shutdown();
        return;
    }
    
    // Load configuration
    load_configuration();
    
    // Initialize network (optional)
    initialize_network();
    
    // Start runtime
    if (!start_runtime()) {
        WEBSCREEN_LOG_FATAL("Main", "Runtime initialization failed");
        emergency_shutdown();
        return;
    }
    
    WEBSCREEN_LOG_INFO("Main", "WebScreen initialization complete - State: %s", 
                       g_use_fallback ? "Fallback" : "JavaScript");
}

void webscreen_loop(void) {
    run_main_loop();
}