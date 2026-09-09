#pragma once
#include <stdint.h>
#include <string.h>

static uint32_t webscreen_parse_color(const char *text, uint32_t fallback) {
  if (!text || text[0] != '#') return fallback;
  size_t digits = strlen(text + 1);
  if (digits != 3 && digits != 6) return fallback;
  uint32_t color = 0;
  for (size_t i = 1; i <= digits; i++) {
    char c = text[i];
    uint32_t value;
    if (c >= '0' && c <= '9') value = c - '0';
    else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
    else return fallback;
    color = digits == 3 ? (color << 8) | value * 17 : (color << 4) | value;
  }
  return color;
}
