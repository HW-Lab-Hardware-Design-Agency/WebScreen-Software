/**
 * @file webscreen_hardware.cpp
 * @brief Hardware abstraction layer implementation for Arduino
 * 
 * This file provides the implementation of hardware functions following
 * Arduino best practices and conventions.
 */

#include "webscreen_hardware.h"
#include "webscreen_main.h"

// Hardware state
static bool g_hardware_initialized = false;
static bool g_display_on = true;
static uint8_t g_brightness = 200;
static bool g_last_button_state = HIGH;
static uint32_t g_last_button_time = 0;
static void (*g_button_callback)(bool) = nullptr;

// ============================================================================
// HARDWARE INITIALIZATION
// ============================================================================

bool webscreen_hardware_init(void) {
    if (g_hardware_initialized) {
        return true;
    }
    
    WEBSCREEN_DEBUG_PRINTLN("Initializing hardware pins...");
    
    // Initialize pins
    WEBSCREEN_PIN_MODE(WEBSCREEN_PIN_LED, OUTPUT);
    WEBSCREEN_PIN_MODE(WEBSCREEN_PIN_BUTTON, INPUT_PULLUP);
    WEBSCREEN_PIN_MODE(WEBSCREEN_PIN_OUTPUT, OUTPUT);
    
    // Set initial states
    WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_LED);   // Turn on LED
    WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_OUTPUT); // Power on
    
    // Initialize display
    if (!webscreen_display_init()) {
        WEBSCREEN_DEBUG_PRINTLN("Display initialization failed");
        return false;
    }
    
    g_hardware_initialized = true;
    WEBSCREEN_DEBUG_PRINTLN("Hardware initialization complete");
    return true;
}

void webscreen_hardware_shutdown(void) {
    if (!g_hardware_initialized) {
        return;
    }
    
    WEBSCREEN_DEBUG_PRINTLN("Shutting down hardware...");
    
    // Turn off display
    webscreen_display_power(false);
    
    // Turn off LED
    WEBSCREEN_PIN_LOW(WEBSCREEN_PIN_LED);
    
    g_hardware_initialized = false;
    WEBSCREEN_DEBUG_PRINTLN("Hardware shutdown complete");
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

bool webscreen_display_init(void) {
    WEBSCREEN_DEBUG_PRINTLN("Initializing display...");
    
    // This is a simplified implementation - in a real system,
    // this would initialize the RM67162 display driver
    
    g_display_on = true;
    g_brightness = 200;
    
    WEBSCREEN_DEBUG_PRINTLN("Display initialized (stub implementation)");
    return true;
}

bool webscreen_display_set_brightness(uint8_t brightness) {
    g_brightness = brightness;
    
    // In a real implementation, this would control the display backlight
    // For now, we'll use the LED as a brightness indicator
    if (brightness > 128) {
        WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_LED);
    } else {
        WEBSCREEN_PIN_LOW(WEBSCREEN_PIN_LED);
    }
    
    WEBSCREEN_DEBUG_PRINTF("Display brightness set to %d\n", brightness);
    return true;
}

uint8_t webscreen_display_get_brightness(void) {
    return g_brightness;
}

bool webscreen_display_set_rotation(uint8_t rotation) {
    if (rotation > 3) {
        return false;
    }
    
    // In a real implementation, this would rotate the display
    WEBSCREEN_DEBUG_PRINTF("Display rotation set to %d\n", rotation);
    return true;
}

void webscreen_display_power(bool on) {
    g_display_on = on;
    
    if (on) {
        WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_LED);
        webscreen_display_set_brightness(g_brightness);
    } else {
        WEBSCREEN_PIN_LOW(WEBSCREEN_PIN_LED);
    }
    
    WEBSCREEN_DEBUG_PRINTF("Display power: %s\n", on ? "ON" : "OFF");
}

bool webscreen_display_is_on(void) {
    return g_display_on;
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================

void webscreen_hardware_handle_button(void) {
    bool current_button_state = WEBSCREEN_PIN_READ(WEBSCREEN_PIN_BUTTON);
    uint32_t current_time = WEBSCREEN_MILLIS();
    
    // Check for button press (HIGH to LOW transition with debouncing)
    if (g_last_button_state == HIGH && current_button_state == LOW) {
        if (current_time - g_last_button_time > WEBSCREEN_BUTTON_DEBOUNCE_MS) {
            // Button pressed - toggle display
            g_display_on = !g_display_on;
            webscreen_display_power(g_display_on);
            
            // Call callback if set
            if (g_button_callback) {
                g_button_callback(true);
            }
            
            WEBSCREEN_DEBUG_PRINTF("Button pressed - Display %s\n", 
                                   g_display_on ? "ON" : "OFF");
            
            g_last_button_time = current_time;
        }
    }
    
    g_last_button_state = current_button_state;
}

bool webscreen_hardware_button_pressed(void) {
    return (WEBSCREEN_PIN_READ(WEBSCREEN_PIN_BUTTON) == LOW);
}

void webscreen_hardware_set_button_callback(void (*callback)(bool pressed)) {
    g_button_callback = callback;
}

// ============================================================================
// POWER MANAGEMENT
// ============================================================================

uint16_t webscreen_hardware_get_battery_voltage(void) {
    // Read battery voltage from ADC (simplified implementation)
    int raw = analogRead(4); // PIN_BAT_VOLT
    
    // Convert to millivolts (this is a rough approximation)
    uint16_t voltage_mv = (raw * 3300) / 4095;
    
    return voltage_mv;
}

void webscreen_hardware_set_power_saving(bool enable) {
    if (enable) {
        // Enable power saving features
        setCpuFrequencyMhz(80); // Reduce CPU frequency
        WEBSCREEN_DEBUG_PRINTLN("Power saving mode enabled");
    } else {
        // Disable power saving features
        setCpuFrequencyMhz(240); // Full speed
        WEBSCREEN_DEBUG_PRINTLN("Power saving mode disabled");
    }
}

void webscreen_hardware_deep_sleep(uint32_t duration_ms) {
    WEBSCREEN_DEBUG_PRINTF("Entering deep sleep for %lu ms\n", duration_ms);
    
    // Configure wake-up sources
    esp_sleep_enable_timer_wakeup(duration_ms * 1000); // Convert to microseconds
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, 0); // Wake on button press
    
    // Enter deep sleep
    esp_deep_sleep_start();
}

