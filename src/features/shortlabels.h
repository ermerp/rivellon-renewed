#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace ddmod::features {

// Workaround for menu labels that the menu family's face (Uncial Antiqua, wider than the original
// bitmap letters) draws over the options pages' left arrow: they get a shorter text in memory
// after the game has loaded its localized texts (localizations\<language>\text.cmp,
// docs/re/text-rendering.md, "Text table"). Only while the glyph renderer replaces the menu
// font; [Fonts] ShortLabels=0 keeps the original texts. To be dropped when the menus get an own
// layout (PLAN.md).

// One label: the original text and its shorter replacement (code page 1252, at most as long).
struct ShortLabel {
    std::string_view original;
    std::string_view shorter;
};
// DE "HINTERGRUNDGERÄUSCHE" (sound page) -> "HINTERGRUND".
inline constexpr ShortLabel kShortLabels[] = {{"HINTERGRUNDGER\xC4USCHE", "HINTERGRUND"}};

// Start of the text file loader in div.exe (__thiscall on the game's text table, one argument:
// the file name).
inline constexpr std::string_view kTextLoaderPattern =
    "8B 44 24 04 55 57 68 ?? ?? ?? ?? 50 8B F9 E8 ?? ?? ?? ?? 8B E8 83 C4 08 89 6C 24 0C 85 ED 0F 84 ?? ?? ?? ?? "
    "53 56 33 F6 39 77 04 8D 5F 04 7E";
// RVA of the match in the verified 1.0062A build (fallback only, rule 8).
inline constexpr std::uint32_t kTextLoaderRva = 0x105100;

// Layout of the loaded table: table +0 section pointers, +4 section count; section +4 entry
// pointers, +8 entry count; entry +0 text, +8 text with escape sequences resolved (null if it has
// none), +0x14 text length.
inline constexpr std::size_t kTableSections = 0x00, kTableSectionCount = 0x04;
inline constexpr std::size_t kSectionEntries = 0x04, kSectionEntryCount = 0x08;
inline constexpr std::size_t kEntryText = 0x00, kEntryResolved = 0x08, kEntryLength = 0x14;

// Replaces, in place, every text of `table` that equals one of `labels`' originals; returns the
// number of texts changed. Exposed for the unit tests.
int ShortenLabels(void* table, std::span<const ShortLabel> labels);

// Hooks the text loader so that ShortenLabels(kShortLabels) runs after every load. Must run in
// DllMain, after hooks::Init and game::IdentifyExe; the caller checks [Fonts] ShortLabels and
// that the glyph renderer draws the menu font.
bool InstallShortLabels();

}  // namespace ddmod::features
