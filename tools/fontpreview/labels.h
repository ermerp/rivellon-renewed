#pragma once

#include <filesystem>

namespace fontpreview {

// Menu label check: every capitalized text of a localized text.cmp (docs/re/file-formats.md) with
// its width in the original menu font (fontbig36.fnt) and in the menu family's face at the size
// the glyph renderer gives it, one line per text on stdout (UTF-8; game text - redirect into the
// scratchpad only). `faceDir` holds the bundled faces (assets/fonts). Returns false if a file
// cannot be read.
bool PrintLabelWidths(const std::filesystem::path& textCmp, const std::filesystem::path& fntDir,
                      const std::filesystem::path& faceDir, unsigned codePage);

}  // namespace fontpreview
