#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

#include <cmath>
#include <set>
#include <string>
#include <vector>

namespace esphome {
namespace redarc_rbus {

namespace espbt = esphome::esp32_ble_tracker;

// REDARC custom GATT service "09022015-...-524544415243" (UUID tail = ASCII "REDARC").
//   char ...8001 = telemetry NOTIFY source (and init write)
//   char ...8002 = filter / gateway control writes
static const char *const REDARC_SERVICE_UUID = "09022015-5160-4000-8000-524544415243";
static const char *const REDARC_CHAR_8001_UUID = "09022015-5160-4000-8001-524544415243";
static const char *const REDARC_CHAR_8002_UUID = "09022015-5160-4000-8002-524544415243";

// R-Bus PGNs used for derived values (REDARC's own J1939 numbering, from the reversed app).
// Every other field is described by a FieldSpec supplied from the Python field tables.
static const uint32_t PGN_BATTERY_SENSOR_MEASUREMENTS = 127234;  // I bytes0-3, V bytes4-5, T byte6
static const uint32_t PGN_SOLAR_INPUT_STATUS = 127496;           // I bytes0-3, V bytes4-5

// Where a field lives in the 8-byte payload, and how to scale it:
//   raw = (payload as little-endian u64 >> shift) & ((1 << bits) - 1)
//   value = (raw + offset) * scale
// Raw values in the J1939 "not available / error" band are dropped (see field_raw_()).
struct FieldSpec {
  uint32_t pgn;
  uint8_t shift;
  uint8_t bits;
  int32_t offset;
  float scale;
};

// Publish-on-change with a periodic heartbeat, so ~1 Hz telemetry doesn't flood HA.
struct PublishGate {
  float last{NAN};
  uint32_t last_ms{0};
  bool due(float v, uint32_t now);
};

class RedarcRbus : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

  // Set the pairing passkey. If the Display is currently waiting at a passkey
  // prompt, this answers it immediately (so you enter the code it shows *now*).
  void set_passkey(uint32_t passkey);
  // Forget the stored bond and force a fresh pairing using the current passkey.
  // Call this (e.g. from a button in HA) after entering a freshly-shown rotating code.
  void pair_now();

  // Set the Display's BLE MAC at runtime (e.g. from a text field in HA), as
  // "AA:BB:CC:DD:EE:FF". Lets you point this hub at a unit without recompiling;
  // the connection picks up on the next advertisement.
  void set_remote_mac(const std::string &mac);

  // Write a raw R-Bus command frame to char 8001 (used by output switches).
  // Returns false if not connected/ready, so the switch can reflect the failure.
  bool send_command(const std::vector<uint8_t> &frame);
  bool ready() const { return this->notify_started_ && this->notify_handle_ != 0; }
  // Bind an entity to an R-Bus field (called from the generated code).
  void add_sensor(sensor::Sensor *s, uint32_t pgn, uint8_t shift, uint8_t bits, int32_t offset,
                  float scale) {
    this->sensors_.push_back({s, {pgn, shift, bits, offset, scale}, {}});
  }
#ifdef USE_BINARY_SENSOR
  // 2-bit J1939 boolean: 0 = off, 1 = on, 2 = error, 3 = not available.
  void add_binary_sensor(binary_sensor::BinarySensor *s, uint32_t pgn, uint8_t shift, uint8_t bits) {
    this->binary_sensors_.push_back({s, {pgn, shift, bits, 0, 1.0f}, {}});
  }
#endif
#ifdef USE_TEXT_SENSOR
  // Enum field rendered through a name table indexed by raw value.
  void add_text_sensor(text_sensor::TextSensor *s, uint32_t pgn, uint8_t shift, uint8_t bits,
                       const std::vector<std::string> &names) {
    this->text_sensors_.push_back({s, {pgn, shift, bits, 0, 1.0f}, names, {}});
  }
#endif
  // Derived: battery power = battery V x I (127234); solar power = solar V x I (127496).
  void set_battery_power_sensor(sensor::Sensor *s) { this->battery_power_sensor_ = s; }
  void set_solar_power_sensor(sensor::Sensor *s) { this->solar_power_sensor_ = s; }
  void set_rssi_sensor(sensor::Sensor *s) { this->rssi_sensor_ = s; }
  void set_rssi_interval(uint32_t ms) { this->rssi_interval_ms_ = ms; }
  // Optional activity LED: pulsed whenever BLE frames flow (e.g. the onboard
  // blue LED on GPIO2 of a standard esp32dev board).
  void set_status_led(GPIOPin *pin) { this->status_led_ = pin; }

