// Font comparison sheet for design decisions (CLAUDE.md rule 12): renders the same sample lines
// in the game's bitmap fonts (read from the installation), GDI fonts and font files, with the
// overlay's own text code, and saves a PNG. Output belongs outside the repo.
//
// fontpreview --out <png> [--bg R,G,B] entries...
//   --fnt <file.fnt> <label>                game bitmap font, drawn at 1x and 2x (nearest neighbor)
//   --gdi <face> <height> <weight> <label>  installed font, like CreateFontA(height, weight): cell height
//   --ttf <file> <face> <px> <weight> <label>  font file loaded for this process only, at px and 2*px

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

using namespace ddmod::overlay;

namespace {

const std::vector<std::wstring> kSamples = {
    L"ddmod 0.1.0 active – 42 FPS",
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

// One block of the sheet: a caption and the sample lines.
struct Block {
    std::wstring caption;
    std::vector<Canvas> lines;  // already at final size
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
    Canvas sheet = MakeCanvas(kWidth, height, bg);
    int y = kMargin;
    for (const Block& b : blocks) {
        FillRect(sheet.View(), kMargin, y - kMargin / 2, kWidth - 2 * kMargin, 1, Rgb{90, 80, 65}, 255);
        DrawString(sheet.View(), *captionFont, kMargin, y, b.caption, Rgb{150, 170, 200}, false);
        y += captionFont->LineHeight() + 6;
        for (const Canvas& l : b.lines) {
            BlitScaled(sheet, l, kMargin, y, 1);
            y += l.height;
        }
        y += kMargin;
    }

    Gdiplus::GdiplusStartupInput input;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &input, nullptr);
    const bool ok = SavePng(sheet, out);
    Gdiplus::GdiplusShutdown(token);
    std::fwprintf(stdout, ok ? L"saved %ls (%dx%d)\n" : L"could not save %ls\n", out.c_str(), sheet.width, sheet.height);
    return ok ? 0 : 1;
}
