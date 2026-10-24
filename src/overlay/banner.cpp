#include "overlay/banner.h"

#include <algorithm>
#include <memory>

#include <windows.h>

#include "core/log.h"
#include "core/paths.h"
#include "fonts/familypaint.h"
#include "overlay/fnt.h"
#include "overlay/glyphs.h"
#include "text/face.h"

namespace ddmod::overlay {
namespace {

constexpr int kPad = 3;

struct PaintedPart {
    text::Image image;
    int baseline = 0;  // capitals' bottom row in `image`
};

bool PaintPart(const std::filesystem::path& fontsDir, const BannerPart& part, PaintedPart& out) {
    const std::filesystem::path file = fontsDir / (part.font + ".fnt");
    GlyphCache original(CreateFntGlyphSource(file));
    if (original.LineHeight() <= 0) {
        log::Warn("Overlay banner: cannot read {}", paths::ToUtf8(file));
        return false;
    }
    const fonts::ResolvedFamily& family = fonts::Resolved(part.family);
    if (!family.face) {
        log::Warn("Overlay banner: family {} has no face", fonts::FamilyName(part.family));
        return false;
    }
    const fonts::FamilySettings& settings = family.settings;
    text::Capitals caps;
    for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) caps.push_back(original.Get(ch));
    const text::CapMetrics cap = text::MeasureCap(caps, settings.capitals);
    if (cap.height <= 0) return false;
    const int height = text::HeightForCap(family.source, settings.weight, cap.height, settings.capitals);
    const std::unique_ptr<text::Face> openedFace = text::OpenFace(family.source, height, settings.weight);
    const text::Face& face = *openedFace;
    if (!face.Valid()) return false;
    const text::CapMetrics faceCap = face.Capitals(settings.capitals);
    const fonts::FontColors colors = fonts::ColorsOf(part.font, part.family, caps);
    const fonts::FamilyPaint paint(settings, caps, fonts::ToRgb(colors.text), fonts::ToRgb(colors.edge));
    out.image = paint.Paint(text::LineMask(face, part.text, kPad), kPad + faceCap.top, faceCap.height);
    out.baseline = kPad + faceCap.top + faceCap.height - 1;
    return true;
}

}  // namespace

std::vector<BannerPart> DefaultBanner(const std::wstring& name, const std::wstring& version, const std::wstring& state) {
    // In capitals, like the menu's entries (Uncial Antiqua's lowercase are uncial forms that read
    // differently: "Rivellou Rehewed").
    std::wstring capitals = name;
    CharUpperBuffW(capitals.data(), static_cast<DWORD>(capitals.size()));
    return {{"fontbig24", fonts::Family::Menu, capitals, 0},
            {"fontn2", fonts::Family::Interface, version, 12},
            {"fontgr1", fonts::Family::Interface, state, 8}};
}

BannerImage RenderBanner(const std::filesystem::path& fontsDir, const std::vector<BannerPart>& parts) {
    std::vector<PaintedPart> painted(parts.size());
    int above = 0, below = 0;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (!PaintPart(fontsDir, parts[i], painted[i])) return {};
        above = std::max(above, painted[i].baseline);
        below = std::max(below, painted[i].image.height - painted[i].baseline);
    }
    // Each part's pen starts kPad into its image; the next one starts after its ink and the gap.
    std::vector<int> x(parts.size(), 0);
    int width = 0;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) x[i] = x[i - 1] + painted[i - 1].image.width - 2 * kPad + parts[i].gap;
        width = std::max(width, x[i] + painted[i].image.width);
    }
    BannerImage out;
    out.image = text::MakeImage(width, above + below);
    out.baseline = above;
    out.left = kPad;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const text::Image& img = painted[i].image;
        const int y0 = above - painted[i].baseline;
        for (int y = 0; y < img.height; ++y) {
            for (int px = 0; px < img.width; ++px) {
                const float a = img.A(px, y);
                if (a <= 0.0f) continue;
                out.image.A(x[i] + px, y0 + y) = a;
                out.image.C(x[i] + px, y0 + y) = img.C(px, y);
            }
        }
    }
    return out;
}

}  // namespace ddmod::overlay
