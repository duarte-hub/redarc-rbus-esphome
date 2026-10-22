#include "redarc_rbus.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <cstring>
#include <cstdio>

namespace esphome {
namespace redarc_rbus {

static const char *const TAG = "redarc_rbus";

// Initial write the app sends to char 8001 before the filter burst.
static const uint8_t INIT_8001[] = {0x2f, 0xe6, 0xff, 0xfa, 0x08, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// The 32 filter-enrollment frames the RedVision app writes to char 8002, in order.
// Each is [index][0x2a][...]; they tell the R-Bus gateway which PGNs to forward.
// Captured 1:1 from a real app session (capture/session1.pklg). Entry 0x02 (f1 04)
// enrols the battery-SoC PGN 127236 and entry 0x0a (f2 0a) the voltage PGN 127498.
static const uint8_t FILTERS[][12] = {
    {0x00, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf0, 0x02, 0x00, 0x00, 0x00},
    {0x01, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf1, 0x00, 0x00, 0x00, 0x00},
    {0x02, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf1, 0x04, 0x00, 0x00, 0x00},
    {0x03, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf1, 0x08, 0x00, 0x00, 0x00},
    {0x04, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf1, 0x0a, 0x00, 0x03, 0xe8},
    {0x05, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf2, 0x00, 0x00, 0x03, 0xe8},
    {0x06, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf2, 0x04, 0x00, 0x03, 0xe8},
    {0x07, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf2, 0x06, 0x00, 0x03, 0xe8},
    {0x08, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf2, 0x08, 0x00, 0x03, 0xe8},
    {0x09, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf2, 0x07, 0x00, 0x00, 0x00},
    {0x0a, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf2, 0x0a, 0x00, 0x03, 0xe8},
    {0x0b, 0x2a, 0xa0, 0xff, 0xf9, 0x00, 0x00, 0xf2, 0x80, 0x00, 0x00, 0x00},
    {0x0c, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf3, 0x04, 0x00, 0x00, 0x00},
    {0x0d, 0x2a, 0xa3, 0xff, 0xfc, 0x00, 0x03, 0xf4, 0x00, 0x00, 0x00, 0x00},
    {0x0e, 0x2a, 0xa3, 0xff, 0xfe, 0x00, 0x03, 0xf4, 0x04, 0x00, 0x00, 0x00},
    {0x0f, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf4, 0x08, 0x00, 0x00, 0x00},
    {0x10, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf4, 0x0a, 0x00, 0x00, 0x00},
    {0x11, 0x2a, 0xa0, 0xff, 0xf9, 0x00, 0x00, 0xfc, 0xd0, 0x00, 0x00, 0x00},
    {0x12, 0x2a, 0xa0, 0xfe, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00, 0x00},
    {0x13, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf2, 0x0f, 0x00, 0x00, 0x00},
    {0x14, 0x2a, 0xa0, 0xff, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00},
    {0x15, 0x2a, 0xa3, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x16, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf2, 0x01, 0x00, 0x03, 0xe8},
    {0x17, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf3, 0x08, 0x00, 0x00, 0x00},
    {0x18, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf1, 0x02, 0x00, 0x07, 0xd0},
    {0x19, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf4, 0x06, 0x00, 0x00, 0x00},
    {0x1a, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf2, 0x05, 0x00, 0x00, 0x00},
    {0x1b, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf1, 0x0e, 0x00, 0x00, 0x00},
    {0x1c, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf1, 0x10, 0x00, 0x00, 0x00},
    {0x1d, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf1, 0x12, 0x00, 0x00, 0x00},
    {0x1e, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf4, 0x0e, 0x00, 0x00, 0x00},
    {0x1f, 0x2a, 0xa0, 0xff, 0xff, 0x00, 0x00, 0xf2, 0x09, 0x00, 0x00, 0x00},
};
static const size_t FILTER_COUNT = sizeof(FILTERS) / sizeof(FILTERS[0]);
static const uint32_t FILTER_WRITE_INTERVAL_MS = 40;

void RedarcRbus::setup() {
  this->configure_security_();
  if (this->status_led_ != nullptr) {
    this->status_led_->setup();
    this->status_led_->digital_write(false);
  }
}

// Briefly light the status LED to show BLE traffic. loop() turns it back off,
// so continuous traffic makes it flicker ("flashing while talking over BT").
void RedarcRbus::blink_activity_() {
  if (this->status_led_ == nullptr)
    return;
  this->status_led_->digital_write(true);
  this->led_off_at_ = millis() + 30;
}

void RedarcRbus::configure_security_() {
  // The Display only streams to a BONDED central. It uses legacy passkey pairing:
  // it DISPLAYS a 6-digit passkey and we (the central) must enter it, so we present
  // KeyboardOnly IO and request MITM + bonding.
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_MITM | ESP_LE_AUTH_BOND;
  esp_ble_io_cap_t iocap = ESP_IO_CAP_IN;  // keyboard only -> we input the passkey
  uint8_t key_size = 16;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(iocap));
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key));
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(rsp_key));
}

