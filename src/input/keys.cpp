#include "input/keys.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>

#include <windows.h>

namespace ddmod::input {

namespace {

struct NamedKey {
    std::string_view name;
    std::uint8_t vk;
};

// Keys with a fixed name. The first entry of a vk is its canonical name; later ones are aliases.
// F1-F24, A-Z, 0-9 and Num0-Num9 are handled in code. PrintScreen is left out on purpose:
// Windows sends no key-down message for it.
constexpr std::array kNamedKeys{
    NamedKey{"Space", VK_SPACE},       NamedKey{"Enter", VK_RETURN},       NamedKey{"Return", VK_RETURN},
    NamedKey{"Escape", VK_ESCAPE},     NamedKey{"Esc", VK_ESCAPE},         NamedKey{"Tab", VK_TAB},
    NamedKey{"Backspace", VK_BACK},    NamedKey{"Insert", VK_INSERT},      NamedKey{"Ins", VK_INSERT},
    NamedKey{"Delete", VK_DELETE},     NamedKey{"Del", VK_DELETE},         NamedKey{"Home", VK_HOME},
    NamedKey{"End", VK_END},           NamedKey{"PageUp", VK_PRIOR},       NamedKey{"PgUp", VK_PRIOR},
    NamedKey{"PageDown", VK_NEXT},     NamedKey{"PgDn", VK_NEXT},          NamedKey{"Up", VK_UP},
    NamedKey{"Down", VK_DOWN},         NamedKey{"Left", VK_LEFT},          NamedKey{"Right", VK_RIGHT},
    NamedKey{"Pause", VK_PAUSE},       NamedKey{"ScrollLock", VK_SCROLL},  NamedKey{"NumAdd", VK_ADD},
    NamedKey{"NumSub", VK_SUBTRACT},   NamedKey{"NumMul", VK_MULTIPLY},    NamedKey{"NumDiv", VK_DIVIDE},
    NamedKey{"NumDecimal", VK_DECIMAL},
};

constexpr std::array<std::pair<std::string_view, std::uint8_t>, 3> kModifiers{{
    {"Ctrl", kCtrl},
    {"Shift", kShift},
    {"Alt", kAlt},
}};

bool EqualsNoCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

bool StartsWithNoCase(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && EqualsNoCase(text.substr(0, prefix.size()), prefix);
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    return text;
}

// Parses a decimal number of 1-2 digits without sign or leading zero.
std::optional<int> SmallNumber(std::string_view digits) {
    if (digits.empty() || digits.size() > 2 || digits[0] == '0') return std::nullopt;
    int value = 0;
    for (const char c : digits) {
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + (c - '0');
    }
    return value;
}

std::optional<std::uint8_t> ParseKeyName(std::string_view name) {
    if (name.size() == 1) {
        const auto c = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return static_cast<std::uint8_t>(c);
        return std::nullopt;
    }
    for (const auto& key : kNamedKeys) {
        if (EqualsNoCase(name, key.name)) return key.vk;
    }
    if (StartsWithNoCase(name, "Num") && name.size() == 4 && name[3] >= '0' && name[3] <= '9') {
        return static_cast<std::uint8_t>(VK_NUMPAD0 + (name[3] - '0'));
    }
    if (StartsWithNoCase(name, "F")) {
        const auto n = SmallNumber(name.substr(1));
        if (n && *n >= 1 && *n <= 24) return static_cast<std::uint8_t>(VK_F1 + *n - 1);
    }
    return std::nullopt;
}

std::optional<std::uint8_t> ParseModifier(std::string_view name) {
    if (EqualsNoCase(name, "Control")) return kCtrl;
    for (const auto& [modName, bit] : kModifiers) {
        if (EqualsNoCase(name, modName)) return bit;
    }
    return std::nullopt;
}

// A whole decimal number (up to 4 digits), or nullopt.
std::optional<unsigned> Number(std::string_view text) {
    if (text.empty() || text.size() > 4) return std::nullopt;
    unsigned value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + static_cast<unsigned>(c - '0');
    }
    return value;
}

}  // namespace

RepeatTiming WindowsRepeatTiming(unsigned delaySetting, unsigned speedSetting) {
    RepeatTiming timing;
    timing.delayMs = 250 * (std::min(delaySetting, 3u) + 1);
    // Speed 0 = about 2.5, 31 = about 30 repeats per second, in even steps.
    const double perSecond = 2.5 + std::min(speedSetting, 31u) * (27.5 / 31.0);
    timing.intervalMs = static_cast<unsigned>(1000.0 / perSecond + 0.5);
    return timing;
}

unsigned ParseRepeatDelay(std::string_view text, unsigned windowsMs, bool& valid) {
    text = Trim(text);
    valid = true;
    if (EqualsNoCase(text, "windows")) return windowsMs;
    if (const auto ms = Number(text)) return std::clamp(*ms, 100u, 2000u);
    valid = false;
    return kDefaultRepeatDelayMs;
}

unsigned ParseRepeatInterval(std::string_view text, unsigned windowsIntervalMs, bool& valid) {
    text = Trim(text);
    valid = true;
    if (EqualsNoCase(text, "windows")) return windowsIntervalMs;
    unsigned perSecond = kDefaultRepeatRate;
    if (const auto rate = Number(text)) {
        perSecond = std::clamp(*rate, 1u, 50u);
    } else {
        valid = false;
    }
    return (1000 + perSecond / 2) / perSecond;
}

std::optional<KeyCombo> ParseKeyCombo(std::string_view text) {
    text = Trim(text);
    if (text.empty() || EqualsNoCase(text, "None")) return KeyCombo{};

    KeyCombo combo;
    while (true) {
        const auto plus = text.find('+');
        const auto part = Trim(text.substr(0, plus));
        if (part.empty()) return std::nullopt;  // "+F1", "Ctrl++F1", "Ctrl+"

        if (plus == std::string_view::npos) {
            // Last part: the key itself.
            const auto vk = ParseKeyName(part);
            if (!vk) return std::nullopt;
            combo.vk = *vk;
            return combo;
        }
        const auto mod = ParseModifier(part);
        if (!mod || (combo.mods & *mod)) return std::nullopt;  // unknown or repeated modifier
        combo.mods |= *mod;
        text.remove_prefix(plus + 1);
    }
}

std::string ToString(KeyCombo combo) {
    if (!combo.Bound()) return "None";

    std::string text;
    for (const auto& [name, bit] : kModifiers) {
        if (combo.mods & bit) {
            text += name;
            text += '+';
        }
    }
    const std::uint8_t vk = combo.vk;
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
        text += static_cast<char>(vk);
    } else if (vk >= VK_F1 && vk <= VK_F24) {
        text += std::format("F{}", vk - VK_F1 + 1);
    } else if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        text += std::format("Num{}", vk - VK_NUMPAD0);
    } else {
        std::string_view name;
        for (const auto& key : kNamedKeys) {
            if (key.vk == vk) {
                name = key.name;
                break;
            }
        }
        text += name.empty() ? std::format("VK_{:02X}", vk) : std::string(name);
    }
    return text;
}

}  // namespace ddmod::input
