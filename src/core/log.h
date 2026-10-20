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

// "ddmod.log" -> "ddmod-logs": folder for the logs of older starts.
std::filesystem::path ArchiveDir(const std::filesystem::path& file);

// Archive file name from the first line of a log ("2026-10-20 17:11:06.504 ..." with stem "ddmod"
// -> "ddmod-20261020-171106.log"); empty if the line does not start with a timestamp.
std::wstring ArchiveName(std::wstring_view stem, std::string_view firstLine);

// Opens a new log file. An existing one is first renamed to PreviousFile(file); the copy that was
// there moves to ArchiveDir(file), named by its start time, where the newest `history` files are
// kept (0 = the older copy is replaced, nothing is archived). Messages below `minLevel` are dropped.
void Open(const std::filesystem::path& file, Level minLevel, int history = 0);
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
