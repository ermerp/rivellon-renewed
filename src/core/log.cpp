#include "core/log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

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

std::filesystem::path ArchiveDir(const std::filesystem::path& file) {
    return file.parent_path() / file.stem() / L"logs";
}

std::wstring ArchiveName(std::wstring_view stem, std::string_view firstLine) {
    // '0' stands for a digit, everything else must match literally.
    constexpr std::string_view kPattern = "0000-00-00 00:00:00";
    if (firstLine.size() < kPattern.size()) return {};
    std::wstring digits;  // YYYYMMDDHHMMSS
    for (std::size_t i = 0; i < kPattern.size(); ++i) {
        const char c = firstLine[i];
        if (kPattern[i] != '0') {
            if (c != kPattern[i]) return {};
        } else if (c >= '0' && c <= '9') {
            digits += static_cast<wchar_t>(c);
        } else {
            return {};
        }
    }
    return std::wstring(stem) + L"-" + digits.substr(0, 8) + L"-" + digits.substr(8) + L".log";
}

namespace {

// The start time of a log: its first line, else the file's last write time in the same format.
std::string StartTime(const std::filesystem::path& file) {
    char buffer[32] = {};
    std::size_t read = 0;
    if (std::FILE* in = _wfsopen(file.c_str(), L"rb", _SH_DENYNO)) {
        read = std::fread(buffer, 1, sizeof(buffer) - 1, in);
        std::fclose(in);
    }
    std::string line(buffer, read);
    if (!ArchiveName(L"x", line).empty()) return line;
    WIN32_FILE_ATTRIBUTE_DATA data;
    FILETIME local;
    SYSTEMTIME t;
    if (!GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &data) ||
        !FileTimeToLocalFileTime(&data.ftLastWriteTime, &local) || !FileTimeToSystemTime(&local, &t)) {
        return {};
    }
    return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                       t.wSecond);
}

// Moves `prev` into the archive folder and deletes all but the newest `history` archived logs.
void Archive(const std::filesystem::path& file, const std::filesystem::path& prev, int history) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (history <= 0 || !fs::exists(prev, ec)) return;
    const fs::path dir = ArchiveDir(file);
    fs::create_directories(dir, ec);
    const std::wstring stem = file.stem().wstring();
    std::wstring name = ArchiveName(stem, StartTime(prev));
    if (name.empty()) name = stem + L"-00000000-000000.log";
    fs::path target = dir / name;
    for (int n = 2; fs::exists(target, ec) && n < 100; ++n) {
        target = dir / (fs::path(name).stem().wstring() + std::format(L"-{}", n) + L".log");
    }
    if (!MoveFileExW(prev.c_str(), target.c_str(), 0)) return;

    std::vector<fs::path> archived;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        const std::wstring fileName = entry.path().filename().wstring();
        if (entry.is_regular_file(ec) && fileName.starts_with(stem + L"-") && entry.path().extension() == L".log") {
            archived.push_back(entry.path());
        }
    }
    if (archived.size() <= static_cast<std::size_t>(history)) return;
    // The names start with the start time, so the oldest sort first.
    std::sort(archived.begin(), archived.end());
    for (std::size_t i = 0; i + history < archived.size(); ++i) fs::remove(archived[i], ec);
}

}  // namespace

void Open(const std::filesystem::path& file, Level minLevel, int history) {
    std::lock_guard lock(g_mutex);
    if (g_file) std::fclose(g_file);
    // Keep the logs of earlier starts (testers often restart the game before sending the log).
    const std::filesystem::path prev = PreviousFile(file);
    Archive(file, prev, history);
    MoveFileExW(file.c_str(), prev.c_str(), MOVEFILE_REPLACE_EXISTING);
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