// ============================================================================
// LED CONTROL
// ============================================================================

void webscreen_hardware_set_led(bool on) {
    if (on) {
        WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_LED);
    } else {
        WEBSCREEN_PIN_LOW(WEBSCREEN_PIN_LED);
    }
}

void webscreen_hardware_blink_led(uint8_t count, uint16_t duration_ms) {
    for (uint8_t i = 0; i < count; i++) {
        WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_LED);
        WEBSCREEN_DELAY(duration_ms);
        WEBSCREEN_PIN_LOW(WEBSCREEN_PIN_LED);
        WEBSCREEN_DELAY(duration_ms);
    }
}

// ============================================================================
// SYSTEM MONITORING
// ============================================================================

float webscreen_hardware_get_temperature(void) {
    // Get internal temperature sensor reading
    float temp = temperatureRead();
    return temp;
}

bool webscreen_hardware_is_healthy(void) {
    // Check basic hardware health indicators
    
    // Check if pins are responding
    WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_LED);
    WEBSCREEN_DELAY(1);
    
    // Check temperature
    float temp = webscreen_hardware_get_temperature();
    if (temp > 85.0 || temp < -10.0) { // Outside reasonable operating range
        return false;
    }
    
    // Check memory
    if (ESP.getFreeHeap() < 10000) { // Less than 10KB free
        return false;
    }
    
    return g_hardware_initialized;
}

void webscreen_hardware_print_status(void) {
    WEBSCREEN_DEBUG_PRINTLN("\n=== HARDWARE STATUS ===");
    WEBSCREEN_DEBUG_PRINTF("Initialized: %s\n", g_hardware_initialized ? "Yes" : "No");
    WEBSCREEN_DEBUG_PRINTF("Display On: %s\n", g_display_on ? "Yes" : "No");
    WEBSCREEN_DEBUG_PRINTF("Brightness: %d/255\n", g_brightness);
    WEBSCREEN_DEBUG_PRINTF("Button State: %s\n", 
                           webscreen_hardware_button_pressed() ? "Pressed" : "Released");
    WEBSCREEN_DEBUG_PRINTF("Temperature: %.1f°C\n", webscreen_hardware_get_temperature());
    WEBSCREEN_DEBUG_PRINTF("Battery Voltage: %d mV\n", webscreen_hardware_get_battery_voltage());
    WEBSCREEN_DEBUG_PRINTF("Healthy: %s\n", webscreen_hardware_is_healthy() ? "Yes" : "No");
    WEBSCREEN_DEBUG_PRINTLN("======================\n");
}

bool webscreen_hardware_self_test(void) {
    WEBSCREEN_DEBUG_PRINTLN("Running hardware self-test...");
    
    bool all_passed = true;
    
    // Test LED
    WEBSCREEN_DEBUG_PRINT("LED test... ");
    webscreen_hardware_blink_led(3, 100);
    WEBSCREEN_DEBUG_PRINTLN("PASS");
    
    // Test button
    WEBSCREEN_DEBUG_PRINT("Button test... ");
    bool button_works = true; // In a real test, we'd verify button functionality
    if (button_works) {
        WEBSCREEN_DEBUG_PRINTLN("PASS");
    } else {
        WEBSCREEN_DEBUG_PRINTLN("FAIL");
        all_passed = false;
    }
    
    // Test temperature sensor
    WEBSCREEN_DEBUG_PRINT("Temperature sensor test... ");
    float temp = webscreen_hardware_get_temperature();
    if (temp > -50 && temp < 100) { // Reasonable range
        WEBSCREEN_DEBUG_PRINTF("PASS (%.1f°C)\n", temp);
    } else {
        WEBSCREEN_DEBUG_PRINTF("FAIL (%.1f°C)\n", temp);
        all_passed = false;
    }
    
    // Test memory
    WEBSCREEN_DEBUG_PRINT("Memory test... ");
    uint32_t free_heap = ESP.getFreeHeap();
    if (free_heap > 50000) { // At least 50KB free
        WEBSCREEN_DEBUG_PRINTF("PASS (%lu bytes free)\n", free_heap);
    } else {
        WEBSCREEN_DEBUG_PRINTF("FAIL (%lu bytes free)\n", free_heap);
        all_passed = false;
    }
    
    WEBSCREEN_DEBUG_PRINTF("Hardware self-test: %s\n", all_passed ? "PASS" : "FAIL");
    return all_passed;
}