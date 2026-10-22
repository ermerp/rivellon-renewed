#include "fonts/glyphtable.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>

#include <windows.h>

#include "core/config.h"
#include "core/hooks.h"
#include "core/log.h"
#include "core/paths.h"
#include "fonts/fontchain.h"
#include "game/exe.h"
#include "overlay/glyphs.h"

namespace ddmod::fonts {
namespace {

// Game font object (docs/re/text-rendering.md, "Glyph table"): +0x0C line height, +0x14 table of
// 256 glyph pointers, +0x18 the .fnt file block (null for a GDI font), +0x2C space width (i16).
constexpr std::size_t kLineHeight = 0x0C;
constexpr std::size_t kGlyphTable = 0x14;
constexpr std::size_t kFileBlock = 0x18;
constexpr std::size_t kSpaceWidth = 0x2C;
// Glyph: +0x0C origin y (i32). Sprite: +0x00 total size, +0x04 offset of the pixel block,
// +0x0A height (u16).
constexpr std::size_t kGlyphOriginY = 0x0C;
constexpr std::size_t kSpriteTotal = 0x00;
constexpr std::size_t kSpritePixels = 0x04;
constexpr std::size_t kSpriteHeight = 0x0A;

// Edge tones the originals blend their anti-aliasing into (Colors).
constexpr overlay::Rgb kBlack{0, 0, 0};
constexpr overlay::Rgb kPaper{0x9C, 0x8E, 0x5A};
constexpr overlay::Rgb kDiary{0x94, 0x7D, 0x4A};

using LoadFontFileFn = void*(__thiscall*)(void* font, const char* path);

LoadFontFileFn g_loadFontFile = nullptr;
std::vector<std::string> g_fonts;
int g_linePitchPercent = 100;
std::vector<const void*> g_ownFonts;
// The game never frees glyphs of a bitmap font one by one (only its file block), so the own
// glyphs live as long as the process.
std::vector<std::unique_ptr<std::uint8_t[]>> g_glyphMemory;

template <typename T>
T& Field(void* object, std::size_t offset) {
    return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(object) + offset);
}

template <typename T>
T Read(const void* object, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const std::uint8_t*>(object) + offset, sizeof(T));
    return value;
}

template <typename T>
void Write(std::uint8_t* at, T value) {
    std::memcpy(at, &value, sizeof(T));
}

overlay::Rgb FromRgb565(std::uint16_t value) {
    const int r = (value >> 11) & 0x1F, g = (value >> 5) & 0x3F, b = value & 0x1F;
    return {static_cast<std::uint8_t>(r * 255 / 31), static_cast<std::uint8_t>(g * 255 / 63),
            static_cast<std::uint8_t>(b * 255 / 31)};
}

overlay::Rgb Scaled(overlay::Rgb color, double factor) {
    const auto channel = [factor](std::uint8_t c) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<int>(c * factor + 0.5), 0, 255));
    };
    return {channel(color.r), channel(color.g), channel(color.b)};
}

overlay::Rgb EdgeToneFor(const std::string& stem) {
    if (stem.starts_with("fontblack") || stem.starts_with("paper")) return kPaper;
    if (stem.starts_with("diary")) return kDiary;
    return kBlack;
}

// What the original font looks like: the most frequent pixel color of its capitals, and the top
// row and height of its "H" relative to the top of the line.
struct OriginalLook {
    overlay::Rgb color{0xE7, 0xE7, 0xE7};
    int capTop = 0;
    int capHeight = 0;
};

