#include "fonts/familypaint.h"

#include <algorithm>
#include <map>

namespace ddmod::fonts {

FontColors ColorsOf(const std::string& stem, Family family, const text::Capitals& caps) {
    for (const FontColorEntry& entry : FontColorTable()) {
        if (entry.file == stem) return entry.original;
    }
    std::map<std::uint32_t, int> counts;
    for (const overlay::Glyph& g : caps) {
        for (std::size_t i = 0; i < g.colors.size(); ++i) {
            if (g.coverage[i]) ++counts[static_cast<std::uint32_t>(g.colors[i].r) << 16 | g.colors[i].g << 8 | g.colors[i].b];
        }
    }
    FontColors colors{{0xE7, 0xE7, 0xE7}, {0, 0, 0}};
    const auto most = std::max_element(counts.begin(), counts.end(),
                                       [](const auto& a, const auto& b) { return a.second < b.second; });
    if (most != counts.end()) {
        colors.text = {static_cast<std::uint8_t>(most->first >> 16), static_cast<std::uint8_t>(most->first >> 8),
                       static_cast<std::uint8_t>(most->first)};
    }
    if (family == Family::Books) colors.edge = {0x9C, 0x8E, 0x5A};
    if (family == Family::DiaryTitle) colors.edge = {0x94, 0x7D, 0x4A};
    return colors;
}

FamilyPaint::FamilyPaint(const FamilySettings& settings, const text::Capitals& caps, text::Rgb textColor, text::Rgb edge)
    : style_(settings.style), text_(textColor), edge_(edge) {
    if (style_ == GlyphStyle::Ramp) return;
    params_.profile = text::MeasureProfile(caps, settings.capitals);
    params_.contour = edge;
    if (style_ == GlyphStyle::Measured) {
        bevel_ = text::MeasureBevel(caps, params_.profile, settings.capitals);
        params_.bevel = &bevel_;
    } else {
        shine_ = text::MeasureShine(caps);
        params_.shine = &shine_;
        params_.shineStrength = settings.shineStrength;
        params_.texture = settings.texture;
    }
}

text::Image FamilyPaint::Paint(const text::Image& mask, int capTop, int capHeight) const {
    return style_ == GlyphStyle::Ramp ? text::PaintRamp(mask, text_, edge_) : text::PaintStyled(mask, capTop, capHeight, params_);
}

}  // namespace ddmod::fonts
