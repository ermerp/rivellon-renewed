#include "overlay/surface.h"

#include <algorithm>
#include <cstring>

namespace ddmod::overlay {
namespace {

std::uint8_t Expand5(std::uint32_t v) { return static_cast<std::uint8_t>((v << 3) | (v >> 2)); }
std::uint8_t Expand6(std::uint32_t v) { return static_cast<std::uint8_t>((v << 2) | (v >> 4)); }

int BytesPerPixel(PixelFormat format) { return format == PixelFormat::Xrgb8888 ? 4 : 2; }

std::uint32_t Read(const std::uint8_t* at, int bytes) {
    if (bytes == 2) {
        std::uint16_t v;
        std::memcpy(&v, at, 2);
        return v;
    }
    std::uint32_t v;
    std::memcpy(&v, at, 4);
    return v;
}

void Write(std::uint8_t* at, int bytes, std::uint32_t value) {
    if (bytes == 2) {
        const auto v = static_cast<std::uint16_t>(value);
        std::memcpy(at, &v, 2);
    } else {
        std::memcpy(at, &value, 4);
    }
}

std::uint8_t Scale(std::uint8_t a, std::uint8_t b) { return static_cast<std::uint8_t>((a * b + 127) / 255); }

// Intersects [x, x+w) x [y, y+h) with the view; returns false if nothing is left.
bool Clip(const SurfaceView& view, int& x, int& y, int& w, int& h, int& skipX, int& skipY) {
    skipX = std::max(0, -x);
    skipY = std::max(0, -y);
    x += skipX;
    y += skipY;
    w = std::min(w - skipX, view.width - x);
    h = std::min(h - skipY, view.height - y);
    return w > 0 && h > 0 && view.pixels;
}

void BlendPixel(std::uint8_t* at, PixelFormat format, Rgb color, std::uint8_t alpha) {
    const int bytes = BytesPerPixel(format);
    const std::uint32_t out =
        alpha == 255 ? Pack(format, color) : Pack(format, Mix(Unpack(format, Read(at, bytes)), color, alpha));
    Write(at, bytes, out);
}

}  // namespace

const char* ToString(PixelFormat format) {
    switch (format) {
        case PixelFormat::Rgb565: return "RGB565";
        case PixelFormat::Rgb555: return "RGB555";
        case PixelFormat::Xrgb8888: return "XRGB8888";
    }
    return "?";
}

std::uint32_t Pack(PixelFormat format, Rgb c) {
    switch (format) {
        case PixelFormat::Rgb565: return ((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3);
        case PixelFormat::Rgb555: return ((c.r >> 3) << 10) | ((c.g >> 3) << 5) | (c.b >> 3);
        case PixelFormat::Xrgb8888: return (std::uint32_t{c.r} << 16) | (std::uint32_t{c.g} << 8) | c.b;
    }
    return 0;
}

Rgb Unpack(PixelFormat format, std::uint32_t p) {
    switch (format) {
        case PixelFormat::Rgb565: return {Expand5((p >> 11) & 31), Expand6((p >> 5) & 63), Expand5(p & 31)};
        case PixelFormat::Rgb555: return {Expand5((p >> 10) & 31), Expand5((p >> 5) & 31), Expand5(p & 31)};
        case PixelFormat::Xrgb8888:
            return {static_cast<std::uint8_t>(p >> 16), static_cast<std::uint8_t>(p >> 8), static_cast<std::uint8_t>(p)};
    }
    return {};
}

Rgb Mix(Rgb dst, Rgb src, std::uint8_t alpha) {
    const auto mix = [alpha](std::uint8_t d, std::uint8_t s) {
        return static_cast<std::uint8_t>((s * alpha + d * (255 - alpha) + 127) / 255);
    };
    return {mix(dst.r, src.r), mix(dst.g, src.g), mix(dst.b, src.b)};
}

void FillRect(const SurfaceView& view, int x, int y, int width, int height, Rgb color, std::uint8_t alpha) {
    int skipX = 0, skipY = 0;
    if (alpha == 0 || !Clip(view, x, y, width, height, skipX, skipY)) return;
    const int bytes = BytesPerPixel(view.format);
    for (int row = 0; row < height; ++row) {
        std::uint8_t* at = view.pixels + (y + row) * view.pitch + x * bytes;
        for (int col = 0; col < width; ++col, at += bytes) BlendPixel(at, view.format, color, alpha);
    }
}

void DrawMask(const SurfaceView& view, int x, int y, const std::uint8_t* mask, int maskWidth, int maskHeight,
              int maskPitch, Rgb color, std::uint8_t opacity) {
    int skipX = 0, skipY = 0;
    if (!mask || opacity == 0 || !Clip(view, x, y, maskWidth, maskHeight, skipX, skipY)) return;
    const int bytes = BytesPerPixel(view.format);
    for (int row = 0; row < maskHeight; ++row) {
        const std::uint8_t* coverage = mask + (skipY + row) * maskPitch + skipX;
        std::uint8_t* at = view.pixels + (y + row) * view.pitch + x * bytes;
        for (int col = 0; col < maskWidth; ++col, at += bytes) {
            const std::uint8_t alpha = Scale(coverage[col], opacity);
            if (alpha != 0) BlendPixel(at, view.format, color, alpha);
        }
    }
}

void DrawColorMask(const SurfaceView& view, int x, int y, const std::uint8_t* mask, const Rgb* colors, int maskWidth,
                   int maskHeight, int maskPitch, std::uint8_t opacity) {
    int skipX = 0, skipY = 0;
    if (!mask || !colors || opacity == 0 || !Clip(view, x, y, maskWidth, maskHeight, skipX, skipY)) return;
    const int bytes = BytesPerPixel(view.format);
    for (int row = 0; row < maskHeight; ++row) {
        const std::size_t first = static_cast<std::size_t>(skipY + row) * maskPitch + skipX;
        std::uint8_t* at = view.pixels + (y + row) * view.pitch + x * bytes;
        for (int col = 0; col < maskWidth; ++col, at += bytes) {
            const std::uint8_t alpha = Scale(mask[first + col], opacity);
            if (alpha != 0) BlendPixel(at, view.format, colors[first + col], alpha);
        }
    }
}

}  // namespace ddmod::overlay
