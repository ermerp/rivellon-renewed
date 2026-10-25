#include "overlay/fnt.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include <windows.h>

#include "core/log.h"
#include "core/paths.h"

namespace ddmod::overlay {
namespace {

// ML_FONT_01 layout (docs/re/file-formats.md).
constexpr char kMagic[] = "ML_FONT_01";
constexpr std::size_t kMagicSize = 10;
constexpr std::size_t kLineHeightAt = 14;
constexpr std::size_t kTableAt = 22;
constexpr std::size_t kTableEntries = 256;
constexpr std::size_t kRecordBase = 38;     // table entries are relative to this offset
constexpr std::size_t kRecordHeader = 18;   // advance, code, sizes, width, height
constexpr std::size_t kTopBefore = 4;       // i32 just before the record: minus the glyph's top row

class FntGlyphSource final : public GlyphSource {
public:
    FntGlyphSource(std::vector<std::uint8_t> data, unsigned codePage) : data_(std::move(data)), codePage_(codePage) {}

    bool Valid() const {
        return data_.size() >= kTableAt + 4 * kTableEntries && std::memcmp(data_.data(), kMagic, kMagicSize) == 0;
    }

    int LineHeight() const override { return static_cast<int>(U32(kLineHeightAt)); }

    bool Rasterize(wchar_t ch, Glyph& out) override {
        char byte = 0;
        BOOL defaulted = FALSE;
        if (WideCharToMultiByte(codePage_, WC_NO_BEST_FIT_CHARS, &ch, 1, &byte, 1, nullptr, &defaulted) != 1 ||
            defaulted) {
            return false;
        }
        std::int32_t entry = I32(kTableAt + 4 * static_cast<std::uint8_t>(byte));
        if (entry < 0) entry = -entry;  // alias of another glyph
        if (entry == 0 && byte == ' ') {
            // No space glyph and no width in the header; the game's own spacing is not known yet,
            // so use the width of 'i' (estimate).
            Glyph i;
            out = {};
            out.advance = ch != L'i' && Rasterize(L'i', i) ? i.advance : std::max(3, LineHeight() / 4);
            return true;
        }
        if (entry == 0) return false;
        const std::size_t record = kRecordBase + static_cast<std::size_t>(entry);
        if (record < kTopBefore || record + kRecordHeader > data_.size()) return false;

        out = {};
        out.advance = static_cast<int>(U32(record));
        out.width = U16(record + 14);
        out.height = U16(record + 16);
        out.offsetY = -I32(record - kTopBefore);

        // Rows: span count, byte offset into the pixel block, spans (x, length), span count again.
        struct Span {
            int y, x, length;
            std::size_t pixelOffset;
        };
        std::vector<Span> spans;
        std::size_t at = record + kRecordHeader;
        for (int y = 0; y < out.height; ++y) {
            if (at + 6 > data_.size()) return false;
            const int count = U16(at);
            std::size_t offset = U32(at + 2);
            at += 6;
            for (int i = 0; i < count; ++i, at += 4) {
                if (at + 4 > data_.size()) return false;
                const int x = U16(at);
                const int length = U16(at + 2);
                spans.push_back({y, x, length, offset});
                offset += 2 * static_cast<std::size_t>(length);
            }
            at += 2;
        }
        const std::size_t pixels = at;
        const std::size_t area = static_cast<std::size_t>(out.width) * out.height;
        out.coverage.assign(area, 0);
        out.colors.assign(area, Rgb{});
        for (const Span& span : spans) {
            for (int i = 0; i < span.length; ++i) {
                const std::size_t src = pixels + span.pixelOffset + 2 * static_cast<std::size_t>(i);
                const int x = span.x + i;
                if (src + 2 > data_.size() || x >= out.width) return false;
                const std::size_t dst = static_cast<std::size_t>(span.y) * out.width + x;
                out.coverage[dst] = 255;
                out.colors[dst] = Unpack(PixelFormat::Rgb565, U16(src));
            }
        }
        return true;
    }

private:
    std::uint16_t U16(std::size_t at) const {
        std::uint16_t v;
        std::memcpy(&v, data_.data() + at, 2);
        return v;
    }
    std::uint32_t U32(std::size_t at) const {
        std::uint32_t v;
        std::memcpy(&v, data_.data() + at, 4);
        return v;
    }
    std::int32_t I32(std::size_t at) const { return static_cast<std::int32_t>(U32(at)); }

    std::vector<std::uint8_t> data_;
    unsigned codePage_;
};

}  // namespace

std::unique_ptr<GlyphSource> CreateFntGlyphSource(const std::filesystem::path& file, unsigned codePage) {
    std::ifstream in(file, std::ios::binary);
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto source = std::make_unique<FntGlyphSource>(std::move(data), codePage);
    if (!source->Valid()) {
        log::Error("Bitmap font {}: missing or not ML_FONT_01", paths::ToUtf8(file));
        return nullptr;
    }
    return source;
}

}  // namespace ddmod::overlay
