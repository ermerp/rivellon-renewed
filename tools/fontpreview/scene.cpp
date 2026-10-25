// Scene sheets: each candidate font draws the same real game lines on the same crop of a real
// game screen, the way the game would draw them, at 1x and as a 2x copy for detail.
//
// Spec file (UTF-8, one entry per line, fields separated by '|', '#' starts a comment):
//   title|<text>                                  section caption
//   background|<png>|<x>|<y>|<w>|<h>              crop of a game screenshot (mirrored if lines are wider)
//   fntdir|<dir>                                  the game's fonts folder (read only)
//   align|left|center    spacing|<px>    top|<px>    left|<px>
//   line|<file.fnt>|<GDI height>|<text RRGGBB>|<edge RRGGBB>|<text>
//                                                 one game line: its bitmap font, and for GDI fonts
//                                                 the game's height and the mod's colors
//   original|<label>                              the bitmap fonts as the game draws them
//   smooth|<label>                                bitmap fonts; the 2x copy smoothed (distance field)
//   font|<label>|<face>|<font file or empty>|<weight>[|option...]  options: match, capitals, ramp|gradient|
//                                                 gloss|measured, lower, bold<n>|thin<n>, track<n>,
//                                                 fit<n> (largest height whose ink - the lines plus
//                                                 accented capitals and all extenders - fits in n px),
//                                                 matchx (lowercase x as high as in the bitmap font,
//                                                 same baseline), squeeze<n> (ascenders and
//                                                 descenders compressed so the ink fits in n px)
//   alt|<face>|<file>|<weight>|<from>|<to>        following font candidates draw the characters in
//                                                 <from> as the same-position characters of <to> in
//                                                 this face, at the same capital height; `alt` alone
//                                                 ends it
//                                                 a GDI font as in the font mode: positive height
//                                                 (cell), GGO_GRAY8_BITMAP, 65-step ramp from the
//                                                 edge to the text color, RGB565, the font's own
//                                                 advance ([Fonts] Spacing=font). `match`: height
//                                                 so the capital H is as high as in the line's
//                                                 bitmap font, and at the same place (`capitals`:
//                                                 the median capital over A-Z instead of the H,
//                                                 also for the color profile). `gradient`: color profile measured
//                                                 from the bitmap font plus a contour in the edge
//                                                 color; `gloss`: also light upper and dark lower
//                                                 stroke edges; `measured`: profile plus the edge
//                                                 shading measured from the bitmap font. `lower`:
//                                                 text in lowercase (the uncial forms of unicase
//                                                 faces). `bold<n>`/`thin<n>`: stroke weight +/- n
//                                                 quarter pixels. `track<n>`: n extra pixels per
//                                                 character
//   banner|<label>|<x>|<baseline y>[|center]      a mixed line on the whole background crop: the
//                                                 following parts, until `endbanner`, each in its
//                                                 own font, capitals standing on the baseline;
//                                                 `center` centers each line on x; a 2x copy of the
//                                                 banner's surroundings goes to the right
//   part|<gap>|<newline>|<file.fnt>|<text RRGGBB>|<edge RRGGBB>|<face>|<font file>|<weight>|<options>|<text>
//                                                 one part: `gap` pixels after the previous part,
//                                                 `newline` > 0 starts a new line that many pixels
//                                                 lower; options as for `font`, space-separated
//   endbanner
//   charset|<file.fnt>|<scale>|<text RRGGBB>|<edge RRGGBB>|<bg RRGGBB>
//                                                 instead of background/line: the whole character
//                                                 set, the bitmap font scaled by <scale> and every
//                                                 GDI font sized to the same capital height

#include "scene.h"

#include <algorithm>
#include <climits>
#include <fstream>
#include <memory>
#include <sstream>

#include <windows.h>
#include <objidl.h>
using std::max;
using std::min;
#include <gdiplus.h>

#include "overlay/fnt.h"
#include "overlay/glyphs.h"
#include "scale.h"
#include "text/gdiface.h"
#include "text/style.h"

namespace fontpreview {
namespace {

using ddmod::overlay::Glyph;
using ddmod::overlay::GlyphCache;
using ddmod::text::GdiFace;
namespace text = ddmod::text;

const std::wstring kGerman = L"ÄÖÜäöüß";
const std::wstring kFrench = L"ÀÂÇÈÉÊËÎÏÔÙÛàâçèéêëîïôùûÿŒœÆæ«»";
const std::wstring kRussian =
    L"АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя";

struct Picture {
    int width = 0, height = 0;
    std::vector<Rgb> px;
    Rgb& At(int x, int y) { return px[static_cast<std::size_t>(y) * width + x]; }
};

struct Line {
    std::wstring fnt;
    int height = 0;
    Rgb text, edge;
    std::wstring str;
};

struct Spec {
    std::wstring bgFile;
    int bgX = 0, bgY = 0, bgW = 0, bgH = 0;
    std::wstring fntDir;
    bool center = false;
    int spacing = 20, top = 8, left = 12;
    std::vector<Line> lines;
    // Second face for single characters (Candidate::altFace), set by an `alt` entry.
    std::wstring altFace, altFile, altFrom, altTo;
    int altWeight = 400;
    // Character set mode.
    std::wstring charsetFnt;
    int charsetScale = 1;
    Rgb text, edge, bg;
};

const std::vector<std::wstring> kCharset = {
    L"ABCDEFGHIJKLM", L"NOPQRSTUVWXYZ", L"ÄÖÜ ÀÂÇÉÈÊËÎÔÙ", L"abcdefghijklm", L"nopqrstuvwxyz",
    L"äöüß àâçéèêëîïôûù", L"0123456789 .,;:!?-'\"()&%",
};

std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    std::wstring w(static_cast<std::size_t>(MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0)), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), static_cast<int>(w.size()));
    return w;
}

