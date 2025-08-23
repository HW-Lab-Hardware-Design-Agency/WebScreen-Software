/**
 * @file display_manager.h
 * @brief Hardware abstraction layer for display management
 * 
 * Provides a unified interface for display operations with performance optimizations
 * and power management features.
 */

#pragma once

#include <Arduino.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Display rotation values
 */
typedef enum {
    DISPLAY_ROTATION_0 = 0,     ///< 0 degrees
    DISPLAY_ROTATION_90 = 1,    ///< 90 degrees clockwise
    DISPLAY_ROTATION_180 = 2,   ///< 180 degrees
    DISPLAY_ROTATION_270 = 3    ///< 270 degrees clockwise
} display_rotation_t;

/**
 * @brief Display power states
 */
typedef enum {
    DISPLAY_POWER_OFF = 0,      ///< Display completely off
    DISPLAY_POWER_STANDBY,      ///< Display in standby mode
    DISPLAY_POWER_LOW,          ///< Low power mode (dimmed)
    DISPLAY_POWER_NORMAL        ///< Normal operation
} display_power_state_t;

/**
 * @brief Display configuration
 */
typedef struct {
    uint16_t width;             ///< Display width in pixels
    uint16_t height;            ///< Display height in pixels
    uint16_t buffer_size;       ///< Size of display buffer
    display_rotation_t rotation; ///< Initial rotation
    uint8_t brightness;         ///< Initial brightness (0-255)
    bool use_double_buffer;     ///< Use double buffering for smoother updates
    bool use_dma;              ///< Use DMA for faster transfers
} display_config_t;

/**
 * @brief Display statistics
 */
typedef struct {
    uint32_t frames_rendered;   ///< Total frames rendered
    uint32_t flush_operations;  ///< Total flush operations
    uint32_t avg_frame_time_us; ///< Average frame time in microseconds
    uint32_t last_fps;          ///< Last calculated FPS
    uint32_t memory_used;       ///< Memory used by display buffers
} display_stats_t;

/**
 * @brief Initialize the display system
 * @param config Display configuration, or NULL for defaults
 * @return true if initialization successful, false otherwise
 */
bool display_manager_init(const display_config_t* config);

/**
 * @brief Shutdown the display system
 */
void display_manager_shutdown(void);

/**
 * @brief Set display power state
 * @param state New power state
 * @return true if successful, false otherwise
 */
bool display_set_power_state(display_power_state_t state);

/**
 * @brief Get current display power state
 * @return Current power state
 */
display_power_state_t display_get_power_state(void);

/**
 * @brief Set display brightness
 * @param brightness Brightness level (0-255)
 * @return true if successful, false otherwise
 */
bool display_set_brightness(uint8_t brightness);

/**
 * @brief Get current display brightness
 * @return Current brightness level (0-255)
 */
uint8_t display_get_brightness(void);

/**
 * @brief Set display rotation
 * @param rotation New rotation
 * @return true if successful, false otherwise
 */
bool display_set_rotation(display_rotation_t rotation);

/**
 * @brief Get current display rotation
 * @return Current rotation
 */
display_rotation_t display_get_rotation(void);

/**
 * @brief Get display dimensions
 * @param width Pointer to store width (output parameter)
 * @param height Pointer to store height (output parameter)
 */
void display_get_dimensions(uint16_t* width, uint16_t* height);

/**
 * @brief Force a display refresh
 */
void display_force_refresh(void);

/**
 * @brief Get display statistics
 * @param stats Pointer to statistics structure to fill
 */
void display_get_stats(display_stats_t* stats);

/**
 * @brief Print display status and statistics
 */
void display_print_status(void);

/**
 * @brief Check if display is ready for operations
 * @return true if ready, false if busy or not initialized
 */
bool display_is_ready(void);

/**
 * @brief Get LVGL display driver instance
 * @return Pointer to LVGL display driver
 */
lv_disp_t* display_get_lvgl_display(void);

/**
 * @brief Enable or disable performance monitoring
 * @param enable true to enable, false to disable
 */
void display_set_performance_monitoring(bool enable);

/**
 * @brief Optimize display settings for power saving
 * @param enable true to enable power optimizations, false for performance
 */
void display_set_power_optimization(bool enable);

/**
 * @brief Test display functionality
 * @return true if all tests pass, false otherwise
 */
bool display_run_self_test(void);

// Power button integration
/**
 * @brief Toggle display power state based on power button
 * Called from power button interrupt or polling
 */
void display_handle_power_button(void);

/**
 * @brief Set power button callback for display control
 * @param callback Function to call when power button is pressed
 */
void display_set_power_button_callback(void (*callback)(bool screen_on));

#ifdef __cplusplus
}
#endif