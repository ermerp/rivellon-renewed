#include "fonts/fontcolors.h"

#include <format>
#include <string>

#include "core/config.h"
#include "core/hooks.h"
#include "core/log.h"
#include "game/exe.h"

namespace ddmod::fonts {
namespace {

constexpr Rgb8 kBlack{0x00, 0x00, 0x00};
constexpr Rgb8 kGray{0x7F, 0x7F, 0x7F};
constexpr Rgb8 kGray80{0x80, 0x80, 0x80};
// The bitmap fonts for paper (books, letters) shade their edges towards this paper tone.
constexpr Rgb8 kPaper{0x9C, 0x8E, 0x5A};
// The diary headings shade towards a slightly warmer tone.
constexpr Rgb8 kDiaryEdge{0x94, 0x7D, 0x4A};

constexpr FontColors OnBlack(std::uint8_t r, std::uint8_t g, std::uint8_t b) { return {{r, g, b}, kBlack}; }
constexpr FontColors OnPaper(std::uint8_t r, std::uint8_t g, std::uint8_t b) { return {{r, g, b}, kPaper}; }

// Interface family: five sizes per color, the same colors in every size.
constexpr int kInterfaceHeights[5] = {15, 14, 13, 12, 11};

struct InterfaceColor {
    const char* files[5];
    FontColors game;
    FontColors original;
};

// Original colors: the dominant color of the glyph interiors in each bitmap font; the textured
// menu capitals and diary headings use the interior mean. All bitmap fonts shade their edges
// linearly towards black (dark backgrounds) or towards the paper tone (docs/re/text-rendering.md).
constexpr InterfaceColor kInterface[8] = {
    {{"fontn1", "fontn2", "fontn3", "fontn4", "fontn5"}, OnBlack(0xFF, 0xFF, 0xFF), OnBlack(0xE7, 0xE7, 0xE7)},
    {{"fontgra1", "fontgra2", "fontgra3", "fontgra4", "fontgra5"}, OnBlack(0x7F, 0x7F, 0x7F), OnBlack(0x94, 0x96, 0x94)},
    {{"fontred1", "fontred2", "fontred3", "fontred4", "fontred5"}, OnBlack(0xFF, 0x00, 0x00), OnBlack(0xDE, 0x34, 0x31)},
    {{"fontb1", "fontb2", "fontb3", "fontb4", "fontb5"}, OnBlack(0x00, 0x00, 0xFF), OnBlack(0x18, 0x8E, 0xDE)},
    {{"fontgo1", "fontgo2", "fontgo3", "fontgo4", "fontgo5"}, OnBlack(0xC8, 0xC8, 0x40), OnBlack(0xE7, 0xB2, 0x31)},
    {{"fontor1", "fontor2", "fontor3", "fontor4", "fontor5"}, OnBlack(0xFF, 0x7F, 0x00), OnBlack(0xFF, 0x7D, 0x29)},
    {{"fontyel1", "fontyel2", "fontyel3", "fontyel4", "fontyel5"}, OnBlack(0xFF, 0xFF, 0x00), OnBlack(0xC6, 0xCF, 0x4A)},
    {{"fontgr1", "fontgr2", "fontgr3", "fontgr4", "fontgr5"}, OnBlack(0x00, 0xFF, 0x00), OnBlack(0x7B, 0xBA, 0x42)},
};

constexpr FontColorEntry kOthers[] = {
    {"fontblack1", 13, {kBlack, kGray}, OnPaper(0x08, 0x08, 0x08)},
    {"fontblack2", 15, {{0x00, 0x00, 0x7F}, kGray}, OnPaper(0x00, 0x34, 0x6B)},
    {"fontbig24", 24, {{0xFF, 0xFF, 0x7F}, kGray}, OnBlack(0xC2, 0xB9, 0x96)},
    {"fontbig24highlight", 24, {{0xF8, 0xD3, 0x07}, kGray}, OnBlack(0xE7, 0xCF, 0x73)},
    {"fontbig24gray", 24, {kGray, kGray}, OnBlack(0x61, 0x5F, 0x56)},
    {"fontbig36", 36, {{0xFF, 0xFF, 0x7F}, kGray}, OnBlack(0xC2, 0xB9, 0x96)},
    {"fontbig36highlight", 36, {{0xF8, 0xD3, 0x07}, kGray}, OnBlack(0xE7, 0xCF, 0x73)},
    {"fontbig36gray", 36, {kGray, kGray}, OnBlack(0x61, 0x5F, 0x56)},
    {"dialog_white", 14, OnBlack(0xFF, 0xFF, 0xFF), OnBlack(0xEF, 0xEF, 0xEF)},
    {"dialog_grey", 14, OnBlack(0x7F, 0x7F, 0x7F), OnBlack(0xAD, 0xAA, 0xAD)},
    {"dialog_yellow", 14, OnBlack(0xFF, 0xFF, 0x00), OnBlack(0xDE, 0xBA, 0x18)},
    {"dialog_red", 14, OnBlack(0xFF, 0x00, 0x00), OnBlack(0xE7, 0x59, 0x39)},
    {"fontblack_highlight1", 13, {{0x40, 0x40, 0x40}, kGray}, OnPaper(0x08, 0x08, 0x08)},
    {"fontblack_highlight2", 15, {{0x40, 0x40, 0xC3}, kGray}, OnPaper(0x18, 0x61, 0xB5)},
    {"diary_head_1024", 32, {kBlack, kGray}, {{0x5D, 0x42, 0x1F}, kDiaryEdge}},
    {"diary_head_800", 26, {kBlack, kGray}, {{0x5D, 0x42, 0x1F}, kDiaryEdge}},
    {"diary_head_640", 20, {kBlack, kGray}, {{0x5D, 0x42, 0x1F}, kDiaryEdge}},
    {"paper_green", 15, {{0x00, 0x80, 0x00}, kGray80}, OnPaper(0x00, 0x6D, 0x42)},
    {"paper_green_high", 15, {{0x20, 0xA0, 0x20}, kGray80}, OnPaper(0x29, 0x96, 0x6B)},
    {"paper_red", 15, {{0x80, 0x00, 0x00}, kGray80}, OnPaper(0x6B, 0x30, 0x00)},
    {"paper_red_high", 15, {{0xA0, 0x20, 0x20}, kGray80}, OnPaper(0xA5, 0x59, 0x18)},
};
static_assert(8 * 5 + std::size(kOthers) == kGameFontCount);

constexpr std::array<FontColorEntry, kGameFontCount> BuildTable() {
    std::array<FontColorEntry, kGameFontCount> table{};
    std::size_t i = 0;
    for (const InterfaceColor& color : kInterface) {
        for (int size = 0; size < 5; ++size) table[i++] = {color.files[size], kInterfaceHeights[size], color.game, color.original};
    }
    for (const FontColorEntry& entry : kOthers) table[i++] = entry;
    return table;
}

constexpr std::array<FontColorEntry, kGameFontCount> kTable = BuildTable();

using AddFontDescriptorFn = int(__thiscall*)(void* list, const char* face, const char* face2, int height, int textR,
                                             int textG, int textB, int edgeR, int edgeG, int edgeB);

AddFontDescriptorFn g_addFontDescriptor = nullptr;
std::size_t g_next = 0;      // index of the next descriptor the game adds
bool g_mismatch = false;     // the game's calls stopped matching the table: keep its colors
std::size_t g_replaced = 0;

std::string Hex(const Rgb8& c) { return std::format("#{:02X}{:02X}{:02X}", c.r, c.g, c.b); }

// See FontInitDetour (fontmode.cpp) for the __fastcall stand-in of __thiscall.
int __fastcall AddFontDescriptorDetour(void* list, void* /*edx*/, const char* face, const char* face2, int height,
                                       int textR, int textG, int textB, int edgeR, int edgeG, int edgeB) {
    const std::size_t index = g_next++;
    const auto byte = [](int v) { return static_cast<std::uint8_t>(v); };
    const FontColors game{{byte(textR), byte(textG), byte(textB)}, {byte(edgeR), byte(edgeG), byte(edgeB)}};
    if (!g_mismatch) {
        if (const auto colors = OriginalColors(index, height, game)) {
            log::Debug("Font colors: #{} {} text {} edge {} (game: {} / {})", index, kTable[index].file,
                       Hex(colors->text), Hex(colors->edge), Hex(game.text), Hex(game.edge));
            if (++g_replaced == kGameFontCount) log::Info("Font colors: original colors set for all {} fonts", g_replaced);
            const Rgb8 text = ToDescriptor(colors->text);
            const Rgb8 edge = ToDescriptor(colors->edge);
            return g_addFontDescriptor(list, face, face2, height, text.r, text.g, text.b, edge.r, edge.g, edge.b);
        }
        g_mismatch = true;
        log::Warn("Font colors: font #{} (height {}, {} / {}) is not the expected game font - keeping the game's "
                  "colors from here on ({} replaced)",
                  index, height, Hex(game.text), Hex(game.edge), g_replaced);
    }
    return g_addFontDescriptor(list, face, face2, height, textR, textG, textB, edgeR, edgeG, edgeB);
}

}  // namespace

std::optional<ColorMode> ParseColorMode(std::string_view text) {
    if (text == "game") return ColorMode::Game;
    if (text == "original") return ColorMode::Original;
    return std::nullopt;
}

const std::array<FontColorEntry, kGameFontCount>& FontColorTable() { return kTable; }

std::optional<FontColors> OriginalColors(std::size_t index, int height, const FontColors& game) {
    if (index >= kTable.size()) return std::nullopt;
    const FontColorEntry& entry = kTable[index];
    if (entry.height != height || entry.game != game) return std::nullopt;
    return entry.original;
}

Rgb8 ToDescriptor(const Rgb8& visible) {
    // Smallest c with c * max / 255 == level: ceil(level * 255 / max).
    const auto channel = [](std::uint8_t value, int bits) {
        const int max = (1 << bits) - 1;
        const int level = value >> (8 - bits);
        return static_cast<std::uint8_t>((level * 255 + max - 1) / max);
    };
    return {channel(visible.r, 5), channel(visible.g, 6), channel(visible.b, 5)};
}

bool InstallFontColors() {
    const std::string text = config::GetString("Fonts", "Colors", "original");
    const auto mode = ParseColorMode(text);
    if (!mode) {
        log::Warn("[Fonts] Colors=\"{}\" is not original or game - using the game's colors", text);
        return false;
    }
    // Best effort for the Russian edition: no signatures for its EXE (font mode says so in the log).
    if (*mode == ColorMode::Game || game::Exe().edition == game::Edition::Russian) return true;
    const std::uintptr_t target =
        game::Resolve({"AddFontDescriptor", kAddFontDescriptorPattern, 0, kAddFontDescriptorRva});
    if (!target) {
        log::Warn("Font colors: font descriptor function not found - using the game's colors");
        return false;
    }
    if (!hooks::CreateRaw("AddFontDescriptor", reinterpret_cast<void*>(target),
                          reinterpret_cast<void*>(&AddFontDescriptorDetour),
                          reinterpret_cast<void**>(&g_addFontDescriptor)) ||
        !hooks::Enable("AddFontDescriptor")) {
        return false;
    }
    log::Info("Font colors: {} ([Fonts] Colors)", text);
    return true;
}

}  // namespace ddmod::fonts
