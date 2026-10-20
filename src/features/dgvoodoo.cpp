#include "features/dgvoodoo.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <system_error>

#include <windows.h>

#include "core/config.h"
#include "core/log.h"
#include "core/paths.h"

namespace ddmod::features {
namespace {

constexpr std::string_view kSection = "DirectX";
constexpr std::string_view kKey = "dgVoodooWatermark";

bool IsBlank(char c) { return c == ' ' || c == '\t'; }

bool EqualsNoCase(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

// [DgVoodoo] Watermark: true/false, or nullopt for "keep" and unknown values (logged).
std::optional<bool> WantedState() {
    const std::string value = config::GetString("DgVoodoo", "Watermark", "0");
    for (std::string_view off : {"0", "false", "off", "no"}) {
        if (EqualsNoCase(value, off)) return false;
    }
    for (std::string_view on : {"1", "true", "on", "yes"}) {
        if (EqualsNoCase(value, on)) return true;
    }
    if (!EqualsNoCase(value, "keep")) {
        log::Warn("dgVoodoo watermark: unknown value \"{}\" ([DgVoodoo] Watermark) - dgVoodoo.conf left alone",
                  value);
    } else {
        log::Info("dgVoodoo watermark: left as in dgVoodoo.conf ([DgVoodoo] Watermark=keep)");
    }
    return std::nullopt;
}

}  // namespace

WatermarkEdit SetWatermark(std::string& text, bool on) {
    const std::string_view wanted = on ? "true" : "false";
    bool inSection = false;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        std::size_t lineEnd = end;
        if (lineEnd > pos && text[lineEnd - 1] == '\r') --lineEnd;

        std::size_t i = pos;
        while (i < lineEnd && IsBlank(text[i])) ++i;
        const std::string_view line(text.data() + i, lineEnd - i);
        if (line.starts_with('[')) {
            const std::size_t close = line.find(']');
            inSection = close != std::string_view::npos && EqualsNoCase(line.substr(1, close - 1), kSection);
        } else if (inSection && line.size() > kKey.size() && EqualsNoCase(line.substr(0, kKey.size()), kKey) &&
                   (IsBlank(line[kKey.size()]) || line[kKey.size()] == '=')) {
            const std::size_t eq = text.find('=', i);
            if (eq == std::string::npos || eq >= lineEnd) return WatermarkEdit::NotFound;
            std::size_t valueStart = eq + 1;
            while (valueStart < lineEnd && IsBlank(text[valueStart])) ++valueStart;
            std::size_t valueEnd = valueStart;
            while (valueEnd < lineEnd && !IsBlank(text[valueEnd])) ++valueEnd;
            if (EqualsNoCase(std::string_view(text).substr(valueStart, valueEnd - valueStart), wanted)) {
                return WatermarkEdit::Unchanged;
            }
            // "key = " without a value gets the space dgVoodoo writes after '='.
            const std::string value = valueStart == eq + 1 ? " " + std::string(wanted) : std::string(wanted);
            text.replace(valueStart, valueEnd - valueStart, value);
            return WatermarkEdit::Changed;
        }
        pos = end + 1;
    }
    return WatermarkEdit::NotFound;
}

void ApplyWatermarkSetting(const std::filesystem::path& conf) {
    const std::optional<bool> on = WantedState();
    if (!on) return;
    const char* state = *on ? "on" : "off";
    std::error_code ec;
    if (!std::filesystem::exists(conf, ec)) {
        log::Info("dgVoodoo watermark: no dgVoodoo.conf in the game folder - nothing to do");
        return;
    }
    std::string text;
    {
        std::ifstream in(conf, std::ios::binary);
        if (!in) {
            log::Warn("dgVoodoo watermark: cannot read {}", paths::ToUtf8(conf));
            return;
        }
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    switch (SetWatermark(text, *on)) {
        case WatermarkEdit::Unchanged:
            log::Info("dgVoodoo watermark: already {} in dgVoodoo.conf", state);
            return;
        case WatermarkEdit::NotFound:
            log::Warn("dgVoodoo watermark: no {} in [{}] of {} - left alone", kKey, kSection, paths::ToUtf8(conf));
            return;
        case WatermarkEdit::Changed:
            break;
    }
    // Written next to the file and moved over it, so a failed write never leaves half a config.
    std::filesystem::path temp = conf;
    temp += L".rrtmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) {
            out.close();
            std::filesystem::remove(temp, ec);
            log::Warn("dgVoodoo watermark: cannot write in {} (no write permission?) - "
                      "turn it {} in dgVoodooCpl.exe", paths::ToUtf8(conf.parent_path()), state);
            return;
        }
    }
    if (!MoveFileExW(temp.c_str(), conf.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        const DWORD error = GetLastError();
        std::filesystem::remove(temp, ec);
        log::Warn("dgVoodoo watermark: cannot replace {} (error {}) - turn it {} in dgVoodooCpl.exe",
                  paths::ToUtf8(conf), error, state);
        return;
    }
    if (GetModuleHandleW(L"ddraw.dll")) {
        log::Warn("dgVoodoo watermark: set to {} in dgVoodoo.conf, but DDraw.dll is already loaded - "
                  "takes effect at the next start", state);
    } else {
        log::Info("dgVoodoo watermark: set to {} in dgVoodoo.conf ([DgVoodoo] Watermark)", state);
    }
}

}  // namespace ddmod::features
