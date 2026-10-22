#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace ddmod::fonts {

// Letter spacing of the game's GDI fonts (docs/re/text-rendering.md, "Pair spacing"). The game
// places each character by sliding it left towards the previous one until their pixels would
// touch; the bitmap fonts are drawn for that, a Windows font is not (a period after "1" merges with
// its foot). [Fonts] Spacing picks how the GDI fonts and the fonts with an own glyph table
// (glyphtable.h) are spaced; the original bitmap fonts always keep the game's.
enum class SpacingMode {
    Game,  // as the game computes it: characters may touch
    Gap,   // one pixel more where the game slid characters together (the game's test misses the
           // bottom row, so feet like in "Li" can still be covered)
    Font,  // the font's own advance, no sliding (default)
};

std::optional<SpacingMode> ParseSpacingMode(std::string_view text);

// Distance from the previous character's pen position to the next one's. `gameSpacing` is the
// game's result; `originX` and `advance` are the previous glyph's GDI metrics. The game draws a
// glyph at pen + originX, so the font's own spacing is `advance`; when nothing touches, the game
// returns originX + advance (its left bearing counted once more).
int AdjustSpacing(SpacingMode mode, int gameSpacing, int originX, int advance);

// Start of the game's pair spacing function (__thiscall on the font, previous and next character
// as stack arguments, returns the distance in pixels).
inline constexpr std::string_view kPairSpacingPattern =
    "83 EC 10 53 55 56 8B F1 8B 4C 24 20 8B 56 14 0F B6 C1 83 3C 82 00 8D 04 82 75 ?? 83 3E 00 74 ?? 51 8B CE E8";
// RVA of the match in the verified 1.0062A build (fallback only, rule 8).
inline constexpr std::uint32_t kPairSpacingRva = 0x94670;

// Hooks the pair spacing function according to [Fonts] Spacing (nothing for "game"). Needs the
// font mode (FontHook=1); call during startup after hooks::Init and game::IdentifyExe.
bool InstallSpacing();

}  // namespace ddmod::fonts
