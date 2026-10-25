#include "arrows.h"

#include <algorithm>
#include <memory>

#include <windows.h>
#include <objidl.h>
using std::max;
using std::min;
#include <gdiplus.h>

#include "fonts/familypaint.h"
#include "fonts/families.h"
#include "overlay/fnt.h"
#include "overlay/glyphs.h"
#include "text/gdiface.h"
#include "text/style.h"
#include "text/symbols.h"

namespace fontpreview {
namespace {

using ddmod::overlay::Glyph;
using ddmod::overlay::GlyphCache;
using ddmod::overlay::Rgb;
using ddmod::text::Image;
namespace fonts = ddmod::fonts;
namespace text = ddmod::text;

constexpr int kZoom = 3;
constexpr int kLeft = 12;
constexpr int kValueGap = 40;   // from the left arrow's pen to the value, and from the value to the right arrow
constexpr int kLineGap = 6;
const std::wstring kValue = L"1.00";
const wchar_t* kStates[] = {L"fontbig36", L"fontbig36highlight", L"fontbig36gray"};

struct Picture {
    int width = 0, height = 0;
    std::vector<Rgb> px;
    Rgb& At(int x, int y) { return px[static_cast<std::size_t>(y) * width + x]; }
};

bool LoadCrop(const std::filesystem::path& file, int x, int y, int w, int h, Picture& out) {
    Gdiplus::Bitmap bmp(file.c_str());
    if (bmp.GetLastStatus() != Gdiplus::Ok) return false;
    Gdiplus::Rect rect(x, y, w, h);
    Gdiplus::BitmapData data{};
    if (bmp.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppRGB, &data) != Gdiplus::Ok) return false;
    out = {w, h, std::vector<Rgb>(static_cast<std::size_t>(w) * h)};
    for (int r = 0; r < h; ++r) {
        const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(data.Scan0) + r * data.Stride);
        for (int c = 0; c < w; ++c) {
            out.At(c, r) = {static_cast<std::uint8_t>(row[c] >> 16), static_cast<std::uint8_t>(row[c] >> 8),
                            static_cast<std::uint8_t>(row[c])};
        }
    }
    bmp.UnlockBits(&data);
    return true;
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

// An original glyph as an image with its own colors, placed by its offsets.
Image FntGlyph(const Glyph& g) {
    Image img = text::MakeImage(g.width, g.height);
    for (std::size_t i = 0; i < g.coverage.size(); ++i) {
        img.alpha[i] = g.coverage[i] / 255.0f;
        img.color[i] = g.colors.empty() ? Rgb{255, 255, 255} : g.colors[i];
    }
    return img;
}

struct Variant {
    const wchar_t* name;
    bool original;
    text::ArrowShape shape;
    float width;  // relative to the original arrow's width
};
const Variant kVariants[] = {
    {L"Original (game glyphs)", true, text::ArrowShape::Triangle, 1.0f},
    {L"A – triangle, the original's size", false, text::ArrowShape::Triangle, 1.0f},
    {L"B – slim triangle (75 % width)", false, text::ArrowShape::Triangle, 0.75f},
    {L"C – spear head (notched back)", false, text::ArrowShape::Notched, 1.0f},
    {L"D – bowed sides", false, text::ArrowShape::Convex, 1.0f},
    {L"E – thorn (sides bowed in)", false, text::ArrowShape::Concave, 1.0f},
};

// One state's line on `pic`: left arrow, value, right arrow; top of the line at `top`.
bool DrawLine(Picture& pic, const std::filesystem::path& fntDir, const std::wstring& stem, const Variant& v, int top,
              std::wstring& error) {
    GlyphCache font(ddmod::overlay::CreateFntGlyphSource(fntDir / (stem + L".fnt")));
    if (font.LineHeight() <= 0) {
        error = L"cannot read " + stem + L".fnt";
        return false;
    }
    const Glyph& right = font.Get(L'[');
    const Glyph& left = font.Get(L']');
    const int valueX = kLeft + left.advance + kValueGap;
    if (v.original) {
        Blend(pic, FntGlyph(left), kLeft + left.offsetX, top + left.offsetY);
        int pen = valueX;
        for (const wchar_t ch : kValue) {
            const Glyph& g = font.Get(ch);
            Blend(pic, FntGlyph(g), pen + g.offsetX, top + g.offsetY);
            pen += g.advance;
        }
        Blend(pic, FntGlyph(right), pen + kValueGap + right.offsetX, top + right.offsetY);
        return true;
    }
    const fonts::FamilySettings settings = fonts::DefaultSettings(fonts::Family::Menu);
    text::Capitals caps;
    for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) caps.push_back(font.Get(ch));
    const text::CapMetrics cap = text::MeasureCap(caps, settings.capitals);
    const std::wstring face = L"Uncial Antiqua";
    const int height = text::HeightForCap(face, settings.weight, cap.height, settings.capitals);
    const text::GdiFace gdi(face, height, settings.weight);
    if (!gdi.Valid()) {
        error = L"GDI cannot create Uncial Antiqua";
        return false;
    }
    const text::CapMetrics faceCap = gdi.Capitals(settings.capitals);
    std::string narrow;
    for (const wchar_t ch : stem) narrow += static_cast<char>(ch);  // ASCII file names
    const fonts::FontColors colors = fonts::ColorsOf(narrow, fonts::Family::Menu, caps);
    const fonts::FamilyPaint paint(settings, caps, fonts::ToRgb(colors.text), fonts::ToRgb(colors.edge));

