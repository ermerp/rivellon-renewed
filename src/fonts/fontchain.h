#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ddmod::fonts {

// One entry of the font chain ([Fonts] Chain in RivellonRenewed.ini): a face name, optionally
// with a font file that is loaded for this process only before the face is tried.
struct FontEntry {
    std::wstring face;
    std::filesystem::path file;  // empty = installed font
};

// Parses "Tahoma, DejaVu Sans Condensed @ RivellonRenewed\fonts\DejaVuSansCondensed-Bold.ttf":
// entries are separated by ',', an optional file follows '@' and is relative to `baseDir` unless
// absolute. Empty entries are skipped.
std::vector<FontEntry> ParseChain(std::string_view text, const std::filesystem::path& baseDir);

// True if a value name of the registry font list ("Tahoma Bold (TrueType)", "Cambria & Cambria
// Math (TrueType)") names the font with this full name ("Tahoma Bold"); case-insensitive.
bool FontListNameMatches(std::wstring_view valueName, std::wstring_view fullName);

// Full name ("Tahoma Bold") and file of the installed font GDI selects for `face`, from the
// registry font list (HKLM, then HKCU; relative files lie in %WINDIR%\Fonts - same on Wine).
// `file` is empty if no entry names it, `fullName` empty if GDI has no outline metrics.
struct InstalledFont {
    std::wstring fullName;
    std::filesystem::path file;
};
InstalledFont FindInstalledFont(const std::wstring& face, bool bold);

// Tries the entries in order and returns the first face GDI really provides: its file (if any)
// is registered with AddFontResourceExW(FR_PRIVATE) and stays registered for the process; the
// face counts as available only if GDI selects exactly that face (GetTextFaceW) instead of a
// substitute. Logs every step and the face that is used (for an installed font with its full
// name and file). nullopt if no entry works.
std::optional<FontEntry> Resolve(const std::vector<FontEntry>& chain, bool bold);

// [Fonts] Chain from RivellonRenewed.ini (default kDefaultChain, files relative to the mod
// folder), resolved for a bold face on first use; later calls return the same result. Thread-safe.
const std::optional<FontEntry>& Configured();

// The default chain: Tahoma (system font on Windows; Proton brings its own), then the bundled
// DejaVu Sans Condensed Bold.
constexpr const char* kDefaultChain =
    "Tahoma, DejaVu Sans Condensed @ " DDMOD_FILE_STEM "\\fonts\\DejaVuSansCondensed-Bold.ttf";

}  // namespace ddmod::fonts