std::vector<std::wstring> Split(const std::wstring& s) {
    std::vector<std::wstring> parts;
    std::size_t start = 0;
    for (;;) {
        const std::size_t bar = s.find(L'|', start);
        parts.push_back(s.substr(start, bar - start));
        if (bar == std::wstring::npos) return parts;
        start = bar + 1;
    }
}

Rgb ParseHex(const std::wstring& s) {
    const unsigned long v = std::wcstoul(s.c_str(), nullptr, 16);
    return {static_cast<std::uint8_t>(v >> 16), static_cast<std::uint8_t>(v >> 8), static_cast<std::uint8_t>(v)};
}

bool LoadCrop(const Spec& spec, Picture& out) {
    Gdiplus::Bitmap bmp(spec.bgFile.c_str());
    if (bmp.GetLastStatus() != Gdiplus::Ok) return false;
    Gdiplus::Rect rect(spec.bgX, spec.bgY, spec.bgW, spec.bgH);
    Gdiplus::BitmapData data{};
    if (bmp.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppRGB, &data) != Gdiplus::Ok) return false;
    out = {spec.bgW, spec.bgH, std::vector<Rgb>(static_cast<std::size_t>(spec.bgW) * spec.bgH)};
    for (int y = 0; y < spec.bgH; ++y) {
        const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(data.Scan0) + y * data.Stride);
        for (int x = 0; x < spec.bgW; ++x) {
            out.At(x, y) = {static_cast<std::uint8_t>(row[x] >> 16), static_cast<std::uint8_t>(row[x] >> 8),
                            static_cast<std::uint8_t>(row[x])};
        }
    }
    bmp.UnlockBits(&data);
    return true;
}

// The crop widened to `width`, centered and mirrored at its edges.
Picture Widen(const Picture& crop, int width) {
    Picture out{width, crop.height, std::vector<Rgb>(static_cast<std::size_t>(width) * crop.height)};
    const int shift = (width - crop.width) / 2, period = 2 * crop.width;
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < width; ++x) {
            int sx = ((x - shift) % period + period) % period;
            if (sx >= crop.width) sx = period - 1 - sx;
            out.At(x, y) = crop.px[static_cast<std::size_t>(y) * crop.width + sx];
        }
    }
    return out;
}

Picture Nearest(const Picture& src, int scale) {
    Picture out{src.width * scale, src.height * scale, std::vector<Rgb>(src.px.size() * scale * scale)};
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) out.At(x, y) = src.px[static_cast<std::size_t>(y / scale) * src.width + x / scale];
    }
    return out;
}

void Blend(Picture& dst, const Image& img, int x0, int y0) {
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const int dx = x0 + x, dy = y0 + y;
            const float a = img.A(x, y);
            if (a <= 0.0f || dx < 0 || dy < 0 || dx >= dst.width || dy >= dst.height) continue;
            dst.At(dx, dy) = ddmod::overlay::Mix(dst.At(dx, dy), img.C(x, y), static_cast<std::uint8_t>(a * 255.0f + 0.5f));
        }
    }
}

constexpr int kPad = 3;

// A line in a game bitmap font with the glyphs' own colors (pad of kPad on every side).
Image FntLine(GlyphCache& cache, const std::wstring& text) {
    Image img = MakeImage(ddmod::overlay::MeasureString(cache, text) + 2 * kPad, cache.LineHeight() + 2 * kPad);
    int pen = kPad;
    for (const wchar_t ch : text) {
        const Glyph& g = cache.Get(ch);
        for (int y = 0; y < g.height; ++y) {
            for (int x = 0; x < g.width; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * g.width + x;
                const int ox = pen + g.offsetX + x, oy = kPad + g.offsetY + y;
                if (g.coverage[i] == 0 || ox < 0 || oy < 0 || ox >= img.width || oy >= img.height) continue;
                img.A(ox, oy) = g.coverage[i] / 255.0f;
                img.C(ox, oy) = g.colors.empty() ? Rgb{255, 255, 255} : g.colors[i];
            }
        }
        pen += g.advance;
    }
    return img;
}

