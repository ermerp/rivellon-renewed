#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "fonts/fontchain.h"

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

struct FamilySettings {
    bool enabled = true;
    std::string chain;  // [Fonts] Chain syntax: "Face @ file, Face, ..."
    int weight = 400;
    LineHeight lineHeight;
    GlyphStyle style = GlyphStyle::Ramp;
    float shineStrength = 1.0f;  // GlyphStyle::Shine
    bool texture = false;        // GlyphStyle::Shine
};

// The defaults (human's choice, PLAN.md Decisions): interface Marcellus SC 600, books Fondamento
// 400 with a line height that keeps lines apart, menu Uncial Antiqua 600 with the measured shine
// at twice its strength plus texture, dialog Tahoma Bold (else the bundled DejaVu Sans Condensed
// Bold), diary title Almendra SC 600 with the measured style.
FamilySettings DefaultSettings(Family family);

// The defaults overridden by [Family.<name>] Enabled, Face, Weight and LineHeight (invalid values
// are logged and keep the default). Effects=0 paints a Shine or Measured family with the ramp.
FamilySettings ConfiguredSettings(Family family);

}  // namespace ddmod::fonts
