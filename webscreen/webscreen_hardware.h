/**
 * @file webscreen_hardware.h
 * @brief Hardware abstraction layer for WebScreen
 * 
 * Provides Arduino-style interface for all hardware components.
 * This follows Arduino best practices for hardware abstraction.
 */

#pragma once

#include "webscreen_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// HARDWARE INITIALIZATION
// ============================================================================

/**
 * @brief Initialize all hardware components
 * 
 * Initializes display, pins, and other hardware in the correct order.
 * Must be called before any other hardware functions.
 * 
 * @return true if initialization successful, false otherwise
 */
bool webscreen_hardware_init(void);

/**
 * @brief Shutdown all hardware components
 * 
 * Safely shuts down all hardware to prepare for power off or restart.
 */
void webscreen_hardware_shutdown(void);

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the display
 * @return true if successful, false otherwise
 */
bool webscreen_display_init(void);

/**
 * @brief Set display brightness
 * @param brightness Brightness level (0-255)
 * @return true if successful, false otherwise
 */
bool webscreen_display_set_brightness(uint8_t brightness);

/**
 * @brief Get current display brightness
 * @return Current brightness level (0-255)
 */
uint8_t webscreen_display_get_brightness(void);

/**
 * @brief Set display rotation
 * @param rotation Rotation value (0-3)
 * @return true if successful, false otherwise
 */
bool webscreen_display_set_rotation(uint8_t rotation);

/**
 * @brief Turn display on or off
 * @param on true to turn on, false to turn off
 */
void webscreen_display_power(bool on);

/**
 * @brief Check if display is currently on
 * @return true if display is on, false if off
 */
bool webscreen_display_is_on(void);

// ============================================================================
// BUTTON HANDLING
// ============================================================================

/**
 * @brief Handle power button press
 * 
 * Checks button state and handles press/release events.
 * Should be called regularly from main loop.
 */
void webscreen_hardware_handle_button(void);

/**
 * @brief Check if button is currently pressed
 * @return true if button is pressed, false otherwise
 */
bool webscreen_hardware_button_pressed(void);

/**
 * @brief Set button callback function
 * @param callback Function to call when button is pressed
 */
void webscreen_hardware_set_button_callback(void (*callback)(bool pressed));

// ============================================================================
// POWER MANAGEMENT
// ============================================================================

/**
 * @brief Get battery voltage (if available)
 * @return Battery voltage in millivolts, or 0 if not available
 */
uint16_t webscreen_hardware_get_battery_voltage(void);

/**
 * @brief Enable or disable power saving mode
 * @param enable true to enable power saving, false for normal operation
 */
void webscreen_hardware_set_power_saving(bool enable);

/**
 * @brief Put system into deep sleep
 * @param duration_ms Sleep duration in milliseconds (0 = indefinite)
 */
void webscreen_hardware_deep_sleep(uint32_t duration_ms);

// ============================================================================
// LED CONTROL
// ============================================================================

/**
 * @brief Set LED state
 * @param on true to turn LED on, false to turn off
 */
void webscreen_hardware_set_led(bool on);

/**
 * @brief Blink LED
 * @param count Number of blinks
 * @param duration_ms Duration of each blink in milliseconds
 */
void webscreen_hardware_blink_led(uint8_t count, uint16_t duration_ms);

// ============================================================================
// SYSTEM MONITORING
// ============================================================================

/**
 * @brief Get system temperature (if available)
 * @return Temperature in Celsius, or NaN if not available
 */
float webscreen_hardware_get_temperature(void);

/**
 * @brief Check if hardware is healthy
 * @return true if all hardware is functioning correctly
 */
bool webscreen_hardware_is_healthy(void);

/**
 * @brief Print hardware status to serial
 */
void webscreen_hardware_print_status(void);

/**
 * @brief Run hardware self-test
 * @return true if all tests pass, false if any failures
 */
bool webscreen_hardware_self_test(void);

#ifdef __cplusplus
}
#endif

// ============================================================================
// ARDUINO COMPATIBILITY HELPERS
// ============================================================================

// Pin definitions (from webscreen_config.h)
#define LED_PIN WEBSCREEN_PIN_LED
#define BUTTON_PIN WEBSCREEN_PIN_BUTTON
#define OUTPUT_PIN WEBSCREEN_PIN_OUTPUT

// Legacy compatibility macros
#define PIN_LED WEBSCREEN_PIN_LED
#define INPUT_PIN WEBSCREEN_PIN_BUTTON
#define OUTPUT_PIN WEBSCREEN_PIN_OUTPUT

// Display pin compatibility
#define TFT_CS WEBSCREEN_TFT_CS
#define TFT_DC WEBSCREEN_TFT_DC
#define TFT_RST WEBSCREEN_TFT_RST
#define TFT_SCK WEBSCREEN_TFT_SCK
#define TFT_MOSI WEBSCREEN_TFT_MOSI

// SD card pin compatibility
#define PIN_SD_CMD WEBSCREEN_SD_CMD
#define PIN_SD_CLK WEBSCREEN_SD_CLK
#define PIN_SD_D0 WEBSCREEN_SD_D0