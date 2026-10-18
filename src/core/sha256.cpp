#include "core/sha256.h"

#include <bit>
#include <vector>

#include <windows.h>

namespace ddmod::sha256 {
namespace {

constexpr std::array<std::uint32_t, 64> kRound = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

constexpr std::array<std::uint32_t, 8> kInitial = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
};

std::uint32_t LoadBigEndian(const std::uint8_t* p) {
    return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) | (std::uint32_t{p[2]} << 8) | p[3];
}

}  // namespace

Hasher::Hasher() : state_(kInitial) {}

void Hasher::Block(const std::uint8_t* block) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) w[i] = LoadBigEndian(block + 4 * i);
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t t1 = h + (std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25)) + ((e & f) ^ (~e & g)) +
                                 kRound[i] + w[i];
        const std::uint32_t t2 = (std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Hasher::Update(std::span<const std::uint8_t> data) {
    totalBytes_ += data.size();
    std::size_t pos = 0;
    if (buffered_ > 0) {
        while (buffered_ < 64 && pos < data.size()) buffer_[buffered_++] = data[pos++];
        if (buffered_ < 64) return;
        Block(buffer_.data());
        buffered_ = 0;
    }
    for (; pos + 64 <= data.size(); pos += 64) Block(data.data() + pos);
    while (pos < data.size()) buffer_[buffered_++] = data[pos++];
}

Digest Hasher::Finish() {
    const std::uint64_t bitLength = totalBytes_ * 8;
    buffer_[buffered_++] = 0x80;
    if (buffered_ > 56) {
        while (buffered_ < 64) buffer_[buffered_++] = 0;
        Block(buffer_.data());
        buffered_ = 0;
    }
    while (buffered_ < 56) buffer_[buffered_++] = 0;
    for (int i = 7; i >= 0; --i) buffer_[buffered_++] = static_cast<std::uint8_t>(bitLength >> (8 * i));
    Block(buffer_.data());
    buffered_ = 0;

    Digest out;
    for (int i = 0; i < 8; ++i) {
        out[4 * i + 0] = static_cast<std::uint8_t>(state_[i] >> 24);
        out[4 * i + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
        out[4 * i + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
        out[4 * i + 3] = static_cast<std::uint8_t>(state_[i]);
    }
    state_ = kInitial;
    totalBytes_ = 0;
    return out;
}

Digest Of(std::span<const std::uint8_t> data) {
    Hasher hasher;
    hasher.Update(data);
    return hasher.Finish();
}

std::optional<Digest> OfFile(const std::filesystem::path& file) {
    const HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                                      OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return std::nullopt;

    Hasher hasher;
    std::vector<std::uint8_t> chunk(64 * 1024);
    bool ok = true;
    for (;;) {
        DWORD read = 0;
        if (!ReadFile(handle, chunk.data(), static_cast<DWORD>(chunk.size()), &read, nullptr)) {
            ok = false;
            break;
        }
        if (read == 0) break;
        hasher.Update(std::span(chunk.data(), read));
    }
    CloseHandle(handle);
    if (!ok) return std::nullopt;
    return hasher.Finish();
}

std::string ToHex(const Digest& digest) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(digest.size() * 2);
    for (const std::uint8_t byte : digest) {
        out.push_back(kHex[byte >> 4]);
        out.push_back(kHex[byte & 0x0F]);
    }
    return out;
}

}  // namespace ddmod::sha256
