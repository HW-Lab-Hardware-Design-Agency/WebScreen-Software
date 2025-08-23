/**
 * @file webscreen.ino
 * @brief Arduino entry point for WebScreen v2.0
 * 
 * This is the main Arduino sketch file that serves as the entry point for the
 * refactored WebScreen firmware. The actual application logic is implemented
 * in webscreen_main.cpp for better organization and testing.
 */

#include "webscreen_main.h"

/**
 * @brief Arduino setup function
 * 
 * Called once when the ESP32 starts up. Delegates to the main application
 * setup function.
 */
void setup(void) {
    webscreen_setup();
}

/**
 * @brief Arduino main loop function
 * 
 * Called repeatedly after setup completes. Delegates to the main application
 * loop function.
 */
void loop(void) {
    webscreen_loop();
}

/**
 * @brief Handle watchdog timer reset
 * 
 * This function is called if the watchdog timer expires. It provides a way
 * to handle system lockups gracefully.
 */
void IRAM_ATTR esp_task_wdt_isr_user_handler(void) {
    // Log the watchdog reset
    Serial.println("WATCHDOG: System reset due to watchdog timeout");
    
    // Try to save critical data before reset
    // Note: Keep this minimal as we're in an ISR context
    
    // Force a system restart
    ESP.restart();
}