OriginalLook MeasureOriginal(void* font) {
    OriginalLook look;
    void** table = Field<void**>(font, kGlyphTable);
    std::map<std::uint16_t, int> counts;
    for (int ch = 'A'; ch <= 'Z'; ++ch) {
        const auto* glyph = static_cast<const std::uint8_t*>(table[ch]);
        if (!glyph) continue;
        const std::uint8_t* sprite = glyph + kGlyphSprite;
        const auto total = Read<std::uint32_t>(sprite, kSpriteTotal);
        const auto pixels = Read<std::uint32_t>(sprite, kSpritePixels);
        for (std::uint32_t at = pixels; at + 2 <= total; at += 2) ++counts[Read<std::uint16_t>(sprite, at)];
    }
    const auto most = std::max_element(counts.begin(), counts.end(),
                                       [](const auto& a, const auto& b) { return a.second < b.second; });
    if (most != counts.end()) look.color = FromRgb565(most->first);
    const auto* h = static_cast<const std::uint8_t*>(table['H']);
    if (h) {
        look.capTop = -Read<std::int32_t>(h, kGlyphOriginY);
        look.capHeight = Read<std::uint16_t>(h + kGlyphSprite, kSpriteHeight);
    } else {
        look.capHeight = std::max(1, Field<int>(font, kLineHeight) * 2 / 3);
    }
    return look;
}

std::unique_ptr<overlay::GlyphSource> OpenFace(const FontEntry& face, int pixelHeight) {
    overlay::FontSpec spec;
    spec.face = face.face;
    spec.file = face.file;
    spec.pixelHeight = pixelHeight;
    return overlay::CreateGdiGlyphSource(spec);
}

// Replaces every glyph the original font has (except the space and control characters) with one
// rasterized from the font chain's face at the original's capital height.
void ReplaceGlyphs(void* font, const std::string& stem) {
    const auto& face = Configured();
    if (!face) {
        log::Warn("Glyph table: no face from the font chain - {} keeps its glyphs", stem);
        return;
    }
    const OriginalLook look = MeasureOriginal(font);
    // Scale the face so that its "H" is as tall as the original's.
    overlay::Glyph probe;
    auto reference = OpenFace(*face, 100);
    if (!reference || !reference->Rasterize(L'H', probe) || probe.height <= 0) {
        log::Warn("Glyph table: face cannot be measured - {} keeps its glyphs", stem);
        return;
    }
    const int pixelHeight = std::max(4, (look.capHeight * 100 + probe.height / 2) / probe.height);
    auto source = OpenFace(*face, pixelHeight);
    if (!source || !source->Rasterize(L'H', probe)) return;
    // Our baseline onto the original's.
    const int shift = look.capTop + look.capHeight - (probe.offsetY + probe.height);
    const overlay::Rgb edge = EdgeToneFor(stem);
    const unsigned codePage = game::Exe().edition == game::Edition::Russian ? 1251 : 1252;

    void** table = Field<void**>(font, kGlyphTable);
    int replaced = 0;
    for (int byte = 0x21; byte < 0x100; ++byte) {
        if (!table[byte]) continue;
        const char ansi = static_cast<char>(byte);
        wchar_t wide = 0;
        if (MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, &ansi, 1, &wide, 1) != 1) continue;
        overlay::Glyph glyph;
        if (!source->Rasterize(wide, glyph) || glyph.width <= 0 || glyph.height <= 0) continue;
        PixelGlyph out;
        out.width = glyph.width;
        out.height = glyph.height;
        out.originX = glyph.offsetX;
        out.originY = -(glyph.offsetY + shift);
        out.advance = glyph.advance;
        out.character = static_cast<std::uint16_t>(byte);
        out.pixels.resize(glyph.coverage.size());
        for (int row = 0; row < glyph.height; ++row) {
            // Vertical gradient over the capital height (lighter at the top), the same for every
            // glyph of the line - shows that each pixel can have its own color.
            const int lineY = glyph.offsetY + shift + row;
            const double t = std::clamp(static_cast<double>(lineY - look.capTop) / std::max(1, look.capHeight), 0.0, 1.0);
            const overlay::Rgb color = Scaled(look.color, 1.25 - 0.5 * t);
            for (int col = 0; col < glyph.width; ++col) {
                const std::size_t at = static_cast<std::size_t>(row) * glyph.width + col;
                const std::uint8_t coverage = glyph.coverage[at];
                out.pixels[at] = coverage ? ShadePixel(color, edge, coverage) : 0;
            }
        }
        g_glyphMemory.push_back(BuildGameGlyph(out));
        table[byte] = g_glyphMemory.back().get();
        ++replaced;
    }
    overlay::Glyph space;
    if (source->Rasterize(L' ', space) && space.advance > 0) Field<std::int16_t>(font, kSpaceWidth) = static_cast<std::int16_t>(space.advance);
    int& lineHeight = Field<int>(font, kLineHeight);
    const int originalLine = lineHeight;
    lineHeight = std::max(1, originalLine * g_linePitchPercent / 100);
    g_ownFonts.push_back(font);
    log::Info("Glyph table: {} - {} glyphs replaced (face {} {} px, capital height {}, color #{:02X}{:02X}{:02X}, "
              "line height {} -> {})",
              stem, replaced, paths::ToUtf8(std::filesystem::path(face->face)), pixelHeight, look.capHeight,
              look.color.r, look.color.g, look.color.b, originalLine, lineHeight);
}

