/**
 * @file webscreen_main.cpp
 * @brief Main application implementation for WebScreen
 * 
 * This file contains the core application logic following Arduino best practices.
 * All complex functionality is organized into supporting files within the sketch.
 */

#include "webscreen_main.h"
#include "webscreen_hardware.h"
#include "webscreen_runtime.h"
#include "webscreen_network.h"

// ============================================================================
// GLOBAL STATE
// ============================================================================

// Application state enumeration
typedef enum {
    WEBSCREEN_STATE_INITIALIZING,
    WEBSCREEN_STATE_RUNNING_JS,
    WEBSCREEN_STATE_RUNNING_FALLBACK,
    WEBSCREEN_STATE_ERROR,
    WEBSCREEN_STATE_SHUTDOWN
} webscreen_app_state_t;

// Global state variables
static webscreen_app_state_t g_app_state = WEBSCREEN_STATE_INITIALIZING;
static bool g_use_fallback = false;
static bool g_system_healthy = true;
static uint32_t g_last_health_check = 0;
static uint32_t g_last_stats_print = 0;

// Default configuration
webscreen_config_t g_webscreen_config = {
    .wifi = {
        .ssid = "",
        .password = "",
        .enabled = true,
        .connection_timeout = WEBSCREEN_WIFI_CONNECTION_TIMEOUT_MS,
        .auto_reconnect = true
    },
    .mqtt = {
        .broker = "",
        .port = 1883,
        .username = "",
        .password = "",
        .client_id = "webscreen_001",
        .enabled = false,
        .keepalive = WEBSCREEN_MQTT_KEEPALIVE_SEC
    },
    .display = {
        .brightness = 200,
        .rotation = WEBSCREEN_DISPLAY_ROTATION,
        .background_color = 0x000000,  // Black
        .foreground_color = 0xFFFFFF,  // White
        .auto_brightness = false,
        .screen_timeout = 0  // Never timeout
    },
    .system = {
        .device_name = "WebScreen",
        .timezone = "UTC",
        .log_level = 2,  // Info level
        .performance_mode = false,
        .watchdog_timeout = WEBSCREEN_WATCHDOG_TIMEOUT_SEC * 1000
    },
    .script_file = "/app.js",
    .config_version = 2,
    .last_modified = 0
};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
static bool initialize_hardware(void);
static bool initialize_storage(void);
static bool load_configuration(void);
static bool initialize_network(void);
static bool start_runtime(void);
static void run_main_loop(void);
static void handle_system_health(void);

// ============================================================================
// PUBLIC FUNCTIONS
// ============================================================================

bool webscreen_setup(void) {
    WEBSCREEN_DEBUG_PRINTLN("WebScreen v" WEBSCREEN_VERSION_STRING " initializing...");
    
    // Initialize hardware first
    if (!initialize_hardware()) {
        WEBSCREEN_DEBUG_PRINTLN("Hardware initialization failed");
        return false;
    }
    
    
    // Initialize storage (SD card) - continue even if it fails
    if (!initialize_storage()) {
        WEBSCREEN_DEBUG_PRINTLN("Warning: Storage initialization failed, using fallback mode");
        g_use_fallback = true;
    }
    
    
    // Load configuration from SD card - continue even if it fails
    if (!g_use_fallback && !load_configuration()) {
        WEBSCREEN_DEBUG_PRINTLN("Warning: Configuration load failed, using defaults");
        // Don't force fallback just for missing config
    }
    
    
    // Initialize network (optional)
    if (!g_use_fallback && g_webscreen_config.wifi.enabled) {
        if (!initialize_network()) {
            WEBSCREEN_DEBUG_PRINTLN("Warning: Network initialization failed");
        }
    }
    
    
    // Start appropriate runtime
    if (!start_runtime()) {
        WEBSCREEN_DEBUG_PRINTLN("Runtime initialization failed - using fallback");
        g_use_fallback = true;
        if (!webscreen_runtime_start_fallback()) {
            WEBSCREEN_DEBUG_PRINTLN("Fallback startup failed");
            return false;
        }
    }
    
    WEBSCREEN_DEBUG_PRINTF("WebScreen initialization complete - Mode: %s\n",
                           g_use_fallback ? "Fallback" : "JavaScript");
    
    return true;
}

void webscreen_loop(void) {
    run_main_loop();
}

const char* webscreen_get_state(void) {
    switch (g_app_state) {
        case WEBSCREEN_STATE_INITIALIZING: return "Initializing";
        case WEBSCREEN_STATE_RUNNING_JS: return "Running JavaScript";
        case WEBSCREEN_STATE_RUNNING_FALLBACK: return "Running Fallback";
        case WEBSCREEN_STATE_ERROR: return "Error";
        case WEBSCREEN_STATE_SHUTDOWN: return "Shutdown";
        default: return "Unknown";
    }
}

bool webscreen_is_healthy(void) {
    return g_system_healthy;
}

