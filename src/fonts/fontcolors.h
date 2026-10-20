#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace ddmod::fonts {

// Text colors of the game's GDI fonts (docs/re/text-rendering.md, "Colors"). The game describes
// each of its 61 GDI fonts with a text color and an edge color and maps the 65 gray levels of a
// glyph linearly from edge (level 0) to text (level 64). Its built-in colors are rough stand-ins
// (pure yellow instead of the bitmap fonts' gold, gray edges on paper); [Fonts] Colors=original
// replaces them with the colors of the matching bitmap font.
enum class ColorMode {
    Game,      // the game's built-in GDI colors
    Original,  // the colors of the bitmap font with the same index (default)
};

std::optional<ColorMode> ParseColorMode(std::string_view text);

struct Rgb8 {
    std::uint8_t r, g, b;
    friend bool operator==(const Rgb8&, const Rgb8&) = default;
};

// The two colors of one GDI font: the text color and the edge color its gray ramp starts from.
struct FontColors {
    Rgb8 text;
    Rgb8 edge;
    friend bool operator==(const FontColors&, const FontColors&) = default;
};

// One font as the game describes it and the colors ddmod gives it. Index = position in the game's
// creation order (same order as the bitmap files, text-rendering.md).
struct FontColorEntry {
    std::string_view file;  // bitmap font of the same index, for the log
    int height;             // the game's height for this GDI font
    FontColors game;        // the game's built-in colors
    FontColors original;    // measured from the bitmap font (dominant text color, edge it shades to)
};

inline constexpr std::size_t kGameFontCount = 61;
const std::array<FontColorEntry, kGameFontCount>& FontColorTable();

// Colors for the font the game describes as the `index`-th one, or nullopt if the call does not
// match the expected font (other EXE, a fontlist.txt, index out of range) - then the game's own
// colors stay.
std::optional<FontColors> OriginalColors(std::size_t index, int height, const FontColors& game);

// The game converts the descriptor's 8-bit channels to RGB565 by c * 31 / 255 (green c * 63 / 255),
// rounding down, so most 8-bit colors lose one step. Returns the smallest 8-bit value per channel
// that the game turns into the same 565 value as `visible` (an expanded 565 color keeps exactly its
// own 565 value).
Rgb8 ToDescriptor(const Rgb8& visible);

// Start of the game's "add GDI font descriptor" function (__thiscall on the font list; stack
// arguments: two face names, height, text r, g, b, edge r, g, b - the color bytes as dwords).
inline constexpr std::string_view kAddFontDescriptorPattern =
    "51 53 55 56 57 6A 44 8B E9 E8 ?? ?? ?? ?? 33 DB 83 C4 04 3B C3 74 ?? B9 0F 00 00 00 89 48 18 89 58 14 88 58 04 "
    "89 48 34 89 58 30 88 58 20 8B F0 EB 02 33 F6 8B 54 24 18 89 74 24 10 3B D3";
// RVA of the match in the verified 1.0062A build (fallback only, rule 8).
inline constexpr std::uint32_t kAddFontDescriptorRva = 0x96670;

// Hooks the descriptor function according to [Fonts] Colors (nothing for "game"). Needs the font
// mode (FontHook=1); must run in DllMain like the font mode, after hooks::Init and
// game::IdentifyExe.
bool InstallFontColors();

}  // namespace ddmod::fonts
