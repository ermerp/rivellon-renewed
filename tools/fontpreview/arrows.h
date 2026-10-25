#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "scene.h"

namespace fontpreview {

// Arrow sheet: the options pages' value line ("<  1.00  >") on a crop of a game screenshot, in
// the menu font's three states (fontbig36, highlight, gray) - once with the original glyphs, then
// once per own arrow variant (src/text/symbols.h) with the menu family's face and paint, each at
// 1x with a 3x copy beside it. `faceDir` holds the bundled faces (assets/fonts). GDI+ must be
// started.
bool RenderArrows(const std::filesystem::path& fntDir, const std::filesystem::path& faceDir,
                  const std::filesystem::path& background, int x, int y, int w, int h, std::vector<SceneRow>& rows,
                  std::wstring& error);

}  // namespace fontpreview
