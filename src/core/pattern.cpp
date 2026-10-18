#include "core/pattern.h"

#include <cstring>

namespace ddmod::pattern {
namespace {

int HexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

}  // namespace

std::optional<Pattern> Parse(std::string_view text) {
    Pattern out;
    std::size_t i = 0;
    while (i < text.size()) {
        if (IsSpace(text[i])) {
            ++i;
            continue;
        }
        std::size_t end = i;
        while (end < text.size() && !IsSpace(text[end])) ++end;
        const std::string_view token = text.substr(i, end - i);
        i = end;

        if (token == "?" || token == "??") {
            out.bytes.push_back(0);
            out.wildcard.push_back(true);
            continue;
        }
        if (token.size() != 2) return std::nullopt;
        const int hi = HexDigit(token[0]);
        const int lo = HexDigit(token[1]);
        if (hi < 0 || lo < 0) return std::nullopt;
        out.bytes.push_back(static_cast<std::uint8_t>(hi * 16 + lo));
        out.wildcard.push_back(false);
    }

    bool hasFixedByte = false;
    for (const bool w : out.wildcard) hasFixedByte |= !w;
    if (!hasFixedByte) return std::nullopt;
    return out;
}

std::vector<std::size_t> FindAll(std::span<const std::uint8_t> data, const Pattern& pattern, std::size_t maxMatches) {
    std::vector<std::size_t> matches;
    const std::size_t n = pattern.size();
    if (n == 0 || data.size() < n || maxMatches == 0) return matches;

    // Anchor on the first fixed byte and let memchr skip ahead to its candidates.
    std::size_t anchor = 0;
    while (pattern.wildcard[anchor]) ++anchor;
    const std::uint8_t anchorByte = pattern.bytes[anchor];

    const std::size_t lastStart = data.size() - n;
    std::size_t start = 0;
    while (start <= lastStart) {
        const void* hit = std::memchr(data.data() + start + anchor, anchorByte, lastStart - start + 1);
        if (!hit) break;
        start = static_cast<std::size_t>(static_cast<const std::uint8_t*>(hit) - data.data()) - anchor;

        bool match = true;
        for (std::size_t k = 0; k < n; ++k) {
            if (!pattern.wildcard[k] && data[start + k] != pattern.bytes[k]) {
                match = false;
                break;
            }
        }
        if (match) {
            matches.push_back(start);
            if (matches.size() >= maxMatches) break;
        }
        ++start;
    }
    return matches;
}

}  // namespace ddmod::pattern
