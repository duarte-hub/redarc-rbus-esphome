#include "zigbee_link.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#if defined(USE_ESP32) && defined(USE_ZIGBEE)
#include "freertos/FreeRTOS.h"
#include "esp_zigbee.h"
#include "ezbee/zha.h"
#include "ezbee/nwk.h"
#endif

namespace esphome::zigbee_link {

static const char *const TAG = "zigbee_link";

void ZigbeeLink::update() {
#if defined(USE_ESP32) && defined(USE_ZIGBEE)
  float rssi = NAN;
  float lqi = NAN;
  // The stack runs in its own task; ESPHome's zigbee component takes this lock around its calls too.
  if (!esp_zigbee_lock_acquire(10 / portTICK_PERIOD_MS)) {
    return;  // busy, try again next interval
  }
  if (ezb_bdb_dev_joined()) {
    ezb_nwk_info_iterator_t it = EZB_NWK_INFO_ITERATOR_INIT;
    ezb_nwk_neighbor_info_t nb;
    while (ezb_nwk_get_next_neighbor(&it, &nb) == EZB_ERR_NONE) {
      if (nb.relationship == EZB_NWK_RELATIONSHIP_PARENT) {
        rssi = nb.rssi;
        lqi = nb.lqi;
        break;
      }
    }
  }
  esp_zigbee_lock_release();
  // NaN (not joined / no parent yet) shows as unknown instead of a stale value.
  if (this->rssi_ != nullptr)
    this->rssi_->publish_state(rssi);
  if (this->lqi_ != nullptr)
    this->lqi_->publish_state(lqi);
#else
  ESP_LOGW(TAG, "Zigbee is not available in this build");
#endif
}

void ZigbeeLink::dump_config() {
  ESP_LOGCONFIG(TAG, "Zigbee link quality");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "RSSI", this->rssi_);
  LOG_SENSOR("  ", "LQI", this->lqi_);
}

}  // namespace esphome::zigbee_link
