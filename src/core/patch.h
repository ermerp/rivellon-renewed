#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace ddmod::patch {

// Overwrites `bytes.size()` bytes of code at `address` in this process (VirtualProtect, write,
// restore, FlushInstructionCache). Only ever the game's code in memory, never the file (rule 3).
// The caller makes sure no thread runs through the bytes while they change. False on failure
// (logged under `name`).
bool WriteCode(std::string_view name, std::uintptr_t address, std::span<const std::uint8_t> bytes);

}  // namespace ddmod::patch
