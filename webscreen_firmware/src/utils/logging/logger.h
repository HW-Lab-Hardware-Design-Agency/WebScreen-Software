/**
 * @file logger.h
 * @brief Advanced logging system for WebScreen
 * 
 * Provides structured logging with different levels, modules, and optional
 * output to serial, SD card, or network endpoints.
 */

#pragma once

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Log levels in order of severity
 */
typedef enum {
    WEBSCREEN_LOG_LEVEL_DEBUG = 0,  ///< Detailed information for debugging
    WEBSCREEN_LOG_LEVEL_INFO,       ///< General information
    WEBSCREEN_LOG_LEVEL_WARN,       ///< Warning conditions
    WEBSCREEN_LOG_LEVEL_ERROR,      ///< Error conditions
    WEBSCREEN_LOG_LEVEL_FATAL,      ///< Fatal errors that may cause system halt
    WEBSCREEN_LOG_LEVEL_NONE        ///< No logging
} webscreen_log_level_t;

/**
 * @brief Log output destinations
 */
typedef enum {
    WEBSCREEN_LOG_OUTPUT_SERIAL = 0x01,    ///< Output to Serial
    WEBSCREEN_LOG_OUTPUT_SD = 0x02,        ///< Output to SD card file
    WEBSCREEN_LOG_OUTPUT_NETWORK = 0x04,   ///< Output to network endpoint
    WEBSCREEN_LOG_OUTPUT_ALL = 0xFF        ///< Output to all destinations
} webscreen_log_output_t;

/**
 * @brief Logger configuration
 */
typedef struct {
    webscreen_log_level_t min_level;       ///< Minimum level to log
    uint8_t output_mask;                   ///< Bitmask of output destinations
    bool include_timestamp;                ///< Include timestamp in logs
    bool include_level;                    ///< Include level name in logs
    bool include_module;                   ///< Include module name in logs
    bool colored_output;                   ///< Use ANSI colors for serial output
    const char* sd_log_file;               ///< SD card log file path
    uint32_t max_sd_file_size;             ///< Max SD log file size before rotation
} webscreen_logger_config_t;

/**
 * @brief Initialize the logging system
 * @param config Logger configuration, or NULL for defaults
 * @return true if initialization successful, false otherwise
 */
bool webscreen_logger_init(const webscreen_logger_config_t* config);

/**
 * @brief Log a message with specified level and module
 * @param level Log level
 * @param module Module name (can be NULL)
 * @param format Printf-style format string
 * @param ... Arguments for format string
 */
void webscreen_log(webscreen_log_level_t level, const char* module, const char* format, ...);

/**
 * @brief Set the minimum log level
 * @param level New minimum level
 */
void webscreen_logger_set_level(webscreen_log_level_t level);

/**
 * @brief Set output destinations
 * @param output_mask Bitmask of webscreen_log_output_t flags
 */
void webscreen_logger_set_output(uint8_t output_mask);

/**
 * @brief Flush all log outputs
 */
void webscreen_logger_flush(void);

/**
 * @brief Get current logger statistics
 * @param messages_logged Total messages logged (output parameter)
 * @param errors_logged Error and fatal messages logged (output parameter)
 */
void webscreen_logger_get_stats(uint32_t* messages_logged, uint32_t* errors_logged);

/**
 * @brief Log system information (memory, heap, etc.)
 */
void webscreen_logger_log_system_info(void);

// Convenience macros for different log levels
#define WEBSCREEN_LOG_DEBUG(module, format, ...) \
    webscreen_log(WEBSCREEN_LOG_LEVEL_DEBUG, module, format, ##__VA_ARGS__)

#define WEBSCREEN_LOG_INFO(module, format, ...) \
    webscreen_log(WEBSCREEN_LOG_LEVEL_INFO, module, format, ##__VA_ARGS__)

#define WEBSCREEN_LOG_WARN(module, format, ...) \
    webscreen_log(WEBSCREEN_LOG_LEVEL_WARN, module, format, ##__VA_ARGS__)

#define WEBSCREEN_LOG_ERROR(module, format, ...) \
    webscreen_log(WEBSCREEN_LOG_LEVEL_ERROR, module, format, ##__VA_ARGS__)

#define WEBSCREEN_LOG_FATAL(module, format, ...) \
    webscreen_log(WEBSCREEN_LOG_LEVEL_FATAL, module, format, ##__VA_ARGS__)

// Backward compatibility with existing LOG macros
#define LOG(message) WEBSCREEN_LOG_INFO(nullptr, "%s", message)
#define LOGF(format, ...) WEBSCREEN_LOG_INFO(nullptr, format, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif