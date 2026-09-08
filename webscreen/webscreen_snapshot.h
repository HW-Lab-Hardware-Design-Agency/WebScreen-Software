#pragma once
#include <lvgl.h>
#include <string.h>

// Serial screenshots contain only pixels, never stride padding or alignment slack.
static size_t webscreen_snapshot_read(const lv_draw_buf_t *snapshot, size_t offset,
                                     uint8_t *output, size_t capacity) {
  const size_t row_bytes = (size_t)snapshot->header.w * 2;
  const size_t total = row_bytes * snapshot->header.h;
  if (!row_bytes || offset >= total) return 0;
  size_t copied = 0;
  while (copied < capacity && offset < total) {
    size_t column = offset % row_bytes;
    size_t count = row_bytes - column;
    if (count > capacity - copied) count = capacity - copied;
    memcpy(output + copied, snapshot->data + (offset / row_bytes) * snapshot->header.stride + column, count);
    copied += count;
    offset += count;
  }
  return copied;
}