void RedarcRbus::loop() {
  // Turn the activity LED back off a short time after the last blink.
  if (this->status_led_ != nullptr && this->led_off_at_ != 0 && millis() >= this->led_off_at_) {
    this->status_led_->digital_write(false);
    this->led_off_at_ = 0;
  }

  // Poll the live connection RSSI every 10 s (result arrives via gap_event_handler).
  if (this->rssi_sensor_ != nullptr && this->ready() && this->parent() != nullptr &&
      millis() - this->last_rssi_ms_ >= 10000) {
    this->last_rssi_ms_ = millis();
    esp_ble_gap_read_rssi(this->parent()->get_remote_bda());
  }

  if (!this->sending_filters_)
    return;
  const uint32_t now = millis();
  if (now - this->last_write_ms_ < FILTER_WRITE_INTERVAL_MS)
    return;
  if (this->filter_idx_ >= FILTER_COUNT) {
    this->sending_filters_ = false;
    ESP_LOGD(TAG, "Filter enrollment complete (%u frames)", (unsigned) FILTER_COUNT);
    return;
  }
  esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                           this->write_handle_, 12, (uint8_t *) FILTERS[this->filter_idx_],
                           ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  this->filter_idx_++;
  this->last_write_ms_ = now;
  this->blink_activity_();
}

// GAP events are delivered to every ble_client node, not just ours, so we must
// only act on events that belong to OUR Display. Otherwise we would hand our
// passkey to, or mark ourselves bonded against, some other peer the ESP32 is
// pairing with.
bool RedarcRbus::is_our_peer_(const uint8_t *bda) {
  return bda != nullptr && this->parent() != nullptr &&
         memcmp(bda, this->parent()->get_remote_bda(), 6) == 0;
}

void RedarcRbus::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_PASSKEY_REQ_EVT: {
      // Only handle a passkey request coming from our Display.
      if (!this->is_our_peer_(param->ble_security.ble_req.bd_addr))
        break;
      memcpy(this->pending_bda_, param->ble_security.ble_req.bd_addr, sizeof(esp_bd_addr_t));
      // The Display shows a code that ROTATES, so a value entered long ago is
      // stale. Only auto-answer if a passkey was entered in the last 25 s
      // (i.e. you entered it and pressed Pair Now). Otherwise wait, and answer
      // as soon as you enter the code the Display is showing now.
      const bool fresh = this->have_passkey_ && (millis() - this->last_passkey_set_ms_ < 25000);
      if (fresh) {
        ESP_LOGI(TAG, "Passkey requested; replying with recently-entered code %06u",
                 (unsigned) this->passkey_);
        esp_ble_passkey_reply(this->pending_bda_, true, this->passkey_);
        this->passkey_pending_ = false;
      } else {
        this->passkey_pending_ = true;
        ESP_LOGW(TAG, "Display is requesting pairing. Enter the 6-digit code it is "
                      "showing NOW into 'REDARC Pairing Code' (within ~30 s).");
      }
      break;
    }
    case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
      if (this->rssi_sensor_ != nullptr && param->read_rssi_cmpl.status == ESP_BT_STATUS_SUCCESS &&
          this->is_our_peer_(param->read_rssi_cmpl.remote_addr))
        this->rssi_sensor_->publish_state(param->read_rssi_cmpl.rssi);
      break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
      // Only treat auth completion for our Display as our bond.
      if (!this->is_our_peer_(param->ble_security.auth_cmpl.bd_addr))
        break;
      if (param->ble_security.auth_cmpl.success) {
        ESP_LOGI(TAG, "Bonded with Display");
        this->bonded_ = true;
        this->start_notify_();
      } else {
        ESP_LOGW(TAG, "Pairing failed, reason 0x%02x (wrong passkey?)",
                 param->ble_security.auth_cmpl.fail_reason);
      }
      break;
    default:
      break;
  }
}

