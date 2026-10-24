#include "text/face.h"

#include <algorithm>
#include <climits>

#include "text/gdiface.h"
#include "text/ttface.h"

namespace ddmod::text {

int Face::CapTop() const {
    Box box;
    return GlyphBox(L'H', box) ? Ascent() + box.top : 0;
}

int Face::CapHeight() const {
    Box box;
    return GlyphBox(L'H', box) ? box.height : 0;
}

CapMetrics Face::Capitals(CapReference ref) const {
    if (ref == CapReference::H) return {CapTop(), CapHeight()};
    std::vector<int> tops, bottoms;
    for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) {
        Box box;
        if (!Missing(std::wstring(1, ch)).empty() || !GlyphBox(ch, box) || box.height == 0) continue;
        const int top = Ascent() + box.top;
        tops.push_back(top);
        bottoms.push_back(top + box.height - 1);
    }
    return MedianCap(std::move(tops), std::move(bottoms));
}

int Face::XHeight() const {
    Box box;
    return GlyphBox(L'x', box) ? box.height : 0;
}

void Face::InkRange(const std::wstring& text, int& top, int& bottom) const {
    top = INT_MAX;
    bottom = INT_MIN;
    for (const wchar_t ch : text) {
        Box box;
        if (ch == L' ' || !GlyphBox(ch, box)) continue;
        top = std::min(top, box.top);
        bottom = std::max(bottom, box.top + box.height);
    }
}

int Face::InkExtent(const std::wstring& text) const {
    int top = 0, bottom = 0;
    InkRange(text, top, bottom);
    return bottom > top ? bottom - top : 0;
}

std::unique_ptr<Face> OpenFace(const FaceSource& source, int height, int weight) {
    if (source.raster == Raster::Own && !source.file.empty()) return std::make_unique<TtFace>(source.file, height, weight);
    return std::make_unique<GdiFace>(source.face, height, weight);
}

Image LineMask(const Face& face, const std::wstring& text, int pad) {
    struct Placed {
        Face::Mask m;
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

int HeightForCap(const FaceSource& source, int weight, int target, CapReference ref) {
    int height = 4;
    while (height < 600 && OpenFace(source, height, weight)->Capitals(ref).height < target) ++height;
    return height;
}

int HeightForX(const FaceSource& source, int weight, int target) {
    int height = 4;
    while (height < 600 && OpenFace(source, height, weight)->XHeight() < target) ++height;
    return height;
}

int HeightForCap(const std::wstring& face, int weight, int target, CapReference ref) {
    return HeightForCap(FaceSource{face, {}, Raster::Gdi}, weight, target, ref);
}

int HeightForX(const std::wstring& face, int weight, int target) {
    return HeightForX(FaceSource{face, {}, Raster::Gdi}, weight, target);
}

}  // namespace ddmod::text
