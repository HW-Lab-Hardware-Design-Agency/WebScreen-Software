/**
 * @file config_manager.h
 * @brief Centralized configuration management system
 * 
 * Provides a unified interface for loading, saving, and managing application
 * configuration with validation and default value handling.
 */

#pragma once

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi configuration structure
 */
typedef struct {
    char ssid[64];              ///< WiFi SSID
    char password[64];          ///< WiFi password
    bool enabled;               ///< WiFi enabled flag
    uint32_t connection_timeout; ///< Connection timeout in ms
    bool auto_reconnect;        ///< Auto-reconnect on disconnect
} webscreen_wifi_config_t;

/**
 * @brief MQTT configuration structure
 */
typedef struct {
    char broker[128];           ///< MQTT broker URL
    uint16_t port;              ///< MQTT broker port
    char username[64];          ///< MQTT username
    char password[64];          ///< MQTT password
    char client_id[32];         ///< MQTT client ID
    bool enabled;               ///< MQTT enabled flag
    uint16_t keepalive;         ///< Keep-alive interval in seconds
} webscreen_mqtt_config_t;

/**
 * @brief Display configuration structure
 */
typedef struct {
    uint8_t brightness;         ///< Display brightness (0-255)
    uint8_t rotation;           ///< Display rotation (0-3)
    uint32_t background_color;  ///< Background color (RGB)
    uint32_t foreground_color;  ///< Foreground color (RGB)
    bool auto_brightness;       ///< Auto-brightness enabled
    uint32_t screen_timeout;    ///< Screen timeout in ms (0 = never)
} webscreen_display_config_t;

/**
 * @brief System configuration structure
 */
typedef struct {
    char device_name[32];       ///< Device name
    char timezone[32];          ///< Timezone string
    uint8_t log_level;          ///< Logging level
    bool performance_mode;      ///< Performance mode enabled
    uint32_t watchdog_timeout;  ///< Watchdog timeout in ms
} webscreen_system_config_t;

/**
 * @brief Main configuration structure
 */
typedef struct {
    webscreen_wifi_config_t wifi;       ///< WiFi configuration
    webscreen_mqtt_config_t mqtt;       ///< MQTT configuration  
    webscreen_display_config_t display; ///< Display configuration
    webscreen_system_config_t system;   ///< System configuration
    char script_file[128];              ///< JavaScript file to execute
    uint32_t config_version;            ///< Configuration version
    uint32_t last_modified;             ///< Last modification timestamp
} webscreen_config_t;

/**
 * @brief Configuration validation result
 */
typedef enum {
    CONFIG_VALIDATION_OK = 0,           ///< Configuration is valid
    CONFIG_VALIDATION_WARNING,          ///< Configuration has warnings
    CONFIG_VALIDATION_ERROR             ///< Configuration has errors
} config_validation_result_t;

/**
 * @brief Initialize the configuration manager
 * @return true if initialization successful, false otherwise
 */
bool config_manager_init(void);

/**
 * @brief Load configuration from file
 * @param filename Configuration file path (JSON format)
 * @return true if loaded successfully, false otherwise
 */
bool config_manager_load(const char* filename);

/**
 * @brief Save current configuration to file
 * @param filename Configuration file path (NULL for default)
 * @return true if saved successfully, false otherwise
 */
bool config_manager_save(const char* filename);

/**
 * @brief Get pointer to current configuration
 * @return Pointer to configuration structure
 */
webscreen_config_t* config_manager_get_config(void);

/**
 * @brief Reset configuration to factory defaults
 */
void config_manager_reset_to_defaults(void);

/**
 * @brief Validate current configuration
 * @return Validation result
 */
config_validation_result_t config_manager_validate(void);

/**
 * @brief Update WiFi configuration
 * @param wifi_config New WiFi configuration
 * @return true if updated successfully, false otherwise
 */
bool config_manager_set_wifi(const webscreen_wifi_config_t* wifi_config);

/**
 * @brief Update MQTT configuration
 * @param mqtt_config New MQTT configuration
 * @return true if updated successfully, false otherwise
 */
bool config_manager_set_mqtt(const webscreen_mqtt_config_t* mqtt_config);

/**
 * @brief Update display configuration
 * @param display_config New display configuration
 * @return true if updated successfully, false otherwise
 */
bool config_manager_set_display(const webscreen_display_config_t* display_config);

/**
 * @brief Update system configuration
 * @param system_config New system configuration
 * @return true if updated successfully, false otherwise
 */
bool config_manager_set_system(const webscreen_system_config_t* system_config);

/**
 * @brief Set script file to execute
 * @param script_file Path to JavaScript file
 * @return true if set successfully, false otherwise
 */
bool config_manager_set_script_file(const char* script_file);

/**
 * @brief Get configuration as JSON string
 * @param buffer Buffer to store JSON string
 * @param buffer_size Size of buffer
 * @return true if successful, false if buffer too small
 */
bool config_manager_to_json(char* buffer, size_t buffer_size);

/**
 * @brief Load configuration from JSON string
 * @param json_string JSON configuration string
 * @return true if loaded successfully, false otherwise
 */
bool config_manager_from_json(const char* json_string);

/**
 * @brief Check if configuration has been modified since last save
 * @return true if modified, false otherwise
 */
bool config_manager_is_modified(void);

/**
 * @brief Mark configuration as modified
 */
void config_manager_mark_modified(void);

/**
 * @brief Print current configuration to serial
 */
void config_manager_print_config(void);

/**
 * @brief Get configuration file version
 * @return Configuration version number
 */
uint32_t config_manager_get_version(void);

/**
 * @brief Backup current configuration
 * @param backup_filename Backup file path
 * @return true if backup successful, false otherwise
 */
bool config_manager_backup(const char* backup_filename);

/**
 * @brief Restore configuration from backup
 * @param backup_filename Backup file path
 * @return true if restore successful, false otherwise
 */
bool config_manager_restore(const char* backup_filename);

#ifdef __cplusplus
}
#endif