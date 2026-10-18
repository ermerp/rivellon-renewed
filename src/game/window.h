#pragma once

#include <windows.h>

namespace ddmod::game {

// The game window is created by the renderer plugin (slash*.dll) long after ddmod is loaded.
// WatchForMainWindow starts a background thread that polls for it and calls `onFound` once,
// on that thread, as soon as it exists (docs/re/input.md). Returns false if no thread could
// be started. Safe to call from DllMain: the thread only runs after the loader lock is free.
using WindowCallback = void (*)(HWND window);
bool WatchForMainWindow(WindowCallback onFound);

// The game window of this process, or nullptr if it does not exist (yet).
HWND FindMainWindow();

}  // namespace ddmod::game
