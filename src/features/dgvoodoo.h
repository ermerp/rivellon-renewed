#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace ddmod::features {

// Settings the mod keeps in dgVoodoo2's config. dgVoodoo reads dgVoodoo.conf next to its
// DDraw.dll when the renderer plugin loads it - after the mod's DllMain - so the mod rewrites
// single keys of the game folder's dgVoodoo.conf just before (docs/re/environment.md):
// - [DgVoodoo] Watermark: dgVoodooWatermark in [DirectX] (on in dgVoodoo's default config);
//   0 = off (default), 1 = on, keep.
// - [DgVoodoo] FullScreen: FullscreenAttributes in [GeneralExt]; borderless (default) = "fake"
//   (a screen-size window without a frame, which stays one after Alt+Tab), exclusive = empty
//   (dgVoodoo's own full screen), keep.
// No dgVoodoo.conf (Steam Deck, no dgVoodoo) = nothing to do.

enum class ConfEdit {
    Changed,    // `text` holds the file with the new value
    Unchanged,  // the value is already the wanted one
    NotFound,   // no such key in the section
};

// Sets the value of `key` in `section` of the dgVoodoo.conf text `text` (names without case),
// keeping everything else (alignment, comments, line endings) as it is. An empty value leaves
// "key = " as dgVoodoo writes it.
ConfEdit SetConfValue(std::string& text, std::string_view section, std::string_view key, std::string_view value);

// Applies [DgVoodoo] to `conf` (normally ModDir() / "dgVoodoo.conf") and logs the outcome. Call
// from DllMain, before the renderer plugin loads dgVoodoo.
void ApplyDgVoodooSettings(const std::filesystem::path& conf);

}  // namespace ddmod::features
