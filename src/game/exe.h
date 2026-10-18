#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace ddmod::game {

// Identity of the host executable, established once at startup by hashing the file on disk.
struct ExeInfo {
    std::string sha256;           // upper-case hex; empty if the file could not be read
    const char* build = nullptr;  // name of the verified build, nullptr if unknown
    bool verified = false;        // hash matches a build listed in docs/re/game-version.md
};

// Hashes `exe` and compares it against the known builds. Logs the result.
void IdentifyExe(const std::filesystem::path& exe);
const ExeInfo& Exe();

// A code/data location found by signature, with an optional fixed fallback (rule 8).
struct Signature {
    std::string_view name;
    std::string_view pattern;
    // Byte offset from the start of the match to the address the caller wants.
    std::ptrdiff_t offset = 0;
    // RVA in the verified build; used only if the scan fails AND the EXE hash is verified.
    std::optional<std::uint32_t> fallbackRva;
    bool code = true;  // false: search all sections (strings, tables)
};

// Resolves `sig` in div.exe. Returns 0 if neither the scan nor an allowed fallback worked.
std::uintptr_t Resolve(const Signature& sig);

}  // namespace ddmod::game
