#pragma once

#include <filesystem>
#include <format>
#include <string_view>

namespace ddmod::log {

enum class Level { Debug = 0, Info, Warn, Error, Off };

// Parses "debug", "info", "warn", "error", "off" (case-insensitive); unknown values give `fallback`.
Level ParseLevel(std::string_view text, Level fallback);

// "ddmod.log" -> "ddmod.prev.log": where Open keeps the log of the previous start.
std::filesystem::path PreviousFile(const std::filesystem::path& file);

// Opens a new log file; an existing one is first renamed to PreviousFile(file), replacing the
// older copy. Messages below `minLevel` are dropped.
void Open(const std::filesystem::path& file, Level minLevel);
void Close();

void SetLevel(Level minLevel);
bool Enabled(Level level);

void Write(Level level, std::string_view message);

template <typename... Args>
void Debug(std::format_string<Args...> fmt, Args&&... args) {
    if (Enabled(Level::Debug)) Write(Level::Debug, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void Info(std::format_string<Args...> fmt, Args&&... args) {
    if (Enabled(Level::Info)) Write(Level::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void Warn(std::format_string<Args...> fmt, Args&&... args) {
    if (Enabled(Level::Warn)) Write(Level::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void Error(std::format_string<Args...> fmt, Args&&... args) {
    if (Enabled(Level::Error)) Write(Level::Error, std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace ddmod::log
