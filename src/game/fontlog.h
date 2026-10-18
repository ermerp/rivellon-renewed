#pragma once

namespace ddmod::game {

// Diagnostics for the game's text paths (docs/re/text-rendering.md): logs the characters each GDI
// font rasterizes (GetGlyphOutlineA, use8bitfont 0) and every bitmap font file (.fnt) the game
// opens (use8bitfont 1). The fonts themselves are logged by the font hook (fonts/fonthook.h).
// Only installed with LogLevel=debug, during startup, after hooks::Init.
bool InstallFontLog();

}  // namespace ddmod::game