// Draws like the game's font mode: gray level 0 transparent, 1-64 opaque on the ramp.
// `tracking` adds pixels after every character.
Image RampLine(const GdiFace& font, const std::wstring& text, Rgb textColor, Rgb edgeColor, int tracking = 0) {
    struct Placed {
        GLYPHMETRICS gm{};
        std::vector<BYTE> bits;
        int pen = 0;
    };
    std::vector<Placed> placed;
    int pen = kPad, right = 0;
    for (const wchar_t ch : text) {
        Placed p;
        p.pen = pen;
        const bool has = font.GlyphBits(ch, p.gm, p.bits);
        if (has) {
            right = std::max(right, pen + static_cast<int>(p.gm.gmptGlyphOrigin.x + p.gm.gmBlackBoxX));
            pen += p.gm.gmCellIncX + tracking;
        }
        placed.push_back(std::move(p));
    }
    Image img = MakeImage(std::max(pen, right) + kPad, font.CellHeight() + 2 * kPad);
    Rgb ramp[65];
    for (int v = 0; v <= 64; ++v) {
        const auto lerp = [v](std::uint8_t e, std::uint8_t t) {
            return static_cast<std::uint8_t>((e * (64 - v) + t * v + 32) / 64);
        };
        ramp[v] = text::To565({lerp(edgeColor.r, textColor.r), lerp(edgeColor.g, textColor.g), lerp(edgeColor.b, textColor.b)});
    }
    for (const Placed& p : placed) {
        if (p.bits.empty()) continue;
        const int pitch = (static_cast<int>(p.gm.gmBlackBoxX) + 3) & ~3;
        for (int y = 0; y < static_cast<int>(p.gm.gmBlackBoxY); ++y) {
            for (int x = 0; x < static_cast<int>(p.gm.gmBlackBoxX); ++x) {
                const int v = std::min<int>(p.bits[static_cast<std::size_t>(y) * pitch + x], 64);
                const int ox = p.pen + p.gm.gmptGlyphOrigin.x + x;
                const int oy = kPad + font.Ascent() - p.gm.gmptGlyphOrigin.y + y;
                if (v == 0 || ox < 0 || oy < 0 || ox >= img.width || oy >= img.height) continue;
                img.A(ox, oy) = 1.0f;
                img.C(ox, oy) = ramp[v];
            }
        }
    }
    return img;
}

enum class Kind { Original, Smooth, Font };
// Ramp: the game's two-color ramp. Gradient: measured color profile plus a one-pixel contour in
// the edge color. Gloss: gradient plus a fixed light upper and dark lower stroke edge. Measured:
// profile plus the edge shading measured from the bitmap font (Bevel), no extra contour.
enum class Style { Ramp, Gradient, Gloss, Measured, Shine };

struct Candidate {
    Kind kind;
    std::wstring label, face, file;
    int weight = 600;
    bool matchHeight = false;  // capitals as high as in the bitmap font instead of the game's height
    text::CapReference capitals = text::CapReference::H;  // which capitals `match` compares
    Style style = Style::Ramp;
    bool lower = false;  // draw the text in lowercase (unicase faces keep their uncial forms there)
    int morph = 0;       // stroke weight in quarter pixels: > 0 bolder, < 0 thinner
    int tracking = 0;    // extra pixels after every character
    int fit = 0;         // > 0: largest height whose ink over all lines' characters fits in this many pixels
    bool matchX = false; // lowercase x as high as in the bitmap font
    int squeeze = 0;     // > 0: ascenders and descenders compressed so the ink fits in this many pixels
    // Characters drawn from a second face (a mix like the original's roman capitals with uncial
    // forms): altFrom[i] in the text is drawn as altTo[i] of altFace, scaled to the same capital height.
    std::wstring altFace, altFile, altFrom, altTo;
    int altWeight = 400;
    float shineStrength = 1.0f;  // Style::Shine: 1 = as measured
    bool texture = false;        // Style::Shine: add our own noise of the measured texture strength
};

// The capitals A-Z of a bitmap font, the input of the style measurements (src/text/style.h).
text::Capitals Caps(GlyphCache& cache) {
    text::Capitals caps;
    for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) caps.push_back(cache.Get(ch));
    return caps;
}

// Distance from the top of the line to the first row of the capitals (the H by default) in a
// bitmap font.
int FntCapTop(GlyphCache& cache, text::CapReference ref = text::CapReference::H) {
    return text::MeasureCap(Caps(cache), ref).top;
}

// Rows with any pixel of the capitals (the H by default) in a bitmap font.
int FntCapHeight(GlyphCache& cache, text::CapReference ref = text::CapReference::H) {
    return text::MeasureCap(Caps(cache), ref).height;
}
// Coverage of a line (alpha 0-1) with the same layout as RampLine. Characters listed in
// `from` are taken from `alt` (character `to[i]`), standing on the same baseline.
Image Coverage(const GdiFace& font, const std::wstring& text, int tracking, const GdiFace* alt = nullptr,
               const std::wstring& from = {}, const std::wstring& to = {}) {
    struct Placed {
        GLYPHMETRICS gm{};
        std::vector<BYTE> bits;
        int pen = 0;
    };
    std::vector<Placed> placed;
    int pen = kPad, right = 0;
    for (const wchar_t ch : text) {
        const std::size_t k = alt ? from.find(ch) : std::wstring::npos;
        const bool useAlt = k != std::wstring::npos && k < to.size();
        Placed p;
        p.pen = pen;
        if (!(useAlt ? alt->GlyphBits(to[k], p.gm, p.bits) : font.GlyphBits(ch, p.gm, p.bits))) continue;
        right = std::max(right, pen + static_cast<int>(p.gm.gmptGlyphOrigin.x + p.gm.gmBlackBoxX));
        pen += p.gm.gmCellIncX + tracking;
        placed.push_back(std::move(p));
    }
    Image mask = MakeImage(std::max(pen, right) + kPad, font.CellHeight() + 2 * kPad);
    for (const Placed& p : placed) {
        if (p.bits.empty()) continue;
        const int pitch = (static_cast<int>(p.gm.gmBlackBoxX) + 3) & ~3;
        for (int y = 0; y < static_cast<int>(p.gm.gmBlackBoxY); ++y) {
            for (int x = 0; x < static_cast<int>(p.gm.gmBlackBoxX); ++x) {
                const int v = std::min<int>(p.bits[static_cast<std::size_t>(y) * pitch + x], 64);
                const int ox = p.pen + p.gm.gmptGlyphOrigin.x + x;
                const int oy = kPad + font.Ascent() - p.gm.gmptGlyphOrigin.y + y;
                if (v == 0 || ox < 0 || oy < 0 || ox >= mask.width || oy >= mask.height) continue;
                mask.A(ox, oy) = std::max(mask.A(ox, oy), v / 64.0f);
            }
        }
    }
    return mask;
}

