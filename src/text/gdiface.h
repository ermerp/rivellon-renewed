#pragma once

#include <string>
#include <vector>

#include <windows.h>

#include "text/style.h"

namespace ddmod::text {

// A vector face rasterized by GDI the way the game's font mode does it: positive height (cell
// height), the given weight (600 and up emboldens faces that have only a regular cut),
// GGO_GRAY8_BITMAP with 65 levels. A bundled face must be registered first
// (AddFontResourceExW, FR_PRIVATE); ActualFace tells whether GDI took a substitute.
class GdiFace {
public:
    GdiFace(const std::wstring& face, int height, int weight);
    ~GdiFace();
    GdiFace(const GdiFace&) = delete;
    GdiFace& operator=(const GdiFace&) = delete;

    bool Valid() const { return font_ != nullptr && dc_ != nullptr; }
    // The face GDI really selected.
    std::wstring ActualFace() const;
    // Characters of `chars` the face has no glyph for.
    std::wstring Missing(const std::wstring& chars) const;

    int Ascent() const { return tm_.tmAscent; }
    int CellHeight() const { return tm_.tmHeight; }
    // Distance from the top of the line to the top of the capital H, and its height.
    int CapTop() const;
    int CapHeight() const;
    // Height of the lowercase x.
    int XHeight() const;
    // Highest and lowest inked row over all characters of `text`, relative to the baseline (top
    // negative), and the pixels between them.
    void InkRange(const std::wstring& text, int& top, int& bottom) const;
    int InkExtent(const std::wstring& text) const;

    // Gray-level bitmap (0-64, rows padded to 4 bytes) of one character; false if GDI has none.
    bool GlyphBits(wchar_t ch, GLYPHMETRICS& gm, std::vector<BYTE>& bits) const;

    // One character as a coverage mask (alpha = level / 64) with `pad` empty pixels on every
    // side. `left` and `top` place the mask's top-left corner relative to the pen position and
    // the top of the line; `advance` is the face's own advance. False if GDI has no glyph.
    struct Mask {
        Image mask;
        int left = 0;
        int top = 0;
        int advance = 0;
    };
    bool GlyphMask(wchar_t ch, int pad, Mask& out) const;

private:
    HDC dc_ = nullptr;
    HFONT font_ = nullptr;
    HGDIOBJ old_ = nullptr;
    TEXTMETRICW tm_{};
};

// Smallest GDI height whose capital H is at least `target` pixels high.
int HeightForCap(const std::wstring& face, int weight, int target);
// Smallest GDI height whose lowercase x is at least `target` pixels high.
int HeightForX(const std::wstring& face, int weight, int target);

}  // namespace ddmod::text
