#pragma once

#include "text/style.h"

namespace ddmod::text {

// Own vector symbols for slots of the game's fonts that hold a symbol instead of a character
// (the options pages' arrows in the menu fonts, docs/re/text-rendering.md). Painted like the
// family's letters (FamilyPaint), so they share its colors, shine and texture.

// Outline of an arrow head pointing right, in a box from (0,0) to (1,1) with the tip at (1, 0.5):
// Triangle - straight sides; Notched - the back cut in like a spear head; Convex - sides bowed
// outwards; Concave - sides bowed inwards (a thorn).
enum class ArrowShape { Triangle, Notched, Convex, Concave };

// Coverage mask of `width` x `height` pixels that the arrow fills from edge to edge (anti-aliased
// by 8 x 8 samples per pixel); `right` = false mirrors it to point left.
Image ArrowMask(ArrowShape shape, bool right, int width, int height);

}  // namespace ddmod::text
