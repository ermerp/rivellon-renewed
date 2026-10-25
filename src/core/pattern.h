#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace ddmod::pattern {

// Byte pattern in IDA/x64dbg style: "8B 0D ?? ?? ?? ?? 85 C9" ("?" and "??" are wildcards).
struct Pattern {
    std::vector<std::uint8_t> bytes;
    std::vector<bool> wildcard;  // same length as `bytes`

    std::size_t size() const { return bytes.size(); }
};

// std::nullopt for empty input, bad tokens, or a pattern consisting only of wildcards.
std::optional<Pattern> Parse(std::string_view text);

// Offsets of all matches in `data`, in ascending order; stops after `maxMatches`.
std::vector<std::size_t> FindAll(std::span<const std::uint8_t> data, const Pattern& pattern,
                                 std::size_t maxMatches = SIZE_MAX);

}  // namespace ddmod::pattern