void webscreen_shutdown(void) {
    WEBSCREEN_DEBUG_PRINTLN("Initiating graceful shutdown...");
    
    // Stop runtime
    webscreen_runtime_shutdown();
    
    // Shutdown network
    webscreen_network_shutdown();
    
    // Shutdown hardware
    webscreen_hardware_shutdown();
    
    g_app_state = WEBSCREEN_STATE_SHUTDOWN;
    WEBSCREEN_DEBUG_PRINTLN("Shutdown complete");
}

// ============================================================================
// PRIVATE IMPLEMENTATION
// ============================================================================

static bool initialize_hardware(void) {
    WEBSCREEN_DEBUG_PRINTLN("Initializing hardware...");
    
    // Initialize pins
    WEBSCREEN_PIN_MODE(WEBSCREEN_PIN_LED, OUTPUT);
    WEBSCREEN_PIN_MODE(WEBSCREEN_PIN_BUTTON, INPUT_PULLUP);
    WEBSCREEN_PIN_MODE(WEBSCREEN_PIN_OUTPUT, OUTPUT);
    
    // Turn on power LED
    WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_LED);
    WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_OUTPUT);
    
    // Initialize display hardware
    if (!webscreen_hardware_init()) {
        WEBSCREEN_DEBUG_PRINTLN("Error: Display initialization failed");
        return false;
    }
    
    WEBSCREEN_DEBUG_PRINTLN("Hardware initialization complete");
    return true;
}

