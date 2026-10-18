#pragma once

#include <string_view>

#include <windows.h>

#include "input/keys.h"

namespace ddmod::input {

// ddmod's hotkeys, delivered through a subclass of the game's window procedure (docs/re/input.md).
// A key-down that matches a registered hotkey (exact modifiers, no auto-repeat) runs its handler
// on the game's window thread; that key-down, its repeats, its key-up and the character message
// it produces are not passed to the game. Every other message reaches the game unchanged.

using HotkeyHandler = void (*)();

// Registers a hotkey from [Keys] in ddmod.ini, falling back to `fallback` if the key is missing.
// An empty value or "None" leaves the hotkey unbound; invalid text is logged and also leaves it
// unbound. Call before Install.
void RegisterHotkey(const char* iniKey, const char* fallback, HotkeyHandler handler);

// Subclasses `window`. `onDestroyed` runs on the window thread after the window is gone,
// so the caller can wait for a new one. Returns false if the subclass could not be installed.
bool Install(HWND window, void (*onDestroyed)());

// Restores the original window procedure if ours is still the outermost one.
void Uninstall();

}  // namespace ddmod::input
