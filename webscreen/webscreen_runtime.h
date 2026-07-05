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
 * @brief Request an in-place restart of the JS app (no device reboot)
 *
 * The JS task tears down timers/widgets/styles/media buffers, re-creates the
 * Elk engine over the same arena, and re-evaluates the current script.
 * Safe to call from any task.
 *
 * @param reason Short human-readable reason for logging
 */

  void webscreen_runtime_request_restart(const char* reason);

  /**
 * @brief Automatic restart request from the timer-error escalation path
 *
 * Unlike the explicit variant this does not lift safe mode, and repeated
 * automatic restarts without a healthy interval park the app in safe mode.
 * Internal use (timer bridge); user-facing code should call
 * webscreen_runtime_request_restart().
 *
 * @param reason Short human-readable reason for logging
 */

  void webscreen_runtime_request_restart_auto(const char* reason);

  /**
 * @brief Switch to a different script and restart the JS app in place
 *
 * @param script_file Path to JavaScript file on SD card
 * @return true if the file exists and the restart was scheduled
 */

  bool webscreen_runtime_load_new_script(const char* script_file);

  /**
 * @brief Current Elk arena usage
 *
 * @param used  Out: bytes currently used (0 if engine not running)
 * @param total Out: arena capacity in bytes
 */

  void webscreen_runtime_get_js_arena(uint32_t* used, uint32_t* total);

  /**
 * @brief Configure the Elk arena size before the engine starts
 *
 * Clamped to 64..1024 KB. No effect once the arena is allocated.
 *
 * @param kb Requested arena size in kilobytes
 */

  void webscreen_runtime_set_js_heap_kb(int kb);

  /**
 * @brief Queue a one-shot JS snippet for evaluation (serial /eval REPL)
 *
 * The snippet is evaluated by the JS task at its next safe point; the
 * result (or error) is printed to Serial with an [EVAL] prefix. One
 * snippet can be in flight at a time.
 *
 * @param code JS source, at most 255 chars
 * @return true if queued, false if busy / not running / too long
 */

  bool webscreen_runtime_eval_snippet(const char* code);

  /**
 * @brief Record the most recent JS error (kept for the /errors command)
 *
 * Called from the eval/timer/button error paths. Safe from any task.
 *
 * @param msg Short error description
 */

  void webscreen_runtime_note_js_error(const char* msg);

  /**
 * @brief Print the JS error/restart-ladder report to Serial (/errors)
 */

  void webscreen_runtime_print_error_report(void);

  /**
 * @brief Notify the runtime of a power-button short press (loopTask)
 *
 * Queued lock-free for the JS task, which delivers it to the app via
 * on_button()/get_button_event(). Registered as the hardware button
 * callback by dynamic_js_setup().
 *
 * @param pressed Always true (release events are not delivered)
 */

  void webscreen_runtime_notify_button(bool pressed);

  /**
 * @brief Queue a screen capture (/screenshot)
 *
 * The JS task renders the active screen into a PSRAM snapshot at its next
 * safe point and streams it to Serial as base64 between
 * "=== SCREENSHOT <w>x<h> RGB565_SWAP ===" and "=== SCREENSHOT END ===".
 *
 * @return true if queued, false if the runtime is not running or busy
 */

  bool webscreen_runtime_request_screenshot(void);

  /**
 * @brief Run JavaScript runtime loop
 *
 * Executes one iteration of the JavaScript runtime. Should be called
 * repeatedly from the main loop when in JavaScript mode.
 */

  void webscreen_runtime_loop_javascript(void);

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
  // STATUS
  // ============================================================================

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