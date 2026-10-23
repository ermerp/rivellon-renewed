#pragma once

#include <string>

#include "fonts/families.h"
#include "fonts/fontcolors.h"
#include "text/style.h"
#include "text/symbols.h"

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

// An own arrow in place of an original arrow glyph (FamilySettings::arrows): `shape` filling the
// original's inked box (`widthFactor` < 1 narrower, centered), painted in the style measured from
// the original arrow itself - color profile and edge shading, no shine or texture (the original
// arrows are smooth). `left` and `top` place the image relative to the pen position and the top
// of the line, like the original's ink. An empty image if the original has no ink.
struct OwnSymbol {
    text::Image image;
    int left = 0;
    int top = 0;
};
OwnSymbol PaintArrow(const overlay::Glyph& original, text::ArrowShape shape, bool pointsRight, text::Rgb textColor,
                     text::Rgb edge, float widthFactor = 1.0f);

}  // namespace ddmod::fonts
