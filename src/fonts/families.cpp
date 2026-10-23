#include "fonts/families.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <mutex>

#include <windows.h>

#include "core/config.h"
#include "core/log.h"
#include "core/paths.h"

namespace ddmod::fonts {
namespace {

#define DDMOD_FONT_DIR DDMOD_FILE_STEM "\\fonts\\"

bool IsInterface(std::string_view stem) {
    for (const std::string_view color : {"fontn", "fontgra", "fontred", "fontb", "fontgo", "fontor", "fontyel", "fontgr"}) {
        if (stem.size() == color.size() + 1 && stem.starts_with(color) && stem.back() >= '1' && stem.back() <= '5') {
            return true;
        }
    }
    return false;
}

}  // namespace

const char* FamilyName(Family family) {
    switch (family) {
        case Family::Interface: return "Interface";
        case Family::Books: return "Books";
        case Family::Menu: return "Menu";
        case Family::Dialog: return "Dialog";
        case Family::DiaryTitle: return "DiaryTitle";
    }
    return "?";
}

std::optional<Family> FamilyOf(std::string_view stem) {
    if (IsInterface(stem)) return Family::Interface;
    if (stem.starts_with("fontblack") || stem.starts_with("paper_")) return Family::Books;
    if (stem.starts_with("fontbig24") || stem.starts_with("fontbig36")) return Family::Menu;
    if (stem.starts_with("dialog_")) return Family::Dialog;
    if (stem.starts_with("diary_head_")) return Family::DiaryTitle;
    return std::nullopt;
}

std::optional<LineHeight> ParseLineHeight(std::string_view text) {
    std::string value(text);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (value == "auto") return LineHeight{true, 100};
    int percent = 0;
    const auto [end, ec] = std::from_chars(value.data(), value.data() + value.size(), percent);
    if (ec != std::errc{} || end != value.data() + value.size() || percent < 50 || percent > 300) return std::nullopt;
    return LineHeight{false, percent};
}

std::optional<TopFit> ParseTopFit(std::string_view text) {
    std::string value(text);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (value == "none") return TopFit::None;
    if (value == "lower") return TopFit::Lower;
    if (value == "squeeze") return TopFit::Squeeze;
    if (value == "fit") return TopFit::Fit;
    return std::nullopt;
}

FamilySettings DefaultSettings(Family family) {
    FamilySettings s;
    switch (family) {
        case Family::Interface:
            s.chain = "Marcellus SC @ " DDMOD_FONT_DIR "MarcellusSC-Regular.ttf";
            s.weight = 600;
            break;
        case Family::Books:
            s.chain = "Fondamento @ " DDMOD_FONT_DIR "Fondamento-Regular.ttf";
            s.weight = 400;
            s.lineHeight = {true, 100};
            break;
        case Family::Menu:
            s.chain = "Uncial Antiqua @ " DDMOD_FONT_DIR "UncialAntiqua-Regular.ttf";
            s.weight = 600;
            s.style = GlyphStyle::Shine;
            s.shineStrength = 2.0f;
            s.texture = true;
            s.keepOriginal = "[]";  // right and left arrow beside the values on the options pages
            s.capitals = text::CapReference::Median;  // its H reaches 6 px above the other capitals
            s.topFit = TopFit::Lower;
            break;
        case Family::Dialog:
            s.chain = kDefaultChain;
            s.weight = 700;
            break;
        case Family::DiaryTitle:
            s.chain = "Almendra SC @ " DDMOD_FONT_DIR "AlmendraSC-Regular.ttf";
            s.weight = 600;
            s.style = GlyphStyle::Measured;
            s.capitals = text::CapReference::Median;  // its H reaches 5 px above the other capitals
            s.topFit = TopFit::Lower;
            break;
    }
    return s;
}

FamilySettings ConfiguredSettings(Family family) {
    FamilySettings s = DefaultSettings(family);
    const std::string section = std::string("Family.") + FamilyName(family);
    s.enabled = config::GetBool(section.c_str(), "Enabled", true);
    s.chain = config::GetString(section.c_str(), "Face", s.chain.c_str());
    const int weight = config::GetInt(section.c_str(), "Weight", s.weight);
    if (weight >= 100 && weight <= 900) {
        s.weight = weight;
    } else {
        log::Warn("[{}] Weight={} is not 100-900 - using {}", section, weight, s.weight);
    }
    const std::string lineText = config::GetString(section.c_str(), "LineHeight", s.lineHeight.automatic ? "auto" : "100");
    if (const auto line = ParseLineHeight(lineText)) {
        s.lineHeight = *line;
    } else {
        log::Warn("[{}] LineHeight=\"{}\" is not auto or 50-300 - using the default", section, lineText);
    }
    if (!config::GetBool(section.c_str(), "Effects", true)) s.style = GlyphStyle::Ramp;
    static constexpr const char* kTopFits[] = {"none", "lower", "squeeze", "fit"};
    const std::string fitText = config::GetString(section.c_str(), "TopFit", kTopFits[static_cast<int>(s.topFit)]);
    if (const auto fit = ParseTopFit(fitText)) {
        s.topFit = *fit;
    } else {
        log::Warn("[{}] TopFit=\"{}\" is not none, lower, squeeze or fit - using the default", section, fitText);
    }
    return s;
}

const ResolvedFamily& Resolved(Family family) {
    static std::mutex mutex;
    static std::array<std::optional<ResolvedFamily>, std::size(kFamilies)> resolved;
    const std::lock_guard lock(mutex);
    std::optional<ResolvedFamily>& slot = resolved[static_cast<std::size_t>(family)];
    if (slot) return *slot;
    slot.emplace();
    slot->settings = ConfiguredSettings(family);
    const char* name = FamilyName(family);
    if (!slot->settings.enabled) {
        log::Info("Fonts: family {} off ([Family.{}] Enabled=0) - original glyphs", name, name);
        return *slot;
    }
    slot->face = Resolve(ParseChain(slot->settings.chain, paths::ModDir()), slot->settings.weight >= FW_SEMIBOLD);
    if (!slot->face) {
        log::Warn("Fonts: family {} has no available face - original glyphs", name);
        return *slot;
    }
    const LineHeight& line = slot->settings.lineHeight;
    log::Info("Fonts: family {} - \"{}\" weight {}, line height {}", name, paths::ToUtf8(slot->face->face),
              slot->settings.weight, line.automatic ? std::string("auto") : std::to_string(line.percent) + "%");
    return *slot;
}

}  // namespace ddmod::fonts
