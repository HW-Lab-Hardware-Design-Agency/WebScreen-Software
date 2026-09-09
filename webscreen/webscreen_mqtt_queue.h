#pragma once
#include <cstdint>
#include <cstring>

// One task owns the MQTT client and this queue. Reject overflow rather than
// replacing unread messages or handing JS a truncated JSON payload.
class WebscreenMqttQueue {
public:
  static constexpr unsigned Capacity = 4;
  struct Message { char topic[128]; uint8_t payload[1024]; uint16_t length; };
  bool push(const char *topic, const uint8_t *payload, size_t length) {
    if (!topic || (!payload && length) || std::strlen(topic) >= sizeof(Message::topic) ||
        length > sizeof(Message::payload) || count_ == Capacity) {
      dropped_++;
      return false;
    }
    Message &message = messages_[(head_ + count_) % Capacity];
    std::strcpy(message.topic, topic);
    if (length) std::memcpy(message.payload, payload, length);
    message.length = (uint16_t)length;
    count_++;
    return true;
  }
  const Message *front() const { return count_ ? &messages_[head_] : nullptr; }
  void pop() { if (count_) { head_ = (head_ + 1) % Capacity; count_--; } }
  void clear() { head_ = count_ = dropped_ = 0; }
  uint32_t dropped() const { return dropped_; }
private:
  Message messages_[Capacity] = {};
  unsigned head_ = 0, count_ = 0;
  uint32_t dropped_ = 0;
};

class WebscreenMqttSubscriptions {
public:
  static constexpr unsigned Capacity = 8;
  bool can_add(const char *topic) const {
    return topic && topic[0] && std::strlen(topic) < sizeof(topics_[0]) &&
           (find(topic) >= 0 || count_ < Capacity);
  }
  bool remember(const char *topic) {
    if (!can_add(topic)) return false;
    if (find(topic) < 0) std::strcpy(topics_[count_++], topic);
    return true;
  }
  void request_restore() { pending_ = (1u << count_) - 1; }
  template<class Subscribe> void restore(Subscribe subscribe) {
    for (unsigned i = 0; i < count_; i++) {
      if ((pending_ & (1u << i)) && subscribe(topics_[i])) pending_ &= ~(1u << i);
    }
  }
  bool pending() const { return pending_ != 0; }
  void clear() { count_ = pending_ = 0; }
private:
  int find(const char *topic) const {
    for (unsigned i = 0; i < count_; i++) if (std::strcmp(topics_[i], topic) == 0) return (int)i;
    return -1;
  }
  char topics_[Capacity][128] = {};
  unsigned count_ = 0, pending_ = 0;
};
