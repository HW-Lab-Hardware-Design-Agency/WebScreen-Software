/**
 * @file logger.cpp
 * @brief Implementation of advanced logging system
 */

#include "logger.h"
#include <stdarg.h>
#include <FS.h>
#include <SD_MMC.h>

// ANSI color codes for terminal output
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Default configuration
static webscreen_logger_config_t g_logger_config = {
    .min_level = WEBSCREEN_LOG_LEVEL_INFO,
    .output_mask = WEBSCREEN_LOG_OUTPUT_SERIAL,
    .include_timestamp = true,
    .include_level = true,
    .include_module = true,
    .colored_output = true,
    .sd_log_file = "/webscreen.log",
    .max_sd_file_size = 1024 * 1024  // 1MB
};

// Statistics
static uint32_t g_messages_logged = 0;
static uint32_t g_errors_logged = 0;
static bool g_logger_initialized = false;

// Level names and colors
static const char* g_level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static const char* g_level_colors[] = {
    ANSI_COLOR_CYAN,    // DEBUG
    ANSI_COLOR_BLUE,    // INFO  
    ANSI_COLOR_YELLOW,  // WARN
    ANSI_COLOR_RED,     // ERROR
    ANSI_COLOR_MAGENTA  // FATAL
};

bool webscreen_logger_init(const webscreen_logger_config_t* config) {
    if (config) {
        g_logger_config = *config;
    }
    
    // Initialize serial if not already done
    if (!Serial) {
        Serial.begin(115200);
    }
    
    g_messages_logged = 0;
    g_errors_logged = 0;
    g_logger_initialized = true;
    
    WEBSCREEN_LOG_INFO("Logger", "Logging system initialized");
    
    return true;
}

static void log_to_serial(webscreen_log_level_t level, const char* module, 
                         const char* timestamp, const char* message) {
    // Build the log line
    String log_line = "";
    
    if (g_logger_config.colored_output && level < WEBSCREEN_LOG_LEVEL_NONE) {
        log_line += g_level_colors[level];
    }
    
    if (g_logger_config.include_timestamp && timestamp) {
        log_line += "[";
        log_line += timestamp;
        log_line += "] ";
    }
    
    if (g_logger_config.include_level && level < WEBSCREEN_LOG_LEVEL_NONE) {
        log_line += g_level_names[level];
        log_line += ": ";
    }
    
    if (g_logger_config.include_module && module) {
        log_line += "[";
        log_line += module;
        log_line += "] ";
    }
    
    log_line += message;
    
    if (g_logger_config.colored_output) {
        log_line += ANSI_COLOR_RESET;
    }
    
    Serial.println(log_line);
}

static void log_to_sd(webscreen_log_level_t level, const char* module, 
                     const char* timestamp, const char* message) {
    if (!g_logger_config.sd_log_file) {
        return;
    }
    
    // Check if SD card is available
    if (!SD_MMC.cardType()) {
        return;
    }
    
    // Check file size and rotate if necessary
    File log_file = SD_MMC.open(g_logger_config.sd_log_file, FILE_READ);
    if (log_file && log_file.size() > g_logger_config.max_sd_file_size) {
        log_file.close();
        
        // Rotate log file
        String backup_name = String(g_logger_config.sd_log_file) + ".old";
        SD_MMC.remove(backup_name.c_str());
        SD_MMC.rename(g_logger_config.sd_log_file, backup_name.c_str());
    } else if (log_file) {
        log_file.close();
    }
    
    // Append to log file
    log_file = SD_MMC.open(g_logger_config.sd_log_file, FILE_APPEND);
    if (log_file) {
        // Build log line for SD (no colors)
        String log_line = "";
        
        if (g_logger_config.include_timestamp && timestamp) {
            log_line += "[";
            log_line += timestamp;
            log_line += "] ";
        }
        
        if (g_logger_config.include_level && level < WEBSCREEN_LOG_LEVEL_NONE) {
            log_line += g_level_names[level];
            log_line += ": ";
        }
        
        if (g_logger_config.include_module && module) {
            log_line += "[";
            log_line += module;
            log_line += "] ";
        }
        
        log_line += message;
        log_line += "\n";
        
        log_file.print(log_line);
        log_file.close();
    }
}