// See FontInitDetour (fontmode.cpp) for the __fastcall stand-in of __thiscall.
void* __fastcall LoadFontFileDetour(void* font, void* /*edx*/, const char* path) {
    void* result = g_loadFontFile(font, path);
    if (!path || !Field<void*>(font, kFileBlock) || !Field<void**>(font, kGlyphTable)) return result;
    const std::string stem = FontStem(path);
    if (std::find(g_fonts.begin(), g_fonts.end(), stem) != g_fonts.end()) ReplaceGlyphs(font, stem);
    return result;
}

}  // namespace

std::unique_ptr<std::uint8_t[]> BuildGameGlyph(const PixelGlyph& glyph) {
    struct Span {
        int x, length;
    };
    std::vector<std::vector<Span>> rows(static_cast<std::size_t>(std::max(0, glyph.height)));
    std::size_t header = 12, pixelCount = 0;
    for (int y = 0; y < glyph.height; ++y) {
        auto& spans = rows[static_cast<std::size_t>(y)];
        for (int x = 0; x < glyph.width;) {
            if (glyph.pixels[static_cast<std::size_t>(y) * glyph.width + x] == 0) {
                ++x;
                continue;
            }
            const int start = x;
            while (x < glyph.width && glyph.pixels[static_cast<std::size_t>(y) * glyph.width + x] != 0) ++x;
            spans.push_back({start, x - start});
            pixelCount += static_cast<std::size_t>(x - start);
        }
        header += 2 + 4 + 4 * spans.size() + 2;
    }
    const std::size_t spriteSize = header + 2 * pixelCount;
    auto memory = std::make_unique<std::uint8_t[]>(kGlyphSprite + spriteSize);
    std::uint8_t* g = memory.get();
    std::memset(g, 0, kGlyphSprite + spriteSize);
    Write<std::uint32_t>(g + 0x00, static_cast<std::uint32_t>(glyph.width));
    Write<std::uint32_t>(g + 0x04, static_cast<std::uint32_t>(glyph.height));
    Write<std::int32_t>(g + 0x08, glyph.originX);
    Write<std::int32_t>(g + 0x0C, glyph.originY);
    Write<std::int16_t>(g + 0x10, static_cast<std::int16_t>(glyph.advance));
    Write<std::int16_t>(g + 0x12, 0);
    Write<std::uint16_t>(g + 0x14, glyph.character);
    std::uint8_t* sprite = g + kGlyphSprite;
    Write<std::uint32_t>(sprite + 0, static_cast<std::uint32_t>(spriteSize));
    Write<std::uint32_t>(sprite + 4, static_cast<std::uint32_t>(header));
    Write<std::uint16_t>(sprite + 8, static_cast<std::uint16_t>(glyph.width));
    Write<std::uint16_t>(sprite + 10, static_cast<std::uint16_t>(glyph.height));
    std::uint8_t* row = sprite + 12;
    std::uint8_t* pixels = sprite + header;
    for (int y = 0; y < glyph.height; ++y) {
        const auto& spans = rows[static_cast<std::size_t>(y)];
        Write<std::uint16_t>(row, static_cast<std::uint16_t>(spans.size()));
        Write<std::uint32_t>(row + 2, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pixels)));
        row += 6;
        for (const Span& span : spans) {
            Write<std::uint16_t>(row, static_cast<std::uint16_t>(span.x));
            Write<std::uint16_t>(row + 2, static_cast<std::uint16_t>(span.length));
            row += 4;
            std::memcpy(pixels, &glyph.pixels[static_cast<std::size_t>(y) * glyph.width + span.x],
                        2 * static_cast<std::size_t>(span.length));
            pixels += 2 * span.length;
        }
        Write<std::uint16_t>(row, static_cast<std::uint16_t>(spans.size()));
        row += 2;
    }
    return memory;
}

