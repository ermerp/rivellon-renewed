#pragma once

#include <windows.h>

namespace ddmod::game {

// Diagnostics for the game's text paths (docs/re/text-rendering.md): logs the characters each GDI
// font rasterizes (use8bitfont 0) and every bitmap font file (.fnt) the game opens (use8bitfont 1).
// The fonts themselves are logged by the font hook (fonts/fonthook.h).
// Only installed with LogLevel=debug, during startup, after hooks::Init.
bool InstallFontLog();

// Called by the font hook's GetGlyphOutlineA detour for every glyph the game rasterizes; does
// nothing unless InstallFontLog ran.
void LogGlyph(HDC dc, UINT ch);

}  // namespace ddmod::game