    // The same own arrows as the glyph renderer draws (fonts::PaintArrow).
    const auto arrow = [&](const Glyph& g, bool pointsRight, int pen) {
        const fonts::OwnSymbol s =
            fonts::PaintArrow(g, v.shape, pointsRight, fonts::ToRgb(colors.text), fonts::ToRgb(colors.edge), v.width);
        Blend(pic, s.image, pen + s.left, top + s.top);
    };
    arrow(left, false, kLeft);
    constexpr int kPad = 3;
    const Image value = paint.Paint(text::LineMask(gdi, kValue, kPad), kPad + faceCap.top, faceCap.height);
    Blend(pic, value, valueX - kPad, top + cap.top - faceCap.top - kPad);
    arrow(right, true, valueX + value.width - 2 * kPad + kValueGap);
    return true;
}

}  // namespace

bool RenderArrows(const std::filesystem::path& fntDir, const std::filesystem::path& faceDir,
                  const std::filesystem::path& background, int x, int y, int w, int h, std::vector<SceneRow>& rows,
                  std::wstring& error) {
    const std::filesystem::path faceFile = faceDir / L"UncialAntiqua-Regular.ttf";
    if (AddFontResourceExW(faceFile.c_str(), FR_PRIVATE, nullptr) == 0) {
        error = L"cannot load " + faceFile.wstring();
        return false;
    }
    Picture crop;
    if (!LoadCrop(background, x, y, w, h, crop)) {
        error = L"cannot read the crop of " + background.wstring();
        return false;
    }
    GlyphCache probe(ddmod::overlay::CreateFntGlyphSource(fntDir / L"fontbig36.fnt"));
    const int lineStep = probe.LineHeight() + kLineGap;
    rows.push_back({L"Options arrows – rows: normal, highlight (selected), gray (end of the range); 1x and 3x", false});
    for (const Variant& v : kVariants) {
        Picture pic = crop;
        for (int s = 0; s < 3; ++s) {
            if (!DrawLine(pic, fntDir, kStates[s], v, kLineGap + s * lineStep, error)) return false;
        }
        // 1x on the left, 3x of the same picture on the right.
        SceneRow row{v.name, false, pic.width * (1 + kZoom) + 16, pic.height * kZoom, {}};
        row.pixels.assign(static_cast<std::size_t>(row.width) * row.height, 0x1C1610);
        for (int py = 0; py < row.height; ++py) {
            for (int px = 0; px < row.width; ++px) {
                Rgb c;
                if (px < pic.width && py < pic.height) {
                    c = pic.At(px, py);
                } else if (px >= pic.width + 16) {
                    c = pic.At((px - pic.width - 16) / kZoom, py / kZoom);
                } else {
                    continue;
                }
                row.pixels[static_cast<std::size_t>(py) * row.width + px] = static_cast<std::uint32_t>(c.r) << 16 |
                                                                            c.g << 8 | c.b;
            }
        }
        rows.push_back(std::move(row));
    }
    return true;
}

}  // namespace fontpreview
