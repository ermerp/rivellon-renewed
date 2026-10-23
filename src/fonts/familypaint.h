#pragma once

#include <string>

#include "fonts/families.h"
#include "fonts/fontcolors.h"
#include "text/style.h"

namespace ddmod::fonts {

// The original's text and edge color: the measured table (fontcolors.cpp), or for a name it does
// not know the most frequent capital color and the family's usual edge tone.
FontColors ColorsOf(const std::string& stem, Family family, const text::Capitals& caps);

inline text::Rgb ToRgb(Rgb8 c) { return {c.r, c.g, c.b}; }

// How a family paints its face for one original font: the style parameters measured from the
// original's capitals (color profile, edge shading or shine, src/text) and its two colors. Shared
// by the glyph renderer (one glyph at a time) and the overlay (whole lines).
class FamilyPaint {
public:
    FamilyPaint(const FamilySettings& settings, const text::Capitals& caps, text::Rgb textColor, text::Rgb edge);
    FamilyPaint(const FamilyPaint&) = delete;
    FamilyPaint& operator=(const FamilyPaint&) = delete;

    // `mask` painted in the family's style; `capTop` is the row of the face's capitals in the
    // mask and `capHeight` their height (GdiFace::Capitals with the family's reference).
    text::Image Paint(const text::Image& mask, int capTop, int capHeight) const;

private:
    GlyphStyle style_;
    text::Rgb text_, edge_;
    text::Bevel bevel_;
    text::Shine shine_;
    text::StyleParams params_;
};

}  // namespace ddmod::fonts
