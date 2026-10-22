#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "overlay/surface.h"

namespace ddmod::fonts {

// Experimental: own glyph tables for the game's bitmap fonts (docs/re/text-rendering.md, "Glyph
// table, drawing and layout"). A `.fnt` glyph is the game's in-memory glyph, so after the game has
// loaded a font file the mod can replace the font's 256 glyph pointers with glyphs of its own, in
// full 16-bit color per pixel. Proof of concept for the target text renderer: [Experimental]
// GlyphTable names the fonts, the glyphs come from the font chain's face with a vertical color
// gradient, and GlyphTableLinePitch changes the font's line height (the layouts' line pitch).

// One glyph before it is turned into the game's layout. `pixels` holds width * height RGB565
// values row by row; 0 = transparent.
struct PixelGlyph {
    int width = 0;
    int height = 0;
    int originX = 0;  // from the pen position to the left edge
    int originY = 0;  // the game draws at pen y minus this value (bitmap fonts: minus the top row)
    int advance = 0;
    std::uint16_t character = 0;
    std::vector<std::uint16_t> pixels;
};

// Game glyph (0x16 bytes) followed by its sprite: u32 total size, u32 offset of the pixel block,
// u16 width, u16 height, per row u16 span count, pointer to the row's first pixel, spans (u16 x,
// u16 length), u16 span count again; then the pixels of all spans. The row pointers are absolute,
// so the glyph is built in its final memory and must not be moved.
std::unique_ptr<std::uint8_t[]> BuildGameGlyph(const PixelGlyph& glyph);

// Offsets in the game's glyph and sprite.
inline constexpr std::size_t kGlyphSprite = 0x16;

// RGB565 of `color` blended over `background` by `coverage` (0-255); never 0, which the game
// treats as transparent.
std::uint16_t ShadePixel(overlay::Rgb color, overlay::Rgb background, std::uint8_t coverage);

// Font names from [Experimental] GlyphTable ("fontn1, fontblack1"), lower case; empty = off.
std::vector<std::string> ParseFontList(std::string_view text);

// File stem of a game font path ("fonts\\fontn1.fnt" -> "fontn1"), lower case.
std::string FontStem(std::string_view path);

// Start of the game's font file loader (__thiscall on the font object, file path as the stack
// argument, returns the font object).
inline constexpr std::string_view kLoadFontFilePattern =
    "53 56 57 8B F1 E8 ?? ?? ?? ?? 8B 5C 24 10 68 ?? ?? ?? ?? 53 E8 ?? ?? ?? ?? 8B F8 83 C4 08 85 FF 74 ?? 57 8B CE "
    "E8 ?? ?? ?? ?? 57 8B CE 83 F8 01";
// RVA of the match in the verified 1.0062A build (fallback only, rule 8).
inline constexpr std::uint32_t kLoadFontFileRva = 0x94440;

// True if [Experimental] GlyphTable names at least one font.
bool GlyphTableConfigured();

// True if `font` is a game font object whose glyphs the mod replaced (spacing.cpp spaces those
// by their own advance).
bool IsOwnGlyphFont(const void* font);

// Hooks the font file loader. Must run in DllMain (fonts are loaded before the game window
// exists), after hooks::Init and game::IdentifyExe.
bool InstallGlyphTable();

}  // namespace ddmod::fonts