void RedarcRbus::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                     esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT: {
      if (param->open.status == ESP_GATT_OK) {
        // Proactively bond; if a bond already exists the stack just re-encrypts.
        esp_ble_set_encryption(param->open.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGD(TAG, "Disconnected");
      this->have_handles_ = false;
      this->bonded_ = false;
      this->notify_started_ = false;
      this->sending_filters_ = false;
      this->filter_idx_ = 0;
      this->passkey_pending_ = false;
      this->seen_pgns_.clear();
      if (this->status_led_ != nullptr) {
        this->status_led_->digital_write(false);
        this->led_off_at_ = 0;
      }
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *chr_notify = this->parent()->get_characteristic(
          espbt::ESPBTUUID::from_raw(REDARC_SERVICE_UUID),
          espbt::ESPBTUUID::from_raw(REDARC_CHAR_8001_UUID));
      auto *chr_write = this->parent()->get_characteristic(
          espbt::ESPBTUUID::from_raw(REDARC_SERVICE_UUID),
          espbt::ESPBTUUID::from_raw(REDARC_CHAR_8002_UUID));
      if (chr_notify == nullptr || chr_write == nullptr) {
        ESP_LOGE(TAG, "REDARC characteristics not found (8001/8002)");
        return;
      }
      this->notify_handle_ = chr_notify->handle;
      this->write_handle_ = chr_write->handle;
      this->have_handles_ = true;
      ESP_LOGD(TAG, "Found 8001=0x%04x (notify) 8002=0x%04x (write)", this->notify_handle_,
               this->write_handle_);
      this->start_notify_();  // runs only once we are also bonded
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      // Enable notifications by writing the CCCD, then replay the init + filters.
      auto *descr = this->parent()->get_config_descriptor(this->notify_handle_);
      if (descr == nullptr) {
        ESP_LOGW(TAG, "No CCCD for 8001; cannot enable notifications");
        return;
      }
      uint8_t cccd[2] = {0x01, 0x00};
      esp_ble_gattc_write_char_descr(gattc_if, this->parent()->get_conn_id(), descr->handle,
                                     sizeof(cccd), cccd, ESP_GATT_WRITE_TYPE_RSP,
                                     ESP_GATT_AUTH_REQ_MITM);
      // Initial 8001 write the app performs before the filter burst.
      esp_ble_gattc_write_char(gattc_if, this->parent()->get_conn_id(), this->notify_handle_,
                               sizeof(INIT_8001), (uint8_t *) INIT_8001,
                               ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
      // Kick off the paced filter enrollment (see loop()).
      this->filter_idx_ = 0;
      this->last_write_ms_ = millis();
      this->sending_filters_ = true;
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->notify_handle_) {
        this->blink_activity_();
        this->handle_frame_(param->notify.value, param->notify.value_len);
      }
      break;
    }
    default:
      break;
  }
}

void RedarcRbus::set_passkey(uint32_t passkey) {
  this->passkey_ = passkey;
  this->have_passkey_ = true;
  this->last_passkey_set_ms_ = millis();
  // If the Display is already waiting at a passkey prompt, answer it now with
  // the just-entered code. This is the path that makes a rotating code work.
  if (this->passkey_pending_) {
    ESP_LOGI(TAG, "Answering pending pairing with entered code %06u", (unsigned) passkey);
    esp_ble_passkey_reply(this->pending_bda_, true, passkey);
    this->passkey_pending_ = false;
  }
}

