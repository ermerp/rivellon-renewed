#include "features/dgvoodoo.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <system_error>
#include <vector>

#include <windows.h>

#include "core/config.h"
#include "core/log.h"
#include "core/paths.h"

namespace ddmod::features {
namespace {

bool IsBlank(char c) { return c == ' ' || c == '\t'; }

bool EqualsNoCase(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

// One key of dgVoodoo.conf the mod sets.
struct Setting {
    const char* label;    // for the log, e.g. "watermark"
    std::string_view section;
    std::string_view key;
    std::string value;    // the value written into dgVoodoo.conf
    std::string state;    // for the log, e.g. "off"
    ConfEdit edit = ConfEdit::NotFound;
};

// The value of [DgVoodoo] `iniKey`, matched against `choices` (INI word, conf value, log state);
// nullopt for "keep" and unknown values (both logged).
std::optional<Setting> Wanted(const char* label, const char* iniKey, const char* fallback,
                              std::initializer_list<std::array<const char*, 3>> choices, Setting setting) {
    const std::string value = config::GetString("DgVoodoo", iniKey, fallback);
    for (const auto& choice : choices) {
        if (!EqualsNoCase(value, choice[0])) continue;
        setting.value = choice[1];
        setting.state = choice[2];
        return setting;
    }
    if (EqualsNoCase(value, "keep")) {
        log::Info("dgVoodoo {}: left as in dgVoodoo.conf ([DgVoodoo] {}=keep)", label, iniKey);
    } else {
        log::Warn("dgVoodoo {}: unknown value \"{}\" ([DgVoodoo] {}) - left as in dgVoodoo.conf", label, value, iniKey);
    }
    return std::nullopt;
}

std::vector<Setting> WantedSettings() {
    std::vector<Setting> settings;
    if (auto s = Wanted("watermark", "Watermark", "0",
                        {{"0", "false", "off"}, {"false", "false", "off"}, {"off", "false", "off"}, {"no", "false", "off"},
                         {"1", "true", "on"}, {"true", "true", "on"}, {"on", "true", "on"}, {"yes", "true", "on"}},
                        {"watermark", "DirectX", "dgVoodooWatermark", {}, {}})) {
        settings.push_back(*s);
    }
    if (auto s = Wanted("full screen", "FullScreen", "borderless",
                        {{"borderless", "fake", "borderless"}, {"exclusive", "", "exclusive"}},
                        {"full screen", "GeneralExt", "FullscreenAttributes", {}, {}})) {
        settings.push_back(*s);
    }
    return settings;
}

}  // namespace

ConfEdit SetConfValue(std::string& text, std::string_view section, std::string_view key, std::string_view value) {
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
            inSection = close != std::string_view::npos && EqualsNoCase(line.substr(1, close - 1), section);
        } else if (inSection && line.size() > key.size() && EqualsNoCase(line.substr(0, key.size()), key) &&
                   (IsBlank(line[key.size()]) || line[key.size()] == '=')) {
            const std::size_t eq = text.find('=', i);
            if (eq == std::string::npos || eq >= lineEnd) return ConfEdit::NotFound;
            std::size_t valueStart = eq + 1;
            while (valueStart < lineEnd && IsBlank(text[valueStart])) ++valueStart;
            std::size_t valueEnd = valueStart;
            while (valueEnd < lineEnd && !IsBlank(text[valueEnd])) ++valueEnd;
            if (EqualsNoCase(std::string_view(text).substr(valueStart, valueEnd - valueStart), value)) {
                return ConfEdit::Unchanged;
            }
            // "key = " without a value gets the space dgVoodoo writes after '='.
            const std::string replacement =
                valueStart == eq + 1 && !value.empty() ? " " + std::string(value) : std::string(value);
            text.replace(valueStart, valueEnd - valueStart, replacement);
            return ConfEdit::Changed;
        }
        pos = end + 1;
    }
    return ConfEdit::NotFound;
}

void ApplyDgVoodooSettings(const std::filesystem::path& conf) {
    std::vector<Setting> settings = WantedSettings();
    if (settings.empty()) return;
    std::error_code ec;
    if (!std::filesystem::exists(conf, ec)) {
        log::Info("dgVoodoo settings: no dgVoodoo.conf in the game folder - nothing to do");
        return;
    }
    std::string text;
    {
        std::ifstream in(conf, std::ios::binary);
        if (!in) {
            log::Warn("dgVoodoo settings: cannot read {}", paths::ToUtf8(conf));
            return;
        }
        text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    bool changed = false;
    for (Setting& s : settings) {
        s.edit = SetConfValue(text, s.section, s.key, s.value);
        if (s.edit == ConfEdit::Unchanged) log::Info("dgVoodoo {}: already {} in dgVoodoo.conf", s.label, s.state);
        if (s.edit == ConfEdit::NotFound) {
            log::Warn("dgVoodoo {}: no {} in [{}] of {} - left alone", s.label, s.key, s.section, paths::ToUtf8(conf));
        }
        changed = changed || s.edit == ConfEdit::Changed;
    }
    if (!changed) return;

    auto failed = [&](const std::string& why) {
        for (const Setting& s : settings) {
            if (s.edit != ConfEdit::Changed) continue;
            log::Warn("dgVoodoo {}: {} - set \"{} = {}\" in [{}] of dgVoodoo.conf by hand", s.label, why, s.key, s.value,
                      s.section);
        }
    };
    // Written next to the file and moved over it, so a failed write never leaves half a config.
    std::filesystem::path temp = conf;
    temp += L".rrtmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) {
            out.close();
            std::filesystem::remove(temp, ec);
            failed("cannot write in " + paths::ToUtf8(conf.parent_path()) + " (no write permission?)");
            return;
        }
    }
    if (!MoveFileExW(temp.c_str(), conf.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        const DWORD error = GetLastError();
        std::filesystem::remove(temp, ec);
        failed("cannot replace " + paths::ToUtf8(conf) + " (error " + std::to_string(error) + ")");
        return;
    }
    const bool late = GetModuleHandleW(L"ddraw.dll") != nullptr;
    for (const Setting& s : settings) {
        if (s.edit != ConfEdit::Changed) continue;
        if (late) {
            log::Warn("dgVoodoo {}: set to {} in dgVoodoo.conf, but DDraw.dll is already loaded - "
                      "takes effect at the next start", s.label, s.state);
        } else {
            log::Info("dgVoodoo {}: set to {} in dgVoodoo.conf ([DgVoodoo])", s.label, s.state);
        }
    }
}

}  // namespace ddmod::features