std::uint16_t ShadePixel(overlay::Rgb color, overlay::Rgb background, std::uint8_t coverage) {
    const auto mix = [coverage](int c, int bg) { return bg + ((c - bg) * coverage + 127) / 255; };
    const int r = mix(color.r, background.r), g = mix(color.g, background.g), b = mix(color.b, background.b);
    const auto value = static_cast<std::uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
    return value ? value : 1;
}

std::vector<std::string> ParseFontList(std::string_view text) {
    std::vector<std::string> fonts;
    std::string current;
    const auto flush = [&] {
        while (!current.empty() && std::isspace(static_cast<unsigned char>(current.back()))) current.pop_back();
        if (!current.empty()) fonts.push_back(current);
        current.clear();
    };
    for (const char c : text) {
        if (c == ',') {
            flush();
        } else if (!current.empty() || !std::isspace(static_cast<unsigned char>(c))) {
            current += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    flush();
    return fonts;
}

std::string FontStem(std::string_view path) {
    const std::size_t slash = path.find_last_of("\\/");
    if (slash != std::string_view::npos) path.remove_prefix(slash + 1);
    const std::size_t dot = path.rfind('.');
    if (dot != std::string_view::npos) path = path.substr(0, dot);
    std::string stem(path);
    for (char& c : stem) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return stem;
}

bool GlyphTableConfigured() {
    static const bool configured = !ParseFontList(config::GetString("Experimental", "GlyphTable", "")).empty();
    return configured;
}

bool IsOwnGlyphFont(const void* font) {
    return std::find(g_ownFonts.begin(), g_ownFonts.end(), font) != g_ownFonts.end();
}

bool InstallGlyphTable() {
    g_fonts = ParseFontList(config::GetString("Experimental", "GlyphTable", ""));
    if (g_fonts.empty()) return true;
    if (game::Exe().edition == game::Edition::Russian) {
        log::Info("Glyph table: not supported on the Russian edition");
        return false;
    }
    g_linePitchPercent = std::clamp(config::GetInt("Experimental", "GlyphTableLinePitch", 100), 50, 300);
    const std::uintptr_t target = game::Resolve({"LoadFontFile", kLoadFontFilePattern, 0, kLoadFontFileRva});
    if (!target) {
        log::Warn("Glyph table: font file loader not found - fonts keep their glyphs");
        return false;
    }
    if (!hooks::CreateRaw("LoadFontFile", reinterpret_cast<void*>(target), reinterpret_cast<void*>(&LoadFontFileDetour),
                          reinterpret_cast<void**>(&g_loadFontFile)) ||
        !hooks::Enable("LoadFontFile")) {
        return false;
    }
    log::Info("Glyph table (experimental): {} font(s), line pitch {}%", g_fonts.size(), g_linePitchPercent);
    return true;
}

}  // namespace ddmod::fonts
