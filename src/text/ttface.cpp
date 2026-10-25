#include "text/ttface.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cwchar>
#include <fstream>
#include <iterator>
#include <mutex>

#pragma warning(push, 0)
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#pragma warning(pop)

namespace ddmod::text {

struct FontFile {
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
    int unitsPerEm = 0;
    int winAscent = 0;
    int winDescent = 0;
    int weightClass = 400;
};

namespace {

// Sub-pixel samples per pixel and direction (GDI's GGO_GRAY8_BITMAP: 8 x 8 = 64, plus 0).
constexpr int kSamples = 8;
constexpr int kLevels = kSamples * kSamples;

int U16(const std::vector<unsigned char>& d, std::size_t at) { return d[at] << 8 | d[at + 1]; }
int S16(const std::vector<unsigned char>& d, std::size_t at) { return static_cast<std::int16_t>(U16(d, at)); }

// Offset and length of a table in the font's table directory at `start`; false if absent or
// outside the file.
bool FindTable(const std::vector<unsigned char>& d, std::size_t start, const char* tag, std::size_t& offset,
               std::size_t& length) {
    if (d.size() < start + 12) return false;
    const int count = U16(d, start + 4);
    for (int i = 0; i < count; ++i) {
        const std::size_t entry = start + 12 + 16 * static_cast<std::size_t>(i);
        if (d.size() < entry + 16) return false;
        if (std::equal(tag, tag + 4, d.begin() + static_cast<std::ptrdiff_t>(entry))) {
            offset = static_cast<std::size_t>(U16(d, entry + 8)) << 16 | U16(d, entry + 10);
            length = static_cast<std::size_t>(U16(d, entry + 12)) << 16 | U16(d, entry + 14);
            return offset + length <= d.size();
        }
    }
    return false;
}

std::shared_ptr<const FontFile> Load(const std::filesystem::path& file) {
    auto font = std::make_shared<FontFile>();
    std::ifstream in(file, std::ios::binary);
    font->data.assign(std::istreambuf_iterator<char>(in), {});
    if (font->data.empty()) return nullptr;
    const int start = stbtt_GetFontOffsetForIndex(font->data.data(), 0);
    if (start < 0 || !stbtt_InitFont(&font->info, font->data.data(), start)) return nullptr;
    const std::vector<unsigned char>& d = font->data;
    std::size_t at = 0, length = 0;
    if (!FindTable(d, static_cast<std::size_t>(start), "head", at, length) || length < 54) return nullptr;
    font->unitsPerEm = U16(d, at + 18);
    if (FindTable(d, static_cast<std::size_t>(start), "OS/2", at, length) && length >= 78) {
        font->weightClass = U16(d, at + 4);
        font->winAscent = U16(d, at + 74);
        font->winDescent = U16(d, at + 76);
    } else if (FindTable(d, static_cast<std::size_t>(start), "hhea", at, length) && length >= 8) {
        font->winAscent = S16(d, at + 4);
        font->winDescent = -S16(d, at + 6);
    }
    if (font->unitsPerEm <= 0 || font->winAscent + font->winDescent <= 0) return nullptr;
    return font;
}

// Font files by path, loaded once for the process (the glyph renderer opens a face per size).
std::shared_ptr<const FontFile> Cached(const std::filesystem::path& file) {
    static std::mutex mutex;
    static std::map<std::filesystem::path, std::shared_ptr<const FontFile>> files;
    const std::lock_guard lock(mutex);
    auto [it, inserted] = files.try_emplace(file);
    if (inserted) it->second = Load(file);
    return it->second;
}

int FloorDiv(int v, int d) { return v >= 0 ? v / d : -((-v + d - 1) / d); }

// A glyph's outline in font units times kUnitSteps (stbtt_vertex holds shorts; the steps keep
// the composite offsets rounded to the pixel grid exact enough).
constexpr int kUnitSteps = 4;
constexpr int kMaxDepth = 8;

int ReadU16(const unsigned char* p) { return p[0] << 8 | p[1]; }
int ReadS16(const unsigned char* p) { return static_cast<std::int16_t>(ReadU16(p)); }
float ReadF2Dot14(const unsigned char* p) { return ReadS16(p) / 16384.0f; }

short ToShort(float v) { return static_cast<short>(std::clamp(std::lround(v), -32768L, 32767L)); }

// A component's place in its composite glyph rounded to whole pixels (ROUND_XY_TO_GRID), like the
// scaler does: the offset scaled to 26.6 fixed point, then rounded to the nearest pixel.
float GridOffset(float units, float scale) {
    const int fixed = static_cast<int>(std::floor(units * scale * 64.0f));
    return static_cast<float>(FloorDiv(fixed + 32, 64)) / scale;
}

// The outline of `glyph` at `scale` (pixels per font unit), in font units times kUnitSteps.
// stb_truetype builds simple glyphs; composite glyphs are assembled here, because stb_truetype
// ignores ROUND_XY_TO_GRID and GDI does not (the dots of the umlauts would sit up to half a pixel
// apart).
void AppendShape(const stbtt_fontinfo* info, int glyph, float scale, int depth, std::vector<stbtt_vertex>& out) {
    const int offset = info->glyf ? stbtt__GetGlyfOffset(info, glyph) : -1;
    const unsigned char* g = offset >= 0 ? info->data + offset : nullptr;
    if (!g || ReadS16(g) >= 0 || depth >= kMaxDepth) {
        stbtt_vertex* v = nullptr;
        const int n = stbtt_GetGlyphShape(info, glyph, &v);
        for (int i = 0; i < n; ++i) {
            stbtt_vertex w = v[i];
            w.x = static_cast<short>(w.x * kUnitSteps);
            w.y = static_cast<short>(w.y * kUnitSteps);
            w.cx = static_cast<short>(w.cx * kUnitSteps);
            w.cy = static_cast<short>(w.cy * kUnitSteps);
            w.cx1 = static_cast<short>(w.cx1 * kUnitSteps);
            w.cy1 = static_cast<short>(w.cy1 * kUnitSteps);
            out.push_back(w);
        }
        stbtt_FreeShape(info, v);
        return;
    }
    // Composite: flags, glyph index, arguments, optional transform per component.
    const unsigned char* p = g + 10;
    for (bool more = true; more;) {
        const int flags = ReadU16(p);
        const int component = ReadU16(p + 2);
        p += 4;
        // Without ARGS_ARE_XY_VALUES the arguments are point numbers to match (not supported: no offset).
        float dx = 0, dy = 0;
        if (flags & 0x0001) {  // ARG_1_AND_2_ARE_WORDS
            if (flags & 0x0002) {
                dx = static_cast<float>(ReadS16(p));
                dy = static_cast<float>(ReadS16(p + 2));
            }
            p += 4;
        } else {
            if (flags & 0x0002) {
                dx = static_cast<float>(static_cast<signed char>(p[0]));
                dy = static_cast<float>(static_cast<signed char>(p[1]));
            }
            p += 2;
        }
        float a = 1, b = 0, c = 0, d = 1;
        if (flags & 0x0008) {  // WE_HAVE_A_SCALE
            a = d = ReadF2Dot14(p);
            p += 2;
        } else if (flags & 0x0040) {  // WE_HAVE_AN_X_AND_Y_SCALE
            a = ReadF2Dot14(p);
            d = ReadF2Dot14(p + 2);
            p += 4;
        } else if (flags & 0x0080) {  // WE_HAVE_A_TWO_BY_TWO
            a = ReadF2Dot14(p);
            b = ReadF2Dot14(p + 2);
            c = ReadF2Dot14(p + 4);
            d = ReadF2Dot14(p + 6);
            p += 8;
        }
        if ((flags & 0x0002) && (flags & 0x0004)) {  // ROUND_XY_TO_GRID
            dx = GridOffset(dx, scale);
            dy = GridOffset(dy, scale);
        }
        std::vector<stbtt_vertex> part;
        AppendShape(info, component, scale, depth + 1, part);
        const float ox = dx * kUnitSteps, oy = dy * kUnitSteps;
        const auto place = [&](short& x, short& y) {
            const float px = x, py = y;
            x = ToShort(a * px + c * py + ox);
            y = ToShort(b * px + d * py + oy);
        };
        for (stbtt_vertex& v : part) {
            place(v.x, v.y);
            place(v.cx, v.cy);
            place(v.cx1, v.cy1);
            out.push_back(v);
        }
        more = (flags & 0x0020) != 0;  // MORE_COMPONENTS
    }
}

// A coordinate (font units times kUnitSteps) in pixels the way GDI's metrics round it: scaled
// down to 26.6 fixed point, then to the nearest pixel.
int MetricPixel(int steps, float scale) {
    const int fixed = static_cast<int>(std::floor(steps * scale * 64.0f / kUnitSteps));
    return FloorDiv(fixed + 32, 64);
}

}  // namespace

TtFace::TtFace(const std::filesystem::path& file, int height, int weight) : font_(Cached(file)) {
    if (!font_) return;
    const int cell = font_->winAscent + font_->winDescent;
    // The metrics' scale in 16.16 fixed point, like GDI (decides advances that end on half a pixel).
    scale_ = static_cast<float>(std::lround(height * 65536.0 / cell) / 65536.0);
    const double ppem = std::floor(static_cast<double>(height) * font_->unitsPerEm / cell + 0.5);
    outline_ = static_cast<float>(std::max(1.0, ppem) / font_->unitsPerEm);
    ascent_ = static_cast<int>(std::lround(font_->winAscent * scale_));
    descent_ = static_cast<int>(std::lround(font_->winDescent * scale_));
    bold_ = weight >= 600 && font_->weightClass < 600;
}

TtFace::~TtFace() = default;

std::wstring TtFace::Missing(const std::wstring& chars) const {
    std::wstring missing;
    for (const wchar_t ch : chars) {
        if (!font_ || stbtt_FindGlyphIndex(&font_->info, ch) == 0) missing += ch;
    }
    return missing;
}

const TtFace::Raster& TtFace::Rasterize(wchar_t ch) const {
    auto [it, inserted] = cache_.try_emplace(ch);
    Raster& r = it->second;
    if (!inserted) return r;
    const stbtt_fontinfo* info = &font_->info;
    const int glyph = stbtt_FindGlyphIndex(info, ch);  // 0 = the missing glyph, like GDI
    int advance = 0, bearing = 0;
    stbtt_GetGlyphHMetrics(info, glyph, &advance, &bearing);
    r.box.advance = static_cast<int>(std::lround(advance * scale_)) + (bold_ ? 1 : 0);
    r.metrics.advance = r.box.advance;
    if (stbtt_IsGlyphEmpty(info, glyph)) return r;
    std::vector<stbtt_vertex> shape;
    AppendShape(info, glyph, outline_, 0, shape);
    if (shape.empty()) return r;

    // GGO_METRICS' box: the box of every outline point, on and off the curve.
    int xMin = INT_MAX, xMax = INT_MIN, yMin = INT_MAX, yMax = INT_MIN;
    const auto extend = [&](int x, int y) {
        xMin = std::min(xMin, x);
        xMax = std::max(xMax, x);
        yMin = std::min(yMin, y);
        yMax = std::max(yMax, y);
    };
    for (const stbtt_vertex& v : shape) {
        extend(v.x, v.y);
        if (v.type == STBTT_vcurve || v.type == STBTT_vcubic) extend(v.cx, v.cy);
        if (v.type == STBTT_vcubic) extend(v.cx1, v.cy1);
    }
    const int left = MetricPixel(xMin, outline_), right = MetricPixel(xMax, outline_);
    const int top = MetricPixel(yMax, outline_), bottom = MetricPixel(yMin, outline_);
    r.metrics.left = left;
    r.metrics.top = -top;
    r.metrics.width = right - left + (bold_ ? 1 : 0);
    r.metrics.height = top - bottom;

    // The outline at kSamples times the size, exact area coverage per sub-pixel; a sample counts
    // when its sub-pixel is at least half covered (its center lies inside).
    const float fine = outline_ * kSamples / kUnitSteps;
    const int x0 = static_cast<int>(std::floor(xMin * fine)), x1 = static_cast<int>(std::ceil(xMax * fine));
    const int y0 = static_cast<int>(std::floor(-yMax * fine)), y1 = static_cast<int>(std::ceil(-yMin * fine));
    const int fw = x1 - x0, fh = y1 - y0;
    if (fw <= 0 || fh <= 0) return r;
    std::vector<unsigned char> sub(static_cast<std::size_t>(fw) * fh);
    stbtt__bitmap bitmap{fw, fh, fw, sub.data()};
    stbtt_Rasterize(&bitmap, 0.35f, shape.data(), static_cast<int>(shape.size()), fine, fine, 0, 0, x0, y0, 1, nullptr);
    const int px0 = FloorDiv(x0, kSamples), py0 = FloorDiv(y0, kSamples);
    const int pw = FloorDiv(x1 - 1, kSamples) + 1 - px0, ph = FloorDiv(y1 - 1, kSamples) + 1 - py0;
    std::vector<std::uint8_t> count(static_cast<std::size_t>(pw) * ph, 0);
    for (int y = 0; y < fh; ++y) {
        const int row = FloorDiv(y0 + y, kSamples) - py0;
        for (int x = 0; x < fw; ++x) {
            if (sub[static_cast<std::size_t>(y) * fw + x] >= 128) ++count[static_cast<std::size_t>(row) * pw + FloorDiv(x0 + x, kSamples) - px0];
        }
    }
    int inkLeft = pw, inkRight = -1, inkTop = ph, inkBottom = -1;
    for (int y = 0; y < ph; ++y) {
        for (int x = 0; x < pw; ++x) {
            if (!count[static_cast<std::size_t>(y) * pw + x]) continue;
            inkLeft = std::min(inkLeft, x);
            inkRight = std::max(inkRight, x);
            inkTop = std::min(inkTop, y);
            inkBottom = std::max(inkBottom, y);
        }
    }
    if (inkRight < 0) return r;
    // Bold: every level plus its left neighbor's, the box one pixel wider.
    const int extra = bold_ ? 1 : 0;
    r.box.left = px0 + inkLeft;
    r.box.top = py0 + inkTop;
    r.box.width = inkRight - inkLeft + 1 + extra;
    r.box.height = inkBottom - inkTop + 1;
    r.levels.assign(static_cast<std::size_t>(r.box.width) * r.box.height, 0);
    const auto at = [&](int x, int y) {
        return x < 0 || x > inkRight - inkLeft ? 0 : count[static_cast<std::size_t>(inkTop + y) * pw + inkLeft + x];
    };
    for (int y = 0; y < r.box.height; ++y) {
        for (int x = 0; x < r.box.width; ++x) {
            const int level = at(x, y) + (bold_ ? at(x - 1, y) : 0);
            r.levels[static_cast<std::size_t>(y) * r.box.width + x] = static_cast<std::uint8_t>(std::min(level, kLevels));
        }
    }
    return r;
}

bool TtFace::GlyphBox(wchar_t ch, Box& box) const {
    if (!font_) return false;
    box = Rasterize(ch).metrics;
    return true;
}

bool TtFace::GlyphMask(wchar_t ch, int pad, Mask& out) const {
    if (!font_) return false;
    const Raster& r = Rasterize(ch);
    out = {};
    out.advance = r.box.advance;
    out.left = r.box.left - pad;
    out.top = ascent_ + r.box.top - pad;
    out.mask = MakeImage(r.box.width + 2 * pad, r.box.height + 2 * pad);
    for (int y = 0; y < r.box.height; ++y) {
        for (int x = 0; x < r.box.width; ++x) {
            out.mask.A(pad + x, pad + y) = r.levels[static_cast<std::size_t>(y) * r.box.width + x] / static_cast<float>(kLevels);
        }
    }
    return true;
}

bool CanRasterize(const std::filesystem::path& file) { return Cached(file) != nullptr; }

bool HoldsFace(const std::filesystem::path& file, const std::wstring& face) {
    const auto font = Cached(file);
    if (!font) return false;
    // Family (1), full name (4) and typographic family (16), Windows platform, Unicode BMP.
    for (const int nameId : {1, 4, 16}) {
        for (const int language : {0x0409, 0}) {
            int length = 0;
            const char* name = stbtt_GetFontNameString(&font->info, &length, STBTT_PLATFORM_ID_MICROSOFT,
                                                       STBTT_MS_EID_UNICODE_BMP, language, nameId);
            if (!name || length <= 0) continue;
            std::wstring text;
            for (int i = 0; i + 1 < length; i += 2) {
                text.push_back(static_cast<wchar_t>(static_cast<unsigned char>(name[i]) << 8 | static_cast<unsigned char>(name[i + 1])));
            }
            if (_wcsicmp(text.c_str(), face.c_str()) == 0) return true;
        }
    }
    return false;
}

int CellHeightForEm(const std::filesystem::path& file, int em) {
    const auto font = Cached(file);
    if (!font || font->unitsPerEm <= 0) return 0;
    return static_cast<int>(std::lround(static_cast<double>(em) * (font->winAscent + font->winDescent) / font->unitsPerEm));
}

}  // namespace ddmod::text
