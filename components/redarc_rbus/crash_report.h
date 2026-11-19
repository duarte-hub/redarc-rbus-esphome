#pragma once

// The last crash, from the core dump the ESP keeps in flash (only when the build has the
// espcoredump component and saves core dumps to flash), as one line for a text sensor:
// "task=... pc=0x... ra=0x... sp=0x... cause=N val=0x... elf=xxxxxxxx reason=...".
// Empty when there is no core dump. Decode the addresses with addr2line against the ELF of the
// build that crashed (its SHA-256 prefix is the "elf" field).

#include <string>

namespace esphome::redarc_rbus {

std::string last_crash_summary();

}  // namespace esphome::redarc_rbus
