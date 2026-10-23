// Unit tests for the parts of the mod that do not need the game: SHA-256, pattern parsing and
// matching, PE section scanning, the MinHook wrapper, the hotkey parser, log rotation and the
// overlay's pixel, blend and glyph code, the text styles and their measurements (src/text)
// (including the ML_FONT_01 bitmap font reader), the font chain and the font mode hook (its
// signature is also checked against a working copy's div.exe if one is present).
// Exit code 0 = all passed.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <windows.h>

#include "core/hooks.h"
#include "core/log.h"
#include "core/pattern.h"
#include "core/scanner.h"
#include "core/sha256.h"
#include "features/dgvoodoo.h"
#include "fonts/fontchain.h"
#include "fonts/fontcolors.h"
#include "fonts/fonthook.h"
#include "fonts/families.h"
#include "fonts/fontmode.h"
#include "fonts/glyphtable.h"
#include "fonts/spacing.h"
#include "input/keys.h"
#include "overlay/banner.h"
#include "overlay/fnt.h"
#include "overlay/glyphs.h"
#include "overlay/overlay.h"
#include "overlay/surface.h"
#include "text/gdiface.h"
#include "text/style.h"

namespace {

int g_failures = 0;
int g_checks = 0;

void Check(bool condition, const char* what, int line) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("FAILED (line %d): %s\n", line, what);
    }
}
#define CHECK(x) Check((x), #x, __LINE__)