// Compresses the rows above the x-height and below the baseline so the ink of `text` (plus the
// tallest and deepest characters any text may hold) fits in `budget` pixels; the band between
// x-height and baseline keeps its size.
Image Squeeze(const Image& mask, const GdiFace& font, int budget, const std::wstring& text) {
    int top = 0, bottom = 0;
    font.InkRange(L"ÄÖÜÁÉbdfghjklpqyß" + text, top, bottom);
    const int xh = font.XHeight();
    const int ascender = -top - xh, descender = bottom;
    if (ascender + descender <= 0) return mask;
    const float k = std::clamp(static_cast<float>(budget - xh) / static_cast<float>(ascender + descender), 0.3f, 1.0f);
    const float baseline = static_cast<float>(kPad + font.Ascent()), xTop = baseline - static_cast<float>(xh);
    Image out = MakeImage(mask.width, mask.height);
    for (int y = 0; y < out.height; ++y) {
        const float yo = y + 0.5f;
        const float ys = yo < xTop ? xTop - (xTop - yo) / k : yo > baseline ? baseline + (yo - baseline) / k : yo;
        const float sy = ys - 0.5f;
        const int y0 = static_cast<int>(std::floor(sy));
        const float f = sy - static_cast<float>(y0);
        for (int x = 0; x < out.width; ++x) {
            const float a0 = y0 >= 0 && y0 < mask.height ? mask.A(x, y0) : 0.0f;
            const float a1 = y0 + 1 >= 0 && y0 + 1 < mask.height ? mask.A(x, y0 + 1) : 0.0f;
            out.A(x, y) = a0 * (1.0f - f) + a1 * f;
        }
    }
    return out;
}

// Grows (steps > 0) or shrinks (steps < 0) the coverage by one pixel per step (4-neighborhood).
Image Morph(Image mask, int steps) {
    for (int s = 0; s < std::abs(steps); ++s) {
        Image next = mask;
        for (int y = 0; y < mask.height; ++y) {
            for (int x = 0; x < mask.width; ++x) {
                float v = mask.A(x, y);
                const int nx[4] = {x - 1, x + 1, x, x}, ny[4] = {y, y, y - 1, y + 1};
                for (int k = 0; k < 4; ++k) {
                    const float n = nx[k] < 0 || ny[k] < 0 || nx[k] >= mask.width || ny[k] >= mask.height
                                        ? 0.0f
                                        : mask.A(nx[k], ny[k]);
                    v = steps > 0 ? std::max(v, n) : std::min(v, n);
                }
                next.A(x, y) = v;
            }
        }
        mask = std::move(next);
    }
    return mask;
}

// Box-filters a coverage rendered at `f` times the size back down, keeping the kPad margin.
Image Downsample(const Image& hi, int f) {
    Image out = MakeImage((hi.width - 2 * kPad) / f + 2 * kPad + 1, (hi.height - 2 * kPad) / f + 2 * kPad + 1);
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            float sum = 0.0f;
            for (int k = 0; k < f * f; ++k) {
                const int hx = kPad + (x - kPad) * f + k % f, hy = kPad + (y - kPad) * f + k / f;
                if (hx >= 0 && hy >= 0 && hx < hi.width && hy < hi.height) sum += hi.A(hx, hy);
            }
            out.A(x, y) = sum / static_cast<float>(f * f);
        }
    }
    return out;
}

// The candidate's coverage at `height`: stroke weight changes and squeezing are made at 4x and
// filtered down.
Image CandidateMask(const Candidate& c, int height, const std::wstring& text) {
    const int f = c.morph != 0 || c.squeeze > 0 ? 4 : 1;
    const GdiFace font(c.face, height * f, c.weight);
    std::unique_ptr<GdiFace> alt;
    if (!c.altFace.empty()) {
        if (!c.altFile.empty()) AddFontResourceExW(c.altFile.c_str(), FR_PRIVATE, nullptr);
        alt = std::make_unique<GdiFace>(c.altFace, text::HeightForCap(c.altFace, c.altWeight, font.CapHeight()), c.altWeight);
    }
    Image mask = Coverage(font, text, c.tracking * f, alt.get(), c.altFrom, c.altTo);
    if (c.morph != 0) mask = Morph(std::move(mask), c.morph);
    if (c.squeeze > 0) mask = Squeeze(mask, font, c.squeeze * f, text);
    return f == 1 ? mask : Downsample(mask, f);
}

