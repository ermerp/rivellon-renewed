#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "text/style.h"

namespace ddmod::text {

// A vector face at one size, sized like GDI's CreateFont with a positive height (cell height) and
// a weight (600 and up emboldens faces that have only a regular cut). Coverage has 65 levels like
// GGO_GRAY8_BITMAP. Implemented by GDI (GdiFace) and by our own rasterizer (TtFace), which repeats
// GDI's measured sizing, sampling and bold (docs/re/gdi-raster.md).
class Face {
public:
    virtual ~Face() = default;

    virtual bool Valid() const = 0;
    // Characters of `chars` the face has no glyph for.
    virtual std::wstring Missing(const std::wstring& chars) const = 0;
    virtual int Ascent() const = 0;
    virtual int CellHeight() const = 0;

    // Inked box of one character relative to the pen position and the baseline (`top` negative
    // above it) and the advance; a character the face lacks gives its missing-glyph box. False if
    // the face cannot tell.
    struct Box {
        int left = 0;
        int top = 0;
        int width = 0;
        int height = 0;
        int advance = 0;
    };
    virtual bool GlyphBox(wchar_t ch, Box& box) const = 0;

    // One character as a coverage mask (alpha = level / 64) with `pad` empty pixels on every
    // side. `left` and `top` place the mask's top-left corner relative to the pen position and
    // the top of the line; `advance` is the face's own advance. False if the face has no glyph.
    struct Mask {
        Image mask;
        int left = 0;
        int top = 0;
        int advance = 0;
    };
    virtual bool GlyphMask(wchar_t ch, int pad, Mask& out) const = 0;

    // Distance from the top of the line to the top of the capital H, and its height.
    int CapTop() const;
    int CapHeight() const;
    // The same for the capitals by `ref` (MeasureCap); CapReference::H gives CapTop/CapHeight.
    CapMetrics Capitals(CapReference ref) const;
    // Height of the lowercase x.
    int XHeight() const;
    // Highest and lowest inked row over all characters of `text`, relative to the baseline (top
    // negative), and the pixels between them.
    void InkRange(const std::wstring& text, int& top, int& bottom) const;
    int InkExtent(const std::wstring& text) const;
};

// Where a face comes from and who rasterizes it: our own rasterizer needs the font file; a face
// without one (only the font preview tool compares installed faces) is always drawn by GDI.
enum class Raster { Gdi, Own };
struct FaceSource {
    std::wstring face;
    std::filesystem::path file;  // empty = installed font
    Raster raster = Raster::Gdi;
};

// A TtFace for Raster::Own with a file, else a GdiFace (the file must then be registered with
// AddFontResourceExW, FR_PRIVATE).
std::unique_ptr<Face> OpenFace(const FaceSource& source, int height, int weight);

// One line of `text` as a coverage mask with `pad` empty pixels on every side: the pen starts at
// x = pad, the top of the face's line at y = pad; every character moves the pen by the face's own
// advance (like [Fonts] Spacing=font). Characters the face has no glyph for are left out.
Image LineMask(const Face& face, const std::wstring& text, int pad);

// Smallest height whose capitals (`ref`) are at least `target` pixels high.
int HeightForCap(const FaceSource& source, int weight, int target, CapReference ref = CapReference::H);
// Smallest height whose lowercase x is at least `target` pixels high.
int HeightForX(const FaceSource& source, int weight, int target);
// The same for an installed or registered face drawn by GDI.
int HeightForCap(const std::wstring& face, int weight, int target, CapReference ref = CapReference::H);
int HeightForX(const std::wstring& face, int weight, int target);

}  // namespace ddmod::text
