// Font comparison sheet for design decisions (CLAUDE.md rule 12): renders the same sample lines
// in the game's bitmap fonts (read from the installation), GDI fonts and font files, with the
// overlay's own text code, and saves a PNG. Output belongs outside the repo.
//
// fontpreview --out <png> [--bg R,G,B] entries...
//   --fnt <file.fnt> <label>                game bitmap font, drawn at 1x and 2x (nearest neighbor)
//   --gdi <face> <height> <weight> <label>  installed font, like CreateFontA(height, weight): cell height
//   --ttf <file> <face> <px> <weight> <label>  font file loaded for this process only, at px and 2*px
//   --overview <dir> <scale>                one line per .fnt file in <dir> (for grouping families)
//   --charsheet <file.fnt> <scale> <threshold%> <label>  A-Z, a-z, 0-9, umlauts, black on white, no smoothing
//                                           (for font identification services; use --bg 255,255,255)
//   --smooth <file.fnt> <label>             game glyphs at 2x and 4x: nearest neighbor, Scale2x,
//                                           distance field
//   --scene <spec.txt>                      real game lines on a crop of a game screenshot, once per
//                                           candidate font, drawn like the font mode (scene.cpp)

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>
#include <objidl.h>  // WIN32_LEAN_AND_MEAN leaves out what GDI+ needs
// GDI+ uses unqualified min/max, which NOMINMAX removes.
using std::max;
using std::min;
#include <gdiplus.h>

#include "overlay/fnt.h"
#include "overlay/glyphs.h"
#include "overlay/surface.h"
#include "scale.h"
#include "scene.h"

using namespace ddmod::overlay;

namespace {

const std::vector<std::wstring> kSamples = {
    L"Rivellon Renewed 0.01 – 42 FPS",
    L"Ärger über Größe – Übung macht den Meister",
    L"Élève à côté, œuvre & façade",
    L"Съешь же ещё этих "
    L"мягких булок",
};
const wchar_t* kCoverageNames[] = {L"overlay", L"German", L"French", L"Cyrillic"};

constexpr int kWidth = 1500;
constexpr int kMargin = 24;

struct Canvas {
    int width = 0, height = 0;
    std::vector<std::uint32_t> pixels;
    SurfaceView View() {
        return {reinterpret_cast<std::uint8_t*>(pixels.data()), width * 4, width, height, PixelFormat::Xrgb8888};
    }
};

Canvas MakeCanvas(int width, int height, Rgb bg) {
    Canvas c{width, height, std::vector<std::uint32_t>(static_cast<std::size_t>(width) * height)};
    FillRect(c.View(), 0, 0, width, height, bg, 255);
    return c;
}

// Copies `src` into `dst` at (x, y), each pixel as a scale x scale block.
void BlitScaled(Canvas& dst, const Canvas& src, int x, int y, int scale) {
    for (int sy = 0; sy < src.height; ++sy) {
        for (int sx = 0; sx < src.width; ++sx) {
            for (int k = 0; k < scale * scale; ++k) {
                const int dx = x + sx * scale + k % scale, dy = y + sy * scale + k / scale;
                if (dx >= 0 && dy >= 0 && dx < dst.width && dy < dst.height) {
                    dst.pixels[static_cast<std::size_t>(dy) * dst.width + dx] =
                        src.pixels[static_cast<std::size_t>(sy) * src.width + sx];
                }
            }
        }
    }
}

Canvas FromImage(const fontpreview::Image& img, Rgb bg) {
    return {img.width, img.height, fontpreview::Composite(img, bg)};
}

const std::vector<std::wstring> kCharSheet = {
    L"ABCDEFGHIJKLM", L"NOPQRSTUVWXYZ", L"abcdefghijklm", L"nopqrstuvwxyz", L"0123456789",
    L"ÄÖÜäöüß éèêàç .,;:!?-'()&",
};

// One block of the sheet: a caption and the sample lines.
struct Block {
    std::wstring caption;
    std::vector<Canvas> lines;  // already at final size
    bool warn = false;          // caption in red
};

Canvas RenderLine(GlyphCache& cache, const std::wstring& text, Rgb bg, Rgb color, int scale) {
    const int lineHeight = cache.LineHeight();
    Canvas line = MakeCanvas(std::max(1, MeasureString(cache, text) + 4), lineHeight + 4, bg);
    DrawString(line.View(), cache, 2, 2, text, color, false);
    if (scale == 1) return line;
    Canvas big = MakeCanvas(line.width * scale, line.height * scale, bg);
    BlitScaled(big, line, 0, 0, scale);
    return big;
}

// Which sample lines the font can draw completely (GDI fonts only).
std::wstring Coverage(const std::wstring& face, const std::wstring& file) {
    if (!file.empty()) AddFontResourceExW(file.c_str(), FR_PRIVATE, nullptr);
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT font = CreateFontW(-32, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_ONLY_PRECIS, 0, 0, 0,
                             face.c_str());
    HGDIOBJ old = SelectObject(dc, font);
    wchar_t actual[LF_FACESIZE] = {};
    GetTextFaceW(dc, LF_FACESIZE, actual);
    std::wstring result = std::wstring(L"[") + actual + L"]";
    for (std::size_t i = 0; i < kSamples.size(); ++i) {
        std::vector<WORD> idx(kSamples[i].size());
        GetGlyphIndicesW(dc, kSamples[i].c_str(), static_cast<int>(kSamples[i].size()), idx.data(),
                         GGI_MARK_NONEXISTING_GLYPHS);
        bool all = true;
        for (const WORD g : idx) all &= g != 0xFFFF;
        result += std::wstring(L"  ") + kCoverageNames[i] + (all ? L" ✓" : L" ✗");
    }
    SelectObject(dc, old);
    DeleteObject(font);
    DeleteDC(dc);
    return result;
}

bool SavePng(const Canvas& c, const std::wstring& path) {
    // PNG encoder of GDI+.
    const CLSID png = {0x557CF406, 0x1A04, 0x11D3, {0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E}};
    Gdiplus::Bitmap bitmap(c.width, c.height, c.width * 4, PixelFormat32bppRGB,
                           reinterpret_cast<BYTE*>(const_cast<std::uint32_t*>(c.pixels.data())));
    return bitmap.Save(path.c_str(), &png, nullptr) == Gdiplus::Ok;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring out;
    Rgb bg{28, 22, 16};
    const Rgb text{235, 225, 205};
    std::vector<Block> blocks;

    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &input, nullptr);

