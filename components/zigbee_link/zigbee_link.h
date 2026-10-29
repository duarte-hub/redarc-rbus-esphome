#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::zigbee_link {

// Reads the RSSI / LQI of the link to the Zigbee parent from the stack's neighbor table.
class ZigbeeLink : public PollingComponent {
 public:
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_ = s; }
  void set_lqi_sensor(sensor::Sensor *s) { this->lqi_ = s; }

  void update() override;
  void dump_config() override;

 protected:
  sensor::Sensor *rssi_{nullptr};
  sensor::Sensor *lqi_{nullptr};
};

}  // namespace esphome::zigbee_link
