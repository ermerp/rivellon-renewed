#pragma once

#include <memory>

#include <windows.h>

#include "overlay/surface.h"

namespace ddmod::overlay {

// Gives the overlay renderer access to the finished frame (docs/re/overlay.md). Called once per
// frame on the game's render thread, right before the frame is presented.
class Backend {
public:
    virtual ~Backend() = default;
    virtual const char* Name() const = 0;
    // Locks the frame; false = nothing to draw into this frame (renderer not ready, surface lost,
    // unsupported format). EndFrame is only called after BeginFrame returned true.
    virtual bool BeginFrame(SurfaceView& view) = 0;
    virtual void EndFrame() = 0;
};

// Direct3D 6 renderer (slash1.dll): draws through the game's CD3D6VideoMem object
// (docs/re/renderer-d3d6.md). Returns nullptr if the module is not the verified build.
std::unique_ptr<Backend> CreateD3d6Backend(HMODULE slash1);

}  // namespace ddmod::overlay
