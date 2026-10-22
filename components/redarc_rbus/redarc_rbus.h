#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/sensor/sensor.h"

#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

namespace esphome {
namespace redarc_rbus {

namespace espbt = esphome::esp32_ble_tracker;

// REDARC custom GATT service "09022015-...-524544415243" (UUID tail = ASCII "REDARC").
//   char ...8001 = telemetry NOTIFY source (and init write)
//   char ...8002 = filter / gateway control writes
static const char *const REDARC_SERVICE_UUID = "09022015-5160-4000-8000-524544415243";
static const char *const REDARC_CHAR_8001_UUID = "09022015-5160-4000-8001-524544415243";
static const char *const REDARC_CHAR_8002_UUID = "09022015-5160-4000-8002-524544415243";

// R-Bus PGNs we decode (REDARC's own J1939 numbering, from the reversed app).
static const uint32_t PGN_BATTERY_SOC = 127236;      // header 13 f1 04 .. ; payload byte0 = SoC %
static const uint32_t PGN_BATTERY_VOLTAGE = 127498;  // header 03 f2 0a .. ; payload bytes4-5 LE = mV

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
  void set_soc_sensor(sensor::Sensor *s) { this->soc_sensor_ = s; }
  void set_voltage_sensor(sensor::Sensor *s) { this->voltage_sensor_ = s; }
  // Optional activity LED: pulsed whenever BLE frames flow (e.g. the onboard
  // blue LED on GPIO2 of a standard esp32dev board).
  void set_status_led(GPIOPin *pin) { this->status_led_ = pin; }

 protected:
  void configure_security_();
  void start_notify_();
  void handle_frame_(const uint8_t *data, uint16_t len);
  bool is_our_peer_(const uint8_t *bda);
  void blink_activity_();  // pulse the status LED on BLE traffic

  GPIOPin *status_led_{nullptr};
  uint32_t led_off_at_{0};

  sensor::Sensor *soc_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};

  uint32_t passkey_{0};
  bool have_passkey_{false};
  uint32_t last_passkey_set_ms_{0};
  bool passkey_pending_{false};     // Display is waiting for us to supply a passkey
  esp_bd_addr_t pending_bda_{};     // peer that issued the pending passkey request

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

}  // namespace redarc_rbus
}  // namespace esphome

#endif  // USE_ESP32
