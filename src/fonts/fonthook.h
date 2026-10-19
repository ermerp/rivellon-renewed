#pragma once

#include <string_view>

#include "game/exe.h"

namespace ddmod::fonts {

inline constexpr int kKeepCharset = -1;     // leave the game's lfCharSet (ANSI_CHARSET) unchanged
inline constexpr int kCharsetInvalid = -2;  // unrecognized [Fonts] Charset value

// lfCharSet for the game's fonts from [Fonts] Charset: "1251" -> RUSSIAN_CHARSET, "1252" ->
// ANSI_CHARSET, "auto" (or empty) -> RUSSIAN_CHARSET for the Russian edition, else kKeepCharset.
// The game passes its text bytes to GetGlyphOutlineA, which maps them through the charset's code
// page (docs/re/text-rendering.md).
int CharsetFor(std::string_view setting, game::Edition edition);

// Font hook for the game's GDI text path (use8bitfont 0, docs/re/text-rendering.md): the game
// creates one GDI font per bitmap font, all as "SimSun", through CreateFontIndirectA (and one
// "Arial Bold" through CreateFontA). The hook replaces the face of every call made from div.exe
// with the font chain's face ([Fonts] Chain) and scales the height by [Fonts] GameFontSize
// (percent). With `replace` false it only logs (LogLevel=debug). The GDI path itself is switched
// on in memory by fontmode.h.
//
// The game creates its fonts within its first ~200 ms, so Install must run during startup
// (DllMain, after hooks::Init). The font chain is resolved on the first call, on the game thread.
bool Install(bool replace);

}  // namespace ddmod::fonts
