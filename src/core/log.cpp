#include "core/log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <string>

#include <windows.h>

namespace ddmod::log {
namespace {

std::mutex g_mutex;
std::FILE* g_file = nullptr;
Level g_minLevel = Level::Info;

const char* LevelName(Level level) {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
        default:           return "?    ";
    }
}

}  // namespace

Level ParseLevel(std::string_view text, Level fallback) {
    std::string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower == "debug") return Level::Debug;
    if (lower == "info") return Level::Info;
    if (lower == "warn" || lower == "warning") return Level::Warn;
    if (lower == "error") return Level::Error;
    if (lower == "off" || lower == "none") return Level::Off;
    return fallback;
}

std::filesystem::path PreviousFile(const std::filesystem::path& file) {
    std::filesystem::path prev = file;
    prev.replace_filename(file.stem().wstring() + L".prev" + file.extension().wstring());
    return prev;
}

void Open(const std::filesystem::path& file, Level minLevel) {
    std::lock_guard lock(g_mutex);
    if (g_file) std::fclose(g_file);
    // Keep the log of the previous start (testers often restart the game before sending the log).
    MoveFileExW(file.c_str(), PreviousFile(file).c_str(), MOVEFILE_REPLACE_EXISTING);
    g_file = _wfsopen(file.c_str(), L"w", _SH_DENYWR);  // others may read the log while the game runs
    g_minLevel = minLevel;
}

void Close() {
    std::lock_guard lock(g_mutex);
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

void SetLevel(Level minLevel) {
    std::lock_guard lock(g_mutex);
    g_minLevel = minLevel;
}

bool Enabled(Level level) {
    return level >= g_minLevel && level != Level::Off;
}

void Write(Level level, std::string_view message) {
    if (!Enabled(level)) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::lock_guard lock(g_mutex);
    if (!g_file) return;
    std::fprintf(g_file, "%04u-%02u-%02u %02u:%02u:%02u.%03u [%s] [%5lu] %.*s\n",
                 t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
                 LevelName(level), GetCurrentThreadId(),
                 static_cast<int>(message.size()), message.data());
    std::fflush(g_file);  // keep the log complete even if the game crashes
}

}  // namespace ddmod::log
