/**
 * @file webscreen_runtime.h
 * @brief Runtime environment management for WebScreen
 * 
 * Manages JavaScript and fallback application runtimes following
 * Arduino best practices for modular design.
 */

#pragma once

#include "webscreen_config.h"

#ifdef __cplusplus
extern "C" {
#endif

  // ============================================================================
  // RUNTIME MANAGEMENT
  // ============================================================================

  /**
 * @brief Start JavaScript runtime
 * 
 * Initializes the JavaScript engine and loads the specified script file.
 * 
 * @param script_file Path to JavaScript file on SD card
 * @return true if runtime started successfully, false otherwise
 */

  bool webscreen_runtime_start_javascript(const char* script_file);

  /**
 * @brief Start fallback application
 * 
 * Starts the built-in fallback application when JavaScript runtime
 * is unavailable or has failed.
 * 
 * @return true if fallback started successfully, false otherwise
 */

  bool webscreen_runtime_start_fallback(void);

  /**
 * @brief Run JavaScript runtime loop
 * 
 * Executes one iteration of the JavaScript runtime. Should be called
 * repeatedly from the main loop when in JavaScript mode.
 */

  void webscreen_runtime_loop_javascript(void);

  /**
 * @brief Run fallback application loop
 * 
 * Executes one iteration of the fallback application. Should be called
 * repeatedly from the main loop when in fallback mode.
 */

  void webscreen_runtime_loop_fallback(void);

  /**
 * @brief Shutdown all runtimes
 * 
 * Cleanly shuts down the active runtime and frees resources.
 */

  void webscreen_runtime_shutdown(void);

  // ============================================================================
  // JAVASCRIPT ENGINE
  // ============================================================================

  /**
 * @brief Check if JavaScript runtime is active
 * @return true if JavaScript is running, false otherwise
 */

  bool webscreen_runtime_is_javascript_active(void);

  /**
 * @brief Get JavaScript engine status
 * @return String describing current status
 */
  const char* webscreen_runtime_get_javascript_status(void);

  /**
 * @brief Execute JavaScript code
 * @param code JavaScript code string to execute
 * @return true if execution successful, false on error
 */

  bool webscreen_runtime_execute_javascript(const char* code);

  /**
 * @brief Get JavaScript execution statistics
 * @param exec_count Pointer to store execution count
 * @param avg_time_us Pointer to store average execution time in microseconds
 * @param error_count Pointer to store error count
 */

  void webscreen_runtime_get_javascript_stats(uint32_t* exec_count,
                                              uint32_t* avg_time_us,
                                              uint32_t* error_count);

  // ============================================================================
  // FALLBACK APPLICATION
  // ============================================================================

  /**
 * @brief Check if fallback application is active
 * @return true if fallback is running, false otherwise
 */

  bool webscreen_runtime_is_fallback_active(void);

  /**
 * @brief Update fallback display text
 * @param text Text to display in fallback application
 */

  void webscreen_runtime_set_fallback_text(const char* text);

  /**
 * @brief Get fallback application status
 * @return String describing current status
 */
  const char* webscreen_runtime_get_fallback_status(void);

  // ============================================================================
  // LVGL INTEGRATION
  // ============================================================================

  /**
 * @brief Initialize LVGL graphics library
 * @return true if initialization successful, false otherwise
 */

  bool webscreen_runtime_init_lvgl(void);

  /**
 * @brief Run LVGL timer handler
 * 
 * Processes LVGL timers and animations. Should be called regularly
 * from both JavaScript and fallback runtime loops.
 */

  void webscreen_runtime_lvgl_timer_handler(void);

  /**
 * @brief Get LVGL display object
 * @return Pointer to LVGL display object, or NULL if not initialized
 */
  void* webscreen_runtime_get_lvgl_display(void);

  /**
 * @brief Set LVGL background color
 * @param color RGB color value (0xRRGGBB)
 */

  void webscreen_runtime_set_background_color(uint32_t color);

  /**
 * @brief Set LVGL foreground color
 * @param color RGB color value (0xRRGGBB)
 */

  void webscreen_runtime_set_foreground_color(uint32_t color);

  // ============================================================================
  // MEMORY MANAGEMENT
  // ============================================================================

  /**
 * @brief Get runtime memory usage
 * @param js_heap_used Pointer to store JavaScript heap usage
 * @param lvgl_memory_used Pointer to store LVGL memory usage
 * @param total_runtime_memory Pointer to store total runtime memory
 */

  void webscreen_runtime_get_memory_usage(uint32_t* js_heap_used,
                                          uint32_t* lvgl_memory_used,
                                          uint32_t* total_runtime_memory);

  /**
 * @brief Force garbage collection (if supported)
 * @return true if garbage collection was performed
 */

  bool webscreen_runtime_garbage_collect(void);

  // ============================================================================
  // ERROR HANDLING
  // ============================================================================

  /**
 * @brief Get last runtime error
 * @return String describing last error, or NULL if no errors
 */
  const char* webscreen_runtime_get_last_error(void);

  /**
 * @brief Clear runtime error state
 */

  void webscreen_runtime_clear_errors(void);

  /**
 * @brief Check if runtime is in error state
 * @return true if runtime has errors, false otherwise
 */

  bool webscreen_runtime_has_errors(void);

  // ============================================================================
  // PERFORMANCE MONITORING
  // ============================================================================

  /**
 * @brief Enable or disable performance monitoring
 * @param enable true to enable monitoring, false to disable
 */

  void webscreen_runtime_set_performance_monitoring(bool enable);

  /**
 * @brief Get performance statistics
 * @param avg_loop_time_us Average loop time in microseconds
 * @param max_loop_time_us Maximum loop time in microseconds
 * @param fps Frames per second (for display updates)
 */

  void webscreen_runtime_get_performance_stats(uint32_t* avg_loop_time_us,
                                               uint32_t* max_loop_time_us,
                                               uint32_t* fps);

  /**
 * @brief Print runtime status to serial
 */

  void webscreen_runtime_print_status(void);

  // ============================================================================
  // JAVASCRIPT ENGINE INTERNAL FUNCTIONS
  // ============================================================================

  /**
 * @brief Initialize the JavaScript engine (Elk)
 * @return true if initialization successful, false otherwise
 */

  bool webscreen_runtime_init_javascript_engine(void);

  /**
 * @brief Load JavaScript script from SD card
 * @param script_file Path to script file
 * @return true if script loaded successfully, false otherwise
 */

  bool webscreen_runtime_load_script(const char* script_file);

  /**
 * @brief Start JavaScript execution task
 * @return true if task started successfully, false otherwise
 */

  bool webscreen_runtime_start_javascript_task(void);

  /**
 * @brief JavaScript execution task function
 * @param pvParameters Task parameters (unused)
 */

  void webscreen_runtime_javascript_task(void* pvParameters);

  /**
 * @brief Register JavaScript API functions
 */

  void webscreen_runtime_register_js_functions(void);

  /**
 * @brief Maintain WiFi and MQTT connections
 */

  void webscreen_runtime_wifi_mqtt_maintain_loop(void);

  /**
 * @brief Initialize LVGL SD filesystem driver ('S' drive)
 * @return true if initialization successful, false otherwise
 */

  bool webscreen_runtime_init_sd_filesystem(void);

  /**
 * @brief Initialize LVGL memory filesystem driver ('M' drive)
 * @return true if initialization successful, false otherwise
 */

  bool webscreen_runtime_init_memory_filesystem(void);

  /**
 * @brief Initialize RAM images storage
 * @return true if initialization successful, false otherwise
 */

  bool webscreen_runtime_init_ram_images(void);

#ifdef __cplusplus
}
#endif

// ============================================================================
// COMPATIBILITY WITH EXISTING CODE
// ============================================================================

// Legacy function names for backward compatibility
#ifdef WEBSCREEN_ENABLE_DEPRECATED_API
#define dynamic_js_setup() webscreen_runtime_start_javascript(g_webscreen_config.script_file)
#define dynamic_js_loop() webscreen_runtime_loop_javascript()
#define fallback_setup() webscreen_runtime_start_fallback()
#define fallback_loop() webscreen_runtime_loop_fallback()
#endif

// ============================================================================
// ARDUINO LIBRARY DEPENDENCIES
// ============================================================================

/*
Runtime Dependencies:
- LVGL library (graphics)
- ArduinoJson (configuration parsing)
- Elk JavaScript engine (included in sketch)

The runtime system automatically handles:
- Memory allocation for graphics buffers
- JavaScript heap management
- LVGL integration and timing
- Error recovery and fallback modes
*/