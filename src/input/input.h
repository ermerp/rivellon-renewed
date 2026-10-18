#pragma once

#include <windows.h>

#include "input/keys.h"

namespace ddmod::input {

// ddmod's hotkeys, delivered through a WH_GETMESSAGE hook on the game's window thread
// (docs/re/input.md). A key-down that matches a registered hotkey (exact modifiers, no
// auto-repeat) runs its handler on that thread. The key-down, its repeats and its key-up are
// turned into WM_NULL before the game's message loop sees them, so TranslateMessage creates no
// character and DefWindowProc no menu. Every other message reaches the game unchanged.

using HotkeyHandler = void (*)();

// Registers a hotkey from [Keys] in ddmod.ini, falling back to `fallback` if the key is missing.
// An empty value or "None" leaves the hotkey unbound; invalid text is logged and also leaves it
// unbound. Call before Install.
void RegisterHotkey(const char* iniKey, const char* fallback, HotkeyHandler handler);

// Hooks the message retrieval of the thread that owns `window`. Windows removes the hook when the
// thread that called Install exits, so that thread must stay alive. Returns false on failure.
bool Install(HWND window);

void Uninstall();

}  // namespace ddmod::input
