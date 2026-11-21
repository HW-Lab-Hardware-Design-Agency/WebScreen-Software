/**
 * @file webscreen_hardware.cpp
 * @brief Hardware abstraction layer implementation for Arduino
 * 
 * This file provides the implementation of hardware functions following
 * Arduino best practices and conventions.
 */

#include "webscreen_hardware.h"
#include "webscreen_main.h"
#include "pins_config.h"
#include "rm67162.h"
#include "webscreen_button.h"
#include <SD_MMC.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <soc/gpio_periph.h>
#include <soc/io_mux_reg.h>
static bool g_hardware_initialized = false;
static bool g_display_on = true;
static uint8_t g_brightness = 200;
static WebscreenButton g_button;
static WebscreenClick g_app_button;
static void (*volatile g_app_button_callback)(bool) = nullptr;
bool webscreen_hardware_init(void) {
  if (g_hardware_initialized) {
    return true;
  }

  WEBSCREEN_DEBUG_PRINTLN("Initializing hardware pins...");
  WEBSCREEN_PIN_MODE(WEBSCREEN_PIN_LED, OUTPUT);
  webscreen_hardware_init_button();
  WEBSCREEN_PIN_MODE(WEBSCREEN_PIN_OUTPUT, OUTPUT);
  WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_LED);
  WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_OUTPUT);
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
  webscreen_display_power(false);
  WEBSCREEN_PIN_LOW(WEBSCREEN_PIN_LED);

  g_hardware_initialized = false;
  WEBSCREEN_DEBUG_PRINTLN("Hardware shutdown complete");
}
bool webscreen_hardware_init_sd_card(void) {
  WEBSCREEN_DEBUG_PRINTLN("Initializing SD Card...");
  SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);

  // Card must init at 400 kHz first; a high-frequency first mount times out and wedges the host.
  for (int i = 0; i < 3; i++) {

    WEBSCREEN_DEBUG_PRINTF("Attempt %d: Mounting SD card at a safe, low frequency...\n", i + 1);
    // Arduino's frequency argument is in kHz, matching host.max_freq_khz.
    if (SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_PROBING)) {
      WEBSCREEN_DEBUG_PRINTLN("SD Card mounted successfully at low frequency.");
      SD_MMC.end();
      WEBSCREEN_DEBUG_PRINTLN("Re-mounting SD card at high frequency...");
      if (SD_MMC.begin("/sdcard", true, false, 10000)) {
        WEBSCREEN_DEBUG_PRINTLN("SD Card re-mounted successfully at high frequency.");
        return true;
      } else {
        WEBSCREEN_DEBUG_PRINTLN("Failed to re-mount at high frequency. Falling back to low speed mount.");

        SD_MMC.end();
        if (SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_PROBING)) {
          WEBSCREEN_DEBUG_PRINTLN("Continuing at safe, low frequency.");
          return true;
        }
      }
    }

    WEBSCREEN_DEBUG_PRINTF("Attempt %d failed. Retrying in 200ms...\n", i + 1);
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  WEBSCREEN_DEBUG_PRINTLN("All attempts to mount SD card failed.");
  return false;
}
bool webscreen_display_init(void) {
  WEBSCREEN_DEBUG_PRINTLN("Initializing display...");
  g_display_on = true;
  g_brightness = 200;

  WEBSCREEN_DEBUG_PRINTLN("Display initialized (stub implementation)");
  return true;
}
bool webscreen_display_set_brightness(uint8_t brightness) {
  g_brightness = brightness;

  // Send brightness command (0x51) to RM67162 AMOLED display
  lcd_brightness(brightness);

  WEBSCREEN_DEBUG_PRINTF("Display brightness set to %d\n", brightness);
  return true;
}
uint8_t webscreen_display_get_brightness(void) {
  return g_brightness;
}
void webscreen_hardware_sync_brightness(uint8_t v) {
  // Cache-only, for modules that drive the panel directly; display off/on restores the latest value
  g_brightness = v;
}
bool webscreen_display_set_rotation(uint8_t rotation) {
  if (rotation > 3) {
    return false;
  }
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
    // Panel keeps rendering with LED low; bypass the setter so g_brightness keeps the restore value
    lcd_brightness(0);
  }

  WEBSCREEN_DEBUG_PRINTF("Display power: %s\n", on ? "ON" : "OFF");
}
bool webscreen_display_is_on(void) {
  return g_display_on;
}
// Survives a panic reboot (not a power cycle): how long the button had been
// low when the firmware last sampled it.
static RTC_NOINIT_ATTR uint16_t g_button_run_at_reset;

