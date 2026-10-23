#include "text/gdiface.h"

#include <algorithm>
#include <climits>

namespace ddmod::text {
namespace {

constexpr MAT2 kIdentity = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};

}  // namespace

GdiFace::GdiFace(const std::wstring& face, int height, int weight) {
    dc_ = CreateCompatibleDC(nullptr);
    font_ = CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, face.c_str());
    if (dc_ && font_) {
        old_ = SelectObject(dc_, font_);
        GetTextMetricsW(dc_, &tm_);
    }
}

GdiFace::~GdiFace() {
    if (dc_ && old_) SelectObject(dc_, old_);
    if (font_) DeleteObject(font_);
    if (dc_) DeleteDC(dc_);
}

std::wstring GdiFace::ActualFace() const {
    wchar_t name[LF_FACESIZE] = {};
    GetTextFaceW(dc_, LF_FACESIZE, name);
    return name;
}

std::wstring GdiFace::Missing(const std::wstring& chars) const {
    std::vector<WORD> idx(chars.size());
    GetGlyphIndicesW(dc_, chars.c_str(), static_cast<int>(chars.size()), idx.data(), GGI_MARK_NONEXISTING_GLYPHS);
    std::wstring missing;
    for (std::size_t i = 0; i < chars.size(); ++i) {
        if (idx[i] == 0xFFFF) missing += chars[i];
    }
    return missing;
}

int GdiFace::CapTop() const {
    GLYPHMETRICS gm{};
    if (GetGlyphOutlineW(dc_, L'H', GGO_METRICS, &gm, 0, nullptr, &kIdentity) == GDI_ERROR) return 0;
    return tm_.tmAscent - gm.gmptGlyphOrigin.y;
}

int GdiFace::CapHeight() const {
    GLYPHMETRICS gm{};
    if (GetGlyphOutlineW(dc_, L'H', GGO_METRICS, &gm, 0, nullptr, &kIdentity) == GDI_ERROR) return 0;
    return static_cast<int>(gm.gmBlackBoxY);
}

CapMetrics GdiFace::Capitals(CapReference ref) const {
    if (ref == CapReference::H) return {CapTop(), CapHeight()};
    std::vector<int> tops, bottoms;
    for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) {
        GLYPHMETRICS gm{};
        if (!Missing(std::wstring(1, ch)).empty() ||
            GetGlyphOutlineW(dc_, ch, GGO_METRICS, &gm, 0, nullptr, &kIdentity) == GDI_ERROR || gm.gmBlackBoxY == 0) {
            continue;
        }
        const int top = tm_.tmAscent - gm.gmptGlyphOrigin.y;
        tops.push_back(top);
        bottoms.push_back(top + static_cast<int>(gm.gmBlackBoxY) - 1);
    }
    return MedianCap(std::move(tops), std::move(bottoms));
}

int GdiFace::XHeight() const {
    GLYPHMETRICS gm{};
    if (GetGlyphOutlineW(dc_, L'x', GGO_METRICS, &gm, 0, nullptr, &kIdentity) == GDI_ERROR) return 0;
    return static_cast<int>(gm.gmBlackBoxY);
}

void GdiFace::InkRange(const std::wstring& text, int& top, int& bottom) const {
    top = INT_MAX;
    bottom = INT_MIN;
    for (const wchar_t ch : text) {
        GLYPHMETRICS gm{};
        if (ch == L' ' || GetGlyphOutlineW(dc_, ch, GGO_METRICS, &gm, 0, nullptr, &kIdentity) == GDI_ERROR) continue;
        top = std::min(top, static_cast<int>(-gm.gmptGlyphOrigin.y));
        bottom = std::max(bottom, static_cast<int>(-gm.gmptGlyphOrigin.y + static_cast<int>(gm.gmBlackBoxY)));
    }
}

int GdiFace::InkExtent(const std::wstring& text) const {
    int top = 0, bottom = 0;
    InkRange(text, top, bottom);
    return bottom > top ? bottom - top : 0;
}

bool GdiFace::GlyphBits(wchar_t ch, GLYPHMETRICS& gm, std::vector<BYTE>& bits) const {
    const DWORD size = GetGlyphOutlineW(dc_, ch, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &kIdentity);
    if (size == GDI_ERROR) return false;
    bits.assign(size, 0);
    if (size > 0) GetGlyphOutlineW(dc_, ch, GGO_GRAY8_BITMAP, &gm, size, bits.data(), &kIdentity);
    return true;
}

bool GdiFace::GlyphMask(wchar_t ch, int pad, Mask& out) const {
    GLYPHMETRICS gm{};
    std::vector<BYTE> bits;
    if (!GlyphBits(ch, gm, bits)) return false;
    out = {};
    out.advance = gm.gmCellIncX;
    const int width = bits.empty() ? 0 : static_cast<int>(gm.gmBlackBoxX);
    const int height = bits.empty() ? 0 : static_cast<int>(gm.gmBlackBoxY);
    out.left = gm.gmptGlyphOrigin.x - pad;
    out.top = tm_.tmAscent - gm.gmptGlyphOrigin.y - pad;
    out.mask = MakeImage(width + 2 * pad, height + 2 * pad);
    const int pitch = (width + 3) & ~3;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int v = std::min<int>(bits[static_cast<std::size_t>(y) * pitch + x], 64);
            out.mask.A(pad + x, pad + y) = v / 64.0f;
        }
    }
    return true;
}

Image LineMask(const GdiFace& face, const std::wstring& text, int pad) {
    struct Placed {
        GdiFace::Mask m;
        int pen = 0;
    };
    std::vector<Placed> placed;
    int pen = 0, right = 0, bottom = face.CellHeight();
    for (const wchar_t ch : text) {
        Placed p;
        if (!face.Missing(std::wstring(1, ch)).empty() || !face.GlyphMask(ch, 0, p.m)) continue;
        p.pen = pen;
        pen += p.m.advance;
        right = std::max({right, pen, p.pen + p.m.left + p.m.mask.width});
        bottom = std::max(bottom, p.m.top + p.m.mask.height);
        placed.push_back(std::move(p));
    }
    // Ink left of the pen start or above the line top falls into the pad (or is cut beyond it).
    Image line = MakeImage(right + 2 * pad, bottom + 2 * pad);
    for (const Placed& p : placed) {
        for (int y = 0; y < p.m.mask.height; ++y) {
            for (int x = 0; x < p.m.mask.width; ++x) {
                const int lx = pad + p.pen + p.m.left + x, ly = pad + p.m.top + y;
                if (lx < 0 || ly < 0 || lx >= line.width || ly >= line.height) continue;
                line.A(lx, ly) = std::max(line.A(lx, ly), p.m.mask.A(x, y));
            }
        }
    }
    return line;
}

int HeightForCap(const std::wstring& face, int weight, int target, CapReference ref) {
    int height = 4;
    while (height < 600 && GdiFace(face, height, weight).Capitals(ref).height < target) ++height;
    return height;
}

int HeightForX(const std::wstring& face, int weight, int target) {
    int height = 4;
    while (height < 600 && GdiFace(face, height, weight).XHeight() < target) ++height;
    return height;
}

}  // namespace ddmod::text
