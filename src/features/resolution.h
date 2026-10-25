#pragma once

#include <cstddef>
#include <cstdint>
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
// "resolution switch #" (keyword 58), read only by the video player.
inline constexpr std::size_t kResolutionSwitchField = 0xD0;
// Start of the video player in div.exe (__cdecl: movie file, "may switch to the movie's size",
// "switch back afterwards", one more flag). Steam lacks the first startup movie (static\cdv.bik),
// the only call that may switch; all others pass 0 for both.
inline constexpr std::string_view kMoviePlayerPattern =
    "6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 81 EC 9C 00 00 00 53 55 56 57 A1 ?? ?? ?? ?? 33 C4 50 "
    "8D 84 24 B0 00 00 00 64 A3 00 00 00 00 33 C0 33 DB 39 9C 24 CC 00 00 00 0F 95 C0";
inline constexpr std::uint32_t kMoviePlayerRva = 0xD1D0;
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

// Full-screen movies: the game's video player switches the renderer to the movie's 640x480 (and
// back) only if "resolution switch" is set, which Steam's config files do not do; otherwise it draws
// the movie centered in the full frame, smaller the higher the resolution. [Display]
// FullScreenMovies=1 (default) sets the field after every read (config.div keeps its own value)
// and lets every movie switch and switch back (hook on the video player); dgVoodoo2 or gamescope
// then scale the 640x480 picture to the screen. 0 = as in the original.
bool InstallFullScreenMovies();
// Same with fixed reader/writer addresses (unit tests).
bool InstallFullScreenMoviesAt(void* configReader, void* configWriter);

// Logs whether the renderer plugin offers the size the mod chose (its mode list is only valid
// once DllSlashedInit has run, so this runs at the deferred start).
void CheckRendererModes();

}  // namespace ddmod::features
