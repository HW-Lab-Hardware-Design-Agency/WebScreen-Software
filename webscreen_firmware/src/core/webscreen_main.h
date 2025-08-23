/**
 * @file webscreen_main.h
 * @brief Header for main WebScreen application functions
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the WebScreen application
 * 
 * This function initializes all subsystems including hardware, networking,
 * configuration management, and runtime environments.
 */
void webscreen_setup(void);

/**
 * @brief Main application loop
 * 
 * This function runs the main application logic and should be called
 * repeatedly from the Arduino loop() function.
 */
void webscreen_loop(void);

#ifdef __cplusplus
}
#endif