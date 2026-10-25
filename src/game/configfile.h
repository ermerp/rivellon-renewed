#pragma once

#include <cstdint>
#include <string_view>

namespace ddmod::game {

// The game's config file reader and config.div writer (docs/re/config-files.md), hooked once and
// shared by every feature that changes a setting: features register observers, then
// InstallConfigHooks puts the hooks in (only if there is an observer).

// Start of the config file reader in div.exe (__thiscall on the settings object, one argument:
// the file name; reads the file line by line into the keyword parser). Runs for config.lcl, then
// config.div, right after the game starts.
inline constexpr std::string_view kConfigReaderPattern =
    "81 EC 04 08 00 00 A1 ?? ?? ?? ?? 33 C4 89 84 24 00 08 00 00 8B 84 24 08 08 00 00 56 57 68 ?? ?? ?? ?? 50 8B F9 "
    "E8 ?? ?? ?? ?? 8B F0 83 C4 08 85 F6 74 ?? 56 E8 ?? ?? ?? ?? 83 C4 04 85 C0 75 ?? 56 8D 44 24 0C 68 00 08 00 00 "
    "50 E8";
// RVA of the match in the verified 1.0062A build (fallback only, rule 8).
inline constexpr std::uint32_t kConfigReaderRva = 0x8D6F0;
// Start of the config.div writer (__thiscall on the settings object, no arguments; runs on exit
// and when an options page is applied).
inline constexpr std::string_view kConfigWriterPattern =
    "55 8B EC 83 E4 C0 83 EC 30 53 55 56 57 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 89 4C 24 34 E8 ?? ?? ?? ?? 83 C4 08 "
    "89 44 24 30 85 C0 0F 84 ?? ?? ?? ?? 8B 74 24 2C 8B AE 6C 01 00 00 8B 9E 54 01 00 00";
inline constexpr std::uint32_t kConfigWriterRva = 0x8C6F0;

// Called after each reader call with the settings object and the file name (may be null).
using ConfigReadObserver = void (*)(void* settings, const char* file);
// Called right before (`before` = true) and right after the writer runs.
using ConfigWriteObserver = void (*)(void* settings, bool before);

// Registering the same function twice has no effect. Observers run in registration order.
void OnConfigRead(ConfigReadObserver observer);
void OnConfigWrite(ConfigWriteObserver observer);
// Forgets all observers (unit tests).
void ClearConfigObservers();

// Hooks `reader` and `writer` (writer may be null). hooks::Init must have succeeded. Exposed for
// the unit tests.
bool InstallConfigHooksAt(void* reader, void* writer);

// Finds reader and writer in div.exe and hooks them if any observer is registered. Must run in
// DllMain, after hooks::Init, game::IdentifyExe and the features' registration.
bool InstallConfigHooks();

}  // namespace ddmod::game
