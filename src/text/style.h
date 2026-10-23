#pragma once

#include <cstdint>
#include <vector>

#include "overlay/glyphs.h"
#include "overlay/surface.h"

namespace ddmod::text {

using overlay::Rgb;

// The mod's text styles (PLAN.md Decisions, target text renderer; docs/re/text-rendering.md,
// Colors): a vector glyph's coverage is painted with style parameters measured from the game's
// bitmap fonts - colors, color profile, edge and across-stroke brightness, texture strength. Only
// these numbers are taken from the originals, never their pixels. Shared by the game's glyph
// tables and the comparison sheets of ddmod_fontpreview.

// A painted glyph or line with straight (not premultiplied) alpha, 0-1. As a coverage mask only
// `alpha` matters.
struct Image {
    int width = 0, height = 0;
    std::vector<float> alpha;
    std::vector<Rgb> color;
    float& A(int x, int y) { return alpha[static_cast<std::size_t>(y) * width + x]; }
    float A(int x, int y) const { return alpha[static_cast<std::size_t>(y) * width + x]; }
    Rgb& C(int x, int y) { return color[static_cast<std::size_t>(y) * width + x]; }
    Rgb C(int x, int y) const { return color[static_cast<std::size_t>(y) * width + x]; }
};

Image MakeImage(int width, int height);

// What the game's 16-bit back buffer keeps of a color (RGB565, expanded back to 0-255).
Rgb To565(Rgb c);

// `c` moved towards `to` by `k` (0 = c, 1 = to).
Rgb Shade(Rgb c, Rgb to, float k);

// The capitals A-Z of an original bitmap font with their pre-shaded colors (overlay::Glyph with
// `colors`; offsetY from the top of the line), the input of every measurement. A missing letter is
// an empty glyph.
using Capitals = std::vector<overlay::Glyph>;  // 26 entries, A first

// Top row (from the top of the line) and height of the inked rows of the capital H.
struct CapMetrics {
    int top = 0;
    int height = 0;
};
CapMetrics MeasureCap(const Capitals& caps);

// Mean color of the inner pixels (all four neighbors drawn, so not the shaded edge) per step from
// the top to the bottom of the capital H, over A-Z.
std::vector<Rgb> MeasureProfile(const Capitals& caps);

// Brightness of the stroke edges relative to the color profile: factor per distance (1-3 pixels)
// from the top, bottom, left and right end of a stroke.
struct Bevel {
    float side[4][3] = {{1, 1, 1}, {1, 1, 1}, {1, 1, 1}, {1, 1, 1}};  // top, bottom, left, right
};
Bevel MeasureBevel(const Capitals& caps, const std::vector<Rgb>& profile);

// Brightness across the strokes: for every horizontal and vertical run of drawn pixels (4 or more
// long), the brightness relative to the run's mean in kShineBins steps from one end to the other;
// plus the remaining unevenness (texture) as a standard deviation.
inline constexpr int kShineBins = 5;
struct Shine {
    float across[2][kShineBins] = {{1, 1, 1, 1, 1}, {1, 1, 1, 1, 1}};  // horizontal runs, vertical runs
    float texture = 0.0f;
};
Shine MeasureShine(const Capitals& caps);

// Deterministic value noise in -1..1 with cells of `cell` pixels (our own texture, not the game's).
float ValueNoise(int x, int y, int cell);

// The game's look for its GDI fonts: every covered pixel opaque on the 65-step ramp from the edge
// to the text color.
Image PaintRamp(const Image& mask, Rgb text, Rgb edge);

// Color from `profile` between the top (`capTop`, in mask rows) and the bottom of the capital H.
// `outline` lays a one-pixel contour in `contour` under the glyph; `gloss` lightens the upper and
// darkens the lower stroke edge by fixed amounts; `bevel` scales the stroke edges by measured
// factors and shades partly covered pixels towards `contour`. `shine` scales every stroke pixel by
// the measured brightness across the stroke (horizontal and vertical run, strength 1 = as
// measured) and, with `texture`, by our own noise of the measured texture strength.
struct StyleParams {
    std::vector<Rgb> profile;
    Rgb contour;
    bool outline = false;
    bool gloss = false;
    const Bevel* bevel = nullptr;
    const Shine* shine = nullptr;
    float shineStrength = 1.0f;
    bool texture = false;
};
Image PaintStyled(const Image& mask, int capTop, int capHeight, const StyleParams& style);

// The game's sprite pixels of a painted image: RGB565 row by row, 0 = transparent. The sprite
// format has no per-pixel alpha, so partly covered pixels are blended into `background` (the
// family's edge tone) in advance, like the originals; a visible pixel is never 0.
std::vector<std::uint16_t> ToGamePixels(const Image& img, Rgb background);

}  // namespace ddmod::text
