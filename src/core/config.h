#pragma once

#include <filesystem>
#include <string>

namespace ddmod::config {

// Selects the INI file; values are read on demand (no caching yet).
void Load(const std::filesystem::path& iniFile);

bool Exists();
const std::filesystem::path& File();

std::string GetString(const char* section, const char* key, const char* fallback);
int GetInt(const char* section, const char* key, int fallback);
// Accepts 1/0, true/false, on/off, yes/no (case-insensitive).
bool GetBool(const char* section, const char* key, bool fallback);

// Writes one value into the INI file (the line of an existing key is replaced, comment lines stay);
// false if there is no INI file or it cannot be written.
bool SetString(const char* section, const char* key, const char* value);

}  // namespace ddmod::config
