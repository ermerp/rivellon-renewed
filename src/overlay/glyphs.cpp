#include "overlay/glyphs.h"

#include <algorithm>

#include <windows.h>

#include "core/log.h"
#include "core/paths.h"

namespace ddmod::overlay {
namespace {

class GdiGlyphSource final : public GlyphSource {
public:
    ~GdiGlyphSource() override {
        if (dc_) {
            if (previous_) SelectObject(dc_, previous_);
            DeleteDC(dc_);
        }
        if (font_) DeleteObject(font_);
        if (!privateFile_.empty()) RemoveFontResourceExW(privateFile_.c_str(), FR_PRIVATE, nullptr);
    }

    bool Open(const FontSpec& spec) {
        if (!spec.file.empty()) {
            if (AddFontResourceExW(spec.file.c_str(), FR_PRIVATE, nullptr) > 0) {
                privateFile_ = spec.file.wstring();
                log::Info("Overlay font file loaded: {}", paths::ToUtf8(spec.file));
            } else {
                log::Warn("Overlay font file {} could not be loaded - using the system font", paths::ToUtf8(spec.file));
            }
        }
        // Negative height = character height, so the size does not depend on the font's leading.
        font_ = CreateFontW(spec.cellHeight ? spec.pixelHeight : -spec.pixelHeight, 0, 0, 0, spec.bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                            DEFAULT_PITCH, spec.face.c_str());
        dc_ = CreateCompatibleDC(nullptr);
        if (!font_ || !dc_) return false;
        previous_ = SelectObject(dc_, font_);
        TEXTMETRICW metrics{};
        if (!GetTextMetricsW(dc_, &metrics)) return false;
        ascent_ = metrics.tmAscent;
        lineHeight_ = metrics.tmHeight;
        wchar_t actualFace[LF_FACESIZE] = {};
        GetTextFaceW(dc_, LF_FACESIZE, actualFace);
        log::Info("Overlay font: \"{}\" {} px (line height {})", paths::ToUtf8(actualFace), spec.pixelHeight,
                  lineHeight_);
        return true;
    }

    int LineHeight() const override { return lineHeight_; }

    bool Rasterize(wchar_t ch, Glyph& out) override {
        static constexpr MAT2 kIdentity = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
        GLYPHMETRICS gm{};
        const DWORD size = GetGlyphOutlineW(dc_, ch, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &kIdentity);
        if (size == GDI_ERROR) return false;
        out = {};
        out.advance = gm.gmCellIncX;
        if (size == 0) return true;  // blank character (space)
        std::vector<std::uint8_t> raw(size);
        if (GetGlyphOutlineW(dc_, ch, GGO_GRAY8_BITMAP, &gm, size, raw.data(), &kIdentity) == GDI_ERROR) return false;
        out.width = static_cast<int>(gm.gmBlackBoxX);
        out.height = static_cast<int>(gm.gmBlackBoxY);
        out.offsetX = gm.gmptGlyphOrigin.x;
        out.offsetY = ascent_ - gm.gmptGlyphOrigin.y;
        // GGO_GRAY8_BITMAP: 65 levels (0-64), rows padded to 4 bytes.
        const int rawPitch = (out.width + 3) & ~3;
        out.coverage.resize(static_cast<std::size_t>(out.width) * out.height);
        for (int row = 0; row < out.height; ++row) {
            for (int col = 0; col < out.width; ++col) {
                const int level = std::min<int>(raw[static_cast<std::size_t>(row) * rawPitch + col], 64);
                out.coverage[static_cast<std::size_t>(row) * out.width + col] =
                    static_cast<std::uint8_t>((level * 255 + 32) / 64);
            }
        }
        return true;
    }

private:
    std::wstring privateFile_;
    HFONT font_ = nullptr;
    HDC dc_ = nullptr;
    HGDIOBJ previous_ = nullptr;
    int ascent_ = 0;
    int lineHeight_ = 0;
};

}  // namespace

std::unique_ptr<GlyphSource> CreateGdiGlyphSource(const FontSpec& spec) {
    auto source = std::make_unique<GdiGlyphSource>();
    if (!source->Open(spec)) {
        log::Error("Overlay font: GDI could not create \"{}\" {} px", paths::ToUtf8(spec.face), spec.pixelHeight);
        return nullptr;
    }
    return source;
}

GlyphCache::GlyphCache(std::unique_ptr<GlyphSource> source) : source_(std::move(source)) {
    lineHeight_ = source_ ? source_->LineHeight() : 0;
}

const Glyph& GlyphCache::Get(wchar_t ch) {
    if (const auto it = glyphs_.find(ch); it != glyphs_.end()) return it->second;
    Glyph glyph;
    if (!source_ || !source_->Rasterize(ch, glyph)) {
        if (ch != L'?') return glyphs_[ch] = Get(L'?');
        glyph = {};
    }
    return glyphs_[ch] = std::move(glyph);
}

int MeasureString(GlyphCache& cache, std::wstring_view text) {
    int width = 0;
    for (const wchar_t ch : text) width += cache.Get(ch).advance;
    return width;
}

void DrawString(const SurfaceView& view, GlyphCache& cache, int x, int y, std::wstring_view text, Rgb color,
                bool shadow) {
    int pen = x;
    for (const wchar_t ch : text) {
        const Glyph& glyph = cache.Get(ch);
        if (!glyph.coverage.empty()) {
            const int left = pen + glyph.offsetX;
            const int top = y + glyph.offsetY;
            if (shadow) {
                DrawMask(view, left + 1, top + 1, glyph.coverage.data(), glyph.width, glyph.height, glyph.width,
                         Rgb{0, 0, 0}, 200);
            }
            if (glyph.colors.empty()) {
                DrawMask(view, left, top, glyph.coverage.data(), glyph.width, glyph.height, glyph.width, color, 255);
            } else {
                DrawColorMask(view, left, top, glyph.coverage.data(), glyph.colors.data(), glyph.width, glyph.height,
                              glyph.width, 255);
            }
        }
        pen += glyph.advance;
    }
}

}  // namespace ddmod::overlay
