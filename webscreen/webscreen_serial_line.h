#pragma once
#include <stddef.h>

// Bounded, incremental input. Discard an oversized line through its newline.
template <size_t Capacity>
class WebscreenSerialLine {
public:
  enum Result { Pending, Ready, Overflow };

  Result push(char c) {
    if (c == '\r') return Pending;
    if (c == '\n') {
      data_[used_] = '\0';
      Result result = overflow_ ? Overflow : Ready;
      used_ = 0;
      overflow_ = false;
      return result;
    }
    if (used_ < Capacity - 1 && !overflow_) data_[used_++] = c;
    else overflow_ = true;
    return Pending;
  }

  const char *data() const { return data_; }

private:
  static_assert(Capacity > 1, "Line buffer must include a terminator");
  char data_[Capacity] = {};
  size_t used_ = 0;
  bool overflow_ = false;
};
