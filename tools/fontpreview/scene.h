#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fontpreview {

// One row of a scene sheet: a caption and, below it, the scene at 1x with a 2x copy beside it.
struct SceneRow {
    std::wstring caption;
    bool warn = false;  // the candidate is out (no German umlauts, face not found)
    int width = 0, height = 0;
    std::vector<std::uint32_t> pixels;  // Xrgb8888; empty for a title row
};

// Renders the scene described by `spec` (UTF-8, format at the top of scene.cpp): real game lines
// on a crop of a real game screenshot, once per candidate font. GDI+ must be started.
bool RenderScene(const std::filesystem::path& spec, std::vector<SceneRow>& rows, std::wstring& error);

}  // namespace fontpreview
