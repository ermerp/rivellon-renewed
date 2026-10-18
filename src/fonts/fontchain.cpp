#include "fonts/fontchain.h"

#include <algorithm>
#include <mutex>

#include <windows.h>

#include "core/config.h"
#include "core/log.h"
#include "core/paths.h"

namespace ddmod::fonts {
namespace {

std::wstring Trim(std::wstring_view text) {
    const auto first = text.find_first_not_of(L" \t");
    if (first == std::wstring_view::npos) return {};
    const auto last = text.find_last_not_of(L" \t");
    return std::wstring(text.substr(first, last - first + 1));
}

std::wstring Widen(std::string_view text) {
    if (text.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), len);
    return wide;
}

// The face GDI actually selects for `face` (it substitutes silently if the face is missing).
std::wstring SelectedFace(const std::wstring& face, bool bold) {
    std::wstring result;
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT font = CreateFontW(-16, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face.c_str());
    if (dc && font) {
        const HGDIOBJ previous = SelectObject(dc, font);
        wchar_t actual[LF_FACESIZE] = {};
        if (GetTextFaceW(dc, LF_FACESIZE, actual) > 0) result = actual;
        SelectObject(dc, previous);
    }
    if (font) DeleteObject(font);
    if (dc) DeleteDC(dc);
    return result;
}

}  // namespace

std::vector<FontEntry> ParseChain(std::string_view text, const std::filesystem::path& baseDir) {
    std::vector<FontEntry> chain;
    const std::wstring wide = Widen(text);
    std::size_t start = 0;
    while (start <= wide.size()) {
        const auto end = std::min(wide.find(L',', start), wide.size());
        const std::wstring_view item(wide.data() + start, end - start);
        const auto at = item.find(L'@');
        FontEntry entry;
        entry.face = Trim(item.substr(0, at));
        if (at != std::wstring_view::npos) {
            const std::wstring file = Trim(item.substr(at + 1));
            if (!file.empty()) {
                entry.file = file;
                if (entry.file.is_relative()) entry.file = baseDir / entry.file;
            }
        }
        if (!entry.face.empty()) chain.push_back(std::move(entry));
        start = end + 1;
    }
    return chain;
}

std::optional<FontEntry> Resolve(const std::vector<FontEntry>& chain, bool bold) {
    for (const FontEntry& entry : chain) {
        const std::string face = paths::ToUtf8(entry.face);
        if (!entry.file.empty()) {
            if (AddFontResourceExW(entry.file.c_str(), FR_PRIVATE, nullptr) == 0) {
                log::Warn("Font chain: \"{}\" - file {} could not be loaded, trying the next entry", face,
                          paths::ToUtf8(entry.file));
                continue;
            }
        }
        const std::wstring selected = SelectedFace(entry.face, bold);
        if (_wcsicmp(selected.c_str(), entry.face.c_str()) == 0) {
            log::Info("Font chain: using \"{}\" ({}{})", paths::ToUtf8(selected),
                      entry.file.empty() ? "installed font" : "file ",
                      entry.file.empty() ? "" : paths::ToUtf8(entry.file));
            return entry;
        }
        log::Info("Font chain: \"{}\" not available (GDI substitutes \"{}\"), trying the next entry", face,
                  paths::ToUtf8(selected));
    }
    log::Warn("Font chain: no entry available");
    return std::nullopt;
}

const std::optional<FontEntry>& Configured() {
    static std::once_flag once;
    static std::optional<FontEntry> chosen;
    std::call_once(once, [] {
        chosen = Resolve(ParseChain(config::GetString("Fonts", "Chain", kDefaultChain), paths::ModDir()), true);
    });
    return chosen;
}

}  // namespace ddmod::fonts
