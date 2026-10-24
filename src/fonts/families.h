#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "fonts/fontchain.h"
#include "text/face.h"
#include "text/style.h"

namespace ddmod::fonts {

// The game's 61 bitmap fonts in five families (docs/re/text-rendering.md, "Which texts use which
// path" and "Faces per family"). With [Fonts] Renderer=glyphs each family gets its own face and
// style; [Family.<name>] in RivellonRenewed.ini switches it and sets face, weight and line height.
enum class Family {
    Interface,   // font{n,gra,red,b,go,or,yel,gr}1-5: tooltips, load/save dialog, character sheet
    Books,       // fontblack*, paper_*: letters, books, diary entries
    Menu,        // fontbig24*, fontbig36*: main-menu capitals
    Dialog,      // dialog_*: NPC dialogs
    DiaryTitle,  // diary_head_*: diary tab title
};

inline constexpr Family kFamilies[] = {Family::Interface, Family::Books, Family::Menu, Family::Dialog,
                                       Family::DiaryTitle};

// Section name part ("Interface" -> [Family.Interface]).
const char* FamilyName(Family family);

// Family of a game font file stem ("fontn1", lower case); nullopt for a name the game does not
// ship (fontlist.txt).
std::optional<Family> FamilyOf(std::string_view stem);

// How a family's glyphs are painted (src/text/style.h): the game's two-color ramp, the measured
// color profile and edge shading, or the measured shine across the strokes plus own texture.
enum class GlyphStyle { Ramp, Measured, Shine };

// Line height of a family's fonts (the layouts' line pitch): a percentage of the original's, or
// `auto` = at least the original and high enough that the face's ascenders and descenders of
// neighboring lines do not touch.
struct LineHeight {
    bool automatic = false;
    int percent = 100;
};
std::optional<LineHeight> ParseLineHeight(std::string_view text);

// What happens to ink above the top of the line, which the game does not draw in pre-rendered
// texts (menu entries, diary title): none = left as it is (cut off there); accents and umlaut
// dots move down into the gap above their letter first, then the rest is handled by lowering the
// whole font until everything fits (lower), by compressing the part above the capitals (squeeze),
// or by drawing such accents smaller and compressing only the other glyphs' part above the
// capitals (fit: the text keeps the original's place, like the originals' flat accents).
enum class TopFit { None, Lower, Squeeze, Fit };
std::optional<TopFit> ParseTopFit(std::string_view text);

struct FamilySettings {
    bool enabled = true;
    std::string chain;  // [Fonts] Chain syntax: "Face @ file, Face, ..."
    int weight = 400;
    LineHeight lineHeight;
    GlyphStyle style = GlyphStyle::Ramp;
    float shineStrength = 1.0f;  // GlyphStyle::Shine
    bool texture = false;        // GlyphStyle::Shine
    // Slots (code page 1252) whose original glyph is a symbol, not that character: they keep it.
    std::string keepOriginal;
    // Slots of a right and a left arrow (in this order), drawn as own arrows (PaintArrow).
    std::string arrows;
    // Which capitals of the original and the face are matched in height and position.
    text::CapReference capitals = text::CapReference::H;
    TopFit topFit = TopFit::None;
};

// The defaults (human's choice, PLAN.md Decisions): interface Marcellus SC 600, books Fondamento
// 400 with a line height that keeps lines apart, menu Uncial Antiqua 600 with the measured shine
// at twice its strength plus texture (its '[' and ']' are the options pages' arrows, drawn as own
// triangles), dialog
// Tahoma Bold (else the bundled DejaVu Sans Condensed Bold), diary title Almendra SC 600 with the
// measured style. Menu and diary title are matched on all their capitals (their faces' H is taller).
FamilySettings DefaultSettings(Family family);

// The defaults overridden by [Family.<name>] Enabled, Face, Weight, LineHeight and TopFit (invalid values
// are logged and keep the default). Effects=0 paints a Shine or Measured family with the ramp;
// Arrows=original keeps the original arrow glyphs.
FamilySettings ConfiguredSettings(Family family);

// [Fonts] Raster: own = our own rasterizer for faces with a font file (the bundled faces; the
// same look on Windows and Proton), gdi = GDI for every face. Installed faces (Tahoma) always
// use GDI. An invalid value is logged and gives own.
std::optional<text::Raster> ParseRaster(std::string_view text);
text::Raster ConfiguredRaster();

// A family's configured settings and its face, resolved and logged on first use (the face's file
// is registered for the process): no face if the family is switched off or none of its faces is
// available. `source` is the face with its rasterizer ([Fonts] Raster; GDI for an installed face
// or a file our rasterizer cannot read). Later calls return the same result. Thread-safe.
struct ResolvedFamily {
    FamilySettings settings;
    std::optional<FontEntry> face;
    text::FaceSource source;
};
const ResolvedFamily& Resolved(Family family);

}  // namespace ddmod::fonts