// Rows with any pixel of `ch` in a bitmap font.
int FntInkHeight(GlyphCache& cache, wchar_t ch) {
    const Glyph& g = cache.Get(ch);
    int top = g.height, bottom = -1;
    for (int y = 0; y < g.height; ++y) {
        for (int x = 0; x < g.width; ++x) {
            if (g.coverage[static_cast<std::size_t>(y) * g.width + x] != 0) {
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }
    return bottom >= top ? bottom - top + 1 : 0;
}

// Appends the face actually used and the coverage per language to the row's caption; a missing
// face or missing German characters mark the candidate as out.
void AddCoverage(const GdiFace& font, const std::wstring& face, SceneRow& row) {
    const std::wstring actual = font.ActualFace();
    const std::wstring de = font.Missing(kGerman), fr = font.Missing(kFrench), ru = font.Missing(kRussian);
    row.caption += L"   [" + actual + L"]   DE " + (de.empty() ? L"✓" : L"✗") + L"  FR " +
                   (fr.empty() ? L"✓" : L"✗ (" + std::to_wstring(fr.size()) + L" missing)") + L"  RU " +
                   (ru.empty() ? L"✓" : L"✗");
    if (_wcsicmp(actual.c_str(), face.c_str()) != 0) {
        row.warn = true;
        row.caption += L"   OUT: face not found";
    } else if (!de.empty()) {
        row.warn = true;
        row.caption += L"   OUT: no " + de;
    }
}

SceneRow ToRow(SceneRow row, const Picture& p) {
    row.width = p.width;
    row.height = p.height;
    row.pixels.resize(p.px.size());
    for (std::size_t i = 0; i < p.px.size(); ++i) row.pixels[i] = ddmod::overlay::Pack(ddmod::overlay::PixelFormat::Xrgb8888, p.px[i]);
    return row;
}

// The whole character set, all candidates at the same capital height.
SceneRow RenderCharset(const Spec& spec, const Candidate& c) {
    SceneRow row;
    row.caption = c.label;
    const std::filesystem::path fnt = std::filesystem::path(spec.fntDir) / spec.charsetFnt;
    GlyphCache reference(ddmod::overlay::CreateFntGlyphSource(fnt));
    const int target = FntCapHeight(reference) * spec.charsetScale;
    std::vector<Image> images;
    if (c.kind == Kind::Font) {
        if (!c.file.empty()) AddFontResourceExW(c.file.c_str(), FR_PRIVATE, nullptr);
        const int height = text::HeightForCap(c.face, c.weight, target);
        const GdiFace font(c.face, height, c.weight);
        AddCoverage(font, c.face, row);
        row.caption += L"   (GDI height " + std::to_wstring(height) + L")";
        for (const auto& s : kCharset) images.push_back(RampLine(font, s, spec.text, spec.edge));
    } else {
        // Leave out characters the bitmap font does not have (they would show as '?').
        auto probe = ddmod::overlay::CreateFntGlyphSource(fnt);
        for (const auto& all : kCharset) {
            std::wstring s;
            for (const wchar_t ch : all) {
                Glyph g;
                s += ch == L' ' || probe->Rasterize(ch, g) ? ch : L' ';
            }
            const Image line = FntLine(reference, s);
            images.push_back(c.kind == Kind::Smooth ? ScaleSdf(line, spec.charsetScale)
                                                    : ScaleNearest(line, spec.charsetScale));
        }
    }
    constexpr int kMargin = 16;
    int width = 0, height = kMargin;
    for (const Image& img : images) {
        width = std::max(width, img.width + 2 * kMargin);
        height += img.height;
    }
    Picture p{width, height + kMargin, std::vector<Rgb>(static_cast<std::size_t>(width) * (height + kMargin), spec.bg)};
    int y = kMargin;
    for (const Image& img : images) {
        Blend(p, img, kMargin, y);
        y += img.height;
    }
    return ToRow(std::move(row), p);
}

// One spec line in a font candidate: the painted image (kPad on every side), the GDI height and
// the vertical shift that puts its capitals where the original's are (matched candidates).
struct LineImage {
    Image image;
    int height = 0;
    int shift = 0;
};
LineImage FontLine(const Spec& spec, const Candidate& c, const Line& line, int fitHeight, bool printMeasures) {
    GlyphCache original(ddmod::overlay::CreateFntGlyphSource(std::filesystem::path(spec.fntDir) / line.fnt));
    LineImage out;
    out.height = c.fit > 0          ? std::max(4, fitHeight)
                 : c.matchX      ? text::HeightForX(c.face, c.weight, FntInkHeight(original, L'x'))
                 : c.matchHeight ? text::HeightForCap(c.face, c.weight, FntCapHeight(original, c.capitals), c.capitals)
                                 : line.height;
    const int height = out.height;
    const GdiFace font(c.face, height, c.weight);
    if (c.matchX) {
        // Same baseline as the original (the row under its capital H).
        out.shift = FntCapTop(original) + FntCapHeight(original) - font.Ascent();
    } else if (c.matchHeight || c.fit > 0) {
        out.shift = FntCapTop(original, c.capitals) - font.Capitals(c.capitals).top;
    }
    std::wstring str = line.str;
    if (c.lower) CharLowerBuffW(str.data(), static_cast<DWORD>(str.size()));
    if (c.style == Style::Ramp) {
        const bool plain = c.morph == 0 && c.squeeze == 0 && c.altFace.empty();
        out.image = plain ? RampLine(font, str, line.text, line.edge, c.tracking)
                          : text::PaintRamp(CandidateMask(c, height, str), line.text, line.edge);
        return out;
    }
    const std::vector<Rgb> profile = text::MeasureProfile(Caps(original), c.capitals);
    const text::Bevel bevel = text::MeasureBevel(Caps(original), profile, c.capitals);
    if (c.style == Style::Measured && printMeasures) {
        std::fwprintf(stdout, L"  %ls: profile", line.fnt.c_str());
        for (const Rgb p : profile) std::fwprintf(stdout, L" %02X%02X%02X", p.r, p.g, p.b);
        std::fwprintf(stdout, L"\n  edge factors (top, bottom, left, right; 1-3 px):");
        for (const auto& s : bevel.side) std::fwprintf(stdout, L"  %.2f %.2f %.2f", s[0], s[1], s[2]);
        std::fwprintf(stdout, L"\n");
    }
    const text::Shine shine = text::MeasureShine(Caps(original));
    if (c.style == Style::Shine && printMeasures) {
        std::fwprintf(stdout, L"  %ls: across strokes (horizontal | vertical):", line.fnt.c_str());
        for (const auto& d : shine.across) {
            for (const float v : d) std::fwprintf(stdout, L" %.2f", v);
            std::fwprintf(stdout, L" |");
        }
        std::fwprintf(stdout, L" texture %.3f\n", shine.texture);
    }
    const bool own = c.style == Style::Measured || c.style == Style::Shine;
    text::StyleParams style;
    style.profile = profile;
    style.contour = line.edge;
    style.outline = !own;
    style.gloss = c.style == Style::Gloss;
    style.bevel = c.style == Style::Measured ? &bevel : nullptr;
    style.shine = c.style == Style::Shine ? &shine : nullptr;
    style.shineStrength = c.shineStrength;
    style.texture = c.texture;
    const text::CapMetrics faceCap = font.Capitals(c.capitals);
    out.image = text::PaintStyled(CandidateMask(c, height, str), kPad + faceCap.top, faceCap.height, style);
    return out;
}

// Lays the lines out on the background and returns the row (1x left, 2x right).
SceneRow RenderCandidate(const Spec& spec, const Picture& crop, const Candidate& c) {
    SceneRow row;
    row.caption = c.label;
    std::vector<Image> images;
    std::vector<int> shifts(spec.lines.size(), 0);  // matched fonts: capital H top where the original's is
    if (c.kind == Kind::Font) {
        if (!c.file.empty()) AddFontResourceExW(c.file.c_str(), FR_PRIVATE, nullptr);
        bool faceChecked = false;
        std::wstring heights;
        int fitHeight = 0;
        if (c.fit > 0) {
            // Also the tallest and deepest characters any text may hold, not only those of the sample.
            std::wstring all = L"ÄÖÜÁÉbdfghjklpqyß";
            for (const Line& line : spec.lines) all += line.str;
            for (int h = 4; h < 200 && GdiFace(c.face, h, c.weight).InkExtent(all) <= c.fit; ++h) fitHeight = h;
        }
        for (const Line& line : spec.lines) {
            const LineImage li = FontLine(spec, c, line, fitHeight, images.empty());
            if (!faceChecked) {
                faceChecked = true;
                AddCoverage(GdiFace(c.face, li.height, c.weight), c.face, row);
            }
            if (heights.find(L" " + std::to_wstring(li.height) + L" ") == std::wstring::npos) {
                heights += L" " + std::to_wstring(li.height) + L" ";
            }
            shifts[images.size()] = li.shift;
            images.push_back(li.image);
        }
        if (c.fit > 0) {
            row.caption += L"   (GDI height" + heights + L"= largest without touching lines " + std::to_wstring(c.fit) +
                           L" px apart)";
        } else if (c.matchX) {
            row.caption += L"   (GDI height" + heights + L"= lowercase x as in the original" +
                           (c.squeeze > 0 ? L", extenders squeezed into " + std::to_wstring(c.squeeze) + L" px)" : L")");
        } else if (c.matchHeight) {
            row.caption += L"   (GDI height" + heights +
                           (c.capitals == text::CapReference::H ? L"= capital H as in the original)"
                                                                : L"= capitals (median) as in the original)");
        }
    } else {
        for (const Line& line : spec.lines) {
            GlyphCache cache(ddmod::overlay::CreateFntGlyphSource(std::filesystem::path(spec.fntDir) / line.fnt));
            images.push_back(FntLine(cache, line.str));
        }
    }

    int width = crop.width;
    for (const Image& img : images) width = std::max(width, img.width + 2 * spec.left);
    Picture one = Widen(crop, width);
    Picture two = Nearest(one, 2);
    for (std::size_t i = 0; i < images.size(); ++i) {
        const int x = spec.center ? (width - images[i].width) / 2 : spec.left - kPad;
        const int y = spec.top + static_cast<int>(i) * spec.spacing - kPad + shifts[i];
        Blend(one, images[i], x, y);
        if (c.kind == Kind::Smooth) Blend(two, ScaleSdf(images[i], 2), 2 * x, 2 * y);
    }
    if (c.kind != Kind::Smooth) two = Nearest(one, 2);

    constexpr int kGap = 24;
    row.width = one.width + kGap + two.width;
    row.height = two.height;
    row.pixels.assign(static_cast<std::size_t>(row.width) * row.height, 0x00101010);
    const auto put = [&](const Picture& p, int x0) {
        for (int y = 0; y < p.height; ++y) {
            for (int x = 0; x < p.width; ++x) {
                row.pixels[static_cast<std::size_t>(y) * row.width + x0 + x] =
                    ddmod::overlay::Pack(ddmod::overlay::PixelFormat::Xrgb8888, p.px[static_cast<std::size_t>(y) * p.width + x]);
            }
        }
    };
    put(one, 0);
    put(two, one.width + kGap);
    return row;
}

// A banner: parts in different fonts on common baselines over the background (the whole crop at
// 1x), plus a 2x copy of the area around it.
struct BannerPart {
    Candidate c;
    Line line;
    int gap = 0;      // pixels after the previous part
    int newline = 0;  // > 0: starts a new line this many pixels below the previous baseline
};
struct Banner {
    std::wstring label;
    int x = 0, baseline = 0;
    bool center = false;  // lines centered on x instead of starting there
    std::vector<BannerPart> parts;
};

SceneRow RenderBanner(const Spec& spec, const Picture& crop, const Banner& b) {
    SceneRow row;
    row.caption = b.label;
    struct Placed {
        LineImage li;
        int x = 0, top = 0;  // line top of the part's font
    };
    std::vector<std::vector<Placed>> lines;
    std::vector<int> baselines;
    int baseline = b.baseline;
    for (const BannerPart& part : b.parts) {
        if (lines.empty() || part.newline > 0) {
            if (!lines.empty()) baseline += part.newline;
            lines.emplace_back();
            baselines.push_back(baseline);
        }
        if (!part.c.file.empty()) AddFontResourceExW(part.c.file.c_str(), FR_PRIVATE, nullptr);
        GlyphCache original(ddmod::overlay::CreateFntGlyphSource(std::filesystem::path(spec.fntDir) / part.line.fnt));
        Placed p;
        p.li = FontLine(spec, part.c, part.line, 0, false);
        // The capitals' bottom row on the baseline.
        p.top = baseline - FntCapTop(original, part.c.capitals) - FntCapHeight(original, part.c.capitals) + 1;
        auto& line = lines.back();
        p.x = line.empty() ? 0 : line.back().x + line.back().li.image.width - 2 * kPad + part.gap;
        line.push_back(std::move(p));
    }
    Picture one = crop;
    int left = INT_MAX, top = INT_MAX, right = INT_MIN, bottom = INT_MIN;
    for (auto& line : lines) {
        const int width = line.empty() ? 0 : line.back().x + line.back().li.image.width - 2 * kPad;
        const int x0 = b.center ? b.x - width / 2 : b.x;
        for (const Placed& p : line) {
            const int x = x0 + p.x - kPad, y = p.top - kPad + p.li.shift;
            Blend(one, p.li.image, x, y);
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x + p.li.image.width);
            bottom = std::max(bottom, y + p.li.image.height);
        }
    }
    // 2x copy of the banner's surroundings.
    constexpr int kAround = 24;
    left = std::max(0, left - kAround);
    top = std::max(0, top - kAround);
    right = std::min(one.width, right + kAround);
    bottom = std::min(one.height, bottom + kAround);
    Picture area{std::max(1, right - left), std::max(1, bottom - top), {}};
    area.px.resize(static_cast<std::size_t>(area.width) * area.height);
    for (int y = 0; y < area.height; ++y) {
        for (int x = 0; x < area.width; ++x) area.At(x, y) = one.At(left + x, top + y);
    }
    const Picture two = Nearest(area, 2);

    constexpr int kGap = 24;
    row.width = one.width + kGap + two.width;
    row.height = std::max(one.height, two.height);
    row.pixels.assign(static_cast<std::size_t>(row.width) * row.height, 0x00101010);
    const auto put = [&](const Picture& p, int x0) {
        for (int y = 0; y < p.height; ++y) {
            for (int x = 0; x < p.width; ++x) {
                row.pixels[static_cast<std::size_t>(y) * row.width + x0 + x] =
                    ddmod::overlay::Pack(ddmod::overlay::PixelFormat::Xrgb8888, p.px[static_cast<std::size_t>(y) * p.width + x]);
            }
        }
    };
    put(one, 0);
    put(two, one.width + kGap);
    return row;
}

// One candidate option (`font` entries, space-separated in banner parts).
void ParseOption(Candidate& c, const std::wstring& t) {
    if (t == L"match") c.matchHeight = true;
    else if (t == L"capitals") c.capitals = text::CapReference::Median;
    else if (t == L"gradient") c.style = Style::Gradient;
    else if (t == L"gloss") c.style = Style::Gloss;
    else if (t == L"measured") c.style = Style::Measured;
    else if (t == L"lower") c.lower = true;
    else if (t.starts_with(L"bold")) c.morph = _wtoi(t.c_str() + 4);
    else if (t.starts_with(L"thin")) c.morph = -_wtoi(t.c_str() + 4);
    else if (t.starts_with(L"track")) c.tracking = _wtoi(t.c_str() + 5);
    else if (t.starts_with(L"fit")) c.fit = _wtoi(t.c_str() + 3);
    else if (t == L"matchx") c.matchX = true;
    else if (t == L"tex") c.texture = true;
    else if (t.starts_with(L"shine")) {
        c.style = Style::Shine;
        if (t.size() > 5) c.shineStrength = static_cast<float>(_wtoi(t.c_str() + 5)) / 100.0f;
    }
    else if (t.starts_with(L"squeeze")) c.squeeze = _wtoi(t.c_str() + 7);
}

}  // namespace

