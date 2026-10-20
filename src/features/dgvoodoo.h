#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace ddmod::features {

// dgVoodoo2's watermark (logo in the bottom corner, on in dgVoodoo's default config). dgVoodoo
// reads dgVoodoo.conf next to its DDraw.dll when the renderer plugin loads it - after the mod's
// DllMain - so the mod sets "dgVoodooWatermark" in [DirectX] of the game folder's dgVoodoo.conf
// just before (docs/re/environment.md, "Watermark"). [DgVoodoo] Watermark: 0 = off (default),
// 1 = on, keep = the file is not touched. No dgVoodoo.conf (Steam Deck, no dgVoodoo) = nothing
// to do.

enum class WatermarkEdit {
    Changed,    // `text` holds the file with the new value
    Unchanged,  // the value is already the wanted one
    NotFound,   // no dgVoodooWatermark key in [DirectX]
};

// Sets the value of dgVoodooWatermark in [DirectX] of the dgVoodoo.conf text `text` to "true" or
// "false", keeping everything else (alignment, comments, line endings) as it is.
WatermarkEdit SetWatermark(std::string& text, bool on);

// Applies [DgVoodoo] Watermark to `conf` (normally ModDir() / "dgVoodoo.conf") and logs the
// outcome. Call from DllMain, before the renderer plugin loads dgVoodoo.
void ApplyWatermarkSetting(const std::filesystem::path& conf);

}  // namespace ddmod::features
