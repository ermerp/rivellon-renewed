#include "core/config.h"

#include <algorithm>
#include <cctype>
#include <charconv>

#include <windows.h>

namespace ddmod::config {
namespace {

std::filesystem::path g_file;
bool g_exists = false;

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string Trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t");
    return s.substr(first, last - first + 1);
}

// Returns false if the key is missing (or empty).
bool Read(const char* section, const char* key, std::string& out) {
    if (!g_exists) return false;
    char buffer[1024];
    const std::string path = g_file.string();
    const DWORD len = GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), path.c_str());
    if (len == 0) return false;
    // Strip trailing comments ("value ; comment").
    std::string value(buffer, len);
    if (const auto pos = value.find(';'); pos != std::string::npos) value.resize(pos);
    out = Trim(value);
    return !out.empty();
}

}  // namespace

void Load(const std::filesystem::path& iniFile) {
    g_file = iniFile;
    std::error_code ec;
    g_exists = std::filesystem::is_regular_file(iniFile, ec);
}

bool Exists() { return g_exists; }

const std::filesystem::path& File() { return g_file; }

std::string GetString(const char* section, const char* key, const char* fallback) {
    std::string value;
    return Read(section, key, value) ? value : std::string(fallback);
}

int GetInt(const char* section, const char* key, int fallback) {
    std::string value;
    if (!Read(section, key, value)) return fallback;
    int result = fallback;
    const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
    return (ec == std::errc() && ptr == value.data() + value.size()) ? result : fallback;
}

bool GetBool(const char* section, const char* key, bool fallback) {
    std::string value;
    if (!Read(section, key, value)) return fallback;
    const std::string v = ToLower(value);
    if (v == "1" || v == "true" || v == "on" || v == "yes") return true;
    if (v == "0" || v == "false" || v == "off" || v == "no") return false;
    return fallback;
}

bool SetString(const char* section, const char* key, const char* value) {
    if (!g_exists) return false;
    return WritePrivateProfileStringA(section, key, value, g_file.string().c_str()) != FALSE;
}

}  // namespace ddmod::config
