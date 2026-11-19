#include "zigbee_sync.h"

#if defined(USE_ESP32) && defined(USE_ZIGBEE)
#include <cmath>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::redarc_rbus {

static const char *const TAG = "redarc_rbus.zigbee";

static constexpr uint32_t TICK_MS = 250;
static constexpr uint8_t REPORTS_PER_TICK = 1;          // at most 4 reports a second (the radio is shared with BLE)
static constexpr uint32_t SENSOR_MIN_MS = 10 * 1000;    // a changing sensor at most every 10 s
static constexpr uint32_t REFRESH_MS = 5 * 60 * 1000;   // everything at least every 5 minutes
static constexpr uint32_t JOIN_SETTLE_MS = 5 * 1000;    // after a join: let the stack finish starting up first

void ZigbeeSync::setup() {
  this->zb_->add_on_join_callback([this](bool) {
    this->resync_pending_ = true;
    this->resync_at_ = millis() + JOIN_SETTLE_MS;
  });
  this->set_interval(TICK_MS, [this]() { this->tick_(); });
}

bool ZigbeeSync::value_(const Entry &e, float &out) const {
  switch (e.kind) {
#ifdef USE_SENSOR
    case KIND_SENSOR: {
      auto *s = static_cast<sensor::Sensor *>(e.entity);
      if (!s->has_state() || std::isnan(s->state))
        return false;
      out = s->state;
      return true;
    }
#endif
#ifdef USE_BINARY_SENSOR
    case KIND_BINARY: {
      auto *s = static_cast<binary_sensor::BinarySensor *>(e.entity);
      if (!s->has_state())
        return false;
      out = s->state ? 1.0f : 0.0f;
      return true;
    }
#endif
#ifdef USE_SWITCH
    case KIND_SWITCH:
      out = static_cast<switch_::Switch *>(e.entity)->state ? 1.0f : 0.0f;
      return true;
#endif
#ifdef USE_NUMBER
    case KIND_NUMBER: {
      auto *n = static_cast<number::Number *>(e.entity);
      if (!n->has_state() || std::isnan(n->state))
        return false;
      out = n->state;
      return true;
    }
#endif
    default:
      return false;
  }
}

void ZigbeeSync::tick_() {
  if (this->entries_.empty())
    return;
  const uint32_t now = millis();
  const bool joined = this->zb_->is_joined();
  if (!joined) {
    this->was_joined_ = false;
    return;
  }
  if (!this->was_joined_) {
    // Joined since the last tick (also covers a join this component missed the callback for)
    this->was_joined_ = true;
    if (!this->resync_pending_) {
      this->resync_pending_ = true;
      this->resync_at_ = now + JOIN_SETTLE_MS;
    }
  }
  if (this->resync_pending_) {
    if (static_cast<int32_t>(now - this->resync_at_) < 0)
      return;
    this->resync_pending_ = false;
    for (auto &e : this->entries_)
      e.has_sent = false;
    ESP_LOGD(TAG, "Joined: sending all %u values", static_cast<unsigned>(this->entries_.size()));
  }

  uint8_t budget = REPORTS_PER_TICK;
  const size_t n = this->entries_.size();
  for (size_t i = 0; i < n && budget > 0; i++) {
    const size_t idx = (this->next_ + i) % n;
    Entry &e = this->entries_[idx];
    float v;
    if (!this->value_(e, v))
      continue;
    const uint32_t age = now - e.sent_ms;
    bool due;
    if (!e.has_sent || age >= REFRESH_MS) {
      due = true;
    } else if (e.kind == KIND_SENSOR) {
      due = std::fabs(v - e.sent) >= e.min_change && age >= SENSOR_MIN_MS;
    } else {
      due = v != e.sent;
    }
    if (!due)
      continue;
    if (e.kind == KIND_BINARY || e.kind == KIND_SWITCH) {
      // Write the state again first: the attribute may hold a stale value (see the header)
      e.attr->set_attr<bool>(v != 0.0f);
    }
    e.attr->report();
    e.sent = v;
    e.sent_ms = now;
    e.has_sent = true;
    budget--;
    this->next_ = (idx + 1) % n;
  }
}

void ZigbeeSync::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Zigbee reports to the coordinator\n"
                "  Values: %u",
                static_cast<unsigned>(this->entries_.size()));
}

}  // namespace esphome::redarc_rbus

#endif
