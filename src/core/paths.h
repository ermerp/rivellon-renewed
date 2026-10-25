#pragma once

#include <filesystem>
#include <string>

#include <windows.h>

namespace ddmod::paths {

// Remembers the plugin module; must be called first (from DllMain).
void Init(HMODULE self);

// Stem of every file the mod puts into the game folder (CMake MOD_FILE_STEM).
inline constexpr const wchar_t* kFileStem = L"" DDMOD_FILE_STEM;

// Folder that contains RivellonRenewed.asi (and RivellonRenewed.ini / RivellonRenewed.log).
const std::filesystem::path& ModDir();

// ModDir() / "RivellonRenewed.ini" and ModDir() / "RivellonRenewed.log".
std::filesystem::path ConfigFile();
std::filesystem::path LogFile();

// Full path of the host executable (div.exe).
const std::filesystem::path& ExePath();

// UTF-8 form of a path, for log output.
std::string ToUtf8(const std::filesystem::path& path);

}  // namespace ddmod::paths
