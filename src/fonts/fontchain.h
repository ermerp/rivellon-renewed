#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ddmod::fonts {

// One entry of the font chain ([Fonts] Chain in RivellonRenewed.ini): a face name and the bundled
// font file it comes from (loaded for this process only). Installed system fonts are not used
// (CLAUDE.md rule 13: the same look on Windows and Proton).
struct FontEntry {
    std::wstring face;
    std::filesystem::path file;  // empty = no file given - Resolve skips such an entry
};

// Parses "DejaVu Sans Condensed @ RivellonRenewed\fonts\DejaVuSansCondensed-Bold.ttf, ...":
// entries are separated by ',', the file follows '@' and is relative to `baseDir` unless
// absolute. Empty entries are skipped.
std::vector<FontEntry> ParseChain(std::string_view text, const std::filesystem::path& baseDir);

// Tries the entries in order and returns the first one whose file holds the named face (read from
// the file itself, text::HoldsFace); the file is then registered with AddFontResourceExW
// (FR_PRIVATE) and stays registered for the process (the GDI renderer path uses it by name). An
// entry without a file is skipped with a warning. Logs every step and the face that is used.
// nullopt if no entry works.
std::optional<FontEntry> Resolve(const std::vector<FontEntry>& chain);

// [Fonts] Chain from RivellonRenewed.ini (default kDefaultChain, files relative to the mod
// folder), resolved for a bold face on first use; later calls return the same result. Thread-safe.
const std::optional<FontEntry>& Configured();

// The default chain: the bundled DejaVu Sans Condensed Bold.
constexpr const char* kDefaultChain = "DejaVu Sans Condensed @ " DDMOD_FILE_STEM "\\fonts\\DejaVuSansCondensed-Bold.ttf";

}  // namespace ddmod::fonts
