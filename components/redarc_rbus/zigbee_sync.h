#pragma once

// Sends the Zigbee-exposed values to the coordinator without bindings.
//
// ESPHome's attribute reporting goes through the stack's reporting, which needs a binding per endpoint and cluster.
// The ESP32 binding table holds 16 entries, fewer than the 37 values this bridge exposes, so Zigbee2MQTT's
// "configure reporting" fails for the rest (TABLE_FULL) and those values never update. This sends each value as an
// unsolicited attribute report straight to the coordinator (the path ESPHome's `report:` option uses), which needs
// no binding, and paces them so the radio is not flooded:
//  - binary inputs, switches and numbers: at once on a change, and again every few minutes;
//  - sensors: per kind of value (volts, amps, watts, percent, temperature, time, signal) a minimum change,
//    a minimum interval and a maximum interval, like Zigbee's own reporting configuration. They are set
//    in the "Zigbee Reporting" text entity (web UI: ESP page) and saved in flash.
//
// It also writes the binary input and switch states into the Zigbee attributes again after each join. The stack
// restores attribute values from flash when it starts, after ESPHome has set them, so a flag that was already on
// at boot (and does not change) stayed off on Zigbee.

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include <string>

namespace esphome::redarc_rbus {
// Sensor report settings per kind of value, shared by every ZigbeeSync (and available without Zigbee,
// so the settings entity compiles on any board). The text is "V:0.2,30,300;A:0.5,30,300;..." with, per
// key, the minimum change and the minimum and maximum seconds between reports. Keys: V volts, A amps,
// W watts, P percent, T temperature, M time (minutes), S signal (dBm). Missing or invalid entries are
// left as they are; an empty text restores the defaults. Returns the settings in use, in that form.
enum ReportGroup : uint8_t { RG_VOLT, RG_AMP, RG_WATT, RG_PCT, RG_TEMP, RG_TIME, RG_SIGNAL, RG_COUNT };
std::string zigbee_reporting_set(const std::string &text);
}  // namespace esphome::redarc_rbus

#if defined(USE_ESP32) && defined(USE_ZIGBEE)
#include <vector>
#include "esphome/components/zigbee/zigbee_attribute_esp32.h"
#include "esphome/components/zigbee/zigbee_esp32.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

namespace esphome::redarc_rbus {

class ZigbeeSync : public Component {
 public:
  explicit ZigbeeSync(zigbee::ZigbeeComponent *zb) : zb_(zb) {}
#ifdef USE_SENSOR
  // group: one of the ReportGroup values (chosen from the unit at build time)
  void add_sensor(zigbee::ZigbeeAttribute *attr, sensor::Sensor *s, uint8_t group) {
    this->entries_.push_back({attr, KIND_SENSOR, s, group});
  }
#endif
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(zigbee::ZigbeeAttribute *attr, binary_sensor::BinarySensor *s) {
    this->entries_.push_back({attr, KIND_BINARY, s});
  }
#endif
#ifdef USE_SWITCH
  void add_switch(zigbee::ZigbeeAttribute *attr, switch_::Switch *s) {
    this->entries_.push_back({attr, KIND_SWITCH, s});
  }
#endif

#ifdef USE_NUMBER
  void add_number(zigbee::ZigbeeAttribute *attr, number::Number *n) {
    this->entries_.push_back({attr, KIND_NUMBER, n});
  }
#endif

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

 protected:
  enum Kind : uint8_t { KIND_SENSOR, KIND_BINARY, KIND_SWITCH, KIND_NUMBER };
  struct Entry {
    zigbee::ZigbeeAttribute *attr;
    Kind kind;
    void *entity;
    uint8_t group{0};
    float sent{0};
    uint32_t sent_ms{0};
    bool has_sent{false};
  };

  void tick_();
  bool value_(const Entry &e, float &out) const;

  zigbee::ZigbeeComponent *zb_;
  std::vector<Entry> entries_;
  size_t next_{0};
  uint32_t resync_at_{0};
  bool resync_pending_{false};
  bool was_joined_{false};
};

}  // namespace esphome::redarc_rbus

#endif
