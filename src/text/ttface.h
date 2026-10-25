#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "text/face.h"

namespace ddmod::text {

struct FontFile;

// A face rasterized by our own code (stb_truetype for the outlines) from its font file, the same
// on Windows and Proton. It repeats what GDI does with an unhinted face (docs/re/gdi-raster.md):
// the height is the cell height usWinAscent + usWinDescent, metrics use that
// scale, outlines the nearest whole ppem; coverage counts 8x8 point samples per pixel (65 levels);
// a weight of 600 and more on a face lighter than 600 overstrikes every glyph one pixel to the
// right (levels added, advance one pixel larger). Hinting instructions are not run.
class TtFace : public Face {
public:
    TtFace(const std::filesystem::path& file, int height, int weight);
    ~TtFace() override;

    bool Valid() const override { return font_ != nullptr; }
    std::wstring Missing(const std::wstring& chars) const override;
    int Ascent() const override { return ascent_; }
    int CellHeight() const override { return ascent_ + descent_; }
    // The box of all outline points rounded to whole pixels (like GDI's GGO_METRICS, which the
    // capital measurements use), not the inked box of the mask.
    bool GlyphBox(wchar_t ch, Box& box) const override;
    bool GlyphMask(wchar_t ch, int pad, Mask& out) const override;

    // True if this face emboldens (weight against the face's own weight class).
    bool Emboldened() const { return bold_; }

private:
    struct Raster {
        Box box;                           // inked box of `levels`
        Box metrics;                       // GlyphBox: the outline's box like GDI's GGO_METRICS
        std::vector<std::uint8_t> levels;  // box.width x box.height, 0-64
    };
    const Raster& Rasterize(wchar_t ch) const;

    std::shared_ptr<const FontFile> font_;
    float scale_ = 0.0f;    // font units -> pixels for metrics
    float outline_ = 0.0f;  // font units -> pixels for outlines (whole ppem)
    int ascent_ = 0;
    int descent_ = 0;
    bool bold_ = false;
    mutable std::map<wchar_t, Raster> cache_;
};

// True if our rasterizer can read the font file (cached; the file stays in memory).
bool CanRasterize(const std::filesystem::path& file);

// True if the font file's name table names `face` (family, full or typographic family name;
// case-insensitive) - checked in the file itself, so an installed font of that name does not count.
bool HoldsFace(const std::filesystem::path& file, const std::wstring& face);

// The TtFace height (cell height) whose em is `em` pixels - what GDI's CreateFont makes of a
// negative height; 0 if the file cannot be read.
int CellHeightForEm(const std::filesystem::path& file, int em);

}  // namespace ddmod::text
