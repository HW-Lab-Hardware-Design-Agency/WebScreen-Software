/**
 * @file error_handler.h
 * @brief Centralized error handling and recovery system
 * 
 * Provides structured error handling with error codes, recovery strategies,
 * and system health monitoring.
 */

#pragma once

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WebScreen error codes
 */
typedef enum {
    WEBSCREEN_ERROR_NONE = 0,
    
    // Hardware errors (1-99)
    WEBSCREEN_ERROR_SD_INIT_FAILED = 1,
    WEBSCREEN_ERROR_SD_MOUNT_FAILED = 2,
    WEBSCREEN_ERROR_DISPLAY_INIT_FAILED = 3,
    WEBSCREEN_ERROR_MEMORY_ALLOCATION_FAILED = 4,
    WEBSCREEN_ERROR_PSRAM_INIT_FAILED = 5,
    
    // Network errors (100-199)
    WEBSCREEN_ERROR_WIFI_CONNECT_FAILED = 100,
    WEBSCREEN_ERROR_WIFI_TIMEOUT = 101,
    WEBSCREEN_ERROR_HTTP_REQUEST_FAILED = 102,
    WEBSCREEN_ERROR_MQTT_CONNECT_FAILED = 103,
    WEBSCREEN_ERROR_BLE_INIT_FAILED = 104,
    
    // Configuration errors (200-299)
    WEBSCREEN_ERROR_CONFIG_FILE_NOT_FOUND = 200,
    WEBSCREEN_ERROR_CONFIG_PARSE_FAILED = 201,
    WEBSCREEN_ERROR_INVALID_CONFIG = 202,
    WEBSCREEN_ERROR_SCRIPT_FILE_NOT_FOUND = 203,
    
    // Runtime errors (300-399)
    WEBSCREEN_ERROR_JS_RUNTIME_FAILED = 300,
    WEBSCREEN_ERROR_LVGL_INIT_FAILED = 301,
    WEBSCREEN_ERROR_INSUFFICIENT_MEMORY = 302,
    WEBSCREEN_ERROR_WATCHDOG_TIMEOUT = 303,
    
    // System errors (400-499)
    WEBSCREEN_ERROR_SYSTEM_OVERHEATED = 400,
    WEBSCREEN_ERROR_POWER_LOW = 401,
    WEBSCREEN_ERROR_SYSTEM_UNSTABLE = 402,
    
    WEBSCREEN_ERROR_UNKNOWN = 999
} webscreen_error_code_t;

/**
 * @brief Error severity levels
 */
typedef enum {
    WEBSCREEN_ERROR_SEVERITY_INFO,      ///< Informational, system continues normally
    WEBSCREEN_ERROR_SEVERITY_WARNING,   ///< Warning, system continues with degraded functionality
    WEBSCREEN_ERROR_SEVERITY_ERROR,     ///< Error, system attempts recovery
    WEBSCREEN_ERROR_SEVERITY_FATAL      ///< Fatal error, system restart required
} webscreen_error_severity_t;

/**
 * @brief Error recovery strategies
 */
typedef enum {
    WEBSCREEN_RECOVERY_NONE,            ///< No recovery action
    WEBSCREEN_RECOVERY_RETRY,           ///< Retry the failed operation
    WEBSCREEN_RECOVERY_FALLBACK,        ///< Use fallback/degraded mode
    WEBSCREEN_RECOVERY_RESTART_MODULE,  ///< Restart specific module
    WEBSCREEN_RECOVERY_SYSTEM_RESTART   ///< Restart entire system
} webscreen_recovery_strategy_t;

/**
 * @brief Error context information
 */
typedef struct {
    webscreen_error_code_t code;
    webscreen_error_severity_t severity;
    const char* module;
    const char* function;
    int line;
    const char* description;
    uint32_t timestamp;
    uint32_t count;  // Number of times this error occurred
} webscreen_error_t;

/**
 * @brief Error handler callback function type
 * @param error Pointer to error information
 * @return Recovery strategy to apply
 */
typedef webscreen_recovery_strategy_t (*webscreen_error_handler_t)(const webscreen_error_t* error);

/**
 * @brief Initialize the error handling system
 * @return true if initialization successful, false otherwise
 */
bool webscreen_error_handler_init(void);

/**
 * @brief Register a custom error handler for specific error codes
 * @param code Error code to handle
 * @param handler Handler function
 */
void webscreen_error_register_handler(webscreen_error_code_t code, webscreen_error_handler_t handler);

/**
 * @brief Report an error to the error handling system
 * @param code Error code
 * @param severity Error severity
 * @param module Module name where error occurred
 * @param function Function name where error occurred
 * @param line Line number where error occurred
 * @param description Human-readable error description
 * @return Recovery strategy applied
 */
webscreen_recovery_strategy_t webscreen_error_report(
    webscreen_error_code_t code,
    webscreen_error_severity_t severity,
    const char* module,
    const char* function,
    int line,
    const char* description
);

/**
 * @brief Get the last error that occurred
 * @return Pointer to last error, or NULL if no errors
 */
const webscreen_error_t* webscreen_error_get_last(void);

/**
 * @brief Get total number of errors reported
 * @return Number of errors
 */
uint32_t webscreen_error_get_count(void);

/**
 * @brief Get number of fatal errors reported
 * @return Number of fatal errors
 */
uint32_t webscreen_error_get_fatal_count(void);

/**
 * @brief Check if system is in healthy state
 * @return true if system is healthy, false if degraded
 */
bool webscreen_error_system_healthy(void);

/**
 * @brief Print error statistics and recent errors
 */
void webscreen_error_print_report(void);

/**
 * @brief Clear error history (use with caution)
 */
void webscreen_error_clear_history(void);

/**
 * @brief Get human-readable string for error code
 * @param code Error code
 * @return String description of error code
 */
const char* webscreen_error_code_to_string(webscreen_error_code_t code);

// Convenience macros for error reporting
#define WEBSCREEN_ERROR_REPORT(code, severity, description) \
    webscreen_error_report(code, severity, __FILE__, __FUNCTION__, __LINE__, description)

#define WEBSCREEN_ERROR_REPORT_INFO(code, description) \
    WEBSCREEN_ERROR_REPORT(code, WEBSCREEN_ERROR_SEVERITY_INFO, description)

#define WEBSCREEN_ERROR_REPORT_WARNING(code, description) \
    WEBSCREEN_ERROR_REPORT(code, WEBSCREEN_ERROR_SEVERITY_WARNING, description)

#define WEBSCREEN_ERROR_REPORT_ERROR(code, description) \
    WEBSCREEN_ERROR_REPORT(code, WEBSCREEN_ERROR_SEVERITY_ERROR, description)

#define WEBSCREEN_ERROR_REPORT_FATAL(code, description) \
    WEBSCREEN_ERROR_REPORT(code, WEBSCREEN_ERROR_SEVERITY_FATAL, description)

// Macros for checking and handling common conditions
#define WEBSCREEN_CHECK_NULL(ptr, error_code, description) \
    do { \
        if (!(ptr)) { \
            WEBSCREEN_ERROR_REPORT_ERROR(error_code, description); \
            return false; \
        } \
    } while(0)

#define WEBSCREEN_CHECK_ALLOC(ptr, description) \
    WEBSCREEN_CHECK_NULL(ptr, WEBSCREEN_ERROR_MEMORY_ALLOCATION_FAILED, description)

#ifdef __cplusplus
}
#endif