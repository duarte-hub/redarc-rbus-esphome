#include "crash_report.h"

#include <cstdio>
#include "esphome/core/defines.h"

#if defined(USE_ESP32) && __has_include(<esp_core_dump.h>)
#include "sdkconfig.h"
#endif

#if defined(USE_ESP32) && defined(CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH) && defined(CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF)
#include <esp_core_dump.h>
#include <memory>
#define REDARC_HAS_COREDUMP 1
#endif

namespace esphome::redarc_rbus {

std::string last_crash_summary() {
#ifdef REDARC_HAS_COREDUMP
  if (esp_core_dump_image_check() != ESP_OK)
    return "";
  auto s = std::make_unique<esp_core_dump_summary_t>();
  if (esp_core_dump_get_summary(s.get()) != ESP_OK)
    return "core dump unreadable";
  char buf[256];
  int n = snprintf(buf, sizeof(buf), "task=%.16s pc=0x%08lx", s->exc_task, (unsigned long) s->exc_pc);
#if defined(__riscv)
  n += snprintf(buf + n, sizeof(buf) - n, " ra=0x%08lx sp=0x%08lx cause=%lu val=0x%08lx",
                (unsigned long) s->ex_info.ra, (unsigned long) s->ex_info.sp, (unsigned long) s->ex_info.mcause,
                (unsigned long) s->ex_info.mtval);
#endif
  n += snprintf(buf + n, sizeof(buf) - n, " elf=%.8s", (const char *) s->app_elf_sha256);
  std::string out(buf, n > 0 && n < (int) sizeof(buf) ? n : sizeof(buf) - 1);
  char reason[160];
  if (esp_core_dump_get_panic_reason(reason, sizeof(reason)) == ESP_OK) {
    out += " reason=";
    out += reason;
  }
  return out;
#else
  return "";
#endif
}

}  // namespace esphome::redarc_rbus