bool RenderScene(const std::filesystem::path& specFile, std::vector<SceneRow>& rows, std::wstring& error) {
    std::ifstream in(specFile, std::ios::binary);
    if (!in) {
        error = L"cannot read " + specFile.wstring();
        return false;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::wstring text = Widen(buffer.str());
    if (!text.empty() && text[0] == 0xFEFF) text.erase(0, 1);

    Spec spec;
    Picture crop;
    Banner banner;
    bool inBanner = false;
    std::wistringstream lines(text);
    std::wstring raw;
    while (std::getline(lines, raw)) {
        if (!raw.empty() && raw.back() == L'\r') raw.pop_back();
        if (raw.empty() || raw[0] == L'#') continue;
        const auto f = Split(raw);
        const std::wstring& key = f[0];
        const auto need = [&](std::size_t n) {
            if (f.size() < n + 1) error = key + L" needs " + std::to_wstring(n) + L" fields: " + raw;
            return f.size() >= n + 1;
        };
        if (key == L"title" && need(1)) {
            rows.push_back({f[1], false, 0, 0, {}});
        } else if (key == L"background" && need(5)) {
            spec.bgFile = f[1];
            spec.bgX = _wtoi(f[2].c_str());
            spec.bgY = _wtoi(f[3].c_str());
            spec.bgW = _wtoi(f[4].c_str());
            spec.bgH = _wtoi(f[5].c_str());
            if (!LoadCrop(spec, crop)) {
                error = L"cannot load background " + spec.bgFile;
                return false;
            }
        } else if (key == L"fntdir" && need(1)) {
            spec.fntDir = f[1];
        } else if (key == L"align" && need(1)) {
            spec.center = f[1] == L"center";
        } else if (key == L"spacing" && need(1)) {
            spec.spacing = _wtoi(f[1].c_str());
        } else if (key == L"top" && need(1)) {
            spec.top = _wtoi(f[1].c_str());
        } else if (key == L"left" && need(1)) {
            spec.left = _wtoi(f[1].c_str());
        } else if (key == L"line" && need(5)) {
            spec.lines.push_back({f[1], _wtoi(f[2].c_str()), ParseHex(f[3]), ParseHex(f[4]), f[5]});
        } else if (key == L"alt") {
            // alt|<face>|<file>|<weight>|<from>|<to> for the following candidates; alt alone clears it.
            spec.altFace = f.size() > 5 ? f[1] : L"";
            spec.altFile = f.size() > 5 ? f[2] : L"";
            spec.altWeight = f.size() > 5 ? _wtoi(f[3].c_str()) : 400;
            spec.altFrom = f.size() > 5 ? f[4] : L"";
            spec.altTo = f.size() > 5 ? f[5] : L"";
        } else if (key == L"charset" && need(5)) {
            spec.charsetFnt = f[1];
            spec.charsetScale = std::max(1, _wtoi(f[2].c_str()));
            spec.text = ParseHex(f[3]);
            spec.edge = ParseHex(f[4]);
            spec.bg = ParseHex(f[5]);
        } else if ((key == L"original" || key == L"smooth" || key == L"font") && need(key == L"font" ? 4 : 1)) {
            const bool charset = !spec.charsetFnt.empty();
            if (!charset && (crop.px.empty() || spec.lines.empty())) {
                error = L"charset, or background and lines, must come before " + key;
                return false;
            }
            Candidate c{key == L"original" ? Kind::Original : key == L"smooth" ? Kind::Smooth : Kind::Font, f[1], {}, {}};
            if (c.kind == Kind::Font) {
                c.face = f[2];
                c.file = f[3];
                c.weight = _wtoi(f[4].c_str());
                for (std::size_t k = 5; k < f.size(); ++k) ParseOption(c, f[k]);
                c.altFace = spec.altFace;
                c.altFile = spec.altFile;
                c.altWeight = spec.altWeight;
                c.altFrom = spec.altFrom;
                c.altTo = spec.altTo;
            }
            rows.push_back(charset ? RenderCharset(spec, c) : RenderCandidate(spec, crop, c));
        } else if (key == L"banner" && need(3)) {
            if (crop.px.empty()) {
                error = L"background must come before banner";
                return false;
            }
            banner = Banner{f[1], _wtoi(f[2].c_str()), _wtoi(f[3].c_str()), f.size() > 4 && f[4] == L"center", {}};
            inBanner = true;
        } else if (key == L"part" && need(10)) {
            if (!inBanner) {
                error = L"part outside banner: " + raw;
                return false;
            }
            BannerPart part;
            part.gap = _wtoi(f[1].c_str());
            part.newline = _wtoi(f[2].c_str());
            part.line = {f[3], 0, ParseHex(f[4]), ParseHex(f[5]), f[10]};
            part.c = Candidate{Kind::Font, {}, f[6], f[7]};
            part.c.weight = _wtoi(f[8].c_str());
            std::wistringstream options(f[9]);
            for (std::wstring t; options >> t;) ParseOption(part.c, t);
            banner.parts.push_back(std::move(part));
        } else if (key == L"endbanner") {
            if (!inBanner || banner.parts.empty()) {
                error = L"endbanner without banner parts";
                return false;
            }
            rows.push_back(RenderBanner(spec, crop, banner));
            inBanner = false;
        } else if (error.empty()) {
            error = L"unknown entry: " + raw;
        }
        if (!error.empty()) return false;
    }
    return true;
}

}  // namespace fontpreview