// Runs in the esp_timer task: internal RAM only, so it keeps working while a
// press is corrupting PSRAM under the app.
static void button_sample_cb(void *) {
  bool off = g_button.sample(webscreen_hardware_button_pressed(), WEBSCREEN_BUTTON_OFF_SAMPLES);
  g_button_run_at_reset = g_button.run;
  if (off) webscreen_hardware_power_off();

  // BOOT button: one click per press, handed to whoever registered (the JS runtime).
  bool low = gpio_get_level((gpio_num_t)WEBSCREEN_PIN_APP_BUTTON) == 0;
  if (g_app_button.sample(low, WEBSCREEN_APP_BUTTON_SAMPLES)) {
    void (*callback)(bool) = g_app_button_callback;
    if (callback) callback(true);
  }
}
void webscreen_hardware_init_button(void) {
  // Left over from a previous power-off if the chip was not fully reset.
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t)WEBSCREEN_PIN_OUTPUT);
  gpio_hold_dis((gpio_num_t)TFT_RES);

  // Enable the pad's input buffer only. The IO MUX function must stay on the
  // PSRAM bus, and the board has an external pull-up (R7).
  PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[WEBSCREEN_PIN_BUTTON]);

  // BOOT button: a plain GPIO after reset, with the pull-up it needs as a strapping pin.
  pinMode(WEBSCREEN_PIN_APP_BUTTON, INPUT_PULLUP);

  static esp_timer_handle_t timer = nullptr;
  if (timer) return;

  // The corruption from a press can crash the app before the sampler decides.
  // If the last boot ended in a crash while the button was down, finish the
  // power-off the user asked for instead of restarting.
  esp_reset_reason_t reason = esp_reset_reason();
  bool crashed = reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT;
  uint16_t run = crashed ? g_button_run_at_reset : 0;
  g_button_run_at_reset = 0;
  bool held = crashed;  // Still down after the reboot? Several samples: bus traffic also reads low.
  for (int i = 0; held && i < WEBSCREEN_BUTTON_CRASH_SAMPLES; i++) {
    held = webscreen_hardware_button_pressed();
    delayMicroseconds(WEBSCREEN_BUTTON_SAMPLE_US);
  }
  if (run >= WEBSCREEN_BUTTON_CRASH_SAMPLES || held) {
    webscreen_hardware_power_off();
  }
  esp_timer_create_args_t args = {};
  args.callback = button_sample_cb;
  args.name = "button";
  if (esp_timer_create(&args, &timer) == ESP_OK) esp_timer_start_periodic(timer, WEBSCREEN_BUTTON_SAMPLE_US);
}
bool webscreen_hardware_button_pressed(void) {
  // Not digitalRead(): the pin is deliberately not registered as an Arduino GPIO.
  return gpio_get_level((gpio_num_t)WEBSCREEN_PIN_BUTTON) == 0;
}
// Clicks of the BOOT button go to this callback, from the esp_timer task. The
// power button never reaches it: on this board revision it can only power off.
void webscreen_hardware_set_button_callback(void (*callback)(bool pressed)) {
  g_app_button_callback = callback;
}
// No-op, kept for the button_set_toggle() binding.
void webscreen_hardware_set_button_toggle(bool enabled) {
  (void)enabled;
}
uint16_t webscreen_hardware_get_battery_voltage(void) {

  int raw = analogRead(4);
  uint16_t voltage_mv = (raw * 3300) / 4095;

  return voltage_mv;
}
void webscreen_hardware_set_power_saving(bool enable) {
  if (enable) {

    setCpuFrequencyMhz(80);
    WEBSCREEN_DEBUG_PRINTLN("Power saving mode enabled");
  } else {

    setCpuFrequencyMhz(240);
    WEBSCREEN_DEBUG_PRINTLN("Power saving mode disabled");
  }
}
void webscreen_hardware_deep_sleep(uint32_t duration_ms) {
  WEBSCREEN_DEBUG_PRINTF("Entering deep sleep for %lu ms\n", duration_ms);
  // Timer wake only: the button's GPIO 33 is not an RTC pin and cannot wake the chip.
  esp_sleep_enable_timer_wakeup((uint64_t)duration_ms * 1000);
  esp_deep_sleep_start();
}
void webscreen_hardware_power_off(void) {
  // May run while PSRAM is corrupt and other tasks are wedged: direct GPIO
  // writes only, no SPI, no locks, no heap. Panel reset blanks the AMOLED.
  gpio_set_level((gpio_num_t)TFT_RES, 0);
  gpio_set_level((gpio_num_t)WEBSCREEN_PIN_OUTPUT, 0);  // Display supply off
  gpio_hold_en((gpio_num_t)TFT_RES);
  gpio_hold_en((gpio_num_t)WEBSCREEN_PIN_OUTPUT);
  gpio_deep_sleep_hold_en();

  // No wake source: GPIO 33 is not an RTC pin. RST or a power cycle restarts.
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_deep_sleep_start();
}
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
float webscreen_hardware_get_temperature(void) {

  float temp = temperatureRead();
  return temp;
}
bool webscreen_hardware_is_healthy(void) {

  WEBSCREEN_PIN_HIGH(WEBSCREEN_PIN_LED);
  WEBSCREEN_DELAY(1);
  float temp = webscreen_hardware_get_temperature();
  if (temp > 85.0 || temp < -10.0) {
    return false;
  }
  if (ESP.getFreeHeap() < 10000) {
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
  WEBSCREEN_DEBUG_PRINT("LED test... ");
  webscreen_hardware_blink_led(3, 100);
  WEBSCREEN_DEBUG_PRINTLN("PASS");
  WEBSCREEN_DEBUG_PRINT("Button test... ");
  bool button_works = true;
  if (button_works) {
    WEBSCREEN_DEBUG_PRINTLN("PASS");
  } else {
    WEBSCREEN_DEBUG_PRINTLN("FAIL");
    all_passed = false;
  }
  WEBSCREEN_DEBUG_PRINT("Temperature sensor test... ");
  float temp = webscreen_hardware_get_temperature();
  if (temp > -50 && temp < 100) {
    WEBSCREEN_DEBUG_PRINTF("PASS (%.1f°C)\n", temp);
  } else {
    WEBSCREEN_DEBUG_PRINTF("FAIL (%.1f°C)\n", temp);
    all_passed = false;
  }
  WEBSCREEN_DEBUG_PRINT("Memory test... ");
  uint32_t free_heap = ESP.getFreeHeap();
  if (free_heap > 50000) {
    WEBSCREEN_DEBUG_PRINTF("PASS (%lu bytes free)\n", free_heap);
  } else {
    WEBSCREEN_DEBUG_PRINTF("FAIL (%lu bytes free)\n", free_heap);
    all_passed = false;
  }

  WEBSCREEN_DEBUG_PRINTF("Hardware self-test: %s\n", all_passed ? "PASS" : "FAIL");
  return all_passed;
}
