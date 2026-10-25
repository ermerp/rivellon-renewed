#pragma once

#include <cstddef>
#include <optional>
#include <string_view>

namespace ddmod::features {

// Ground fog on by default (docs/decisions.md). Steam's config.div switches it off with the line
// "no fog", which the game stores in its settings object (docs/re/config-files.md). The game
// reads config.lcl and config.div with one function per file; the hook (game/configfile.h) sets
// the flag after each call, so config.div is not needed for it. [Features] Fog: 1 = fog on
// (default), 0 = fog off, keep = as config.div says. The game's picture settings switch it while
// the game runs; when the game writes config.div on exit, the mod takes their last state over
// into [Features] Fog.

// "no fog" flag in the settings object (dword, nonzero = no fog).
inline constexpr std::size_t kNoFogField = 0xA4;

// [Features] Fog: true = on, false = off, nullopt = keep (config.div decides). Invalid text gives
// the default (on) and `valid` = false.
std::optional<bool> ParseFog(std::string_view text, bool& valid);

// Hooks `configReader` so that the "no fog" flag of its object matches `fogOn` after every call,
// and `configWriter` (may be null) so that the flag's state before writing is reported to
// `onWrite` (fog on = true). hooks::Init must have succeeded. Exposed for the unit tests.
bool InstallFogAt(void* configReader, void* configWriter, bool fogOn, void (*onWrite)(bool fogOn));

// Applies [Features] Fog: registers with the config file hooks (game::InstallConfigHooks puts
// them in afterwards). Must run in DllMain, before game::InstallConfigHooks.
bool InstallFog();

}  // namespace ddmod::features