std::span<const std::uint8_t> Bytes(std::string_view text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

std::string HexOf(std::string_view text) { return ddmod::sha256::ToHex(ddmod::sha256::Of(Bytes(text))); }

void TestSha256() {
    using namespace ddmod::sha256;
    // FIPS 180-4 / NIST example vectors.
    CHECK(HexOf("") == "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
    CHECK(HexOf("abc") == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
    CHECK(HexOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1");

    const std::string million(1'000'000, 'a');
    CHECK(HexOf(million) == "CDC76E5C9914FB9281A1C7E284D73E67F1809A48A497200E046D39CCC7112CD0");

    // Feeding odd-sized chunks must give the same digest as one call.
    Hasher hasher;
    const auto data = Bytes(million);
    for (std::size_t pos = 0, step = 1; pos < data.size(); pos += step, step = step % 97 + 1) {
        hasher.Update(data.subspan(pos, std::min(step, data.size() - pos)));
    }
    CHECK(ToHex(hasher.Finish()) == HexOf(million));
}

void TestPatternParse() {
    using ddmod::pattern::Parse;
    const auto p = Parse("8B 0D ?? ?? ?? ?? 85 c9");
    CHECK(p.has_value());
    CHECK(p && p->size() == 8);
    CHECK(p && p->bytes[0] == 0x8B && p->bytes[7] == 0xC9);
    CHECK(p && p->wildcard[2] && p->wildcard[5] && !p->wildcard[6]);
    CHECK(Parse("  E8 ? ? ? ?  ").has_value());
    CHECK(!Parse("").has_value());
    CHECK(!Parse("?? ??").has_value());
    CHECK(!Parse("8G").has_value());
    CHECK(!Parse("123").has_value());
    CHECK(!Parse("8B0D").has_value());
}

void TestPatternFind() {
    using namespace ddmod::pattern;
    const std::uint8_t data[] = {0x55, 0x8B, 0xEC, 0x55, 0x8B, 0xEC, 0x90, 0x55, 0x55, 0x8B};
    const auto find = [&](const char* text, std::size_t max = SIZE_MAX) { return FindAll(data, *Parse(text), max); };

    CHECK(find("55 8B EC") == (std::vector<std::size_t>{0, 3}));
    CHECK(find("55 ?? EC") == (std::vector<std::size_t>{0, 3}));
    CHECK(find("?? 55") == (std::vector<std::size_t>{2, 6, 7}));  // leading wildcard
    CHECK(find("55 8B") == (std::vector<std::size_t>{0, 3, 8}));   // match ending at the last byte
    CHECK(find("8B ?? ??").size() == 2);                           // the third 8B has no room
    CHECK(find("55 55") == (std::vector<std::size_t>{7}));
    CHECK(find("55", 2) == (std::vector<std::size_t>{0, 3}));
    CHECK(find("CC").empty());
    CHECK(FindAll(std::span(data, 2), *Parse("55 8B EC")).empty());  // pattern longer than data
}

// A marker in read-only data: found with Area::Image, not with Area::Code.
extern const std::uint8_t kMarker[];
const std::uint8_t kMarker[] = {0xD1, 0x7E, 0x5A, 0x11, 0xC0, 0xDE, 0x13, 0x37, 0xFE, 0xED};

void TestScanner() {
    using namespace ddmod;
    const HMODULE self = GetModuleHandleW(nullptr);
    const auto sections = scanner::Sections(self);
    bool hasCode = false;
    for (const auto& s : sections) hasCode |= s.executable && s.name == ".text";
    CHECK(hasCode);

    const char* text = "D1 7E 5A 11 C0 DE 13 37 FE ED";
    const auto inImage = scanner::FindAll(self, *pattern::Parse(text), scanner::Area::Image);
    CHECK(inImage.size() == 1);
    CHECK(!inImage.empty() && inImage[0] == reinterpret_cast<std::uintptr_t>(kMarker));
    CHECK(scanner::FindAll(self, *pattern::Parse(text), scanner::Area::Code).empty());
    CHECK(scanner::FindUnique("marker", self, "D1 7E 5A ?? C0 DE", scanner::Area::Image) ==
          reinterpret_cast<std::uintptr_t>(kMarker));
    CHECK(scanner::FindUnique("bad", self, "XY", scanner::Area::Image) == 0);

    // call rel32 at offset 0 of the buffer; the target is 0x10 bytes past the end of the call.
    std::uint8_t call[5] = {0xE8, 0x10, 0x00, 0x00, 0x00};
    const auto base = reinterpret_cast<std::uintptr_t>(call);
    CHECK(scanner::FollowRel32(base + 1) == base + 5 + 0x10);
    const std::int32_t back = -0x20;
    std::memcpy(call + 1, &back, sizeof(back));
    CHECK(scanner::FollowRel32(base + 1) == base + 5 - 0x20);
    std::memcpy(call + 1, "\x78\x56\x34\x12", 4);
    CHECK(scanner::ReadU32(base + 1) == 0x12345678u);
}

__declspec(noinline) int __cdecl Multiply(int a, int b) { return a * b + (a ^ 7); }
__declspec(noinline) int __cdecl Subtract(int a, int b) { return (a - b) * 3 + (b | 5); }

using BinaryFn = int(__cdecl*)(int, int);
BinaryFn g_multiplyOriginal = nullptr;
BinaryFn g_subtractOriginal = nullptr;
int g_detourCalls = 0;

int __cdecl MultiplyDetour(int a, int b) {
    ++g_detourCalls;
    return g_multiplyOriginal(a, b) + 1000;
}

int __cdecl SubtractDetour(int a, int b) {
    ++g_detourCalls;
    return g_subtractOriginal(a, b) - 1000;
}

void TestHooks() {
    using namespace ddmod;
    // Call through volatile pointers so the compiler cannot fold the calls.
    BinaryFn volatile multiply = &Multiply;
    BinaryFn volatile subtract = &Subtract;
    const int plainMultiply = multiply(6, 7);
    const int plainSubtract = subtract(9, 4);

    CHECK(hooks::CreateRaw("early", reinterpret_cast<void*>(&Multiply), nullptr, nullptr) == false);  // before Init
    CHECK(hooks::Init());
    CHECK(hooks::Init());  // idempotent

    CHECK(hooks::Create("multiply", &Multiply, &MultiplyDetour, &g_multiplyOriginal));
    CHECK(!hooks::Create("multiply", &Subtract, &SubtractDetour, &g_subtractOriginal));  // duplicate name
    CHECK(!hooks::CreateRaw("null", nullptr, reinterpret_cast<void*>(&SubtractDetour), nullptr));
    CHECK(multiply(6, 7) == plainMultiply);  // created, not enabled yet

    CHECK(hooks::Enable("multiply"));
    CHECK(hooks::IsEnabled("multiply"));
    CHECK(multiply(6, 7) == plainMultiply + 1000);
    CHECK(g_detourCalls == 1);

    CHECK(hooks::Disable("multiply"));
    CHECK(!hooks::IsEnabled("multiply"));
    CHECK(multiply(6, 7) == plainMultiply);

    CHECK(hooks::Create("subtract", &Subtract, &SubtractDetour, &g_subtractOriginal));
    CHECK(hooks::EnableAll());
    CHECK(multiply(6, 7) == plainMultiply + 1000);
    CHECK(subtract(9, 4) == plainSubtract - 1000);

    CHECK(hooks::Remove("subtract"));
    CHECK(subtract(9, 4) == plainSubtract);
    CHECK(!hooks::Enable("subtract"));  // gone

    hooks::Shutdown();
    CHECK(multiply(6, 7) == plainMultiply);
    CHECK(!hooks::IsEnabled("multiply"));
}

// Stand-in for the game's font setup (__thiscall, no stack arguments): records the use8bitfont
// byte it sees while it runs.
std::uint8_t g_fakeSettings[0x174] = {};
std::uint8_t* g_fakeSettingsPointer = nullptr;
int g_seenUse8BitFont = -1;

__declspec(noinline) int __fastcall FakeFontInit(void* self, void* /*edx*/) {
    g_seenUse8BitFont = g_fakeSettingsPointer ? g_fakeSettingsPointer[ddmod::fonts::kUse8BitFontField] : -2;
    return self == &g_seenUse8BitFont ? 61 : -1;
}

void TestFontModeHook() {
    using namespace ddmod;
    using FontInitFn = int(__thiscall*)(void*);
    FontInitFn volatile fontInit = reinterpret_cast<FontInitFn>(&FakeFontInit);
    CHECK(hooks::Init());
    CHECK(fonts::InstallFontModeAt(reinterpret_cast<void*>(&FakeFontInit), &g_fakeSettingsPointer));

    // Settings object not created yet: the call goes through unchanged.
    CHECK(fontInit(&g_seenUse8BitFont) == 61);
    CHECK(g_seenUse8BitFont == -2);

    // use8bitfont 1 from config.lcl: 0 while the fonts are created, 1 again afterwards.
    g_fakeSettingsPointer = g_fakeSettings;
    g_fakeSettings[fonts::kUse8BitFontField] = 1;
    CHECK(fontInit(&g_seenUse8BitFont) == 61);
    CHECK(g_seenUse8BitFont == 0);
    CHECK(g_fakeSettings[fonts::kUse8BitFontField] == 1);
    CHECK(g_fakeSettings[fonts::kUse8BitFontField - 1] == 0 && g_fakeSettings[fonts::kUse8BitFontField + 1] == 0);

    // use8bitfont 0 already: stays 0.
    g_fakeSettings[fonts::kUse8BitFontField] = 0;
    CHECK(fontInit(&g_seenUse8BitFont) == 61);
    CHECK(g_seenUse8BitFont == 0 && g_fakeSettings[fonts::kUse8BitFontField] == 0);

    hooks::Shutdown();
    g_fakeSettings[fonts::kUse8BitFontField] = 1;
    CHECK(fontInit(&g_seenUse8BitFont) == 61 && g_seenUse8BitFont == 1);
}

void TestEditionAndCharset() {
    using namespace ddmod;
    using game::Edition;
    CHECK(game::EditionOfHash("CD9F7A07C7A605C21052B3517346C7A63083063FB491276A3E4C9836C9B42A67") == Edition::Main);
    CHECK(game::EditionOfHash("90599AC1C2F9CC48BF83AEDB9850BA5B487B487E286D3B26C6E7967F0D928E68") ==
          Edition::Russian);
    CHECK(game::EditionOfHash("") == Edition::Unknown);
    CHECK(game::EditionOfHash("cd9f7a07c7a605c21052b3517346c7a63083063fb491276a3e4c9836c9b42a67") == Edition::Unknown);

    // auto: Cyrillic only for the Russian edition, otherwise the game's charset stays.
    CHECK(fonts::CharsetFor("auto", Edition::Russian) == RUSSIAN_CHARSET);
    CHECK(fonts::CharsetFor("auto", Edition::Main) == fonts::kKeepCharset);
    CHECK(fonts::CharsetFor("auto", Edition::Unknown) == fonts::kKeepCharset);
    CHECK(fonts::CharsetFor("", Edition::Russian) == RUSSIAN_CHARSET);
    // Explicit values win over the edition.
    CHECK(fonts::CharsetFor("1251", Edition::Main) == RUSSIAN_CHARSET);
    CHECK(fonts::CharsetFor("1252", Edition::Russian) == ANSI_CHARSET);
    CHECK(fonts::CharsetFor("1250", Edition::Main) == fonts::kCharsetInvalid);
    CHECK(fonts::CharsetFor("Auto", Edition::Russian) == fonts::kCharsetInvalid);

    // Control characters get no glyph (Backspace in text fields); the game sign-extends bytes.
    CHECK(fonts::IsControlCharacter(0x08));
    CHECK(fonts::IsControlCharacter(0x0D));
    CHECK(fonts::IsControlCharacter(0x1F));
    CHECK(!fonts::IsControlCharacter(0x20));
    CHECK(!fonts::IsControlCharacter('.'));
    CHECK(!fonts::IsControlCharacter(0xFFFFFFFCu));  // 0xFC (ü) sign-extended
    CHECK(!fonts::IsControlCharacter(0x7F));
}

// RVA of a raw file offset, from the section headers of a PE file read into memory (0 = none).
std::uint32_t FileOffsetToRva(const std::vector<std::uint8_t>& file, std::size_t offset) {
    std::uint32_t peOffset = 0;
    std::memcpy(&peOffset, file.data() + 0x3C, 4);
    IMAGE_NT_HEADERS32 nt{};
    std::memcpy(&nt, file.data() + peOffset, sizeof(nt));
    const std::size_t first = peOffset + offsetof(IMAGE_NT_HEADERS32, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    for (int i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER section{};
        std::memcpy(&section, file.data() + first + i * sizeof(section), sizeof(section));
        if (offset >= section.PointerToRawData && offset < section.PointerToRawData + section.SizeOfRawData) {
            return static_cast<std::uint32_t>(offset - section.PointerToRawData + section.VirtualAddress);
        }
    }
    return 0;
}

void TestFontModeSignature() {
    using namespace ddmod;
    const auto parsed = pattern::Parse(fonts::kFontInitPattern);
    CHECK(parsed.has_value());
    if (!parsed) return;
    // The settings pointer operand follows "A1"; the pattern ends with the read of the field.
    const auto operand = static_cast<std::size_t>(fonts::kSettingsGlobalOperand);
    CHECK(parsed->bytes[operand - 1] == 0xA1 && !parsed->wildcard[operand - 1] && parsed->wildcard[operand]);
    CHECK(parsed->bytes[parsed->size() - 5] == (fonts::kUse8BitFontField & 0xFF) &&
          parsed->bytes[parsed->size() - 4] == (fonts::kUse8BitFontField >> 8));

    // The real div.exe (never in the repo): DDMOD_DIV_EXE or the German working copy.
    char envPath[MAX_PATH] = {};
    const DWORD envLength = GetEnvironmentVariableA("DDMOD_DIV_EXE", envPath, MAX_PATH);
    const std::filesystem::path exe = envLength > 0 && envLength < MAX_PATH ? envPath : "C:\\dev\\dd-test\\div.exe";
    std::ifstream in(exe, std::ios::binary);
    if (!in) {
        std::printf("note: %s not found - font mode signature not checked against div.exe\n", exe.string().c_str());
        return;
    }
    const std::vector<std::uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (sha256::ToHex(sha256::Of(file)) != "CD9F7A07C7A605C21052B3517346C7A63083063FB491276A3E4C9836C9B42A67") {
        std::printf("note: %s is not 1.0062A - font mode signature not checked\n", exe.string().c_str());
        return;
    }
    const auto matches = pattern::FindAll(file, *parsed);
    CHECK(matches.size() == 1);
    if (matches.size() != 1) return;
    CHECK(FileOffsetToRva(file, matches[0]) == fonts::kFontInitRva);
    std::uint32_t settingsGlobal = 0;
    std::memcpy(&settingsGlobal, file.data() + matches[0] + operand, 4);
    CHECK(settingsGlobal == 0x0065B3D0u);  // global settings pointer (docs/re/config-files.md)

    // Pair spacing function (fonts/spacing.h): unique and at the documented RVA.
    const auto spacing = pattern::Parse(fonts::kPairSpacingPattern);
    CHECK(spacing.has_value());
    if (!spacing) return;
    const auto spacingMatches = pattern::FindAll(file, *spacing);
    CHECK(spacingMatches.size() == 1);
    if (spacingMatches.size() == 1) CHECK(FileOffsetToRva(file, spacingMatches[0]) == fonts::kPairSpacingRva);

    // Font descriptor function (fonts/fontcolors.h): unique and at the documented RVA.
    const auto descriptor = pattern::Parse(fonts::kAddFontDescriptorPattern);
    CHECK(descriptor.has_value());
    if (!descriptor) return;
    const auto descriptorMatches = pattern::FindAll(file, *descriptor);
    CHECK(descriptorMatches.size() == 1);
    if (descriptorMatches.size() == 1) {
        CHECK(FileOffsetToRva(file, descriptorMatches[0]) == fonts::kAddFontDescriptorRva);
    }

    // Font file loader (fonts/glyphtable.h): unique and at the documented RVA.
    const auto loader = pattern::Parse(fonts::kLoadFontFilePattern);
    CHECK(loader.has_value());
    if (!loader) return;
    const auto loaderMatches = pattern::FindAll(file, *loader);
    CHECK(loaderMatches.size() == 1);
    if (loaderMatches.size() == 1) CHECK(FileOffsetToRva(file, loaderMatches[0]) == fonts::kLoadFontFileRva);
}

void TestGlyphTable() {
    using namespace ddmod::fonts;
    // 3x2 glyph: row 0 = pixel, gap, pixel (two spans); row 1 empty.
    PixelGlyph glyph;
    glyph.width = 3;
    glyph.height = 2;
    glyph.originX = 1;
    glyph.originY = -4;
    glyph.advance = 5;
    glyph.character = 'A';
    glyph.pixels = {0x1111, 0, 0x2222, 0, 0, 0};
    const auto memory = BuildGameGlyph(glyph);
    const std::uint8_t* g = memory.get();
    const auto u16 = [](const std::uint8_t* at) { std::uint16_t v; std::memcpy(&v, at, 2); return v; };
    const auto u32 = [](const std::uint8_t* at) { std::uint32_t v; std::memcpy(&v, at, 4); return v; };
    CHECK(u32(g + 0x00) == 3 && u32(g + 0x04) == 2);
    CHECK(static_cast<std::int32_t>(u32(g + 0x08)) == 1 && static_cast<std::int32_t>(u32(g + 0x0C)) == -4);
    CHECK(u16(g + 0x10) == 5 && u16(g + 0x12) == 0 && u16(g + 0x14) == 'A');
    const std::uint8_t* sprite = g + kGlyphSprite;
    // Header: 12 + row 0 (2 + 4 + 2 spans * 4 + 2) + row 1 (2 + 4 + 2) = 36; 2 pixels.
    CHECK(u32(sprite + 4) == 36);
    CHECK(u32(sprite + 0) == 36 + 4);
    CHECK(u16(sprite + 8) == 3 && u16(sprite + 10) == 2);
    const std::uint8_t* row = sprite + 12;
    CHECK(u16(row) == 2);
    CHECK(u32(row + 2) == static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(sprite + 36)));
    CHECK(u16(row + 6) == 0 && u16(row + 8) == 1 && u16(row + 10) == 2 && u16(row + 12) == 1);
    CHECK(u16(row + 14) == 2);
    CHECK(u16(row + 16) == 0 && u16(row + 22) == 0);
    CHECK(u16(sprite + 36) == 0x1111 && u16(sprite + 38) == 0x2222);

    // Back from the game's layout (absolute row pointers, as after the game's loader).
    const ddmod::overlay::Glyph decoded = DecodeGameGlyph(g);
    CHECK(decoded.width == 3 && decoded.height == 2 && decoded.offsetX == 1 && decoded.offsetY == 4 && decoded.advance == 5);
    CHECK(decoded.coverage == std::vector<std::uint8_t>({255, 0, 255, 0, 0, 0}));
    CHECK(ddmod::overlay::Pack(ddmod::overlay::PixelFormat::Rgb565, decoded.colors[0]) == 0x1111 &&
          ddmod::overlay::Pack(ddmod::overlay::PixelFormat::Rgb565, decoded.colors[2]) == 0x2222);

    CHECK(ParseRenderer("glyphs") == Renderer::Glyphs && ParseRenderer("GDI") == Renderer::Gdi && !ParseRenderer("bitmap"));
    CHECK(FontStem("fonts\\FontN1.fnt") == "fontn1");
    CHECK(FontStem("dialog_white") == "dialog_white");
}

void TestFamilies() {
    using namespace ddmod::fonts;
    // Every game font belongs to its family; nothing else does.
    int counts[std::size(kFamilies)] = {};
    for (const FontColorEntry& entry : FontColorTable()) {
        const auto family = FamilyOf(entry.file);
        CHECK(family.has_value());
        if (family) ++counts[static_cast<int>(*family)];
    }
    CHECK(counts[static_cast<int>(Family::Interface)] == 40 && counts[static_cast<int>(Family::Books)] == 8 &&
          counts[static_cast<int>(Family::Menu)] == 6 && counts[static_cast<int>(Family::Dialog)] == 4 &&
          counts[static_cast<int>(Family::DiaryTitle)] == 3);
    CHECK(!FamilyOf("fontn6") && !FamilyOf("fontn") && !FamilyOf("arial") && !FamilyOf("fontbig48"));

    CHECK(ParseLineHeight("auto")->automatic && ParseLineHeight("AUTO")->automatic);
    CHECK(!ParseLineHeight("125")->automatic && ParseLineHeight("125")->percent == 125);
    CHECK(!ParseLineHeight("49") && !ParseLineHeight("301") && !ParseLineHeight("12x") && !ParseLineHeight(""));

    CHECK(DefaultSettings(Family::Books).lineHeight.automatic && DefaultSettings(Family::Books).weight == 400);
    CHECK(DefaultSettings(Family::Menu).style == GlyphStyle::Shine && DefaultSettings(Family::Menu).texture);
    // The menu fonts' '[' and ']' are arrows; no other family has symbol glyphs.
    CHECK(DefaultSettings(Family::Menu).keepOriginal == "[]");
    CHECK(DefaultSettings(Family::Interface).keepOriginal.empty() && DefaultSettings(Family::Books).keepOriginal.empty());
    CHECK(DefaultSettings(Family::DiaryTitle).style == GlyphStyle::Measured);
    // Uncial Antiqua's and Almendra SC's H reach above their other capitals.
    CHECK(DefaultSettings(Family::Menu).capitals == ddmod::text::CapReference::Median &&
          DefaultSettings(Family::DiaryTitle).capitals == ddmod::text::CapReference::Median);
    CHECK(ParseTopFit("lower") == TopFit::Lower && ParseTopFit("Squeeze") == TopFit::Squeeze &&
          ParseTopFit("NONE") == TopFit::None && ParseTopFit("Fit") == TopFit::Fit && !ParseTopFit("shrink"));
    CHECK(DefaultSettings(Family::Menu).topFit == TopFit::Lower && DefaultSettings(Family::DiaryTitle).topFit == TopFit::Fit &&
          DefaultSettings(Family::Books).topFit == TopFit::None && DefaultSettings(Family::Interface).topFit == TopFit::None);
    CHECK(DefaultSettings(Family::Interface).capitals == ddmod::text::CapReference::H &&
          DefaultSettings(Family::Books).capitals == ddmod::text::CapReference::H);
    CHECK(DefaultSettings(Family::Interface).style == GlyphStyle::Ramp && !DefaultSettings(Family::Interface).lineHeight.automatic);

    // The bundled faces: what the defaults name is in assets/fonts, GDI selects exactly that face,
    // and it has every German and French character (no Cyrillic - the Russian edition keeps its
    // bitmap fonts).
    const std::filesystem::path assets = std::filesystem::path(DDMOD_SOURCE_DIR) / L"assets";
    for (const Family family : {Family::Interface, Family::Books, Family::Menu, Family::DiaryTitle}) {
        const auto chain = ParseChain(DefaultSettings(family).chain, assets);
        CHECK(chain.size() == 1 && std::filesystem::exists(assets / L"fonts" / chain[0].file.filename()));
        if (chain.size() != 1) continue;
        const std::filesystem::path file = assets / L"fonts" / chain[0].file.filename();
        CHECK(AddFontResourceExW(file.c_str(), FR_PRIVATE, nullptr) > 0);
        const ddmod::text::GdiFace face(chain[0].face, 20, DefaultSettings(family).weight);
        CHECK(face.ActualFace() == chain[0].face);
        CHECK(face.Missing(L"ÄÖÜäöüß ÀÂÆÇÈÉÊËÎÏÔŒÙÛŸàâæçèéêëîïôœùûÿ«»").empty());
        CHECK(face.Missing(L"Ж") == L"Ж");
        RemoveFontResourceExW(file.c_str(), FR_PRIVATE, nullptr);
    }
}

void TestFontColors() {
    using namespace ddmod::fonts;
    CHECK(ParseColorMode("original") == ColorMode::Original);
    CHECK(ParseColorMode("game") == ColorMode::Game);
    CHECK(!ParseColorMode("Original").has_value());
    CHECK(!ParseColorMode("").has_value());

    const auto& table = FontColorTable();
    // The game's first font: fontn1, height 15, white on black.
    const FontColors white{{0xFF, 0xFF, 0xFF}, {0, 0, 0}};
    const auto first = OriginalColors(0, 15, white);
    CHECK(first && first->text == (Rgb8{0xE7, 0xE7, 0xE7}) && first->edge == (Rgb8{0, 0, 0}));
    CHECK(!OriginalColors(0, 14, white));                                  // other height
    CHECK(!OriginalColors(0, 15, {{0xFF, 0xFF, 0x00}, {0, 0, 0}}));        // other color
    CHECK(!OriginalColors(kGameFontCount, 15, white));                     // beyond the table
    // dialog_yellow (index 50): pure yellow in the game, the bitmap font's gold.
    CHECK(table[50].file == "dialog_yellow");
    const auto yellow = OriginalColors(50, 14, {{0xFF, 0xFF, 0x00}, {0, 0, 0}});
    CHECK(yellow && yellow->text == (Rgb8{0xDE, 0xBA, 0x18}));
    CHECK(table[60].file == "paper_red_high" && table[60].height == 15);

    // The game's 8-bit to 565 conversion (c * 31 / 255, green c * 63 / 255) of ToDescriptor's value
    // gives the 565 level of the visible color for every channel value.
    bool exact = true;
    for (int v = 0; v < 256; ++v) {
        const auto c = static_cast<std::uint8_t>(v);
        const Rgb8 d = ToDescriptor({c, c, c});
        exact = exact && d.r * 31 / 255 == v >> 3 && d.g * 63 / 255 == v >> 2 && d.b * 31 / 255 == v >> 3;
    }
    CHECK(exact);
    CHECK(ToDescriptor({0xDE, 0xBA, 0x18}) == (Rgb8{0xDF, 0xBB, 0x19}));  // gold: one step up
    CHECK(ToDescriptor({0xFF, 0xFF, 0xFF}) == (Rgb8{0xFF, 0xFF, 0xFF}));
    CHECK(ToDescriptor({0, 0, 0}) == (Rgb8{0, 0, 0}));

    // Against the bitmap fonts of the German working copy (never in the repo): the text color of
    // every flat font is the most frequent color inside its glyphs (pixels with four opaque neighbors).
    const std::filesystem::path fonts = L"C:\\dev\\dd-test\\fonts";
    if (!std::filesystem::exists(fonts / L"fontn1.fnt")) {
        std::printf("note: %s not found - font colors not checked against the bitmap fonts\n", fonts.string().c_str());
        return;
    }
    for (const FontColorEntry& entry : table) {
        if (entry.file.starts_with("fontbig") || entry.file.starts_with("diary")) continue;  // textured: mean
        auto source = ddmod::overlay::CreateFntGlyphSource(fonts / (std::string(entry.file) + ".fnt"));
        CHECK(source != nullptr);
        if (!source) continue;
        std::map<std::uint32_t, int> counts;
        for (int ch = 0x21; ch <= 0xFF; ++ch) {
            ddmod::overlay::Glyph g;
            if (!source->Rasterize(static_cast<wchar_t>(ch), g)) continue;
            const auto opaque = [&](int x, int y) {
                return x >= 0 && y >= 0 && x < g.width && y < g.height && g.coverage[y * g.width + x] != 0;
            };
            for (int y = 0; y < g.height; ++y) {
                for (int x = 0; x < g.width; ++x) {
                    if (!opaque(x, y) || !opaque(x - 1, y) || !opaque(x + 1, y) || !opaque(x, y - 1) || !opaque(x, y + 1)) {
                        continue;
                    }
                    const auto& c = g.colors[y * g.width + x];
                    ++counts[(std::uint32_t{c.r} << 16) | (std::uint32_t{c.g} << 8) | c.b];
                }
            }
        }
        std::uint32_t mode = 0;
        int best = 0;
        for (const auto& [color, count] : counts) {
            if (count > best) best = count, mode = color;
        }
        const auto& t = entry.original.text;
        const bool same = mode == ((std::uint32_t{t.r} << 16) | (std::uint32_t{t.g} << 8) | t.b);
        if (!same) std::printf("  %s: most frequent inner color #%06X\n", std::string(entry.file).c_str(), mode);
        CHECK(same);
    }
}

void TestSpacing() {
    using namespace ddmod::fonts;
    CHECK(ParseSpacingMode("game") == SpacingMode::Game);
    CHECK(ParseSpacingMode("gap") == SpacingMode::Gap);
    CHECK(ParseSpacingMode("font") == SpacingMode::Font);
    CHECK(!ParseSpacingMode("Gap").has_value());
    CHECK(!ParseSpacingMode("").has_value());

    // "1." in 12 px Tahoma Bold ('1': origin x 1, advance 6): the game slides the period to 4.
    CHECK(AdjustSpacing(SpacingMode::Game, 4, 1, 6) == 4);
    CHECK(AdjustSpacing(SpacingMode::Gap, 4, 1, 6) == 5);
    CHECK(AdjustSpacing(SpacingMode::Font, 4, 1, 6) == 6);  // the font's advance, origin x not added
    // Gap never goes beyond the game's no-touch distance, and leaves pairs it did not slide alone.
    CHECK(AdjustSpacing(SpacingMode::Gap, 6, 1, 6) == 7);
    CHECK(AdjustSpacing(SpacingMode::Gap, 7, 1, 6) == 7);
    CHECK(AdjustSpacing(SpacingMode::Gap, 8, 1, 6) == 8);
}

void TestKeyCombo() {
    using namespace ddmod::input;
    const auto parse = [](const char* text) { return ParseKeyCombo(text); };
    const auto is = [](std::optional<KeyCombo> c, std::uint8_t vk, std::uint8_t mods) {
        return c && c->vk == vk && c->mods == mods;
    };

    CHECK(is(parse("Ctrl+Shift+F10"), VK_F10, kCtrl | kShift));
    CHECK(is(parse("shift + ctrl + f10"), VK_F10, kCtrl | kShift));  // order and case do not matter
    CHECK(is(parse("Control+Alt+Delete"), VK_DELETE, kCtrl | kAlt));
    CHECK(is(parse("F1"), VK_F1, kNone));
    CHECK(is(parse("F24"), VK_F24, kNone));
    CHECK(is(parse("a"), 'A', kNone));
    CHECK(is(parse("Alt+7"), '7', kAlt));
    CHECK(is(parse("Num0"), VK_NUMPAD0, kNone));
    CHECK(is(parse("Ctrl+NumAdd"), VK_ADD, kCtrl));
    CHECK(is(parse("PgDn"), VK_NEXT, kNone));
    CHECK(is(parse("  Esc "), VK_ESCAPE, kNone));

    // Unbound.
    CHECK(is(parse(""), 0, kNone));
    CHECK(is(parse("   "), 0, kNone));
    CHECK(is(parse("none"), 0, kNone));
    CHECK(!parse("")->Bound());

    // Syntax errors.
    CHECK(!parse("Ctrl"));            // modifier without key
    CHECK(!parse("Ctrl+"));
    CHECK(!parse("+F1"));
    CHECK(!parse("Ctrl++F1"));
    CHECK(!parse("Ctrl+Ctrl+F1"));    // repeated modifier
    CHECK(!parse("F1+Ctrl"));         // key must come last
    CHECK(!parse("F1+F2"));
    CHECK(!parse("F0"));
    CHECK(!parse("F25"));
    CHECK(!parse("F01"));
    CHECK(!parse("Num10"));
    CHECK(!parse("Win+F1"));
    CHECK(!parse("PrintScreen"));
    CHECK(!parse("?"));
    CHECK(!parse("Ctrl+None"));

    // Canonical text, and round trip.
    CHECK(ToString(KeyCombo{}) == "None");
    CHECK(ToString(*parse("shift+alt+ctrl+f10")) == "Ctrl+Shift+Alt+F10");
    CHECK(ToString(*parse("ctrl+pgup")) == "Ctrl+PageUp");
    CHECK(ToString(*parse("num5")) == "Num5");
    CHECK(ToString(*parse("q")) == "Q");
    CHECK(ToString(KeyCombo{0xE2, kNone}) == "VK_E2");
    for (const char* text : {"Ctrl+Shift+F10", "Alt+Enter", "NumDecimal", "Shift+Z", "ScrollLock", "F24"}) {
        CHECK(ToString(*parse(text)) == text);
    }

    // Matching needs the exact modifier set.
    const KeyCombo hotkey = *parse("Ctrl+Shift+F10");
    CHECK(Matches(hotkey, VK_F10, kCtrl | kShift));
    CHECK(!Matches(hotkey, VK_F10, kCtrl));
    CHECK(!Matches(hotkey, VK_F10, kCtrl | kShift | kAlt));
    CHECK(!Matches(hotkey, VK_F11, kCtrl | kShift));
    CHECK(!Matches(KeyCombo{}, 0, kNone));
}

std::string ReadText(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void TestLogRotation() {
    namespace fs = std::filesystem;
    using namespace ddmod;
    const fs::path dir = fs::temp_directory_path() / std::format("ddmod_tests_{}", GetCurrentProcessId());
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path file = dir / "RivellonRenewed.log";
    const fs::path prev = dir / "RivellonRenewed.prev.log";
    CHECK(log::PreviousFile(file) == prev);

    log::Open(file, log::Level::Info);  // nothing to keep yet
    log::Info("first start");
    log::Close();
    CHECK(!fs::exists(prev));

    log::Open(file, log::Level::Info);
    log::Info("second start");
    log::Close();
    CHECK(ReadText(prev).find("first start") != std::string::npos);
    CHECK(ReadText(file).find("second start") != std::string::npos);
    CHECK(ReadText(file).find("first start") == std::string::npos);

    log::Open(file, log::Level::Info);  // only one older copy is kept
    log::Info("third start");
    log::Close();
    CHECK(ReadText(prev).find("second start") != std::string::npos);
    CHECK(ReadText(prev).find("first start") == std::string::npos);
    CHECK(ReadText(file).find("third start") != std::string::npos);
    CHECK(!fs::exists(log::ArchiveDir(file)));  // history 0: nothing archived

    fs::remove_all(dir);
}

void TestLogArchive() {
    namespace fs = std::filesystem;
    using namespace ddmod;
    CHECK(log::ArchiveName(L"RivellonRenewed",
                           "2026-10-20 17:11:06.504 [INFO ] [20900] Rivellon Renewed 0.01 loaded") ==
          L"RivellonRenewed-20261020-171106.log");
    CHECK(log::ArchiveName(L"RivellonRenewed", "2026-10-20 17:11:06") == L"RivellonRenewed-20261020-171106.log");
    CHECK(log::ArchiveName(L"RivellonRenewed", "2026-10-20 17:11").empty());
    CHECK(log::ArchiveName(L"RivellonRenewed", "2026-10-20T17:11:06").empty());
    CHECK(log::ArchiveName(L"RivellonRenewed", "garbage line without a time").empty());
    CHECK(log::ArchiveName(L"RivellonRenewed", "").empty());

    const fs::path dir = fs::temp_directory_path() / std::format("ddmod_tests_archive_{}", GetCurrentProcessId());
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path file = dir / "RivellonRenewed.log";
    const fs::path archive = log::ArchiveDir(file);
    CHECK(archive == dir / "RivellonRenewed" / "logs");

    // Older starts with known start times, written as the previous log before each Open.
    const char* starts[] = {"2026-10-20 17:00:01", "2026-10-20 17:00:02", "2026-10-20 17:00:03",
                            "2026-10-20 17:00:04"};
    for (const char* start : starts) {
        std::ofstream(log::PreviousFile(file), std::ios::binary) << start << ".000 [INFO ] old start\n";
        log::Open(file, log::Level::Info, 2);
        log::Info("current start");
        log::Close();
    }
    // Each Open archived the previous log; only the newest two remain.
    CHECK(!fs::exists(archive / "RivellonRenewed-20261020-170001.log"));
    CHECK(!fs::exists(archive / "RivellonRenewed-20261020-170002.log"));
    CHECK(fs::exists(archive / "RivellonRenewed-20261020-170003.log"));
    CHECK(ReadText(archive / "RivellonRenewed-20261020-170004.log").find("old start") != std::string::npos);
    CHECK(ReadText(log::PreviousFile(file)).find("current start") != std::string::npos);

    // Same start time again: kept under a second name instead of overwriting.
    std::ofstream(log::PreviousFile(file), std::ios::binary) << "2026-10-20 17:00:04.500 [INFO ] twin\n";
    log::Open(file, log::Level::Info, 5);
    log::Close();
    CHECK(ReadText(archive / "RivellonRenewed-20261020-170004-2.log").find("twin") != std::string::npos);
    CHECK(fs::exists(archive / "RivellonRenewed-20261020-170004.log"));

    fs::remove_all(dir);
}

bool Same(ddmod::overlay::Rgb a, ddmod::overlay::Rgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

void TestPixelFormats() {
    using namespace ddmod::overlay;
    CHECK(Pack(PixelFormat::Rgb565, {255, 255, 255}) == 0xFFFF);
    CHECK(Pack(PixelFormat::Rgb555, {255, 255, 255}) == 0x7FFF);
    CHECK(Pack(PixelFormat::Xrgb8888, {0x12, 0x34, 0x56}) == 0x123456);
    CHECK(Pack(PixelFormat::Rgb565, {255, 0, 0}) == 0xF800);
    CHECK(Pack(PixelFormat::Rgb565, {0, 255, 0}) == 0x07E0);
    CHECK(Pack(PixelFormat::Rgb555, {0, 255, 0}) == 0x03E0);
    CHECK(Pack(PixelFormat::Rgb555, {0, 0, 255}) == 0x001F);
    // Expanding 5/6-bit channels reaches full white and black.
    CHECK(Same(Unpack(PixelFormat::Rgb565, 0xFFFF), {255, 255, 255}));
    CHECK(Same(Unpack(PixelFormat::Rgb555, 0x7FFF), {255, 255, 255}));
    CHECK(Same(Unpack(PixelFormat::Rgb565, 0x0000), {0, 0, 0}));
    CHECK(Same(Unpack(PixelFormat::Xrgb8888, 0xFF123456), {0x12, 0x34, 0x56}));  // X byte ignored
    // Round trip keeps the representable bits.
    for (const PixelFormat f : {PixelFormat::Rgb565, PixelFormat::Rgb555}) {
        bool ok = true;
        for (std::uint32_t p = 0; p < 0x10000; ++p) {
            const std::uint32_t v = f == PixelFormat::Rgb555 ? (p & 0x7FFF) : p;
            ok &= Pack(f, Unpack(f, v)) == v;
        }
        CHECK(ok);
    }
}

void TestBlend() {
    using namespace ddmod::overlay;
    const Rgb black{0, 0, 0}, white{255, 255, 255};
    CHECK(Same(Mix(black, white, 0), black));
    CHECK(Same(Mix(black, white, 255), white));
    CHECK(Same(Mix(black, white, 128), {128, 128, 128}));
    CHECK(Same(Mix({100, 200, 50}, {200, 0, 50}, 51), {120, 160, 50}));
}

void TestFillAndMask() {
    using namespace ddmod::overlay;
    // 4x3 RGB565 surface with a padded pitch (12 bytes = 6 pixels); the padding must stay untouched.
    std::uint16_t pixels[3 * 6] = {};
    for (auto& p : pixels) p = 0x1234;
    SurfaceView view{reinterpret_cast<std::uint8_t*>(pixels), 12, 4, 3, PixelFormat::Rgb565};
    const auto at = [&](int x, int y) { return pixels[y * 6 + x]; };

    FillRect(view, -2, -1, 4, 3, {255, 255, 255}, 255);  // clipped to (0,0)-(1,1)
    CHECK(at(0, 0) == 0xFFFF && at(1, 1) == 0xFFFF);
    CHECK(at(2, 0) == 0x1234 && at(0, 2) == 0x1234);
    FillRect(view, 3, 2, 10, 10, {0, 0, 0}, 255);  // clipped to (3,2)
    CHECK(at(3, 2) == 0x0000 && at(4, 2) == 0x1234 && at(5, 2) == 0x1234);  // padding
    FillRect(view, 10, 10, 2, 2, {0, 0, 0}, 255);  // fully outside
    FillRect(view, 0, 0, 4, 3, {0, 0, 0}, 0);      // alpha 0
    CHECK(at(0, 0) == 0xFFFF);

    // Mask 3x2: opaque, half, none / none, opaque, opaque, drawn at (2, 1) -> right column clipped.
    for (auto& p : pixels) p = 0x0000;
    const std::uint8_t mask[] = {255, 128, 0, 0, 255, 255};
    DrawMask(view, 2, 1, mask, 3, 2, 3, {255, 255, 255}, 255);
    CHECK(at(2, 1) == 0xFFFF);
    CHECK(Same(Unpack(PixelFormat::Rgb565, at(3, 1)), Unpack(PixelFormat::Rgb565, Pack(PixelFormat::Rgb565, {128, 128, 128}))));
    CHECK(at(2, 2) == 0x0000 && at(3, 2) == 0xFFFF);
    CHECK(at(4, 1) == 0x0000 && at(4, 2) == 0x0000);  // clipped column lands in the padding
    CHECK(at(1, 1) == 0x0000 && at(2, 0) == 0x0000);

    // 32-bit surface: the X byte is written as 0.
    std::uint32_t wide[2] = {0xAABBCCDD, 0xAABBCCDD};
    SurfaceView view32{reinterpret_cast<std::uint8_t*>(wide), 8, 2, 1, PixelFormat::Xrgb8888};
    FillRect(view32, 1, 0, 1, 1, {0x11, 0x22, 0x33}, 255);
    CHECK(wide[0] == 0xAABBCCDD && wide[1] == 0x00112233);
}

// Glyph source with fixed 2x2 glyphs; 'x' is "missing".
class FakeGlyphs final : public ddmod::overlay::GlyphSource {
public:
    int calls = 0;
    int LineHeight() const override { return 10; }
    bool Rasterize(wchar_t ch, ddmod::overlay::Glyph& out) override {
        ++calls;
        if (ch == L'x') return false;
        out = {};
        out.advance = ch == L'?' ? 7 : 5;
        if (ch == L' ') return true;
        out.width = out.height = 2;
        out.offsetX = 1;
        out.offsetY = 3;
        out.coverage.assign(4, 255);
        return true;
    }
};

void TestGlyphCache() {
    using namespace ddmod::overlay;
    auto source = std::make_unique<FakeGlyphs>();
    FakeGlyphs* fake = source.get();
    GlyphCache cache(std::move(source));
    CHECK(cache.LineHeight() == 10);
    CHECK(MeasureString(cache, L"ab a") == 20);
    CHECK(fake->calls == 3);  // 'a' is rasterized once
    CHECK(cache.Get(L'x').advance == 7);  // missing -> '?'
    const int calls = fake->calls;
    cache.Get(L'x');
    CHECK(fake->calls == calls);  // the fallback is cached too

    std::uint16_t pixels[10 * 10] = {};
    SurfaceView view{reinterpret_cast<std::uint8_t*>(pixels), 20, 10, 10, PixelFormat::Rgb565};
    DrawString(view, cache, 0, 0, L"a a", {255, 255, 255}, false);
    // First 'a' at pen 0: mask at (1,3)-(2,4); the space advances 5; the second 'a' at pen 10 is off-surface.
    CHECK(pixels[3 * 10 + 1] == 0xFFFF && pixels[4 * 10 + 2] == 0xFFFF);
    CHECK(pixels[3 * 10 + 0] == 0 && pixels[2 * 10 + 1] == 0 && pixels[3 * 10 + 3] == 0);
    for (auto& p : pixels) p = 0xFFFF;
    DrawString(view, cache, 0, 0, L"a", {255, 255, 255}, true);  // shadow one pixel down-right
    CHECK(pixels[3 * 10 + 1] == 0xFFFF);                           // text over its shadow
    CHECK(pixels[5 * 10 + 3] != 0 && pixels[5 * 10 + 3] != 0xFFFF);  // shadow only, blended
}

void TestGdiGlyphs() {
    using namespace ddmod::overlay;
    FontSpec spec;  // Tahoma bold 16 px, installed on every Windows
    auto source = CreateGdiGlyphSource(spec);
    CHECK(source != nullptr);
    if (!source) return;
    CHECK(source->LineHeight() >= 16);
    Glyph glyph;
    CHECK(source->Rasterize(L'M', glyph));
    CHECK(glyph.width > 4 && glyph.height > 8 && glyph.advance > glyph.width / 2);
    CHECK(glyph.coverage.size() == static_cast<std::size_t>(glyph.width) * glyph.height);
    bool opaque = false;
    for (const auto c : glyph.coverage) opaque |= c == 255;
    CHECK(opaque);
    CHECK(glyph.offsetY >= 0 && glyph.offsetY + glyph.height <= source->LineHeight());
    Glyph umlaut;
    CHECK(source->Rasterize(L'\u00C4', umlaut) && umlaut.height > glyph.height);  // Ä is taller than M
    Glyph space;
    CHECK(source->Rasterize(L' ', space) && space.coverage.empty() && space.advance > 0);

    FontSpec missing;
    missing.file = std::filesystem::temp_directory_path() / L"ddmod-no-such-font.ttf";
    CHECK(CreateGdiGlyphSource(missing) != nullptr);  // falls back to the installed face
}

// Capitals built by the test (no game data): every letter is a 6x8 block whose rows run from
// `top` to `bottom` in color, and the outermost column is darker by `edge`.
ddmod::text::Capitals TestCapitals(ddmod::overlay::Rgb top, ddmod::overlay::Rgb bottom, float edge) {
    ddmod::text::Capitals caps(26);
    for (auto& g : caps) {
        g.width = 6;
        g.height = 8;
        g.offsetY = 2;
        g.advance = 7;
        g.coverage.assign(48, 255);
        g.colors.resize(48);
        for (int y = 0; y < 8; ++y) {
            const ddmod::overlay::Rgb row = ddmod::text::Shade(top, bottom, y / 7.0f);
            for (int x = 0; x < 6; ++x) {
                const float k = x == 0 || x == 5 ? edge : 1.0f;
                g.colors[static_cast<std::size_t>(y) * 6 + x] = {static_cast<std::uint8_t>(row.r * k),
                                                                 static_cast<std::uint8_t>(row.g * k),
                                                                 static_cast<std::uint8_t>(row.b * k)};
            }
        }
    }
    return caps;
}

// A plain stem from the top to the bottom (no accent).
ddmod::text::Image stem0() {
    ddmod::text::Image img = ddmod::text::MakeImage(2, 10);
    for (int y = 0; y < 10; ++y) img.A(0, y) = 1.0f;
    return img;
}

void TestTextStyle() {
    using namespace ddmod::text;
    CHECK(To565({255, 255, 255}).r == 255 && To565({255, 255, 255}).g == 255);
    CHECK(To565({0x9C, 0x8E, 0x5A}).r == 0x9C && To565({0x9C, 0x8E, 0x5A}).g == 0x8E);
    CHECK(To565({0x9F, 0x8F, 0x5F}).r == 0x9C);  // the low bits are lost, as in the back buffer

    // Measurements: flat color, no edge shading.
    const Capitals flat = TestCapitals({200, 100, 50}, {200, 100, 50}, 1.0f);
    const CapMetrics cap = MeasureCap(flat);
    CHECK(cap.top == 2 && cap.height == 8);
    const auto profile = MeasureProfile(flat);
    CHECK(profile.size() == 16 && profile.front().r == 200 && profile.back().b == 50);
    const Bevel even = MeasureBevel(flat, profile);
    CHECK(std::abs(even.side[2][0] - 1.0f) < 0.01f && std::abs(even.side[0][0] - 1.0f) < 0.01f);
    const Shine still = MeasureShine(flat);
    CHECK(std::abs(still.across[0][0] - 1.0f) < 0.01f && still.texture < 0.001f);
    CHECK(MeasureCap(Capitals{}).height == 0);  // no letters: nothing measured, no crash
    CHECK(MeasureCap(Capitals{}, CapReference::Median).height == 0);

    // Median capitals: an H taller than the others moves the H measure, not the median one.
    Capitals tallH = flat;
    ddmod::overlay::Glyph& h = tallH['H' - 'A'];
    h.offsetY = -3;
    h.height = 13;
    h.coverage.assign(6 * 13, 255);
    h.colors.assign(6 * 13, {200, 100, 50});
    CHECK(MeasureCap(tallH).top == -3 && MeasureCap(tallH).height == 13);
    CHECK(MeasureCap(tallH, CapReference::Median).top == 2 && MeasureCap(tallH, CapReference::Median).height == 8);
    CHECK(MedianCap({1, 5, 3}, {9, 11, 10}).top == 3 && MedianCap({1, 5, 3}, {9, 11, 10}).height == 8);
    CHECK(MedianCap({1}, {}).height == 0);  // mismatched lists: nothing

    // A gradient from top to bottom and darker left/right ends.
    const Capitals shaded = TestCapitals({240, 240, 240}, {80, 80, 80}, 0.5f);
    const auto gradient = MeasureProfile(shaded);
    CHECK(gradient.front().r > gradient.back().r);
    const Bevel sides = MeasureBevel(shaded, gradient);
    CHECK(sides.side[2][0] < 0.7f && sides.side[3][0] < 0.7f);  // outermost column darker
    const Shine across = MeasureShine(shaded);
    CHECK(across.across[0][0] < across.across[0][2]);  // horizontal runs: ends darker than the middle

    // Noise: deterministic, in range.
    CHECK(ValueNoise(5, 7, 2) == ValueNoise(5, 7, 2));
    bool inRange = true;
    for (int i = 0; i < 200; ++i) inRange &= std::abs(ValueNoise(i * 3, i * 7, 2)) <= 1.0f;
    CHECK(inRange);

    // Painting.
    Image mask = MakeImage(8, 8);
    for (int y = 1; y < 7; ++y) {
        for (int x = 1; x < 7; ++x) mask.A(x, y) = 1.0f;
    }
    mask.A(0, 3) = 0.5f;
    const Image ramp = PaintRamp(mask, {255, 255, 255}, {0, 0, 0});
    CHECK(ramp.A(3, 3) == 1.0f && ramp.C(3, 3).r == 255);
    CHECK(ramp.A(0, 3) == 1.0f && ramp.C(0, 3).r > 100 && ramp.C(0, 3).r < 160);  // half coverage: middle of the ramp
    CHECK(ramp.A(0, 0) == 0.0f);

    StyleParams plain;
    plain.profile = std::vector<Rgb>(16, Rgb{200, 100, 48});
    plain.contour = {0, 0, 0};
    const Image painted = PaintStyled(mask, 1, 6, plain);
    CHECK(painted.A(3, 3) == 1.0f && painted.C(3, 3).r == 206 && painted.C(3, 3).b == 49);  // 565 of the profile
    CHECK(painted.A(0, 0) == 0.0f);

    Shine shine;
    shine.across[0][0] = shine.across[0][4] = 0.8f;
    shine.across[0][2] = 1.2f;
    StyleParams glossy = plain;
    glossy.shine = &shine;
    const Image shiny = PaintStyled(mask, 1, 6, glossy);
    CHECK(shiny.C(1, 3).r < shiny.C(3, 3).r);  // stroke end darker than its middle
    glossy.shineStrength = 0.0f;
    CHECK(PaintStyled(mask, 1, 6, glossy).C(1, 3).r == painted.C(1, 3).r);  // strength 0 = no shine
    CHECK(PaintStyled(mask, 1, 6, StyleParams{}).A(3, 3) == 0.0f);         // no profile: nothing drawn

    // Keeping ink below the line top: a dot two rows above a body that starts at row 6, with three
    // empty rows between them, moves down by at most two rows (one empty row stays).
    Image umlaut = MakeImage(4, 10);
    umlaut.A(1, 1) = umlaut.A(1, 2) = 1.0f;
    for (int y = 6; y < 10; ++y) umlaut.A(0, y) = 1.0f;
    CHECK(InkTop(umlaut) == 1 && InkTop(MakeImage(2, 2)) == 2);
    CHECK(LowerAccent(umlaut, 6, 5) == 2 && InkTop(umlaut) == 3 && umlaut.A(1, 4) == 1.0f && umlaut.A(1, 5) == 0.0f);
    CHECK(LowerAccent(umlaut, 6, 5) == 0);  // no gap left to use
    // Dots between the raised arms of a U (no empty row between them and the letter): they still
    // come down, but stay above the capitals' top.
    Image u = MakeImage(8, 10);
    for (int y = 0; y < 10; ++y) u.A(0, y) = u.A(7, y) = 1.0f;
    for (int x = 0; x < 8; ++x) u.A(x, 9) = 1.0f;
    u.A(3, 0) = u.A(4, 0) = 1.0f;
    CHECK(LowerAccent(u, 5, 9) == 3 && u.A(3, 3) == 1.0f && u.A(3, 0) == 0.0f && u.A(0, 0) == 1.0f);
    // A lone accent (the whole mask above the capitals' top, no body): left alone, nothing is
    // written outside the mask.
    Image lone = MakeImage(3, 3);
    lone.A(1, 0) = lone.A(1, 1) = 1.0f;
    CHECK(LowerAccent(lone, 20, 10) == 0 && lone.A(1, 0) == 1.0f);
    // A tall accent scaled into the rows above its letter: 4 rows high, now from row 2 (2 rows).
    Image hat = MakeImage(8, 12);
    for (int y = 0; y < 4; ++y) {
        for (int x = 2; x < 6; ++x) hat.A(x, y) = 1.0f;
    }
    for (int y = 6; y < 12; ++y) {
        for (int x = 0; x < 8; ++x) hat.A(x, y) = 1.0f;
    }
    CHECK(ShrinkAccent(hat, 6, 2) && InkTop(hat) == 2 && hat.A(3, 3) > 0.9f && hat.A(2, 0) == 0.0f);
    CHECK(hat.A(2, 3) < 0.1f && hat.A(4, 3) > 0.9f);  // half as wide, centered on the old middle
    Image plainStem = stem0();
    CHECK(!ShrinkAccent(plainStem, 6, 2));  // no accent: false
    Image stem = MakeImage(2, 10);
    for (int y = 0; y < 10; ++y) stem.A(0, y) = 1.0f;
    CHECK(LowerAccent(stem, 6, 3) == 0);  // a tall stem is no accent
    SqueezeTop(stem, 6, 3);
    CHECK(InkTop(stem) == 3 && stem.A(0, 6) == 1.0f && stem.A(0, 9) == 1.0f);  // the part above row 6 now fills 3..5
    float covered = 0.0f;
    for (int y = 3; y < 6; ++y) covered += stem.A(0, y);
    CHECK(covered > 2.9f);  // compressed, not thinned out

    // Game pixels: transparent stays 0, black stays visible, partial pixels blend into the background.
    Image px = MakeImage(3, 1);
    px.A(1, 0) = 1.0f;
    px.C(1, 0) = {0, 0, 0};
    px.A(2, 0) = 0.5f;
    px.C(2, 0) = {255, 255, 255};
    const auto game = ToGamePixels(px, {0x9C, 0x8E, 0x5A});
    CHECK(game.size() == 3 && game[0] == 0 && game[1] == 1);
    CHECK(game[2] != 0 && (game[2] >> 11) > (0x9C >> 3) && (game[2] >> 11) < 31);
}

int h0Advance(const ddmod::text::GdiFace& font) {
    ddmod::text::GdiFace::Mask m;
    return font.GlyphMask(L'H', 0, m) ? m.advance : 0;
}

void TestGdiFace() {
    using namespace ddmod::text;
    const std::filesystem::path file =
        std::filesystem::path(DDMOD_SOURCE_DIR) / L"assets" / L"fonts" / L"DejaVuSansCondensed-Bold.ttf";
    CHECK(AddFontResourceExW(file.c_str(), FR_PRIVATE, nullptr) > 0);
    const std::wstring face = L"DejaVu Sans Condensed";
    const GdiFace font(face, 20, 700);
    CHECK(font.Valid() && font.ActualFace() == face);
    CHECK(font.CapHeight() > 8 && font.CapTop() > 0 && font.XHeight() < font.CapHeight());
    CHECK(font.Missing(L"ÄéЖ").empty());  // German, French and Cyrillic
    const int height = HeightForCap(face, 700, 12);
    CHECK(GdiFace(face, height, 700).CapHeight() >= 12 && GdiFace(face, height - 1, 700).CapHeight() < 12);
    // CapReference::H is the capital H; the median of a plain sans is within a pixel of it.
    CHECK(font.Capitals(CapReference::H).top == font.CapTop() && font.Capitals(CapReference::H).height == font.CapHeight());
    const CapMetrics median = font.Capitals(CapReference::Median);
    CHECK(std::abs(median.top - font.CapTop()) <= 1 && std::abs(median.height - font.CapHeight()) <= 1);
    const int medianHeight = HeightForCap(face, 700, 12, CapReference::Median);
    CHECK(GdiFace(face, medianHeight, 700).Capitals(CapReference::Median).height >= 12);

    // A line: pen from the pad, advances add up, the H's ink sits where its mask says.
    const Image line = LineMask(font, L"HH", 3);
    CHECK(line.width >= 3 + 2 * h0Advance(font) && line.height == font.CellHeight() + 6);
    CHECK(LineMask(font, L"", 3).width == 6);
    float ink = 0.0f;
    for (int y = 0; y < line.height; ++y) ink = std::max(ink, line.A(3 + 2, y));
    CHECK(ink > 0.5f);  // the H's left stem near the pen start

    GdiFace::Mask h;
    CHECK(font.GlyphMask(L'H', 2, h));
    CHECK(h.mask.width == static_cast<int>(h.mask.alpha.size()) / h.mask.height && h.advance > 0);
    // The mask's top row is the top of the H, minus the pad (the gray bitmap's box may be one row
    // larger than the metrics' box).
    CHECK(std::abs(h.top + 2 - font.CapTop()) <= 1);
    float full = 0.0f;
    for (const float a : h.mask.alpha) full = std::max(full, a);
    CHECK(full == 1.0f && h.mask.A(0, 0) == 0.0f);
    GdiFace::Mask space;
    CHECK(font.GlyphMask(L' ', 0, space) && space.mask.alpha.empty() && space.advance > 0);
    RemoveFontResourceExW(file.c_str(), FR_PRIVATE, nullptr);
}

// A minimal ML_FONT_01 file built by the test (no game data): 'A' is a 2x2 glyph, 'B' an alias of 'A'.
void TestFntFont() {
    using namespace ddmod::overlay;
    std::vector<std::uint8_t> file(38 + 64, 0);
    std::memcpy(file.data(), "ML_FONT_01", 10);
    const auto put32 = [&](std::size_t at, std::uint32_t v) { std::memcpy(file.data() + at, &v, 4); };
    const auto put16 = [&](std::size_t at, std::uint16_t v) { std::memcpy(file.data() + at, &v, 2); };
    file.resize(22 + 4 * 256 + 16, 0);
    put32(10, 256);
    put32(14, 15);  // line height
    const std::size_t record = file.size();
    const std::int32_t entry = static_cast<std::int32_t>(record - 38);
    put32(22 + 4 * 'A', static_cast<std::uint32_t>(entry));
    put32(22 + 4 * 'B', static_cast<std::uint32_t>(-entry));
    put32(record - 4, static_cast<std::uint32_t>(-3));  // top row 3
    file.resize(record + 18 + 2 * 12 + 6, 0);
    put32(record, 5);       // advance
    put16(record + 4, 'A');
    put16(record + 14, 2);  // width
    put16(record + 16, 2);  // height
    std::size_t at = record + 18;
    const int rows[2][4] = {{1, 0, 0, 2}, {1, 4, 1, 1}};  // count, pixel offset, x, length
    for (const auto& r : rows) {
        put16(at, static_cast<std::uint16_t>(r[0]));
        put32(at + 2, static_cast<std::uint32_t>(r[1]));
        put16(at + 6, static_cast<std::uint16_t>(r[2]));
        put16(at + 8, static_cast<std::uint16_t>(r[3]));
        put16(at + 10, static_cast<std::uint16_t>(r[0]));
        at += 12;
    }
    put16(at, 0xFFFF);
    put16(at + 2, 0xF800);
    put16(at + 4, 0x001F);

    const auto path = std::filesystem::temp_directory_path() / L"ddmod-test.fnt";
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(file.data()),
                                                static_cast<std::streamsize>(file.size()));
    auto source = CreateFntGlyphSource(path);
    CHECK(source != nullptr);
    if (source) {
        CHECK(source->LineHeight() == 15);
        Glyph a, b, c, space;
        CHECK(source->Rasterize(L'A', a));
        CHECK(a.advance == 5 && a.width == 2 && a.height == 2 && a.offsetY == 3);
        CHECK(a.coverage == (std::vector<std::uint8_t>{255, 255, 0, 255}));
        CHECK(a.colors.size() == 4 && Same(a.colors[0], {255, 255, 255}) && Same(a.colors[1], {255, 0, 0}) &&
              Same(a.colors[3], {0, 0, 255}));
        CHECK(source->Rasterize(L'B', b) && b.coverage == a.coverage);  // alias
        CHECK(!source->Rasterize(L'C', c));
        CHECK(!source->Rasterize(L'\u0416', c));  // not in code page 1252
        CHECK(source->Rasterize(L' ', space) && space.coverage.empty() && space.advance >= 3);
    }
    std::filesystem::remove(path);
    std::ofstream(path, std::ios::binary) << "not a font";
    CHECK(CreateFntGlyphSource(path) == nullptr);
    std::filesystem::remove(path);
}

void TestDgVoodooWatermark() {
    using ddmod::features::SetWatermark;
    using ddmod::features::WatermarkEdit;
    // Layout as in dgVoodoo's own file: aligned '=', CRLF, the same key name pattern in [Glide].
    const std::string conf =
        "[Glide]\r\n3DfxWatermark                       = true\r\n\r\n"
        "[DirectX]\r\n;dgVoodooWatermark: comment = true\r\n"
        "dgVoodooWatermark                    = true\r\nFastVideoMemoryAccess = false\r\n";
    std::string text = conf;
    CHECK(SetWatermark(text, false) == WatermarkEdit::Changed);
    CHECK(text.find("dgVoodooWatermark                    = false\r\nFastVideoMemoryAccess") != std::string::npos);
    CHECK(text.find("3DfxWatermark                       = true\r\n") != std::string::npos);
    CHECK(text.find(";dgVoodooWatermark: comment = true") != std::string::npos);
    CHECK(text.size() == conf.size() + 1);
    CHECK(SetWatermark(text, false) == WatermarkEdit::Unchanged);
    CHECK(SetWatermark(text, true) == WatermarkEdit::Changed && text == conf);

    std::string bare = "[directx]\ndgVoodooWatermark=\n";  // section name case, no value, LF only
    CHECK(SetWatermark(bare, true) == WatermarkEdit::Changed && bare == "[directx]\ndgVoodooWatermark= true\n");
    std::string upper = "[DirectX]\ndgVoodooWatermark = TRUE";  // no line end at all
    CHECK(SetWatermark(upper, true) == WatermarkEdit::Unchanged);
    std::string elsewhere = "[General]\ndgVoodooWatermark = true\n";
    CHECK(SetWatermark(elsewhere, false) == WatermarkEdit::NotFound);
    std::string longer = "[DirectX]\ndgVoodooWatermarkX = true\n";
    CHECK(SetWatermark(longer, false) == WatermarkEdit::NotFound);
}

void TestFontChain() {
    using namespace ddmod::fonts;
    const std::filesystem::path base = L"C:\\mod";
    const auto chain = ParseChain(" Tahoma ,, DejaVu Sans Condensed @ RivellonRenewed\\fonts\\D.ttf , X @ C:\\abs\\x.ttf,@ only.ttf", base);
    CHECK(chain.size() == 3);
    if (chain.size() == 3) {
        CHECK(chain[0].face == L"Tahoma" && chain[0].file.empty());
        CHECK(chain[1].face == L"DejaVu Sans Condensed" && chain[1].file == base / L"RivellonRenewed\\fonts\\D.ttf");
        CHECK(chain[2].face == L"X" && chain[2].file == L"C:\\abs\\x.ttf");
    }
    CHECK(ParseChain("", base).empty());
    const auto defaults = ParseChain(kDefaultChain, base);
    CHECK(defaults.size() == 2);
    // Install-DdMod copies the bundled font to exactly this folder.
    CHECK(defaults.size() == 2 && defaults[1].file == base / L"RivellonRenewed\\fonts\\DejaVuSansCondensed-Bold.ttf");

    // A missing face is skipped (GDI would substitute it), an installed one is taken.
    auto picked = Resolve(ParseChain("No Such Face 4711, Tahoma", base), true);
    CHECK(picked && picked->face == L"Tahoma");
    // The bundled fallback font loads from its file for this process only.
    const std::filesystem::path assets = std::filesystem::path(DDMOD_SOURCE_DIR) / L"assets";
    picked = Resolve(ParseChain("No Such Face 4711, DejaVu Sans Condensed @ fonts\\DejaVuSansCondensed-Bold.ttf", assets), true);
    CHECK(picked && picked->face == L"DejaVu Sans Condensed");
    CHECK(!Resolve(ParseChain("No Such Face 4711, Y @ missing.ttf", assets), true));

    // Registry font list: type suffix dropped, collections split at '&', whole names only.
    CHECK(FontListNameMatches(L"Tahoma Bold (TrueType)", L"Tahoma Bold"));
    CHECK(FontListNameMatches(L"tahoma bold (TrueType)", L"Tahoma Bold"));
    CHECK(!FontListNameMatches(L"Tahoma Bold (TrueType)", L"Tahoma"));
    CHECK(FontListNameMatches(L"Cambria & Cambria Math (TrueType)", L"Cambria Math"));
    CHECK(FontListNameMatches(L"Cambria & Cambria Math (TrueType)", L"Cambria"));
    CHECK(FontListNameMatches(L"Some Font", L"Some Font"));
    CHECK(!FontListNameMatches(L"Some Font", L""));
    // Tahoma Bold is a font of its own on Windows (tahomabd.ttf); the English full name also on a
    // German Windows, whose GDI calls it "Tahoma Fett".
    const InstalledFont tahoma = FindInstalledFont(L"Tahoma", true);
    CHECK(tahoma.fullName == L"Tahoma Bold");
    CHECK(_wcsicmp(tahoma.file.filename().c_str(), L"tahomabd.ttf") == 0 && std::filesystem::exists(tahoma.file));
}

void TestOverlayStatus() {
    using namespace ddmod::overlay;
    StatusMode mode = StatusMode::Off;
    CHECK(ParseStatusMode("banner", mode) && mode == StatusMode::Banner);
    CHECK(ParseStatusMode("Always", mode) && mode == StatusMode::Always);
    CHECK(ParseStatusMode("OFF", mode) && mode == StatusMode::Off);
    mode = StatusMode::Always;
    CHECK(!ParseStatusMode("bannerx", mode) && mode == StatusMode::Always);
    CHECK(!ParseStatusMode("", mode));
    CHECK(StatusShown(StatusMode::Banner, 0.0, 5));
    CHECK(StatusShown(StatusMode::Banner, 4.99, 5));
    CHECK(!StatusShown(StatusMode::Banner, 5.0, 5));
    CHECK(StatusShown(StatusMode::Always, 1e6, 5));
    CHECK(!StatusShown(StatusMode::Off, 0.0, 5));
    bool gameLook = false;
    CHECK(ParseBannerLook("Game", gameLook) && gameLook);
    CHECK(ParseBannerLook("plain", gameLook) && !gameLook);
    CHECK(!ParseBannerLook("fancy", gameLook) && !gameLook);

    // The banner: three parts in the menu and interface families; a folder without the game's
    // fonts gives no image (the overlay falls back to the plain line).
    const auto parts = DefaultBanner(L"Rivellon Renewed", L"0.01", L"active");
    CHECK(parts.size() == 3 && parts[0].font == "fontbig24" && parts[0].text == L"RIVELLON RENEWED" && parts[0].family == ddmod::fonts::Family::Menu);
    CHECK(parts[1].font == "fontn2" && parts[2].font == "fontgr1" && parts[2].text == L"active");
    CHECK(RenderBanner(std::filesystem::temp_directory_path() / L"ddmod-no-game-fonts", parts).image.width == 0);
}

}  // namespace

int main() {
    TestSha256();
    TestPatternParse();
    TestPatternFind();
    TestScanner();
    TestHooks();
    TestFontModeHook();
    TestFontModeSignature();
    TestSpacing();
    TestGlyphTable();
    TestFamilies();
    TestFontColors();
    TestEditionAndCharset();
    TestKeyCombo();
    TestLogRotation();
    TestLogArchive();
    TestPixelFormats();
    TestBlend();
    TestFillAndMask();
    TestGlyphCache();
    TestGdiGlyphs();
    TestTextStyle();
    TestGdiFace();
    TestFntFont();
    TestFontChain();
    TestDgVoodooWatermark();
    TestOverlayStatus();
    std::printf("%d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
