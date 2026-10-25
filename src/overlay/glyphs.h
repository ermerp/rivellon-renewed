#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "overlay/surface.h"

namespace ddmod::text {
class Face;  // text/face.h, which includes this header through text/style.h
}

namespace ddmod::overlay {

// One rasterized character: an 8-bit coverage mask (0-255) placed relative to the pen position
// at the top of the text line.
struct Glyph {
    int width = 0;
    int height = 0;
    int offsetX = 0;  // from the pen position to the left edge of the mask
    int offsetY = 0;  // from the top of the line to the top edge of the mask
    int advance = 0;  // pen movement to the next character
    std::vector<std::uint8_t> coverage;  // width * height, row by row
    // Optional own color per pixel (pre-shaded bitmap fonts); empty = drawn in the text color.
    std::vector<Rgb> colors;
};

// Where glyphs come from. The overlay only sees this interface, so the font source can change
// (system font, bundled font file, a pre-rendered atlas) without touching cache or renderer.
class GlyphSource {
public:
    virtual ~GlyphSource() = default;
    virtual int LineHeight() const = 0;
    // Fills `out` for `ch`; false if the font cannot provide the character.
    virtual bool Rasterize(wchar_t ch, Glyph& out) = 0;
};

// Rasterizes characters with GDI (GetGlyphOutlineW, gray levels) into memory, like the game's
// own GDI font path does. A non-empty `file` is registered for this process only
// (AddFontResourceExW, FR_PRIVATE) before the font is created; `face` must then name a face in
// that file. If the file cannot be loaded, the system font of the same face name is used.
struct FontSpec {
    std::wstring face = L"Tahoma";
    std::filesystem::path file;
    int pixelHeight = 16;     // character height without internal leading ...
    bool cellHeight = false;  // ... or, if true, cell height (like a positive CreateFont height)
    bool bold = true;
};

// Returns nullptr if GDI cannot create the font or a drawing context. Used by the font preview
// tool to compare system faces; the mod itself draws with CreateFaceGlyphSource.
std::unique_ptr<GlyphSource> CreateGdiGlyphSource(const FontSpec& spec);

// Rasterizes characters with a text::Face (our own rasterizer for a bundled font file - the same
// on Windows and Proton). Returns nullptr if the face is not valid.
std::unique_ptr<GlyphSource> CreateFaceGlyphSource(std::unique_ptr<text::Face> face);

// Rasterizes each character once, on first use. Characters the source cannot provide are drawn
// as '?'.
class GlyphCache {
public:
    explicit GlyphCache(std::unique_ptr<GlyphSource> source);

    int LineHeight() const { return lineHeight_; }
    const Glyph& Get(wchar_t ch);

private:
    std::unique_ptr<GlyphSource> source_;
    int lineHeight_ = 0;
    std::unordered_map<wchar_t, Glyph> glyphs_;
};

// Width of `text` in pixels (sum of advances).
int MeasureString(GlyphCache& cache, std::wstring_view text);

// Draws one line of text with its top-left corner at (x, y). `shadow` adds a dark copy one pixel
// down and right, which keeps the text readable on bright backgrounds. Glyphs with own colors
// ignore `color`.
void DrawString(const SurfaceView& view, GlyphCache& cache, int x, int y, std::wstring_view text, Rgb color,
                bool shadow);

}  // namespace ddmod::overlay
