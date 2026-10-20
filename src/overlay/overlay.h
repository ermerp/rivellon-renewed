#pragma once

namespace ddmod::overlay {

// Minimal overlay ("Rivellon Renewed <version>" + FPS) drawn at the end of every frame
// (docs/re/overlay.md). Hooks DllSlashedEndFrame of the loaded renderer plugin; only slash1.dll
// (Direct3D) has a backend so far. Reads [Overlay] from RivellonRenewed.ini. Call from the
// deferred start, after hooks::Init. Returns false if the overlay could not be installed (logged).
bool Install();

}  // namespace ddmod::overlay
