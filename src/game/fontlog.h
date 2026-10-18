#pragma once

namespace ddmod::game {

// Diagnostics for the game's GDI font path (use8bitfont 0, docs/re/text-rendering.md): hooks
// CreateFontA and CreateFontIndirectA and logs face, height, weight and character set of every
// font the game creates. Only installed with LogLevel=debug. Call after hooks::Init.
bool InstallFontLog();

}  // namespace ddmod::game