    std::unique_ptr<GlyphCache> captionFont;
    {
        FontSpec spec;
        spec.face = L"Segoe UI";
        spec.pixelHeight = 15;
        spec.bold = false;
        captionFont = std::make_unique<GlyphCache>(CreateGdiGlyphSource(spec));
    }

    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        const auto need = [&](int n) {
            if (i + n >= argc) {
                std::fwprintf(stderr, L"%ls needs %d arguments\n", arg.c_str(), n);
                std::exit(2);
            }
        };
        if (arg == L"--out") {
            need(1);
            out = argv[++i];
        } else if (arg == L"--bg") {
            need(1);
            int r = 0, g = 0, b = 0;
            swscanf_s(argv[++i], L"%d,%d,%d", &r, &g, &b);
            bg = {static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b)};
        } else if (arg == L"--fnt") {
            need(2);
            const std::wstring file = argv[++i], label = argv[++i];
            GlyphCache cache(CreateFntGlyphSource(file));
            if (cache.LineHeight() == 0) return 1;
            for (const int scale : {1, 2}) {
                Block block{label + (scale == 1 ? L" – original size" : L" – 2× (nearest neighbor)"), {}};
                for (const auto& s : kSamples) block.lines.push_back(RenderLine(cache, s, bg, text, scale));
                blocks.push_back(std::move(block));
            }
        } else if (arg == L"--overview") {
            need(2);
            const std::wstring dir = argv[++i];
            const int scale = _wtoi(argv[++i]);
            WIN32_FIND_DATAW found{};
            std::vector<std::wstring> names;
            const HANDLE find = FindFirstFileW((dir + L"\\*.fnt").c_str(), &found);
            if (find == INVALID_HANDLE_VALUE) return 1;
            do names.push_back(found.cFileName); while (FindNextFileW(find, &found));
            FindClose(find);
            std::sort(names.begin(), names.end());
            for (const auto& name : names) {
                GlyphCache cache(CreateFntGlyphSource(dir + L"\\" + name));
                if (cache.LineHeight() == 0) continue;
                const auto line = fontpreview::RenderLine(cache, L"Hamburgefonstiv ÄÖÜ äöüß 0123456789 ABCDEFG", text, 1);
                blocks.push_back({name + L" – line " + std::to_wstring(cache.LineHeight()) + L" px",
                                  {FromImage(fontpreview::ScaleNearest(line, scale), bg)}});
            }
        } else if (arg == L"--charsheet") {
            need(4);
            const std::wstring file = argv[++i];
            const int scale = _wtoi(argv[++i]);
            const float threshold = _wtoi(argv[++i]) / 100.0f;
            const std::wstring label = argv[++i];
            GlyphCache cache(CreateFntGlyphSource(file));
            if (cache.LineHeight() == 0) return 1;
            // Leave out characters the font does not have (they would show as '?').
            auto probe = CreateFntGlyphSource(file);
            Block block{label, {}};
            for (const auto& all : kCharSheet) {
                std::wstring s;
                for (const wchar_t ch : all) {
                    Glyph g;
                    if (probe->Rasterize(ch, g)) s += ch;
                }
                if (s.find_first_not_of(L' ') == std::wstring::npos) continue;
                const auto line = fontpreview::Binarize(fontpreview::RenderLine(cache, s, text, 2), threshold);
                block.lines.push_back(FromImage(fontpreview::ScaleNearest(line, scale), bg));
            }
            blocks.push_back(std::move(block));
        } else if (arg == L"--smooth") {
            need(2);
            const std::wstring file = argv[++i], label = argv[++i];
            GlyphCache cache(CreateFntGlyphSource(file));
            if (cache.LineHeight() == 0) return 1;
            std::vector<fontpreview::Image> lines;
            for (std::size_t k = 0; k < 2; ++k) lines.push_back(fontpreview::RenderLine(cache, kSamples[k], text, 2));
            const auto add = [&](const wchar_t* what, auto scaler) {
                Block block{label + L" – " + what, {}};
                for (const auto& l : lines) block.lines.push_back(FromImage(scaler(l), bg));
                blocks.push_back(std::move(block));
            };
            using fontpreview::Image;
            add(L"2× nearest neighbor (reference)", [](const Image& l) { return fontpreview::ScaleNearest(l, 2); });
            add(L"2× Scale2x", [](const Image& l) { return fontpreview::ScaleEpx(l); });
            add(L"2× distance field", [](const Image& l) { return fontpreview::ScaleSdf(l, 2); });
            add(L"4× nearest neighbor (reference)", [](const Image& l) { return fontpreview::ScaleNearest(l, 4); });
            add(L"4× Scale2x twice", [](const Image& l) { return fontpreview::ScaleEpx(fontpreview::ScaleEpx(l)); });
            add(L"4× distance field", [](const Image& l) { return fontpreview::ScaleSdf(l, 4); });
        } else if (arg == L"--scene") {
            need(1);
            std::vector<fontpreview::SceneRow> rows;
            std::wstring error;
            if (!fontpreview::RenderScene(argv[++i], rows, error)) {
                std::fwprintf(stderr, L"%ls\n", error.c_str());
                return 1;
            }
            for (auto& r : rows) {
                std::fwprintf(stdout, L"%ls\n", r.caption.c_str());
                Block block{r.caption, {}, r.warn};
                if (!r.pixels.empty()) block.lines.push_back({r.width, r.height, std::move(r.pixels)});
                blocks.push_back(std::move(block));
            }
        } else if (arg == L"--pixel") {
            // --pixel <file.fnt | face | font file> <face or -> <px> <weight> <label>: first two
            // sample lines at the given size, then 4x nearest neighbor - for pixel-level
            // comparison with a game font.
            need(5);
            const std::wstring source = argv[++i], face = argv[++i];
            const int px = _wtoi(argv[++i]), weight = _wtoi(argv[++i]);
            const std::wstring label = argv[++i];
            std::unique_ptr<GlyphSource> glyphs;
            if (source.ends_with(L".fnt")) {
                glyphs = CreateFntGlyphSource(source);
            } else {
                FontSpec spec;
                spec.face = face == L"-" ? source : face;
                if (face != L"-") spec.file = source;
                spec.pixelHeight = px;
                spec.bold = weight >= FW_SEMIBOLD;
                glyphs = CreateGdiGlyphSource(spec);
            }
            GlyphCache cache(std::move(glyphs));
            if (cache.LineHeight() == 0) return 1;
            for (const int scale : {1, 4}) {
                Block block{label + L" – " + (scale == 1 ? L"1×" : L"4× nearest neighbor"), {}};
                for (std::size_t k = 0; k < 2; ++k) {
                    const auto line = fontpreview::RenderLine(cache, kSamples[k], text, 2);
                    block.lines.push_back(FromImage(fontpreview::ScaleNearest(line, scale), bg));
                }
                blocks.push_back(std::move(block));
            }
        } else if (arg == L"--gdi" || arg == L"--ttf") {
            const bool isFile = arg == L"--ttf";
            need(isFile ? 5 : 4);
            FontSpec spec;
            std::wstring label;
            int weight = FW_NORMAL;
            if (isFile) {
                spec.file = argv[++i];
                spec.face = argv[++i];
                spec.pixelHeight = _wtoi(argv[++i]);
                weight = _wtoi(argv[++i]);
                label = argv[++i];
            } else {
                spec.face = argv[++i];
                spec.pixelHeight = _wtoi(argv[++i]);
                weight = _wtoi(argv[++i]);
                label = argv[++i];
                spec.cellHeight = true;
            }
            spec.bold = weight >= FW_SEMIBOLD;
            const std::wstring coverage = Coverage(spec.face, isFile ? spec.file.wstring() : std::wstring());
            std::fwprintf(stdout, L"%ls: %ls\n", label.c_str(), coverage.c_str());
            for (const int scale : {1, 2}) {
                FontSpec scaled = spec;
                scaled.pixelHeight = spec.pixelHeight * scale;
                GlyphCache cache(CreateGdiGlyphSource(scaled));
                if (cache.LineHeight() == 0) return 1;
                Block block{label + L" – " + std::to_wstring(scaled.pixelHeight) + L" px   " + coverage, {}};
                for (const auto& s : kSamples) block.lines.push_back(RenderLine(cache, s, bg, text, 1));
                blocks.push_back(std::move(block));
            }
        } else {
            std::fwprintf(stderr, L"unknown argument %ls\n", arg.c_str());
            return 2;
        }
    }
    if (out.empty() || blocks.empty()) {
        std::fwprintf(stderr, L"usage: fontpreview --out <png> [--bg R,G,B] --fnt|--gdi|--ttf ...\n");
        return 2;
    }

    int height = kMargin;
    for (const Block& b : blocks) {
        height += captionFont->LineHeight() + 6;
        for (const Canvas& l : b.lines) height += l.height;
        height += kMargin;
    }
    int width = kWidth;
    for (const Block& b : blocks) {
        for (const Canvas& l : b.lines) width = std::max(width, l.width + 2 * kMargin);
    }
    const bool light = bg.r + bg.g + bg.b > 3 * 128;
    Canvas sheet = MakeCanvas(width, height, bg);
    int y = kMargin;
    for (const Block& b : blocks) {
        FillRect(sheet.View(), kMargin, y - kMargin / 2, width - 2 * kMargin, 1,
                 light ? Rgb{200, 200, 200} : Rgb{90, 80, 65}, 255);
        const Rgb captionColor = b.warn ? Rgb{235, 80, 60} : light ? Rgb{60, 60, 60} : Rgb{150, 170, 200};
        DrawString(sheet.View(), *captionFont, kMargin, y, b.caption, captionColor, false);
        y += captionFont->LineHeight() + 6;
        for (const Canvas& l : b.lines) {
            BlitScaled(sheet, l, kMargin, y, 1);
            y += l.height;
        }
        y += kMargin;
    }

    const bool ok = SavePng(sheet, out);
    Gdiplus::GdiplusShutdown(token);
    std::fwprintf(stdout, ok ? L"saved %ls (%dx%d)\n" : L"could not save %ls\n", out.c_str(), sheet.width, sheet.height);
    return ok ? 0 : 1;
}
