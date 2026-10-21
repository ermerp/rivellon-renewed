#pragma once

#include <string_view>

namespace ddmod::overlay {

// Minimal overlay (status line "Rivellon Renewed <version> active" + optional FPS) drawn at the
// end of every frame (docs/re/overlay.md). Hooks DllSlashedEndFrame of the loaded renderer plugin;
// only slash1.dll (Direct3D) has a backend so far. Reads [Overlay] from RivellonRenewed.ini. Call
// from the deferred start, after hooks::Init. Returns false if the overlay could not be installed
// (logged).
bool Install();

// Switches the FPS line on or off (hotkey [Keys] ToggleFps). Safe to call from any thread, also
// before Install.
void ToggleFps();

// [Overlay] Status: banner = the status line for the first BannerSeconds after the first frame,
// always = all the time, off = never.
enum class StatusMode { Banner, Always, Off };

// Case-insensitive; false for unknown text.
bool ParseStatusMode(std::string_view text, StatusMode& mode);

// Whether the status line is shown `elapsedSeconds` after the overlay's first frame.
bool StatusShown(StatusMode mode, double elapsedSeconds, int bannerSeconds);

}  // namespace ddmod::overlay
