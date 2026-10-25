#pragma once

#include <string>
#include <vector>

#include <windows.h>

#include "text/face.h"

namespace ddmod::text {

// A vector face rasterized by GDI the way the game's font mode does it: positive height (cell
// height), the given weight (600 and up emboldens faces that have only a regular cut),
// GGO_GRAY8_BITMAP with 65 levels. A bundled face must be registered first
// (AddFontResourceExW, FR_PRIVATE); ActualFace tells whether GDI took a substitute.
class GdiFace : public Face {
public:
    GdiFace(const std::wstring& face, int height, int weight);
    ~GdiFace() override;
    GdiFace(const GdiFace&) = delete;
    GdiFace& operator=(const GdiFace&) = delete;

    bool Valid() const override { return font_ != nullptr && dc_ != nullptr; }
    // The face GDI really selected.
    std::wstring ActualFace() const;
    std::wstring Missing(const std::wstring& chars) const override;

    int Ascent() const override { return tm_.tmAscent; }
    int CellHeight() const override { return tm_.tmHeight; }
    bool GlyphBox(wchar_t ch, Box& box) const override;
    bool GlyphMask(wchar_t ch, int pad, Mask& out) const override;

    // Gray-level bitmap (0-64, rows padded to 4 bytes) of one character; false if GDI has none.
    bool GlyphBits(wchar_t ch, GLYPHMETRICS& gm, std::vector<BYTE>& bits) const;

private:
    HDC dc_ = nullptr;
    HFONT font_ = nullptr;
    HGDIOBJ old_ = nullptr;
    TEXTMETRICW tm_{};
};

}  // namespace ddmod::text
