#include "zigbee_sync.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace esphome::redarc_rbus {

struct GroupCfg {
  char key;
  float change;     // report when the value moved by at least this much...
  uint32_t min_s;   // ...but no more often than this
  uint32_t max_s;   // and at least this often, changed or not
};
// Defaults; keys in ReportGroup order
static GroupCfg g_groups[RG_COUNT] = {
    {'V', 0.2f, 30, 300},  {'A', 0.5f, 30, 300}, {'W', 20.0f, 30, 300}, {'P', 1.0f, 30, 600},
    {'T', 0.5f, 30, 600},  {'M', 10.0f, 60, 600}, {'S', 5.0f, 60, 600},
};
static const GroupCfg G_DEFAULTS[RG_COUNT] = {
    {'V', 0.2f, 30, 300},  {'A', 0.5f, 30, 300}, {'W', 20.0f, 30, 300}, {'P', 1.0f, 30, 600},
    {'T', 0.5f, 30, 600},  {'M', 10.0f, 60, 600}, {'S', 5.0f, 60, 600},
};

static std::string format_groups(const GroupCfg *g) {
  std::string out;
  char buf[40];
  for (int i = 0; i < RG_COUNT; i++) {
    snprintf(buf, sizeof(buf), "%s%c:%g,%u,%u", i ? ";" : "", g[i].key, g[i].change, (unsigned) g[i].min_s,
             (unsigned) g[i].max_s);
    out += buf;
  }
  return out;
}

static const char *skip_spaces(const char *p) {
  while (*p == ' ')
    p++;
  return p;
}

std::string zigbee_reporting_set(const std::string &text) {
  const char *p = text.c_str();
  while (*p == ' ')
    p++;
  if (!*p) {
    memcpy(g_groups, G_DEFAULTS, sizeof(g_groups));
    return format_groups(g_groups);
  }
  while (*p) {
    // One entry: key ':' change ',' min ',' max; entries are separated by ';'
    while (*p == ' ' || *p == ';')
      p++;
    if (!*p)
      break;
    const char key = (char) toupper((unsigned char) *p++);
    while (*p == ' ')
      p++;
    bool ok = false;
    float change = 0;
    unsigned long mn = 0, mx = 0;
    if (*p == ':') {
      char *end;
      const char *q = p + 1;
      change = strtof(q, &end);
      if (end != q && *skip_spaces(end) == ',') {
        q = skip_spaces(end) + 1;
        mn = strtoul(q, &end, 10);
        if (end != q && *skip_spaces(end) == ',') {
          q = skip_spaces(end) + 1;
          mx = strtoul(q, &end, 10);
          if (end != q) {
            ok = true;
            p = end;
          }
        }
      }
    }
    if (ok && std::isfinite(change) && change >= 0 && mn <= 86400 && mx >= 1 && mx <= 86400) {
      for (auto &g : g_groups) {
        if (g.key == key) {
          g.change = change;
          g.min_s = mn;
          g.max_s = mx < mn ? mn : mx;
        }
      }
    }
    // Skip whatever is left of this entry
    const char *n = strchr(p, ';');
    if (n == nullptr)
      break;
    p = n;
  }
  return format_groups(g_groups);
}

}  // namespace esphome::redarc_rbus

#if defined(USE_ESP32) && defined(USE_ZIGBEE)
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::redarc_rbus {

static const char *const TAG = "redarc_rbus.zigbee";

static constexpr uint32_t TICK_MS = 250;
static constexpr uint8_t REPORTS_PER_TICK = 3;          // at most 12 reports a second
static constexpr uint32_t REFRESH_MS = 5 * 60 * 1000;   // binary inputs and switches at least every 5 minutes
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
    case KIND_SWITCH: {
      // An output switch has no state until the Display reports it; sending its default (off) before then
      // showed every output as OFF on Zigbee after each boot.
      auto *s = static_cast<switch_::Switch *>(e.entity);
      if (!s->has_state())
        return false;
      out = s->state ? 1.0f : 0.0f;
      return true;
    }
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
    if (e.kind == KIND_SENSOR) {
      const GroupCfg &g = g_groups[e.group < RG_COUNT ? e.group : static_cast<uint8_t>(RG_PCT)];
      if (!e.has_sent || age >= g.max_s * 1000u) {
        due = true;
      } else {
        const float moved = std::fabs(v - e.sent);
        due = age >= g.min_s * 1000u && (g.change > 0 ? moved >= g.change : moved > 0);
      }
    } else if (!e.has_sent || age >= REFRESH_MS) {
      due = true;
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
                "  Values: %u\n"
                "  Sensor settings: %s",
                static_cast<unsigned>(this->entries_.size()), format_groups(g_groups).c_str());
}

}  // namespace esphome::redarc_rbus

#endif
