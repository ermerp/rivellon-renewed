#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ddmod::fonts {

// Forces the game's GDI text path (use8bitfont 0) in memory, without touching config.lcl
// (docs/re/text-rendering.md, "Font mode"). The game reads the use8bitfont byte of its settings
// object exactly once, in the function that creates its 61 fonts. The hook sets the byte to 0 for
// the duration of that call and restores it afterwards, so the value the game writes back to
// config.div on exit stays the user's own.

// Start of the font setup function in div.exe. It ends with the read of the use8bitfont byte
// (cmp byte [eax+0x120],0) right after loading the settings pointer (A1 imm32).
inline constexpr std::string_view kFontInitPattern =
    "56 6A 00 68 ?? ?? ?? ?? 8B F1 E8 ?? ?? ?? ?? 83 C4 08 85 C0 A1 ?? ?? ?? ?? 75 ?? 80 B8 20 01 00 00 00";
// RVA of the match in the verified 1.0062A build (fallback only, rule 8).
inline constexpr std::uint32_t kFontInitRva = 0x5BF90;
// Offset of the imm32 in "A1 imm32": the address of the global settings pointer.
inline constexpr std::ptrdiff_t kSettingsGlobalOperand = 21;
// Offset of the use8bitfont byte in the settings object.
inline constexpr std::size_t kUse8BitFontField = 0x120;

// Hooks `fontInit` (a __thiscall function without stack arguments) so that the byte at
// kUse8BitFontField of the object `*settingsGlobal` points to is 0 while it runs.
// hooks::Init must have succeeded. Exposed separately for the unit tests.
bool InstallFontModeAt(void* fontInit, std::uint8_t* const* settingsGlobal);

// Finds the font setup function in div.exe and installs the hook. Must run in DllMain (the game
// creates its fonts long before its window exists), after hooks::Init and game::IdentifyExe.
bool InstallFontMode();

}  // namespace ddmod::fonts
