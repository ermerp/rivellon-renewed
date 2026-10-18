#pragma once

namespace ddmod::fonts {

// Font hook for the game's GDI text path (use8bitfont 0, docs/re/text-rendering.md): the game
// creates one GDI font per bitmap font, all as "SimSun", through CreateFontIndirectA (and one
// "Arial Bold" through CreateFontA). The hook replaces the face of every call made from div.exe
// with the font chain's face ([Fonts] Chain) and scales the height by [Fonts] GameFontSize
// (percent). With `replace` false it only logs (LogLevel=debug).
//
// The game creates its fonts within its first ~200 ms, so Install must run during startup
// (DllMain, after hooks::Init). The font chain is resolved on the first call, on the game thread.
bool Install(bool replace);

}  // namespace ddmod::fonts
