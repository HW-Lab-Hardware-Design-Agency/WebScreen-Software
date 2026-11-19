#pragma once
#include <cstdint>

// The user button shares GPIO 33 with octal-PSRAM data line D4, so the line
// also reads low during bus traffic and a real press corrupts PSRAM
// transfers. A press is therefore only accepted after an unbroken run of low
// samples, and the only safe reaction is to power off.
struct WebscreenButton {
  bool sample(bool low, uint16_t needed) {
    run = low ? (run < UINT16_MAX ? run + 1 : run) : 0;
    return run >= needed;
  }
  uint16_t run = 0;
};