static bool initialize_storage(void) {
    WEBSCREEN_DEBUG_PRINTLN("Initializing SD Card...");
    SD_MMC.setPins(WEBSCREEN_SD_CLK, WEBSCREEN_SD_CMD, WEBSCREEN_SD_D0);

    // Attempt to mount with retries and different speeds (based on working original)
    for (int i = 0; i < 3; i++) {
        // First, try with a slower, more compatible frequency for initialization
        WEBSCREEN_DEBUG_PRINTF("Attempt %d: Mounting SD card at a safe, low frequency...\n", i + 1);
        if (SD_MMC.begin("/sdcard", true, false, 400000)) { // 400 kHz is very safe
            WEBSCREEN_DEBUG_PRINTLN("SD Card mounted successfully at low frequency.");
            
            // Now, try to re-initialize at a higher speed for performance
            SD_MMC.end(); // Unmount first
            WEBSCREEN_DEBUG_PRINTLN("Re-mounting SD card at high frequency...");
            if (SD_MMC.begin("/sdcard", true, false, 10000000)) { // 10 MHz is a good target
                WEBSCREEN_DEBUG_PRINTLN("SD Card re-mounted successfully at high frequency.");
                return true;
            } else {
                WEBSCREEN_DEBUG_PRINTLN("Failed to re-mount at high frequency. Falling back to low speed mount.");
                // If high speed fails, mount again at low speed and continue
                if(SD_MMC.begin("/sdcard", true, false, 400000)) {
                    WEBSCREEN_DEBUG_PRINTLN("Continuing at safe, low frequency.");
                    return true;
                }
            }
        }
        
        WEBSCREEN_DEBUG_PRINTF("Attempt %d failed. Retrying in 200ms...\n", i + 1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    WEBSCREEN_DEBUG_PRINTLN("All attempts to mount SD card failed.");
    return false;
}

static bool load_configuration(void) {
    WEBSCREEN_DEBUG_PRINTLN("Loading configuration...");
    
    // Check if config file exists
    if (!SD_MMC.exists(WEBSCREEN_CONFIG_FILENAME)) {
        WEBSCREEN_DEBUG_PRINTLN("Config file not found, using defaults");
        return false;
    }
    
    // Read config file
    File configFile = SD_MMC.open(WEBSCREEN_CONFIG_FILENAME, FILE_READ);
    if (!configFile) {
        WEBSCREEN_DEBUG_PRINTLN("Failed to open config file");
        return false;
    }
    
    // Parse JSON
    String configStr = configFile.readString();
    configFile.close();
    
    StaticJsonDocument<WEBSCREEN_CONFIG_BUFFER_SIZE> doc;
    DeserializationError error = deserializeJson(doc, configStr);
    
    if (error) {
        WEBSCREEN_DEBUG_PRINTF("Config parse error: %s\n", error.c_str());
        return false;
    }
    
    // Load configuration values
    if (doc["wifi"]["ssid"]) {
        WEBSCREEN_STR_COPY(g_webscreen_config.wifi.ssid, 
                          doc["wifi"]["ssid"], sizeof(g_webscreen_config.wifi.ssid));
    }
    if (doc["wifi"]["password"]) {
        WEBSCREEN_STR_COPY(g_webscreen_config.wifi.password, 
                          doc["wifi"]["password"], sizeof(g_webscreen_config.wifi.password));
    }
    
    g_webscreen_config.wifi.enabled = doc["wifi"]["enabled"] | g_webscreen_config.wifi.enabled;
    g_webscreen_config.display.brightness = doc["display"]["brightness"] | g_webscreen_config.display.brightness;
    g_webscreen_config.system.log_level = doc["system"]["log_level"] | g_webscreen_config.system.log_level;
    
    if (doc["script_file"]) {
        WEBSCREEN_STR_COPY(g_webscreen_config.script_file, 
                          doc["script_file"], sizeof(g_webscreen_config.script_file));
    }
    
    WEBSCREEN_DEBUG_PRINTLN("Configuration loaded successfully");
    return true;
}

static bool initialize_network(void) {
    WEBSCREEN_DEBUG_PRINTLN("Initializing network...");
    
    if (strlen(g_webscreen_config.wifi.ssid) == 0) {
        WEBSCREEN_DEBUG_PRINTLN("No WiFi SSID configured");
        return false;
    }
    
    return webscreen_network_init(&g_webscreen_config);
}

static bool start_runtime(void) {
    WEBSCREEN_DEBUG_PRINTLN("Starting runtime...");
    
    // Check if we should use fallback mode
    if (g_use_fallback) {
        WEBSCREEN_DEBUG_PRINTLN("Starting fallback application");
        g_app_state = WEBSCREEN_STATE_RUNNING_FALLBACK;
        return webscreen_runtime_start_fallback();
    }
    
    // Check if script file exists
    if (!SD_MMC.exists(g_webscreen_config.script_file)) {
        WEBSCREEN_DEBUG_PRINTF("Script file not found: %s\n", g_webscreen_config.script_file);
        WEBSCREEN_DEBUG_PRINTLN("Falling back to fallback application");
        g_use_fallback = true;
        g_app_state = WEBSCREEN_STATE_RUNNING_FALLBACK;
        return webscreen_runtime_start_fallback();
    }
    
    // Start JavaScript runtime
    WEBSCREEN_DEBUG_PRINTF("Starting JavaScript runtime with: %s\n", g_webscreen_config.script_file);
    
    if (webscreen_runtime_start_javascript(g_webscreen_config.script_file)) {
        g_app_state = WEBSCREEN_STATE_RUNNING_JS;
        return true;
    } else {
        WEBSCREEN_DEBUG_PRINTLN("JavaScript runtime failed, using fallback");
        g_use_fallback = true;
        g_app_state = WEBSCREEN_STATE_RUNNING_FALLBACK;
        return webscreen_runtime_start_fallback();
    }
}

static void run_main_loop(void) {
    
    // Handle power button
    webscreen_hardware_handle_button();
    
    // Run appropriate runtime
    switch (g_app_state) {
        case WEBSCREEN_STATE_RUNNING_JS:
            webscreen_runtime_loop_javascript();
            break;
            
        case WEBSCREEN_STATE_RUNNING_FALLBACK:
            webscreen_runtime_loop_fallback();
            break;
            
        case WEBSCREEN_STATE_ERROR:
            // In error state, just wait
            WEBSCREEN_DELAY(1000);
            break;
            
        case WEBSCREEN_STATE_SHUTDOWN:
            // System is shutting down
            return;
            
        default:
            WEBSCREEN_DEBUG_PRINTF("Invalid app state: %d\n", g_app_state);
            g_app_state = WEBSCREEN_STATE_ERROR;
            break;
    }
    
    // Network maintenance
    if (!g_use_fallback && g_webscreen_config.wifi.enabled) {
        webscreen_network_loop();
    }
    
    // System health monitoring
    handle_system_health();
    
    // Small delay to prevent overwhelming the system
    WEBSCREEN_DELAY(WEBSCREEN_LOOP_DELAY_MS);
}

static void handle_system_health(void) {
    uint32_t now = WEBSCREEN_MILLIS();
    
    // Check system health every 30 seconds
    if (now - g_last_health_check > 30000) {
        g_last_health_check = now;
        
        // Check free memory
        uint32_t free_heap = ESP.getFreeHeap();
        uint32_t total_heap = ESP.getHeapSize();
        float memory_usage = 1.0f - ((float)free_heap / (float)total_heap);
        
        if (memory_usage > WEBSCREEN_MEMORY_WARNING_THRESHOLD) {
            WEBSCREEN_DEBUG_PRINTF("Warning: High memory usage (%.1f%%)\n", memory_usage * 100);
            g_system_healthy = false;
        } else {
            g_system_healthy = true;
        }
        
        // Print statistics every 5 minutes
        if (now - g_last_stats_print > WEBSCREEN_STATS_REPORT_INTERVAL_MS) {
            g_last_stats_print = now;
            WEBSCREEN_DEBUG_PRINTF("System Health: %s, Free Heap: %d bytes, Uptime: %lu ms\n",
                                   g_system_healthy ? "Good" : "Degraded",
                                   free_heap, now);
        }
    }
}

