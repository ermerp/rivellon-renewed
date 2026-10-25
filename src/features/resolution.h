#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace ddmod::features {

// Resolution chosen by the mod (docs/re/config-files.md "Resolution at startup"). The game takes
// its screen size from settings +0x38/+0x3C right after each config file read and switches the
// renderer to it; in full screen the size must be one of the renderer's modes, otherwise the game
// ends with "Can't change resolution". The mod sets both fields after each read, so the game
// itself carries the size everywhere; before the game writes config.div it puts the file's values
// back, so config.div stays as it was (uninstalling leaves the old resolution).
//
// [Display] Resolution: auto = the largest display mode of the primary monitor that fits on it,
// no taller than AutoMaxHeight lines (0 = no limit), the monitor's aspect ratio first (default);
// config = as config.div says; <width>x<height> = that size if the monitor offers it.

// Width and height fields of the settings object (dwords).
inline constexpr std::size_t kWidthField = 0x38;
inline constexpr std::size_t kHeightField = 0x3C;
// Default for [Display] AutoMaxHeight: above 1080 lines the game's interface gets small.
inline constexpr int kDefaultAutoMaxHeight = 1080;

struct Size {
    int width = 0;
    int height = 0;
    friend bool operator==(const Size&, const Size&) = default;
};

enum class ResolutionMode { Auto, Config, Fixed };

struct ResolutionSetting {
    ResolutionMode mode = ResolutionMode::Auto;
    Size fixed;  // for Fixed
};

// Parses [Display] Resolution ("auto", "config", "1920x1080"; case and blanks ignored). Invalid
// text gives Auto and `valid` = false.
ResolutionSetting ParseResolution(std::string_view text, bool& valid);

// The auto choice: among `modes`, those at least 640 wide that fit on `monitor` and are at most
// `maxHeight` tall (0 = no limit); the largest (by area) with the monitor's aspect ratio (within
// 1 %), else the largest of them. nullopt if none fits.
std::optional<Size> ChooseAutoResolution(Size monitor, std::span<const Size> modes, int maxHeight);

// Hooks `configReader` and `configWriter` (may be null) so that the settings hold `size` after
// every read and the files' values while the game writes config.div. hooks::Init must have
// succeeded. Exposed for the unit tests.
bool InstallResolutionAt(void* configReader, void* configWriter, Size size);

// Applies [Display]: picks the size and registers with the config file hooks (game::
// InstallConfigHooks puts them in afterwards). Must run in DllMain, before
// game::InstallConfigHooks. False if the mod leaves the resolution alone.
bool InstallResolution();

// Logs whether the renderer plugin offers the size the mod chose (its mode list is only valid
// once DllSlashedInit has run, so this runs at the deferred start).
void CheckRendererModes();

}  // namespace ddmod::features
