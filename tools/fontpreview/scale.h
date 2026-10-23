#pragma once

#include <string_view>
#include <vector>

#include "overlay/glyphs.h"
#include "overlay/surface.h"
#include "text/style.h"

namespace fontpreview {

using ddmod::overlay::Rgb;

// A rendered line with straight (not premultiplied) alpha, 0-1 (the mod's text image).
using ddmod::text::Image;
using ddmod::text::MakeImage;

// Renders `text` with `cache`. Pre-shaded bitmap glyphs (Glyph::colors) are darkened towards
// black at their edges; their brightness relative to the brightest pixel of the line becomes the
// alpha, and the color is brightened back ("unpremultiplied"). Mask glyphs use `color`.
Image RenderLine(ddmod::overlay::GlyphCache& cache, std::wstring_view text, Rgb color, int pad);

Image ScaleNearest(const Image& src, int scale);
// Scale2x / EPX (pixel-art scaler, public-domain algorithm) on the mask alpha >= 0.5: doubles
// the size and rounds off staircase diagonals; hard edges, no new colors.
Image ScaleEpx(const Image& src);
// Signed distance field of the mask alpha >= 0.5, bilinearly upscaled and turned back into an
// anti-aliased edge one output pixel wide: smooth outlines at any scale.
Image ScaleSdf(const Image& src, int scale);

// Black on white, alpha >= `threshold` -> black: for font identification services.
Image Binarize(const Image& src, float threshold);

// Draws `img` over a background color.
std::vector<std::uint32_t> Composite(const Image& img, Rgb bg);

}  // namespace fontpreview
