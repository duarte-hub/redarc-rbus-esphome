// A bigger stack for ESPHome's Zigbee task.
//
// ESPHome starts the Zigbee stack's task ("Zigbee_main") with 4 KB. That was enough with 20
// endpoints, but with the dashboard's settings on Zigbee (42 endpoints) an error message logged
// from that task (vsnprintf) ran past the end of the stack: "Stack protection fault" at boot, and
// the update rolled back. The task is created through xTaskCreatePinnedToCore, which the build
// wraps (-Wl,--wrap, added by zigbee_filter.sync_to_code) so its stack can be raised here.

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_ZIGBEE)
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr uint32_t ZIGBEE_TASK_STACK = 8192;

extern "C" {
BaseType_t __real_xTaskCreatePinnedToCore(TaskFunction_t code, const char *const name, const uint32_t depth,
                                          void *const params, UBaseType_t prio, TaskHandle_t *const created,
                                          const BaseType_t core);

BaseType_t __wrap_xTaskCreatePinnedToCore(TaskFunction_t code, const char *const name, const uint32_t depth,
                                          void *const params, UBaseType_t prio, TaskHandle_t *const created,
                                          const BaseType_t core) {
  uint32_t d = depth;
  if (name != nullptr && strcmp(name, "Zigbee_main") == 0 && d < ZIGBEE_TASK_STACK)
    d = ZIGBEE_TASK_STACK;
  return __real_xTaskCreatePinnedToCore(code, name, d, params, prio, created, core);
}
}
#endif
