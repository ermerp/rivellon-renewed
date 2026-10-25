#pragma once

#include <filesystem>
#include <memory>

#include "overlay/glyphs.h"

namespace ddmod::overlay {

// The game's own bitmap fonts (fonts/*.fnt, format ML_FONT_01, docs/re/file-formats.md), read
// from the installation at runtime - nothing is shipped. Glyphs keep their pre-shaded colors
// (Glyph::colors). Characters are looked up through `codePage` (1252 for DE/EN/FR, 1251 for RU).
// Returns nullptr if the file cannot be read or is not ML_FONT_01.
std::unique_ptr<GlyphSource> CreateFntGlyphSource(const std::filesystem::path& file, unsigned codePage = 1252);

}  // namespace ddmod::overlay
