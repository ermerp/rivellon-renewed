#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace ddmod::sha256 {

using Digest = std::array<std::uint8_t, 32>;

// Incremental SHA-256 (FIPS 180-4). Own implementation, so hashing works inside DllMain
// without loading bcrypt.dll under the loader lock.
class Hasher {
public:
    Hasher();
    void Update(std::span<const std::uint8_t> data);
    Digest Finish();

private:
    void Block(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state_;
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffered_ = 0;
    std::uint64_t totalBytes_ = 0;
};

Digest Of(std::span<const std::uint8_t> data);

// Hashes a whole file; std::nullopt if it cannot be read.
std::optional<Digest> OfFile(const std::filesystem::path& file);

// Upper-case hex, as in docs/re-notes.md.
std::string ToHex(const Digest& digest);

}  // namespace ddmod::sha256
