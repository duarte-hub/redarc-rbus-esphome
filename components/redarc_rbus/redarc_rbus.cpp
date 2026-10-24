#include "redarc_rbus.h"
#include "fault_codes.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <cstring>
#include <cstdio>

namespace esphome {
namespace redarc_rbus {

static const char *const TAG = "redarc_rbus";

// Initial write the app sends to char 8001 before the filter burst: "Keep Awake" (PGN 0xE6xx,
// broadcast). The app only sends it when it connects; the keep-awake watchdog re-sends it.
static const uint8_t INIT_8001[] = {0x2f, 0xe6, 0xff, 0xfa, 0x08, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// The TVMS sleeps (by design in Storage mode) and stops streaming while the BLE link stays
// up. Keep Awake is sent every KEEP_AWAKE_PERIOD_MS while connected; silence longer than
// KEEP_AWAKE_SILENCE_MS is logged once as "asleep". The link is kept: reconnecting to a
// sleeping system cost the BLE bond.
static const uint32_t KEEP_AWAKE_SILENCE_MS = 30000;
static const uint32_t KEEP_AWAKE_PERIOD_MS = 60000;

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
// Re-publish an unchanged value at least this often, so HA graphs/history stay alive.
static const uint32_t PUBLISH_HEARTBEAT_MS = 60000;

void RedarcRbus::setup() {
  this->configure_security_();
  this->setup_done_ = true;
  this->esp_bonded_ = this->esp_has_bond_();
  this->set_link_status_("Searching");
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

  // Poll the live connection RSSI on the configured interval (result arrives via
  // gap_event_handler). 0 disables polling.
  if (this->rssi_sensor_ != nullptr && this->rssi_interval_ms_ != 0 && this->ready() &&
      this->parent() != nullptr && millis() - this->last_rssi_ms_ >= this->rssi_interval_ms_) {
    this->last_rssi_ms_ = millis();
    esp_ble_gap_read_rssi(this->parent()->get_remote_bda());
  }

  const uint32_t now = millis();
  this->label_read_step_(now);
  this->request_history_();
  this->keep_awake_check_(now);
  this->update_gate_(now);
#ifdef USE_TEXT_SENSOR
  this->publish_logs_(now);
#endif

  if (!this->sending_filters_)
    return;
  if (now - this->last_write_ms_ < FILTER_WRITE_INTERVAL_MS)
    return;
  if (this->filter_idx_ >= FILTER_COUNT) {
    this->sending_filters_ = false;
    ESP_LOGD(TAG, "Filter enrollment complete (%u frames)", (unsigned) FILTER_COUNT);
    if (this->label_phase_ == LabelPhase::RESTORING) {
      this->label_phase_ = LabelPhase::IDLE;
      this->publish_labels_();
      this->set_link_status_("Streaming");
    } else {
      this->set_link_status_("Streaming");
      this->reached_stream_ = true;
      this->fail_streak_ = 0;
      this->set_coex_prefer_bt_(false);
      this->request_conn_interval_();
      this->history_next_ms_ = now + 2000;  // first history read shortly after streaming starts
      this->publish_faults_();              // nodes only report faults when they happen
    }
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

// "3h12m", "4m05s", "12s".
static std::string fmt_duration(uint32_t ms) {
  uint32_t s = ms / 1000;
  char buf[24];
  if (s >= 3600)
    snprintf(buf, sizeof(buf), "%uh%02um", (unsigned) (s / 3600), (unsigned) (s / 60 % 60));
  else if (s >= 60)
    snprintf(buf, sizeof(buf), "%um%02us", (unsigned) (s / 60), (unsigned) (s % 60));
  else
    snprintf(buf, sizeof(buf), "%us", (unsigned) s);
  return buf;
}

// HCI disconnect reasons as reported in ESP_GATTC_DISCONNECT_EVT.
static const char *disconnect_reason_name(int reason) {
  switch (reason) {
    case 0x05: return "Authentication failure";
    case 0x06: return "Key missing (bond lost)";
    case 0x08: return "Supervision timeout (Display stopped answering)";
    case 0x13: return "Display closed the link";
    case 0x16: return "ESP closed the link";
    case 0x22: return "Link layer response timeout";
    case 0x3b: return "Unacceptable connection parameters";
    case 0x3d: return "MIC failure (encryption key mismatch)";
    case 0x3e: return "Connection failed to establish";
    case 0x100: return "Connection cancelled";
    default: return "Unknown";
  }
}

// SMP pairing failure reasons (Bluetooth Core spec, Vol 3 Part H 3.5.5).
static const char *pairing_fail_name(int reason) {
  switch (reason) {
    case 0x01: return "Passkey entry failed";
    case 0x03: return "Authentication requirements";
    case 0x04: return "Confirm value failed (wrong code)";
    case 0x05: return "Pairing not supported";
    case 0x06: return "Encryption key size";
    case 0x08: return "Unspecified";
    case 0x09: return "Repeated attempts";
    default: return "Other";
  }
}

void RedarcRbus::set_link_status_(const char *status) {
  if (this->link_status_ == status)
    return;
  this->link_status_ = status;
  ESP_LOGI(TAG, "Link: %s", status);
#ifdef USE_TEXT_SENSOR
  if (this->link_status_text_ != nullptr)
    this->link_status_text_->publish_state(status);
#endif
}

void RedarcRbus::set_pairing_required_(bool on) {
#ifdef USE_BINARY_SENSOR
  if (this->pairing_required_ != nullptr)
    this->pairing_required_->publish_state(on);
#endif
}

// Whether the ESP still holds a bond (keys) for our Display. If it does and the Display asks
// for a passkey anyway, the Display is the side that forgot the bond.
bool RedarcRbus::esp_has_bond_() {
  int n = esp_ble_get_bond_device_num();
  if (n <= 0)
    return false;
  std::vector<esp_ble_bond_dev_t> list(n);
  if (esp_ble_get_bond_device_list(&n, list.data()) != ESP_OK)
    return false;
  for (int i = 0; i < n; i++) {
    if (this->is_our_peer_(list[i].bd_addr))
      return true;
  }
  return false;
}

void RedarcRbus::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_PASSKEY_REQ_EVT: {
      // Only handle a passkey request coming from our Display.
      if (!this->is_our_peer_(param->ble_security.ble_req.bd_addr))
        break;
      memcpy(this->pending_bda_, param->ble_security.ble_req.bd_addr, sizeof(esp_bd_addr_t));
      {
        this->paired_this_conn_ = true;
        this->pairing_requests_++;
        if (this->pairing_requests_sensor_ != nullptr)
          this->pairing_requests_sensor_->publish_state(this->pairing_requests_);
        const bool esp_bond = this->esp_has_bond_();
        char buf[160];
        snprintf(buf, sizeof(buf), "Code requested %s after connect, ESP uptime %s. %s",
                 fmt_duration(millis() - this->connected_at_ms_).c_str(),
                 fmt_duration(millis()).c_str(),
                 esp_bond ? "ESP still had the bond: the Display forgot it"
                          : "ESP had no bond for the Display");
        ESP_LOGW(TAG, "%s", buf);
#ifdef USE_TEXT_SENSOR
        if (this->last_pairing_text_ != nullptr)
          this->last_pairing_text_->publish_state(buf);
#endif
        this->set_pairing_required_(true);
        this->set_link_status_("Pairing required: enter the code on the Display");
      }
      // The Display shows a code that ROTATES, so a value entered long ago is
      // stale. Only auto-answer if a passkey was entered in the last 25 s
      // (i.e. you entered it and pressed Pair Now). Otherwise wait, and answer
      // as soon as you enter the code the Display is showing now.
      const bool fresh = this->have_passkey_ && this->code_fresh_(millis());
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
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
      if (this->is_our_peer_(param->update_conn_params.bda))
        ESP_LOGI(TAG, "BLE connection params: status %d, interval %.2f ms, latency %u, timeout %u ms",
                 param->update_conn_params.status, param->update_conn_params.conn_int * 1.25f,
                 param->update_conn_params.latency, param->update_conn_params.timeout * 10);
      break;
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
        ESP_LOGI(TAG, "Bonded with Display (%s, auth mode %d, key type 0x%02x)",
                 this->paired_this_conn_ ? "new pairing" : "saved bond",
                 param->ble_security.auth_cmpl.auth_mode, param->ble_security.auth_cmpl.key_type);
        this->set_pairing_required_(false);
        this->set_link_status_("Bonded, starting stream");
        this->esp_bonded_ = true;
#ifdef USE_TEXT_SENSOR
        if (this->paired_this_conn_ && this->last_pairing_text_ != nullptr)
          this->last_pairing_text_->publish_state("Paired with a new code at ESP uptime " +
                                                  fmt_duration(millis()));
#endif
        this->bonded_ = true;
        this->start_notify_();
      } else {
        const int reason = param->ble_security.auth_cmpl.fail_reason;
        char buf[96];
        snprintf(buf, sizeof(buf), "Pairing failed: %s (0x%02x)", pairing_fail_name(reason), reason);
        ESP_LOGW(TAG, "%s", buf);
#ifdef USE_TEXT_SENSOR
        if (this->last_pairing_text_ != nullptr)
          this->last_pairing_text_->publish_state(buf);
#endif
        this->set_link_status_("Pairing failed");
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
      if (param->open.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Connect failed, status 0x%02x", param->open.status);
        this->set_link_status_("Connect failed, retrying");
      } else {
        this->connected_at_ms_ = millis();
        this->link_up_ = true;
        this->reached_stream_ = false;
        this->paired_this_conn_ = false;
        ESP_LOGI(TAG, "Connected; ESP %s saved bond for the Display (%d bonds stored)",
                 this->esp_has_bond_() ? "has a" : "has NO", esp_ble_get_bond_device_num());
        this->set_link_status_("Connected, securing link");
        // Proactively bond; if a bond already exists the stack just re-encrypts.
        esp_ble_set_encryption(param->open.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      {
        const int reason = param->disconnect.reason;
        const uint32_t now = millis();
        char buf[200];
        snprintf(buf, sizeof(buf), "%s (0x%02x) after %s connected; state was '%s'; last frame %s ago",
                 disconnect_reason_name(reason), reason,
                 fmt_duration(now - this->connected_at_ms_).c_str(), this->link_status_.c_str(),
                 this->last_frame_ms_ == 0 ? "n/a" : fmt_duration(now - this->last_frame_ms_).c_str());
        ESP_LOGW(TAG, "Disconnected: %s", buf);
#ifdef USE_TEXT_SENSOR
        if (this->last_disconnect_text_ != nullptr)
          this->last_disconnect_text_->publish_state(buf);
#endif
        this->disconnects_++;
        if (this->disconnects_sensor_ != nullptr)
          this->disconnects_sensor_->publish_state(this->disconnects_);
        this->set_pairing_required_(false);
        this->set_link_status_("Disconnected, searching");
        this->set_coex_prefer_bt_(true);  // reconnect + discovery need the radio
        this->link_up_ = false;
        this->esp_bonded_ = this->esp_has_bond_();
        if (!this->reached_stream_ && this->fail_streak_ < 255)
          this->fail_streak_++;
        if (this->fail_streak_ >= 3) {
          const uint8_t steps = std::min<uint8_t>(this->fail_streak_ - 3, 4);
          const uint32_t wait = std::min<uint32_t>(30000u << steps, 300000u);
          this->backoff_until_ = now + wait;
          ESP_LOGW(TAG, "%u connects in a row failed; waiting %us before the next attempt",
                   (unsigned) this->fail_streak_, (unsigned) (wait / 1000));
        }
      }
      this->have_handles_ = false;
      this->bonded_ = false;
      this->notify_started_ = false;
      this->sending_filters_ = false;
      this->filter_idx_ = 0;
      this->passkey_pending_ = false;
      this->seen_pgns_.clear();
      this->source_addr_.clear();
      this->labels_.clear();
      this->label_phase_ = LabelPhase::IDLE;  // reconnect re-sends the standard filters
      this->history_next_ms_ = 0;
      this->history_sending_ = false;
      this->active_faults_.clear();
      this->last_frame_ms_ = 0;
      this->wake_tries_ = 0;
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
      this->last_frame_ms_ = millis();  // start the silence clock from subscription
      this->wake_tries_ = 0;
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->notify_handle_) {
        this->blink_activity_();
        if (this->wake_tries_ != 0) {
          ESP_LOGI(TAG, "Telemetry resumed");
          this->set_link_status_("Streaming");
        }
        this->last_frame_ms_ = millis();
        this->wake_tries_ = 0;
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
  // The code rotates, so only a code entered while running counts as fresh. The YAML
  // `passkey:` is applied before setup() and must not be sent at the first connect after boot.
  if (!this->setup_done_)
    return;
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
  if (!this->have_passkey_) {
    ESP_LOGW(TAG, "Pair Now: enter the code shown on the Display into the pairing code first");
    return;
  }
  // Treat the code in the pairing-code box as just entered: this opens the connection gate
  // (see update_gate_()) and answers the Display's passkey request with it straight away.
  this->last_passkey_set_ms_ = millis();
  if (this->passkey_pending_) {
    ESP_LOGI(TAG, "Pair Now: answering pending pairing with code %06u", (unsigned) this->passkey_);
    esp_ble_passkey_reply(this->pending_bda_, true, this->passkey_);
    this->passkey_pending_ = false;
    return;
  }
  ESP_LOGI(TAG, "Pair Now: connecting with code %06u", (unsigned) this->passkey_);
  // When already linked on an old bond, drop it so the Display issues a fresh passkey challenge.
  if (this->link_up_) {
    uint8_t *bda = this->parent()->get_remote_bda();
    esp_ble_remove_bond_device(bda);
    this->esp_bonded_ = false;
    esp_ble_gap_disconnect(bda);
  }
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

// "RBUS Parameter Set" (PGN 65536, PDU1) as built by the app's toRBusFrameSetSPN, e.g. the
// captured Main Light ON frame 2f 00 24 fa | 08 | cb 00 | 03 | 07 | 01 00 00 00:
// [ID: 0x2f, PF 0x00, dest, src 0xfa][DLC 8][SPN LE16][priority 3][index][value LE32].
bool RedarcRbus::set_parameter(uint8_t dest, uint16_t spn, uint8_t index, uint32_t value) {
  std::vector<uint8_t> frame = {0x2f,
                                0x00,
                                dest,
                                0xfa,
                                0x08,
                                (uint8_t) (spn & 0xFF),
                                (uint8_t) (spn >> 8),
                                0x03,
                                index,
                                (uint8_t) (value & 0xFF),
                                (uint8_t) ((value >> 8) & 0xFF),
                                (uint8_t) ((value >> 16) & 0xFF),
                                (uint8_t) ((value >> 24) & 0xFF)};
  ESP_LOGI(TAG, "Set SPN %u index %u = %u -> node 0x%02x", spn, index, (unsigned) value, dest);
  return this->send_command(frame);
}

// Reading channel labels mirrors the RedVision app's channel-detail read (ReadConfigService:
// setIoDetailsFilters -> RBusNode.loadIOChannelGetLabelData -> setStandardFilters), using the
// exact frames the app sent in a captured session:
//  1. "iodetails" filters: gateway entries 0x00-0x0d cleared, 0x0e = forward PGNs 0xFCxx-0xFDxx
//     only, so label replies don't compete with the normal telemetry stream.
//  2. One PGN Request for "IO Channel Get Label Data" (0xFD04) to the IO node, in the app's
//     format: [requested PGN LE16][00][00][index 00][00 00 00].
//  3. Collect replies for a few seconds (the app's window was ~4 s).
//  4. Restore the standard filter list (identical to the one sent at connect).
static const size_t IODETAILS_FILTER_COUNT = 15;
static const uint32_t LABEL_COLLECT_MS = 5000;

void RedarcRbus::read_channel_labels() {
  if (!this->ready() || this->sending_filters_ || this->label_phase_ != LabelPhase::IDLE) {
    ESP_LOGW(TAG, "Can't read channel labels now (not connected, or busy)");
    return;
  }
  ESP_LOGI(TAG, "Reading channel labels (telemetry pauses ~6 s)");
  this->labels_.clear();
  this->label_dest_ = this->source_of(PGN_DIGITAL_IO_STATUS, 0x24);
  this->label_idx_ = 0;
  this->label_phase_ = LabelPhase::FILTERING;
}

void RedarcRbus::label_read_step_(uint32_t now) {
  switch (this->label_phase_) {
    case LabelPhase::FILTERING: {
      if (now - this->last_write_ms_ < FILTER_WRITE_INTERVAL_MS)
        return;
      if (this->label_idx_ < IODETAILS_FILTER_COUNT) {
        uint8_t f[12] = {(uint8_t) this->label_idx_, 0x2a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        if (this->label_idx_ == 0x0e) {
          const uint8_t io[10] = {0xa0, 0xfe, 0x00, 0x00, 0x00, 0xfd, 0x00, 0x00, 0x00, 0x00};
          memcpy(f + 2, io, sizeof(io));
        }
        esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                 this->write_handle_, sizeof(f), f, ESP_GATT_WRITE_TYPE_NO_RSP,
                                 ESP_GATT_AUTH_REQ_NONE);
        this->label_idx_++;
        this->last_write_ms_ = now;
        return;
      }
      const uint16_t req = PGN_IO_CHANNEL_GET_LABEL & 0xFFFF;
      std::vector<uint8_t> frame = {0x2f, 0x03, this->label_dest_, 0xfa, 0x08,
                                    (uint8_t) (req & 0xFF), (uint8_t) (req >> 8), 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00};
      ESP_LOGD(TAG, "Requesting IO channel labels from node 0x%02x", this->label_dest_);
      this->send_command(frame);
      this->label_until_ = now + LABEL_COLLECT_MS;
      this->label_phase_ = LabelPhase::COLLECTING;
      return;
    }
    case LabelPhase::COLLECTING:
      if ((int32_t) (now - this->label_until_) < 0)
        return;
      // Always put the normal filters back, whatever arrived.
      this->label_phase_ = LabelPhase::RESTORING;
      this->filter_idx_ = 0;
      this->last_write_ms_ = now;
      this->sending_filters_ = true;
      return;
    default:
      return;
  }
}

// History logs (SoC hourly/daily min/max, solar daily/hourly) are not streamed; the app asks the
// Display for its cached copy with a 5-byte "request to read" frame [2f][PF][PS][fa][48] (DLC
// byte 0x48 = read request, no payload), which the Display answers itself without CAN traffic.
// These are the exact frames the app sends (captured), at the app's ~100 ms spacing.
static const uint8_t HISTORY_REQUESTS[][2] = {
    {0xfc, 0xd0},  // 130256 SoC log, hourly
    {0xfc, 0xd2},  // 130258 minimum SoC log, daily
    {0xfc, 0xd4},  // 130260 maximum SoC log, daily
    {0xfc, 0xd6},  // 130262 cumulative solar log, daily
    {0xfc, 0xd5},  // 130261 cumulative solar log, hourly
};
static const size_t HISTORY_REQUEST_COUNT = sizeof(HISTORY_REQUESTS) / sizeof(HISTORY_REQUESTS[0]);

void RedarcRbus::request_history_() {
  const uint32_t now = millis();
  if (this->history_interval_ms_ == 0 || !this->ready() || this->sending_filters_ ||
      this->label_phase_ != LabelPhase::IDLE)
    return;
  if (!this->history_sending_) {
    if (this->history_next_ms_ == 0 || (int32_t) (now - this->history_next_ms_) < 0)
      return;
    this->history_sending_ = true;
    this->history_idx_ = 0;
  }
  if (now - this->history_last_tx_ms_ < 100)
    return;
  if (this->history_idx_ >= HISTORY_REQUEST_COUNT) {
    this->history_sending_ = false;
    this->history_next_ms_ = now + this->history_interval_ms_;
    return;
  }
  const uint8_t *r = HISTORY_REQUESTS[this->history_idx_++];
  this->send_command({0x2f, r[0], r[1], 0xfa, 0x48});
  this->history_last_tx_ms_ = now;
}

#ifdef USE_TEXT_SENSOR
void RedarcRbus::handle_log_(uint32_t pgn, const uint8_t *pay, uint32_t now) {
  for (auto &b : this->logs_) {
    if (b.pgn != pgn)
      continue;
    for (uint8_t k = 0; k < b.per_frame; k++) {
      const size_t idx = (size_t) pay[0] * b.per_frame + k;
      const size_t off = 1 + k * b.value_bytes;
      if (idx >= b.values.size() || off + b.value_bytes > 8)
        continue;
      int32_t raw = pay[off];
      if (b.value_bytes == 2)
        raw |= pay[off + 1] << 8;
      const int32_t na = b.value_bytes == 2 ? 0xFFFB : 0xFB;
      const int32_t v = raw >= na ? -1 : raw;
      if (b.values[idx] != v) {
        b.values[idx] = v;
        b.dirty_ms = now;
      }
    }
  }
}

// Publish a log once its frames have stopped arriving, as "newest,...,oldest".
void RedarcRbus::publish_logs_(uint32_t now) {
  for (auto &b : this->logs_) {
    if (b.dirty_ms == 0 || now - b.dirty_ms < 1000)
      continue;
    b.dirty_ms = 0;
    std::string s;
    for (int32_t v : b.values) {
      if (v < 0)
        break;
      if (!s.empty())
        s += ',';
      s += std::to_string(v);
    }
    if (!s.empty())
      b.sensor->publish_state(s);
  }
}
#endif

static const FaultCode *find_fault(uint16_t code) {
  for (const auto &f : FAULT_CODES)
    if (f.code == code)
      return &f;
  return nullptr;
}

// Error Message: [code LE16][flags LE16: 1 notify, 2 visual, 4 audible, 8 stop charging,
// 0x10 user clearable, 0x20 unignorable][status bits 0-1: 1 active, 0 cleared].
void RedarcRbus::handle_fault_(uint8_t src, const uint8_t *pay) {
  const uint16_t code = pay[0] | (pay[1] << 8);
  const uint16_t flags = pay[2] | (pay[3] << 8);
  const uint8_t status = pay[4] & 0x03;
  if (code >= 0xFFFB || status > 1)
    return;
  const FaultCode *f = find_fault(code);
  ESP_LOGW(TAG, "Fault %s from node 0x%02x: 0x%04X %s (flags 0x%04X)", status ? "ACTIVE" : "cleared",
           src, code, f != nullptr ? f->title : "unknown", flags);
  const uint32_t key = ((uint32_t) src << 16) | code;
  const size_t before = this->active_faults_.size();
  if (status == 1) {
    auto it = this->active_faults_.find(key);
    if (it != this->active_faults_.end() && it->second == flags)
      return;  // repeat of a known active fault
    if (this->active_faults_.size() >= 16 && it == this->active_faults_.end())
      return;  // bound the set
    this->active_faults_[key] = flags;
  } else if (this->active_faults_.erase(key) == 0 && before != 0) {
    return;
  }
  this->publish_faults_();
}

void RedarcRbus::publish_faults_() {
#ifdef USE_BINARY_SENSOR
  if (this->fault_active_ != nullptr)
    this->fault_active_->publish_state(!this->active_faults_.empty());
#endif
#ifdef USE_TEXT_SENSOR
  if (this->faults_text_ == nullptr)
    return;
  std::string s;
  for (auto &kv : this->active_faults_) {
    const uint16_t code = kv.first & 0xFFFF;
    const FaultCode *f = find_fault(code);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s%s (0x%04X)", s.empty() ? "" : "; ",
             f != nullptr ? f->title : "Unknown fault", code);
    if (s.size() + strlen(buf) > 250)
      break;
    s += buf;
  }
  this->faults_text_->publish_state(s.empty() ? "None" : s);
#endif
}

// [channel][chunk index][6 characters]; chunks concatenate into the channel's label.
void RedarcRbus::handle_label_(const uint8_t src, const uint8_t *pay) {
  // Bound what we store: only replies during our read, from the node we asked, real channel
  // numbers, and the app's maximum of 8 chunks (data index 0-7).
  if (this->label_phase_ != LabelPhase::COLLECTING || src != this->label_dest_ || pay[0] == 0 ||
      pay[0] > 64 || pay[1] > 7)
    return;
  std::string text;
  for (int i = 2; i < 8; i++) {
    if (pay[i] == 0x00 || pay[i] == 0xFF)
      break;
    text += (char) pay[i];
  }
  this->labels_[pay[0]][pay[1]] = text;
}

void RedarcRbus::publish_labels_() {
  std::string all;
  for (auto &ch : this->labels_) {
    std::string label;
    for (auto &chunk : ch.second)
      label += chunk.second;
    ESP_LOGI(TAG, "IO channel %u label: \"%s\"", ch.first, label.c_str());
    if (all.size() < 240)
      all += (all.empty() ? "" : "; ") + std::to_string(ch.first) + "=" + label;
  }
  ESP_LOGI(TAG, "Channel label read finished: %u channels", (unsigned) this->labels_.size());
#ifdef USE_TEXT_SENSOR
  if (this->labels_sensor_ != nullptr)
    this->labels_sensor_->publish_state(all.empty() ? "none received" : all);
#endif
}

void RedarcRbus::request_conn_interval_() {
  if (this->conn_interval_ms_ == 0 || this->parent() == nullptr)
    return;
  // Units: interval 1.25 ms, supervision timeout 10 ms. Allow +25 ms of slack for the peer.
  esp_ble_conn_update_params_t p = {};
  memcpy(p.bda, this->parent()->get_remote_bda(), sizeof(esp_bd_addr_t));
  p.min_int = (uint16_t) (this->conn_interval_ms_ * 4 / 5);
  p.max_int = p.min_int + 20;
  p.latency = 0;
  p.timeout = 600;  // 6 s
  ESP_LOGD(TAG, "Requesting BLE connection interval %u-%u ms", (unsigned) this->conn_interval_ms_,
           (unsigned) (this->conn_interval_ms_ + 25));
  esp_ble_gap_update_conn_params(&p);
}

void RedarcRbus::keep_awake_check_(uint32_t now) {
  if (!this->keep_awake_ || !this->ready() || this->sending_filters_ ||
      this->label_phase_ != LabelPhase::IDLE || this->last_frame_ms_ == 0)
    return;
  // Never drop the link here: reconnecting while the system sleeps made the Display ask for a
  // new passkey, whereas staying connected keeps the bond and streaming resumes on wake.
  if (this->wake_tries_ == 0 && now - this->last_frame_ms_ >= KEEP_AWAKE_SILENCE_MS) {
    this->wake_tries_ = 1;  // "asleep" until the next frame (see ESP_GATTC_NOTIFY_EVT)
    this->set_link_status_("Connected, system asleep (no telemetry)");
    ESP_LOGW(TAG, "No telemetry for %us: system asleep (e.g. Storage mode). Staying connected, "
                  "sending Keep Awake every %us",
             (unsigned) ((now - this->last_frame_ms_) / 1000), (unsigned) (KEEP_AWAKE_PERIOD_MS / 1000));
  }
  if (now - this->last_wake_ms_ < KEEP_AWAKE_PERIOD_MS)
    return;
  this->last_wake_ms_ = now;
  this->send_command(std::vector<uint8_t>(INIT_8001, INIT_8001 + sizeof(INIT_8001)));
}

uint8_t RedarcRbus::source_of(uint32_t pgn, uint8_t fallback) const {
  auto it = this->source_addr_.find(pgn);
  return it == this->source_addr_.end() ? fallback : it->second;
}

void RedarcRbusSwitch::write_state(bool state) {
  if (this->parent_ == nullptr)
    return;
  if (this->channel_ == 0) {
    const std::vector<uint8_t> &frame = state ? this->on_frame_ : this->off_frame_;
    if (this->parent_->send_command(frame))
      this->publish_state(state);  // optimistic; raw frames have no read-back
    return;
  }
  const uint8_t dest = this->node_address_ >= 0 ? (uint8_t) this->node_address_
                                                : this->parent_->source_of(130304, 0x24);
  // SPN 203 = Digital IO State; index = IO channel (app: ControlNotifier.toggleChannelState).
  if (this->parent_->set_parameter(dest, 203, this->channel_, state ? 1 : 0)) {
    this->commanded_ = state;
    this->hold_until_ = millis() + COMMAND_HOLD_MS;
    this->publish_state(state);
  }
}

void RedarcRbusSwitch::on_raw(uint32_t raw) {
  if (raw > 1)
    return;  // 2 = error
  const bool state = raw == 1;
  if (state != this->commanded_ && (int32_t) (millis() - this->hold_until_) < 0)
    return;  // still waiting for the node to report the commanded state
  this->hold_until_ = 0;
  this->commanded_ = state;
  if (!this->has_state() || state != this->state)
    this->publish_state(state);
}

void RedarcRbusSwitch::dump_config() {
  LOG_SWITCH("", "REDARC Output Switch", this);
  if (this->channel_ != 0)
    ESP_LOGCONFIG(TAG, "  IO channel: %u", this->channel_);
}

#ifdef USE_SELECT
void RedarcRbusSelect::control(size_t index) {
  if (this->parent_ == nullptr)
    return;
  const uint8_t dest =
      this->dest_pgn_ ? this->parent_->source_of(this->dest_pgn_, this->dest_fallback_) : this->dest_fallback_;
  if (this->parent_->set_parameter(dest, this->spn_, 0, (uint32_t) index)) {
    this->commanded_ = index;
    this->hold_until_ = millis() + COMMAND_HOLD_MS;
    this->publish_state(index);
  }
}

void RedarcRbusSelect::on_raw(uint32_t raw) {
  if (raw >= this->size())
    return;
  if (raw != this->commanded_ && (int32_t) (millis() - this->hold_until_) < 0)
    return;
  this->hold_until_ = 0;
  this->commanded_ = raw;
  if (!this->has_state() || this->active_index_ != raw)
    this->publish_state((size_t) raw);
}

void RedarcRbusSelect::dump_config() { LOG_SELECT("", "REDARC Setting", this); }
#endif

#ifdef USE_BUTTON
void RedarcRbusButton::press_action() {
  if (this->parent_ == nullptr)
    return;
  if (this->read_labels_) {
    this->parent_->read_channel_labels();
    return;
  }
  const uint8_t dest =
      this->dest_pgn_ ? this->parent_->source_of(this->dest_pgn_, this->dest_fallback_) : this->dest_fallback_;
  this->parent_->set_parameter(dest, this->spn_, this->index_, this->value_);
}

void RedarcRbusButton::dump_config() { LOG_BUTTON("", "REDARC Action", this); }
#endif

void RedarcRbus::start_notify_() {
  if (this->notify_started_ || !this->have_handles_ || !this->bonded_)
    return;
  this->notify_started_ = true;
  ESP_LOGD(TAG, "Registering for notifications on 8001");
  esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(),
                                    this->parent()->get_remote_bda(), this->notify_handle_);
}

bool PublishGate::due(float v, uint32_t now, uint32_t min_interval_ms) {
  if (v == this->last && now - this->last_ms < PUBLISH_HEARTBEAT_MS)
    return false;
  // Rate-limit changing values; frames repeat every ~0.5-2 s, so the latest value still goes
  // out within one interval.
  if (v != this->last && min_interval_ms != 0 && !std::isnan(this->last) &&
      now - this->last_ms < min_interval_ms)
    return false;
  this->last = v;
  this->last_ms = now;
  return true;
}

// J1939 reserves the top of each field's range: for >=8-bit fields 0xFB..0xFF (per byte)
// mean error / not available; for short fields the all-ones value means not available.
bool RedarcRbus::field_raw_(const FieldSpec &f, uint64_t payload, uint32_t &raw) {
  const uint8_t index = payload & 0xFF;  // byte 0: log frame number / first IO channel
  if (f.mux >= 0 && index != f.mux)
    return false;
  uint32_t shift = f.shift;
  if (f.channel != 0) {
    if (f.channel < index || f.channel >= index + f.per_frame)
      return false;
    shift += (1 + (f.channel - index) * f.slot_bytes) * 8;
  }
  const uint64_t mask = (f.bits >= 32) ? 0xFFFFFFFFULL : ((1ULL << f.bits) - 1);
  raw = (uint32_t) ((payload >> shift) & mask);
  if (f.bits >= 8)
    return raw <= mask - 5;
  return raw != mask;
}

bool RedarcRbus::field_value_(const FieldSpec &f, uint64_t payload, float &value) {
  uint32_t raw;
  if (!field_raw_(f, payload, raw))
    return false;
  value = (float) ((double) ((int64_t) raw + f.offset) * f.scale);
  return true;
}

// Power from a "status" frame laid out as I = bytes0-3 (offset -1e6, mA), V = bytes4-5 (mV).
void RedarcRbus::publish_power_(sensor::Sensor *s, PublishGate &gate, uint64_t payload, uint32_t now) {
  static const FieldSpec CURRENT{0, 0, 32, -1000000, 0.001f};
  static const FieldSpec VOLTAGE{0, 32, 16, 0, 0.001f};
  float i, v;
  if (s == nullptr || !field_value_(CURRENT, payload, i) || !field_value_(VOLTAGE, payload, v))
    return;
  const float w = roundf(v * i * 10.0f) / 10.0f;
  if (gate.due(w, now, this->min_publish_ms_))
    s->publish_state(w);
}

void RedarcRbus::update_gate_(uint32_t now) {
  if (this->parent() == nullptr || this->link_up_ || now - this->last_gate_ms_ < 500)
    return;
  this->last_gate_ms_ = now;
  const bool fresh = this->code_fresh_(now);
  const bool backing_off = (int32_t) (this->backoff_until_ - now) > 0;
  // A freshly entered code always opens the gate: the user is at the Display right now.
  const bool open = fresh || (this->esp_bonded_ && !backing_off);
  if (open == this->gate_open_)
    return;
  this->gate_open_ = open;
  this->parent()->set_enabled(open);
  if (open) {
    if (fresh)
      this->backoff_until_ = 0;
    this->set_pairing_required_(false);
    this->set_link_status_(fresh ? "Code entered, connecting" : "Disconnected, searching");
  } else if (!this->esp_bonded_) {
    this->set_pairing_required_(true);
    this->set_link_status_("Waiting for pairing code: open Settings > Bluetooth on the Display "
                           "and enter the code shown");
  } else {
    this->set_link_status_("Paused after repeated failed connects, retrying soon");
  }
}

// Coexistence: ESPHome sets "prefer Bluetooth" to connect and only reverts once no BLE
// connection is open, i.e. never for this always-on link. Once our handshake and filter
// enrollment are done we only receive notifications, so balanced is enough for BLE and gives
// Wi-Fi (the HA API) its airtime back.
void RedarcRbus::set_coex_prefer_bt_(bool prefer_bt) {
#ifdef USE_ESP32_BLE_SOFTWARE_COEXISTENCE
  if (!this->balance_coex_)
    return;
  esp_coex_preference_set(prefer_bt ? ESP_COEX_PREFER_BT : ESP_COEX_PREFER_BALANCE);
  ESP_LOGI(TAG, "Radio coexistence: %s", prefer_bt ? "prefer Bluetooth" : "balanced (streaming)");
#endif
}

// Same rules as the app's ModelUtils._formatTimeRemaining: positive = to full (hours rounded
// up), negative = to flat (rounded down); "Full" when charging at 100 % SoC; "< 1 hr";
// hours up to 23, then days; over 28 days = "More than 28 days". The battery monitor caps the
// value at about +-50000 min (35 days) when the current is near zero, so the numeric sensor
// goes unknown for "Full" and over-28-day values instead of swinging between the caps.
void RedarcRbus::publish_time_remaining_(uint64_t payload, uint32_t now) {
  static const FieldSpec SOC{PGN_BATTERY_CHARGE_STATUS, 0, 8, 0, 1.0f};
  static const FieldSpec TIME{PGN_BATTERY_CHARGE_STATUS, 16, 16, -32127, 5.0f};
  float soc, minutes;
  if (!field_value_(TIME, payload, minutes))
    return;
  const bool have_soc = field_value_(SOC, payload, soc);
  const bool charging = minutes > 0;
  const bool full = charging && have_soc && soc >= 100;
  const float hours = std::fabs(minutes) / 60.0f;
  const float h = charging ? std::ceil(hours) : std::floor(hours);
  const float d = charging ? std::ceil(hours / 24.0f) : std::floor(hours / 24.0f);
  const bool beyond = h > 23 && d > 28;

  if (this->time_remaining_sensor_ != nullptr) {
    if (full || beyond) {
      if (!std::isnan(this->time_remaining_gate_.last) || !this->time_remaining_sensor_->has_state()) {
        this->time_remaining_gate_.last = NAN;
        this->time_remaining_gate_.last_ms = now;
        this->time_remaining_sensor_->publish_state(NAN);
      }
    } else if (this->time_remaining_gate_.due(minutes, now, this->min_publish_ms_)) {
      this->time_remaining_sensor_->publish_state(minutes);
    }
  }
#ifdef USE_TEXT_SENSOR
  if (this->time_remaining_text_ == nullptr)
    return;
  const char *dir = charging ? "to full" : "to flat";
  char buf[40];
  if (full)
    snprintf(buf, sizeof(buf), "Full");
  else if (h < 1)
    snprintf(buf, sizeof(buf), "Less than 1 hr %s", dir);
  else if (h <= 23)
    snprintf(buf, sizeof(buf), "%.0f hr%s %s", h, h == 1 ? "" : "s", dir);
  else if (d > 28)
    snprintf(buf, sizeof(buf), "More than 28 days %s", dir);
  else
    snprintf(buf, sizeof(buf), "%.0f day%s %s", d, d == 1 ? "" : "s", dir);
  if (this->time_remaining_text_->state != buf)
    this->time_remaining_text_->publish_state(buf);
#endif
}

// R-Bus telemetry frame = [4-byte J1939 29-bit CAN id][DLC=0x08][8-byte payload].
// The app's CANBytesParser reads fields as (value >> ((startByte-1)*8 + startBit-1)) & mask over
// the payload; that is plain little-endian extraction, so each field is a shift into a LE u64.
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

  // Log each distinct PGN once per connection with its payload, so missing data
  // (e.g. voltage on a different PGN for some hardware) is easy to diagnose.
  if (this->seen_pgns_.insert(pgn).second)
    ESP_LOGD(TAG, "RX R-Bus PGN %u payload=%02x%02x%02x%02x%02x%02x%02x%02x", (unsigned) pgn, pay[0], pay[1],
             pay[2], pay[3], pay[4], pay[5], pay[6], pay[7]);

  uint64_t payload = 0;
  for (int i = 7; i >= 0; i--)
    payload = (payload << 8) | pay[i];
  const uint32_t now = millis();
  this->source_addr_[pgn] = hdr[3];

  if (pgn == PGN_IO_CHANNEL_GET_LABEL)
    this->handle_label_(hdr[3], pay);
  if (pgn == PGN_ERROR_MESSAGE)
    this->handle_fault_(hdr[3], pay);
#ifdef USE_TEXT_SENSOR
  this->handle_log_(pgn, pay, now);
#endif
  if (pgn == PGN_ACKNOWLEDGEMENT)  // reply to our parameter sets: type, SPN, index, status
    ESP_LOGD(TAG, "Ack from node 0x%02x: type %u SPN %u index %u status %u", hdr[3], pay[0],
             pay[1] | (pay[2] << 8), pay[3], pay[4]);

  auto only = this->pgn_source_.find(pgn);
  if (only != this->pgn_source_.end() && only->second != hdr[3])
    return;  // same PGN from another node; entities follow the configured one

  for (auto &b : this->listeners_) {
    uint32_t raw;
    if (b.field.pgn == pgn && field_raw_(b.field, payload, raw))
      b.listener->on_raw(raw);
  }

  for (auto &b : this->sensors_) {
    float v;
    if (b.field.pgn == pgn && field_value_(b.field, payload, v) && b.gate.due(v, now, this->min_publish_ms_))
      b.sensor->publish_state(v);
  }
#ifdef USE_BINARY_SENSOR
  for (auto &b : this->binary_sensors_) {
    uint32_t raw;
    // 2 = error: leave the last good state rather than guessing.
    if (b.field.pgn == pgn && field_raw_(b.field, payload, raw) && raw <= 1 &&
        b.gate.due((float) raw, now))
      b.sensor->publish_state(raw == 1);
  }
#endif
#ifdef USE_TEXT_SENSOR
  for (auto &b : this->text_sensors_) {
    uint32_t raw;
    if (b.field.pgn != pgn || !field_raw_(b.field, payload, raw) || !b.gate.due((float) raw, now))
      continue;
    if (raw < b.names.size() && !b.names[raw].empty()) {
      b.sensor->publish_state(b.names[raw]);
    } else {
      char buf[16];
      snprintf(buf, sizeof(buf), "Unknown (%u)", (unsigned) raw);
      b.sensor->publish_state(buf);
    }
  }
#endif

  if (pgn == PGN_BATTERY_CHARGE_STATUS)
    this->publish_time_remaining_(payload, now);
  if (pgn == PGN_BATTERY_SENSOR_MEASUREMENTS)
    this->publish_power_(this->battery_power_sensor_, this->battery_power_gate_, payload, now);
  else if (pgn == PGN_SOLAR_INPUT_STATUS)
    this->publish_power_(this->solar_power_sensor_, this->solar_power_gate_, payload, now);
}

void RedarcRbus::dump_config() {
  ESP_LOGCONFIG(TAG, "REDARC RedVision R-Bus BLE proxy:");
  ESP_LOGCONFIG(TAG, "  Passkey configured: %s", YESNO(this->have_passkey_));
  ESP_LOGCONFIG(TAG, "  Saved bond for Display: %s (%d bonds stored)", YESNO(this->esp_has_bond_()),
                esp_ble_get_bond_device_num());
  ESP_LOGCONFIG(TAG, "  Link: %s", this->link_status_.c_str());
  for (auto &b : this->sensors_)
    LOG_SENSOR("  ", "Field", b.sensor);
  ESP_LOGCONFIG(TAG, "  Control read-back bindings: %u", (unsigned) this->listeners_.size());
  LOG_SENSOR("  ", "Battery Power", this->battery_power_sensor_);
  LOG_SENSOR("  ", "Solar Power", this->solar_power_sensor_);
#ifdef USE_BINARY_SENSOR
  for (auto &b : this->binary_sensors_)
    LOG_BINARY_SENSOR("  ", "Field", b.sensor);
#endif
#ifdef USE_TEXT_SENSOR
  for (auto &b : this->text_sensors_)
    LOG_TEXT_SENSOR("  ", "Field", b.sensor);
#endif
}

}  // namespace redarc_rbus
}  // namespace esphome

#endif  // USE_ESP32
