#include "fonts/fontchain.h"

#include <algorithm>
#include <cstddef>
#include <mutex>

#include <windows.h>

#include "core/config.h"
#include "core/log.h"
#include "core/paths.h"
#include "text/ttface.h"

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

std::optional<FontEntry> Resolve(const std::vector<FontEntry>& chain) {
    for (const FontEntry& entry : chain) {
        const std::string face = paths::ToUtf8(entry.face);
        if (entry.file.empty()) {
            log::Warn("Font chain: \"{}\" has no font file - installed fonts are not used, only \"Face @ file\"", face);
            continue;
        }
        if (!text::HoldsFace(entry.file, entry.face)) {
            log::Warn("Font chain: \"{}\" - file {} cannot be read or does not hold that face, trying the next entry",
                      face, paths::ToUtf8(entry.file));
            continue;
        }
        if (AddFontResourceExW(entry.file.c_str(), FR_PRIVATE, nullptr) == 0) {
            log::Warn("Font chain: \"{}\" - Windows could not load file {}, trying the next entry", face,
                      paths::ToUtf8(entry.file));
            continue;
        }
        log::Info("Font chain: using \"{}\" (file {})", face, paths::ToUtf8(entry.file));
        return entry;
    }
    log::Warn("Font chain: no entry available");
    return std::nullopt;
}

const std::optional<FontEntry>& Configured() {
    static std::once_flag once;
    static std::optional<FontEntry> chosen;
    std::call_once(once, [] {
        chosen = Resolve(ParseChain(config::GetString("Fonts", "Chain", kDefaultChain), paths::ModDir()));
    });
    return chosen;
}

}  // namespace ddmod::fonts
