#include "fonts/glyphtable.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstring>
#include <map>

#include <windows.h>

#include "core/config.h"
#include "core/hooks.h"
#include "core/log.h"
#include "core/paths.h"
#include "fonts/families.h"
#include "fonts/fontchain.h"
#include "fonts/fontcolors.h"
#include "game/exe.h"
#include "text/gdiface.h"
#include "text/style.h"

namespace ddmod::fonts {
namespace {

// Game font object (docs/re/text-rendering.md, "Glyph table"): +0x0C line height, +0x14 table of
// 256 glyph pointers, +0x18 the .fnt file block (null for a GDI font), +0x2C space width (i16).
constexpr std::size_t kLineHeight = 0x0C;
constexpr std::size_t kGlyphTable = 0x14;
constexpr std::size_t kFileBlock = 0x18;
constexpr std::size_t kSpaceWidth = 0x2C;
// Glyph: +0x08 origin x (i32), +0x0C origin y (i32), +0x10 advance (i16). Sprite: +0x08 width,
// +0x0A height (u16), row entries from +0x0C.
constexpr std::size_t kGlyphOriginX = 0x08;
constexpr std::size_t kGlyphOriginY = 0x0C;
constexpr std::size_t kGlyphAdvance = 0x10;
constexpr std::size_t kSpriteWidth = 0x08;
constexpr std::size_t kSpriteHeight = 0x0A;
constexpr std::size_t kSpriteRows = 0x0C;
// Characters that reach down and up the furthest in ordinary text; an `auto` line height keeps
// them of neighboring lines apart.
constexpr wchar_t kExtenders[] = L"bdfhklgjpqyß";

using LoadFontFileFn = void*(__thiscall*)(void* font, const char* path);

LoadFontFileFn g_loadFontFile = nullptr;
std::vector<const void*> g_ownFonts;
// The game never frees glyphs of a bitmap font one by one (only its file block), so the own
// glyphs live as long as the process.
std::vector<std::unique_ptr<std::uint8_t[]>> g_glyphMemory;
int g_fontsSeen = 0;
int g_fontsReplaced = 0;
int g_glyphsReplaced = 0;
double g_milliseconds = 0.0;

// A family's settings and face, resolved when its first font is loaded.
struct FamilyState {
    bool resolved = false;
    FamilySettings settings;
    std::optional<FontEntry> face;
    std::map<int, int> heightForCap;  // capital height -> GDI height
};
std::array<FamilyState, std::size(kFamilies)> g_families;

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

FamilyState& StateOf(Family family) {
    FamilyState& state = g_families[static_cast<std::size_t>(family)];
    if (state.resolved) return state;
    state.resolved = true;
    state.settings = ConfiguredSettings(family);
    const char* name = FamilyName(family);
    if (!state.settings.enabled) {
        log::Info("Glyph renderer: family {} off ([Family.{}] Enabled=0) - original glyphs", name, name);
        return state;
    }
    state.face = Resolve(ParseChain(state.settings.chain, paths::ModDir()), state.settings.weight >= FW_SEMIBOLD);
    if (!state.face) {
        log::Warn("Glyph renderer: family {} has no available face - original glyphs", name);
        return state;
    }
    const LineHeight& line = state.settings.lineHeight;
    log::Info("Glyph renderer: family {} - \"{}\" weight {}, line height {}", name, paths::ToUtf8(state.face->face),
              state.settings.weight, line.automatic ? std::string("auto") : std::to_string(line.percent) + "%");
    return state;
}

// The original's text and edge color: the measured table (fontcolors.cpp), or for a name it does
// not know the most frequent capital color and the family's usual edge tone.
text::Rgb ToRgb(Rgb8 c) { return {c.r, c.g, c.b}; }

FontColors ColorsOf(const std::string& stem, Family family, const text::Capitals& caps) {
    for (const FontColorEntry& entry : FontColorTable()) {
        if (entry.file == stem) return entry.original;
    }
    std::map<std::uint32_t, int> counts;
    for (const overlay::Glyph& g : caps) {
        for (std::size_t i = 0; i < g.colors.size(); ++i) {
            if (g.coverage[i]) ++counts[static_cast<std::uint32_t>(g.colors[i].r) << 16 | g.colors[i].g << 8 | g.colors[i].b];
        }
    }
    FontColors colors{{0xE7, 0xE7, 0xE7}, {0, 0, 0}};
    const auto most = std::max_element(counts.begin(), counts.end(),
                                       [](const auto& a, const auto& b) { return a.second < b.second; });
    if (most != counts.end()) {
        colors.text = {static_cast<std::uint8_t>(most->first >> 16), static_cast<std::uint8_t>(most->first >> 8),
                       static_cast<std::uint8_t>(most->first)};
    }
    if (family == Family::Books) colors.edge = {0x9C, 0x8E, 0x5A};
    if (family == Family::DiaryTitle) colors.edge = {0x94, 0x7D, 0x4A};
    return colors;
}

// Replaces every glyph the original font has (except the space and control characters, and
// characters the family's face lacks) with one of the family's face at the original's capital
// height, painted in the family's style.
void ReplaceGlyphs(void* font, const std::string& stem, Family family) {
    FamilyState& state = StateOf(family);
    if (!state.settings.enabled || !state.face) return;
    const auto start = std::chrono::steady_clock::now();
    void** table = Field<void**>(font, kGlyphTable);
    text::Capitals caps;
    for (int ch = 'A'; ch <= 'Z'; ++ch) caps.push_back(table[ch] ? DecodeGameGlyph(table[ch]) : overlay::Glyph{});
    const text::CapMetrics cap = text::MeasureCap(caps);
    if (cap.height <= 0) {
        log::Warn("Glyph renderer: {} has no capital H to measure - original glyphs", stem);
        return;
    }
    const FamilySettings& settings = state.settings;
    const std::wstring& faceName = state.face->face;
    auto [known, inserted] = state.heightForCap.try_emplace(cap.height, 0);
    if (inserted) known->second = text::HeightForCap(faceName, settings.weight, cap.height);
    const int height = known->second;
    const text::GdiFace face(faceName, height, settings.weight);
    if (!face.Valid()) {
        log::Warn("Glyph renderer: {} - GDI cannot create \"{}\" {} - original glyphs", stem, paths::ToUtf8(faceName), height);
        return;
    }
    // The face's capital H where the original's is.
    const int shift = cap.top - face.CapTop();
    const FontColors colors = ColorsOf(stem, family, caps);
    const text::Rgb textColor = ToRgb(colors.text), edge = ToRgb(colors.edge);

    text::Bevel bevel;
    text::Shine shine;
    text::StyleParams style;
    if (settings.style != GlyphStyle::Ramp) {
        style.profile = text::MeasureProfile(caps);
        style.contour = edge;
        if (settings.style == GlyphStyle::Measured) {
            bevel = text::MeasureBevel(caps, style.profile);
            style.bevel = &bevel;
        } else {
            shine = text::MeasureShine(caps);
            style.shine = &shine;
            style.shineStrength = settings.shineStrength;
            style.texture = settings.texture;
        }
    }

    int replaced = 0, kept = 0;
    for (int byte = 0x21; byte < 0x100; ++byte) {
        if (!table[byte]) continue;
        const char ansi = static_cast<char>(byte);
        if (settings.keepOriginal.find(ansi) != std::string::npos) {
            ++kept;
            continue;
        }
        wchar_t wide = 0;
        text::GdiFace::Mask m;
        if (MultiByteToWideChar(1252, MB_ERR_INVALID_CHARS, &ansi, 1, &wide, 1) != 1 ||
            !face.Missing(std::wstring(1, wide)).empty() || !face.GlyphMask(wide, 0, m) || m.mask.alpha.empty()) {
            ++kept;
            continue;
        }
        const text::Image painted = settings.style == GlyphStyle::Ramp
                                        ? text::PaintRamp(m.mask, textColor, edge)
                                        : text::PaintStyled(m.mask, face.CapTop() - m.top, face.CapHeight(), style);
        PixelGlyph out;
        out.width = m.mask.width;
        out.height = m.mask.height;
        out.originX = m.left;
        out.originY = -(m.top + shift);
        out.advance = m.advance;
        out.character = static_cast<std::uint16_t>(byte);
        out.pixels = text::ToGamePixels(painted, edge);
        g_glyphMemory.push_back(BuildGameGlyph(out));
        table[byte] = g_glyphMemory.back().get();
        ++replaced;
    }
    text::GdiFace::Mask space;
    if (face.GlyphMask(L' ', 0, space) && space.advance > 0) {
        Field<std::int16_t>(font, kSpaceWidth) = static_cast<std::int16_t>(space.advance);
    }
    int& lineHeight = Field<int>(font, kLineHeight);
    const int originalLine = lineHeight;
    if (settings.lineHeight.automatic) {
        lineHeight = std::max(originalLine, face.InkExtent(kExtenders) + 1);
    } else {
        lineHeight = std::max(1, originalLine * settings.lineHeight.percent / 100);
    }
    g_ownFonts.push_back(font);
    ++g_fontsReplaced;
    g_glyphsReplaced += replaced;
    g_milliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    log::Debug("Glyph renderer: {} - {} glyphs replaced, {} kept (GDI height {}, capital height {}, color "
               "#{:02X}{:02X}{:02X} on #{:02X}{:02X}{:02X}, line height {} -> {})",
               stem, replaced, kept, height, cap.height, textColor.r, textColor.g, textColor.b, edge.r, edge.g, edge.b,
               originalLine, lineHeight);
}

// See FontInitDetour (fontmode.cpp) for the __fastcall stand-in of __thiscall.
void* __fastcall LoadFontFileDetour(void* font, void* /*edx*/, const char* path) {
    void* result = g_loadFontFile(font, path);
    if (!path || !Field<void*>(font, kFileBlock) || !Field<void**>(font, kGlyphTable)) return result;
    const std::string stem = FontStem(path);
    if (const auto family = FamilyOf(stem)) ReplaceGlyphs(font, stem, *family);
    if (++g_fontsSeen == static_cast<int>(kGameFontCount)) {
        log::Info("Glyph renderer: {} of {} fonts replaced, {} glyphs, {:.0f} ms", g_fontsReplaced, g_fontsSeen,
                  g_glyphsReplaced, g_milliseconds);
    }
    return result;
}

}  // namespace

std::optional<Renderer> ParseRenderer(std::string_view text) {
    std::string value(text);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (value == "glyphs") return Renderer::Glyphs;
    if (value == "gdi") return Renderer::Gdi;
    return std::nullopt;
}

Renderer ConfiguredRenderer() {
    const std::string text = config::GetString("Fonts", "Renderer", "glyphs");
    if (const auto renderer = ParseRenderer(text)) return *renderer;
    log::Warn("[Fonts] Renderer=\"{}\" is not glyphs or gdi - using glyphs", text);
    return Renderer::Glyphs;
}

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

overlay::Glyph DecodeGameGlyph(const void* glyph) {
    overlay::Glyph out;
    const auto* sprite = static_cast<const std::uint8_t*>(glyph) + kGlyphSprite;
    const int width = Read<std::uint16_t>(sprite, kSpriteWidth);
    const int height = Read<std::uint16_t>(sprite, kSpriteHeight);
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) return out;
    out.width = width;
    out.height = height;
    out.offsetX = Read<std::int32_t>(glyph, kGlyphOriginX);
    out.offsetY = -Read<std::int32_t>(glyph, kGlyphOriginY);
    out.advance = Read<std::int16_t>(glyph, kGlyphAdvance);
    out.coverage.assign(static_cast<std::size_t>(width) * height, 0);
    out.colors.assign(out.coverage.size(), overlay::Rgb{});
    const std::uint8_t* row = sprite + kSpriteRows;
    for (int y = 0; y < height; ++y) {
        const int count = Read<std::uint16_t>(row, 0);
        const auto* pixels = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(Read<std::uint32_t>(row, 2)));
        row += 6;
        for (int i = 0; i < count; ++i, row += 4) {
            const int x = Read<std::uint16_t>(row, 0), length = Read<std::uint16_t>(row, 2);
            if (x + length > width) return {};
            for (int k = 0; k < length; ++k) {
                const std::size_t at = static_cast<std::size_t>(y) * width + x + k;
                out.coverage[at] = 255;
                out.colors[at] = overlay::Unpack(overlay::PixelFormat::Rgb565, Read<std::uint16_t>(pixels, 0));
                pixels += 2;
            }
        }
        row += 2;
    }
    return out;
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

bool IsOwnGlyphFont(const void* font) {
    return std::find(g_ownFonts.begin(), g_ownFonts.end(), font) != g_ownFonts.end();
}

bool InstallGlyphTable() {
    if (game::Exe().edition == game::Edition::Russian) {
        log::Info("Glyph renderer: Russian edition - its Cyrillic bitmap fonts stay");
        return false;
    }
    const std::uintptr_t target = game::Resolve({"LoadFontFile", kLoadFontFilePattern, 0, kLoadFontFileRva});
    if (!target) {
        log::Warn("Glyph renderer: font file loader not found - original bitmap fonts");
        return false;
    }
    if (!hooks::CreateRaw("LoadFontFile", reinterpret_cast<void*>(target), reinterpret_cast<void*>(&LoadFontFileDetour),
                          reinterpret_cast<void**>(&g_loadFontFile)) ||
        !hooks::Enable("LoadFontFile")) {
        return false;
    }
    log::Info("Glyph renderer installed ([Fonts] Renderer=glyphs)");
    return true;
}

}  // namespace ddmod::fonts
