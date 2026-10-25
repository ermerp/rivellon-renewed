#include "fonts/spacing.h"

#include <algorithm>
#include <string>

#include "core/config.h"
#include "core/hooks.h"
#include "core/log.h"
#include "fonts/glyphtable.h"
#include "game/exe.h"

namespace ddmod::fonts {
namespace {

// Game font object (docs/re/glyph-layout.md): +0x00 pointer to the GDI part (null for a bitmap
// font), +0x14 table of 256 glyph pointers. Glyph: +0x08 origin x (i32), +0x10 advance (i16).
constexpr std::size_t kGdiPart = 0x00;
constexpr std::size_t kGlyphTable = 0x14;
constexpr std::size_t kGlyphOriginX = 0x08;
constexpr std::size_t kGlyphAdvance = 0x10;

using PairSpacingFn = int(__thiscall*)(void* font, int previous, int next);

PairSpacingFn g_pairSpacing = nullptr;
SpacingMode g_mode = SpacingMode::Game;

template <typename T>
T Field(const void* object, std::size_t offset) {
    return *reinterpret_cast<const T*>(static_cast<const std::uint8_t*>(object) + offset);
}

// See FontInitDetour (fontmode.cpp) for the __fastcall stand-in of __thiscall.
int __fastcall PairSpacingDetour(void* font, void* /*edx*/, int previous, int next) {
    // The original also creates both glyphs on first use.
    const int spacing = g_pairSpacing(font, previous, next);
    if ((!Field<void*>(font, kGdiPart) && !IsOwnGlyphFont(font)) || (previous & 0xFF) == ' ') return spacing;
    const auto* const* glyphs = Field<void* const*>(font, kGlyphTable);
    const void* glyph = glyphs ? glyphs[previous & 0xFF] : nullptr;
    if (!glyph) return spacing;
    return AdjustSpacing(g_mode, spacing, Field<std::int32_t>(glyph, kGlyphOriginX),
                         Field<std::int16_t>(glyph, kGlyphAdvance));
}

}  // namespace

std::optional<SpacingMode> ParseSpacingMode(std::string_view text) {
    if (text == "game") return SpacingMode::Game;
    if (text == "gap") return SpacingMode::Gap;
    if (text == "font") return SpacingMode::Font;
    return std::nullopt;
}

int AdjustSpacing(SpacingMode mode, int gameSpacing, int originX, int advance) {
    switch (mode) {
        case SpacingMode::Gap: {
            // Only where the game slid the character closer than its no-touch distance.
            const int untouched = originX + advance;
            return gameSpacing < untouched ? std::min(gameSpacing + 1, untouched) : gameSpacing;
        }
        case SpacingMode::Font:
            return advance;
        case SpacingMode::Game:
        default:
            return gameSpacing;
    }
}

bool InstallSpacing() {
    const std::string text = config::GetString("Fonts", "Spacing", "font");
    const auto mode = ParseSpacingMode(text);
    if (!mode) {
        log::Warn("[Fonts] Spacing=\"{}\" is not game, gap or font - using the game's spacing", text);
        return false;
    }
    // Best effort for the Russian edition: no signatures for its EXE (font mode says so in the log).
    if (*mode == SpacingMode::Game || game::Exe().edition == game::Edition::Russian) return true;
    const std::uintptr_t target = game::Resolve({"PairSpacing", kPairSpacingPattern, 0, kPairSpacingRva});
    if (!target) {
        log::Warn("Font spacing: pair spacing function not found - using the game's spacing");
        return false;
    }
    g_mode = *mode;
    if (!hooks::CreateRaw("PairSpacing", reinterpret_cast<void*>(target), reinterpret_cast<void*>(&PairSpacingDetour),
                          reinterpret_cast<void**>(&g_pairSpacing)) ||
        !hooks::Enable("PairSpacing")) {
        return false;
    }
    log::Info("Font spacing: {} ([Fonts] Spacing)", text);
    return true;
}

}  // namespace ddmod::fonts
