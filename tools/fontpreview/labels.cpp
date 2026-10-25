#include "labels.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <windows.h>

#include "fonts/families.h"
#include "overlay/fnt.h"
#include "overlay/glyphs.h"
#include "text/gdiface.h"
#include "text/style.h"

namespace fontpreview {
namespace {

namespace text = ddmod::text;

struct Entry {
    std::uint32_t section = 0, id = 0;
    std::string text;
};

// text.cmp: u32 section count; per section u32 type, i32 entry count (negative = 16-bit
// characters); per entry u32 id, then two strings, each u32 length plus length + 1 characters
// (nothing for length 0).
bool ReadTextCmp(const std::vector<std::uint8_t>& data, std::vector<Entry>& out) {
    std::size_t at = 0;
    const auto u32 = [&](std::uint32_t& v) {
        if (at + 4 > data.size()) return false;
        std::memcpy(&v, data.data() + at, 4);
        at += 4;
        return true;
    };
    const auto str = [&](bool wide, std::string* keep) {
        std::uint32_t length = 0;
        if (!u32(length)) return false;
        if (length == 0) return true;
        const std::size_t bytes = (static_cast<std::size_t>(length) + 1) * (wide ? 2 : 1);
        if (at + bytes > data.size()) return false;
        if (keep && !wide) keep->assign(reinterpret_cast<const char*>(data.data() + at), length);
        at += bytes;
        return true;
    };
    std::uint32_t sections = 0;
    if (!u32(sections)) return false;
    for (std::uint32_t s = 0; s < sections; ++s) {
        std::uint32_t type = 0, rawCount = 0;
        if (!u32(type) || !u32(rawCount)) return false;
        const auto count = static_cast<std::int32_t>(rawCount);
        const bool wide = count < 0;
        for (std::int32_t i = 0; i < (wide ? -count : count); ++i) {
            Entry e{type, 0, {}};
            if (!u32(e.id) || !str(wide, &e.text) || !str(wide, nullptr)) return false;
            out.push_back(std::move(e));
        }
    }
    return true;
}

std::wstring Widen(const std::string& s, unsigned codePage) {
    std::wstring w(s.size(), L'\0');
    const int n = MultiByteToWideChar(codePage, 0, s.data(), static_cast<int>(s.size()), w.data(), static_cast<int>(w.size()));
    w.resize(static_cast<std::size_t>(std::max(0, n)));
    return w;
}

std::string Utf8(const std::wstring& w) {
    std::string s(static_cast<std::size_t>(WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr)), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), static_cast<int>(s.size()), nullptr, nullptr);
    return s;
}

// A menu label: at least two letters and no lowercase letter.
bool IsLabel(const std::wstring& w) {
    int letters = 0;
    for (const wchar_t ch : w) {
        if (IsCharLowerW(ch)) return false;
        if (IsCharAlphaW(ch)) ++letters;
    }
    return letters >= 2;
}

}  // namespace

bool PrintLabelWidths(const std::filesystem::path& textCmp, const std::filesystem::path& fntDir,
                      const std::filesystem::path& faceDir, unsigned codePage) {
    std::ifstream in(textCmp, std::ios::binary);
    if (!in) return false;
    const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<Entry> entries;
    if (!ReadTextCmp(data, entries)) {
        std::fwprintf(stderr, L"cannot parse %ls\n", textCmp.c_str());
        return false;
    }
    ddmod::overlay::GlyphCache original(ddmod::overlay::CreateFntGlyphSource(fntDir / L"fontbig36.fnt", codePage));
    if (original.LineHeight() <= 0) return false;
    const std::filesystem::path faceFile = faceDir / L"UncialAntiqua-Regular.ttf";
    if (AddFontResourceExW(faceFile.c_str(), FR_PRIVATE, nullptr) == 0) return false;
    const ddmod::fonts::FamilySettings settings = ddmod::fonts::DefaultSettings(ddmod::fonts::Family::Menu);
    text::Capitals caps;
    for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) caps.push_back(original.Get(ch));
    const text::CapMetrics cap = text::MeasureCap(caps, settings.capitals);
    const int height = text::HeightForCap(L"Uncial Antiqua", settings.weight, cap.height, settings.capitals);
    const text::GdiFace face(L"Uncial Antiqua", height, settings.weight);
    if (!face.Valid()) return false;
    // Width = sum of advances, like the game with [Fonts] Spacing=font.
    const auto faceWidth = [&](const std::wstring& w) {
        int sum = 0;
        for (const wchar_t ch : w) {
            text::GdiFace::Mask m;
            if (face.GlyphMask(ch, 0, m)) sum += m.advance;
        }
        return sum;
    };
    std::printf("# %s - fontbig36 capital height %d, Uncial Antiqua GDI height %d\n# section id original mod text\n",
                textCmp.filename().string().c_str(), cap.height, height);
    for (const Entry& e : entries) {
        const std::wstring w = Widen(e.text, codePage);
        if (!IsLabel(w)) continue;
        std::printf("%u %u %d %d %s\n", e.section, e.id, ddmod::overlay::MeasureString(original, w), faceWidth(w),
                    Utf8(w).c_str());
    }
    return true;
}

}  // namespace fontpreview
