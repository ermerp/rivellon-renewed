#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>

#include "core/pattern.h"

namespace ddmod::scanner {

// Which part of a loaded module to search.
enum class Area {
    Code,   // executable sections only (functions)
    Image,  // every section (also strings and tables)
};

struct Section {
    std::string name;
    std::span<const std::uint8_t> bytes;
    bool executable = false;
};

// Sections of a module mapped in this process, taken from its PE headers.
std::vector<Section> Sections(HMODULE module);

// Absolute addresses of all matches, ascending; stops after `maxMatches`.
std::vector<std::uintptr_t> FindAll(HMODULE module, const pattern::Pattern& pattern, Area area,
                                    std::size_t maxMatches = 16);

// Address of the single match of `patternText`, or 0 if it is invalid, missing or ambiguous.
// The outcome is logged under `name`.
std::uintptr_t FindUnique(std::string_view name, HMODULE module, std::string_view patternText,
                          Area area = Area::Code);

// Target of a relative call/jump: `operand` points at a rel32 that ends the instruction
// (e.g. E8/E9 rel32 -> operand = instruction + 1).
std::uintptr_t FollowRel32(std::uintptr_t operand);

// Reads a 32-bit absolute value (e.g. the address in "A1 imm32" or "8B 0D imm32").
std::uint32_t ReadU32(std::uintptr_t address);

}  // namespace ddmod::scanner