void RedarcRbus::set_remote_mac(const std::string &mac) {
  unsigned b[6];
  if (std::sscanf(mac.c_str(), "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
    ESP_LOGW(TAG, "Ignoring invalid MAC '%s' (want AA:BB:CC:DD:EE:FF)", mac.c_str());
    return;
  }
  uint64_t addr = 0;
  for (int i = 0; i < 6; i++)
    addr = (addr << 8) | (b[i] & 0xFF);
  if (this->parent() == nullptr)
    return;
  ESP_LOGI(TAG, "Target MAC set to %s; connecting on next advertisement", mac.c_str());
  this->parent()->set_address(addr);
  this->parent()->set_state(espbt::ClientState::IDLE);  // let the tracker re-promote it
}

void RedarcRbus::pair_now() {
  if (this->parent() == nullptr)
    return;
  uint8_t *bda = this->parent()->get_remote_bda();
  ESP_LOGI(TAG, "Re-pairing: removing stored bond and reconnecting");
  // Drop any old bond so the Display issues a fresh passkey challenge, which we
  // answer with the currently-configured (just-entered) rotating code.
  esp_ble_remove_bond_device(bda);
  this->bonded_ = false;
  this->notify_started_ = false;
  this->sending_filters_ = false;
  this->filter_idx_ = 0;
  // Disconnect if connected; the ble_client auto-reconnects and pairing restarts.
  esp_ble_gap_disconnect(bda);
}

bool RedarcRbus::send_command(const std::vector<uint8_t> &frame) {
  if (!this->ready() || this->parent() == nullptr) {
    ESP_LOGW(TAG, "Not connected/ready; command dropped");
    return false;
  }
  esp_err_t err = esp_ble_gattc_write_char(
      this->parent()->get_gattc_if(), this->parent()->get_conn_id(), this->notify_handle_,
      frame.size(), const_cast<uint8_t *>(frame.data()), ESP_GATT_WRITE_TYPE_NO_RSP,
      ESP_GATT_AUTH_REQ_NONE);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Command write failed: %d", err);
    return false;
  }
  this->blink_activity_();
  return true;
}

void RedarcRbusSwitch::write_state(bool state) {
  const std::vector<uint8_t> &frame = state ? this->on_frame_ : this->off_frame_;
  if (this->parent_ != nullptr && this->parent_->send_command(frame))
    this->publish_state(state);  // optimistic; no device read-back yet
}

void RedarcRbusSwitch::dump_config() { LOG_SWITCH("", "REDARC Output Switch", this); }

void RedarcRbus::start_notify_() {
  if (this->notify_started_ || !this->have_handles_ || !this->bonded_)
    return;
  this->notify_started_ = true;
  ESP_LOGD(TAG, "Registering for notifications on 8001");
  esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                    this->parent()->get_remote_bda(), this->notify_handle_);
}

// R-Bus telemetry frame = [4-byte J1939 29-bit CAN id][DLC=0x08][8-byte payload].
// Field extraction matches the app: payload is one big-endian integer (byte0 = MSB) and
// field = (value >> ((startByte-1)*8)) & ((1<<bitLen)-1), startByte counted from the last byte.
void RedarcRbus::handle_frame_(const uint8_t *data, uint16_t len) {
  if (len < 13 || data[4] != 0x08)
    return;
  const uint8_t *hdr = data;
  const uint8_t *pay = data + 5;  // 8-byte payload

  const uint8_t pf = hdr[1];
  const uint8_t ps = hdr[2];
  const uint8_t dp = hdr[0] & 0x01;
  const uint32_t pgn = (pf >= 240) ? (((uint32_t) dp << 16) | ((uint32_t) pf << 8) | ps)
                                   : (((uint32_t) dp << 16) | ((uint32_t) pf << 8));

  // Log each distinct PGN once per connection, so missing data (e.g. voltage)
  // is easy to diagnose: if 127498 never appears, that unit isn't sending it.
  if (this->seen_pgns_.insert(pgn).second)
    ESP_LOGD(TAG, "RX R-Bus PGN %u", pgn);

  if (pgn == PGN_BATTERY_SOC) {
    const uint8_t soc = pay[0];  // byte0, 0..100 %  (0xFF = not available)
    if (this->soc_sensor_ != nullptr && soc != 0xFF)
      this->soc_sensor_->publish_state(soc);
  } else if (pgn == PGN_BATTERY_VOLTAGE) {
    const uint16_t mv = (uint16_t) pay[4] | ((uint16_t) pay[5] << 8);  // bytes4-5 LE, millivolts
    if (this->voltage_sensor_ != nullptr && mv != 0xFFFF)
      this->voltage_sensor_->publish_state(mv / 1000.0f);
  }
}

void RedarcRbus::dump_config() {
  ESP_LOGCONFIG(TAG, "REDARC RedVision R-Bus BLE proxy:");
  ESP_LOGCONFIG(TAG, "  Passkey configured: %s", YESNO(this->have_passkey_));
  LOG_SENSOR("  ", "State of Charge", this->soc_sensor_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
}

}  // namespace redarc_rbus
}  // namespace esphome

#endif  // USE_ESP32