 protected:
  void configure_security_();
  void start_notify_();
  void handle_frame_(const uint8_t *data, uint16_t len);
  bool is_our_peer_(const uint8_t *bda);
  void blink_activity_();  // pulse the status LED on BLE traffic
  static bool field_raw_(const FieldSpec &f, uint64_t payload, uint32_t &raw);
  static bool field_value_(const FieldSpec &f, uint64_t payload, float &value);
  void publish_power_(sensor::Sensor *s, PublishGate &gate, uint64_t payload, uint32_t now);

  struct SensorBinding {
    sensor::Sensor *sensor;
    FieldSpec field;
    PublishGate gate;
  };
  std::vector<SensorBinding> sensors_;
#ifdef USE_BINARY_SENSOR
  struct BinarySensorBinding {
    binary_sensor::BinarySensor *sensor;
    FieldSpec field;
    PublishGate gate;
  };
  std::vector<BinarySensorBinding> binary_sensors_;
#endif
#ifdef USE_TEXT_SENSOR
  struct TextSensorBinding {
    text_sensor::TextSensor *sensor;
    FieldSpec field;
    std::vector<std::string> names;
    PublishGate gate;
  };
  std::vector<TextSensorBinding> text_sensors_;
#endif
  sensor::Sensor *battery_power_sensor_{nullptr};
  sensor::Sensor *solar_power_sensor_{nullptr};
  PublishGate battery_power_gate_;
  PublishGate solar_power_gate_;

  GPIOPin *status_led_{nullptr};
  uint32_t led_off_at_{0};

  sensor::Sensor *rssi_sensor_{nullptr};
  uint32_t rssi_interval_ms_{60000};
  uint32_t last_rssi_ms_{0};

  uint32_t passkey_{0};
  bool have_passkey_{false};
  uint32_t last_passkey_set_ms_{0};
  bool passkey_pending_{false};     // Display is waiting for us to supply a passkey
  esp_bd_addr_t pending_bda_{};     // peer that issued the pending passkey request
  std::set<uint32_t> seen_pgns_;    // which R-Bus PGNs have streamed (logged once each)

  uint16_t notify_handle_{0};  // char 8001 value handle
  uint16_t write_handle_{0};   // char 8002 value handle
  bool have_handles_{false};
  bool bonded_{false};
  bool notify_started_{false};

  // Replay of the filter-enrollment writes the RedVision app sends to char 8002.
  bool sending_filters_{false};
  size_t filter_idx_{0};
  uint32_t last_write_ms_{0};
};

// A switch that turns a REDARC output on/off by replaying captured R-Bus
// command frames (write to char 8001). Using the exact bytes the RedVision app
// sends avoids any risk of addressing the wrong channel.
class RedarcRbusSwitch : public switch_::Switch, public Component {
 public:
  void set_parent(RedarcRbus *parent) { this->parent_ = parent; }
  void set_on_frame(const std::vector<uint8_t> &f) { this->on_frame_ = f; }
  void set_off_frame(const std::vector<uint8_t> &f) { this->off_frame_ = f; }
  void dump_config() override;
  // No reliable per-output feedback yet, so report as an assumed-state switch.
  bool assumed_state() override { return true; }

 protected:
  void write_state(bool state) override;

  RedarcRbus *parent_{nullptr};
  std::vector<uint8_t> on_frame_;
  std::vector<uint8_t> off_frame_;
};

}  // namespace redarc_rbus
}  // namespace esphome

#endif  // USE_ESP32
