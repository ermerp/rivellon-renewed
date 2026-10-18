#pragma once

#include <filesystem>

#include <windows.h>

namespace ddmod::paths {

// Remembers the plugin module; must be called first (from DllMain).
void Init(HMODULE self);

// Folder that contains ddmod.asi (and ddmod.ini / ddmod.log).
const std::filesystem::path& ModDir();

// Full path of the host executable (div.exe).
const std::filesystem::path& ExePath();

}  // namespace ddmod::paths
