#pragma once

#include <stddef.h>
#include <stdint.h>

// Encodes `len` bytes into `out`, which must hold 4*ceil(len/3)+1 chars.
// Returns the number of chars written (excluding the NUL terminator).
static inline size_t webscreen_base64_encode(const uint8_t *in, size_t len, char *out) {
  static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t o = 0;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t v = (uint32_t)in[i] << 16;
    if (i + 1 < len) v |= (uint32_t)in[i + 1] << 8;
    if (i + 2 < len) v |= (uint32_t)in[i + 2];
    out[o++] = tbl[(v >> 18) & 63];
    out[o++] = tbl[(v >> 12) & 63];
    out[o++] = (i + 1 < len) ? tbl[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < len) ? tbl[v & 63] : '=';
  }
  out[o] = '\0';
  return o;
}
