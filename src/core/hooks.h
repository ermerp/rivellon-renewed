#pragma once

#include <string_view>

namespace ddmod::hooks {

// Thin, named wrapper around MinHook. Every hook gets a name for log output and lookup;
// all calls return false on failure and log the MinHook status.

bool Init();
// Disables and removes all hooks. Only call while the game code is not running into them
// (not during process termination).
void Shutdown();

bool CreateRaw(std::string_view name, void* target, void* detour, void** original);

// Typed helper: `original` receives the trampoline that calls the unhooked function.
template <typename Fn>
bool Create(std::string_view name, Fn* target, Fn* detour, Fn** original) {
    return CreateRaw(name, reinterpret_cast<void*>(target), reinterpret_cast<void*>(detour),
                     reinterpret_cast<void**>(original));
}

bool Enable(std::string_view name);
bool Disable(std::string_view name);
bool Remove(std::string_view name);
bool IsEnabled(std::string_view name);

// Applies all pending enables in one go (MinHook suspends the other threads once).
bool EnableAll();

}  // namespace ddmod::hooks
