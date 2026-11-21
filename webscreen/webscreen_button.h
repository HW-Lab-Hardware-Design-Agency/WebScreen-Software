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

// The BOOT button (GPIO 0) is an ordinary input once the firmware runs, so it
// serves as the app button. Reports one click per press: true on the single
// sample where the line has been low for `needed` samples in a row.
struct WebscreenClick {
  bool sample(bool low, uint16_t needed) {
    if (!low) {
      run = 0;
      return false;
    }
    if (run >= needed) return false;
    return ++run == needed;
  }
  uint16_t run = 0;
};
