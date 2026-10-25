#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ddmod::features {

// Ground fog on by default (docs/decisions.md). Steam's config.div switches it off with the line
// "no fog", which the game stores in its settings object (docs/re/config-files.md). The game
// reads config.lcl and config.div with one function per file; the hook sets the flag after each
// call, so config.div is not needed for it. [Features] Fog: 1 = fog on (default), 0 = fog off,
// keep = as config.div says. The game's picture settings switch it while the game runs; when the
// game writes config.div on exit, the mod takes their last state over into [Features] Fog.

// Start of the config file reader in div.exe (__thiscall on the settings object, one argument:
// the file name; reads the file line by line into the keyword parser).
inline constexpr std::string_view kConfigReaderPattern =
    "81 EC 04 08 00 00 A1 ?? ?? ?? ?? 33 C4 89 84 24 00 08 00 00 8B 84 24 08 08 00 00 56 57 68 ?? ?? ?? ?? 50 8B F9 "
    "E8 ?? ?? ?? ?? 8B F0 83 C4 08 85 F6 74 ?? 56 E8 ?? ?? ?? ?? 83 C4 04 85 C0 75 ?? 56 8D 44 24 0C 68 00 08 00 00 "
    "50 E8";
// RVA of the match in the verified 1.0062A build (fallback only, rule 8).
inline constexpr std::uint32_t kConfigReaderRva = 0x8D6F0;
// Start of the config.div writer (__thiscall on the settings object, no arguments; runs on exit).
inline constexpr std::string_view kConfigWriterPattern =
    "55 8B EC 83 E4 C0 83 EC 30 53 55 56 57 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 89 4C 24 34 E8 ?? ?? ?? ?? 83 C4 08 "
    "89 44 24 30 85 C0 0F 84 ?? ?? ?? ?? 8B 74 24 2C 8B AE 6C 01 00 00 8B 9E 54 01 00 00";
inline constexpr std::uint32_t kConfigWriterRva = 0x8C6F0;
// "no fog" flag in the settings object (dword, nonzero = no fog).
inline constexpr std::size_t kNoFogField = 0xA4;

// [Features] Fog: true = on, false = off, nullopt = keep (config.div decides). Invalid text gives
// the default (on) and `valid` = false.
std::optional<bool> ParseFog(std::string_view text, bool& valid);

// Hooks `configReader` so that the "no fog" flag of its object matches `fogOn` after every call,
// and `configWriter` (may be null) so that the flag's state before writing is reported to
// `onWrite` (fog on = true). hooks::Init must have succeeded. Exposed for the unit tests.
bool InstallFogAt(void* configReader, void* configWriter, bool fogOn, void (*onWrite)(bool fogOn));

// Applies [Features] Fog: finds reader and writer and installs the hooks. Must run in DllMain (the
// game reads its config files right after start), after hooks::Init and game::IdentifyExe.
bool InstallFog();

}  // namespace ddmod::features