void webscreen_log(webscreen_log_level_t level, const char* module, const char* format, ...) {
    if (!g_logger_initialized) {
        webscreen_logger_init(nullptr);
    }
    
    // Check if this level should be logged
    if (level < g_logger_config.min_level) {
        return;
    }
    
    // Format the message
    char message[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    // Generate timestamp if needed
    char timestamp[32] = {0};
    if (g_logger_config.include_timestamp) {
        uint32_t now = millis();
        snprintf(timestamp, sizeof(timestamp), "%lu.%03lu", now / 1000, now % 1000);
    }
    
    // Output to configured destinations
    if (g_logger_config.output_mask & WEBSCREEN_LOG_OUTPUT_SERIAL) {
        log_to_serial(level, module, timestamp, message);
    }
    
    if (g_logger_config.output_mask & WEBSCREEN_LOG_OUTPUT_SD) {
        log_to_sd(level, module, timestamp, message);
    }
    
    // Update statistics
    g_messages_logged++;
    if (level >= WEBSCREEN_LOG_LEVEL_ERROR) {
        g_errors_logged++;
    }
}

void webscreen_logger_set_level(webscreen_log_level_t level) {
    g_logger_config.min_level = level;
    WEBSCREEN_LOG_INFO("Logger", "Log level set to %s", 
                       level < WEBSCREEN_LOG_LEVEL_NONE ? g_level_names[level] : "NONE");
}

void webscreen_logger_set_output(uint8_t output_mask) {
    g_logger_config.output_mask = output_mask;
    WEBSCREEN_LOG_INFO("Logger", "Log output mask set to 0x%02X", output_mask);
}

void webscreen_logger_flush(void) {
    if (g_logger_config.output_mask & WEBSCREEN_LOG_OUTPUT_SERIAL) {
        Serial.flush();
    }
    
    // SD card writes are typically synchronous, so no explicit flush needed
}

void webscreen_logger_get_stats(uint32_t* messages_logged, uint32_t* errors_logged) {
    if (messages_logged) {
        *messages_logged = g_messages_logged;
    }
    if (errors_logged) {
        *errors_logged = g_errors_logged;
    }
}

void webscreen_logger_log_system_info(void) {
    WEBSCREEN_LOG_INFO("System", "=== SYSTEM INFORMATION ===");
    WEBSCREEN_LOG_INFO("System", "Chip Model: %s", ESP.getChipModel());
    WEBSCREEN_LOG_INFO("System", "Chip Revision: %d", ESP.getChipRevision());
    WEBSCREEN_LOG_INFO("System", "CPU Frequency: %d MHz", ESP.getCpuFreqMHz());
    WEBSCREEN_LOG_INFO("System", "Flash Size: %d bytes", ESP.getFlashChipSize());
    WEBSCREEN_LOG_INFO("System", "Free Heap: %d bytes", ESP.getFreeHeap());
    WEBSCREEN_LOG_INFO("System", "PSRAM Size: %d bytes", ESP.getPsramSize());
    WEBSCREEN_LOG_INFO("System", "Free PSRAM: %d bytes", ESP.getFreePsram());
    WEBSCREEN_LOG_INFO("System", "Uptime: %lu ms", millis());
    
    // Log memory statistics if memory manager is available
    extern void memory_get_stats(memory_stats_t* stats);
    extern bool memory_psram_available(void);
    
    if (memory_psram_available) {
        WEBSCREEN_LOG_INFO("System", "Memory Manager: Available");
    }
    
    WEBSCREEN_LOG_INFO("System", "=== END SYSTEM INFO ===");
}