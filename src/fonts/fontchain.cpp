#include "fonts/fontchain.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
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

// Selects `face` into a memory DC and calls `use(dc)`.
template <typename Use>
void WithFace(const std::wstring& face, bool bold, Use use) {
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT font = CreateFontW(-16, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face.c_str());
    if (dc && font) {
        const HGDIOBJ previous = SelectObject(dc, font);
        use(dc);
        SelectObject(dc, previous);
    }
    if (font) DeleteObject(font);
    if (dc) DeleteDC(dc);
}

// The face GDI actually selects for `face` (it substitutes silently if the face is missing).
std::wstring SelectedFace(const std::wstring& face, bool bold) {
    std::wstring result;
    WithFace(face, bold, [&](HDC dc) {
        wchar_t actual[LF_FACESIZE] = {};
        if (GetTextFaceW(dc, LF_FACESIZE, actual) > 0) result = actual;
    });
    return result;
}

std::uint16_t BigEndian16(const std::vector<std::uint8_t>& data, std::size_t at) {
    return static_cast<std::uint16_t>(data[at] << 8 | data[at + 1]);
}

// The US English full name (name ID 4, Windows platform) from the selected font's `name` table -
// the registry font list uses it, while GDI's metrics give the name in the UI language
// ("Tahoma Fett" on a German Windows).
std::wstring FullNameOf(const std::wstring& face, bool bold) {
    std::wstring result;
    WithFace(face, bold, [&](HDC dc) {
        constexpr DWORD kNameTable = 'eman';  // "name", byte order as GetFontData expects
        const DWORD size = GetFontData(dc, kNameTable, 0, nullptr, 0);
        if (size == GDI_ERROR || size < 6) return;
        std::vector<std::uint8_t> data(size);
        if (GetFontData(dc, kNameTable, 0, data.data(), size) != size) return;
        const std::size_t count = BigEndian16(data, 2), strings = BigEndian16(data, 4);
        int bestRank = 0;
        for (std::size_t i = 0; i < count && 6 + i * 12 + 12 <= size; ++i) {
            const std::size_t record = 6 + i * 12;
            const auto platform = BigEndian16(data, record), language = BigEndian16(data, record + 4);
            const auto nameId = BigEndian16(data, record + 6);
            const std::size_t length = BigEndian16(data, record + 8), offset = strings + BigEndian16(data, record + 10);
            if (platform != 3 || nameId != 4 || offset + length > size) continue;
            const int rank = language == 0x0409 ? 2 : 1;
            if (rank <= bestRank) continue;
            bestRank = rank;
            result.clear();
            for (std::size_t at = offset; at + 1 < offset + length; at += 2) {
                result.push_back(static_cast<wchar_t>(BigEndian16(data, at)));
            }
        }
    });
    return result;
}

// File of the font list entry naming `fullName` under `root`, or empty.
std::filesystem::path FontListFile(HKEY root, const std::wstring& fullName) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return {};
    }
    std::filesystem::path result;
    for (DWORD index = 0; result.empty(); ++index) {
        wchar_t name[512];
        wchar_t data[MAX_PATH * 2];
        DWORD nameLen = static_cast<DWORD>(std::size(name));
        DWORD dataSize = sizeof(data) - sizeof(wchar_t);
        DWORD type = 0;
        const LSTATUS status = RegEnumValueW(key, index, name, &nameLen, nullptr, &type, reinterpret_cast<BYTE*>(data), &dataSize);
        if (status == ERROR_NO_MORE_ITEMS) break;
        if (status != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) continue;
        data[dataSize / sizeof(wchar_t)] = L'\0';
        if (!FontListNameMatches(std::wstring_view(name, nameLen), fullName)) continue;
        result = data;
        if (result.is_relative()) {
            wchar_t windows[MAX_PATH] = {};
            GetWindowsDirectoryW(windows, MAX_PATH);
            result = std::filesystem::path(windows) / L"Fonts" / result;
        }
    }
    RegCloseKey(key);
    return result;
}

}  // namespace

bool FontListNameMatches(std::wstring_view valueName, std::wstring_view fullName) {
    if (fullName.empty()) return false;
    // Drop the font type: "Tahoma Bold (TrueType)" -> "Tahoma Bold".
    if (const auto paren = valueName.rfind(L" ("); paren != std::wstring_view::npos && valueName.ends_with(L')')) {
        valueName = valueName.substr(0, paren);
    }
    // A collection lists its fonts joined by " & ".
    std::size_t start = 0;
    while (start <= valueName.size()) {
        const auto end = std::min(valueName.find(L'&', start), valueName.size());
        const std::wstring name = Trim(valueName.substr(start, end - start));
        if (name.size() == fullName.size() && _wcsnicmp(name.c_str(), fullName.data(), name.size()) == 0) return true;
        start = end + 1;
    }
    return false;
}

InstalledFont FindInstalledFont(const std::wstring& face, bool bold) {
    InstalledFont font;
    font.fullName = FullNameOf(face, bold);
    if (font.fullName.empty()) return font;
    font.file = FontListFile(HKEY_LOCAL_MACHINE, font.fullName);
    if (font.file.empty()) font.file = FontListFile(HKEY_CURRENT_USER, font.fullName);
    return font;
}

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
            if (!entry.file.empty()) {
                log::Info("Font chain: using \"{}\" (file {})", paths::ToUtf8(selected), paths::ToUtf8(entry.file));
                return entry;
            }
            const InstalledFont installed = FindInstalledFont(entry.face, bold);
            log::Info("Font chain: using \"{}\" (installed font \"{}\", {})", paths::ToUtf8(selected),
                      paths::ToUtf8(installed.fullName),
                      installed.file.empty() ? std::string("file not in the registry font list")
                                             : "file " + paths::ToUtf8(installed.file));
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
