#pragma once
#include <Arduino.h>

/* ------------------------------------------------------------------
   Compile‑time switch:
   * 0 → completely strip all logging from the image (no flash, no RAM, no cycles)
   * 1 → keep logging enabled
   ------------------------------------------------------------------*/
#ifndef WEBSCREEN_LOG_LEVEL          // let the build system override
#define WEBSCREEN_LOG_LEVEL 1        // 1 = on, 0 = off
#endif

/* ------------------------------------------------------------------
   Macros
   ------------------------------------------------------------------*/
#if WEBSCREEN_LOG_LEVEL
  #define LOGF(...)       Serial.printf(__VA_ARGS__)
  #define LOG(...)        Serial.println(__VA_ARGS__)
#else
  /* compile to nothing */
  #define LOGF(...)       do { } while (0)
  #define LOG(...)        do { } while (0)
#endif
