#pragma once

#include <cstdint>

namespace ddmod::overlay {

// Pixel formats the overlay can draw into. The game renders 16 bit (565 or 555, full screen);
// windowed on a 32-bit desktop the back buffer has the desktop's format (docs/re/overlay.md).
enum class PixelFormat { Rgb565, Rgb555, Xrgb8888 };

const char* ToString(PixelFormat format);

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
};

// A locked frame: `pixels` points at (0, 0), rows are `pitch` bytes apart.
struct SurfaceView {
    std::uint8_t* pixels = nullptr;
    int pitch = 0;
    int width = 0;
    int height = 0;
    PixelFormat format = PixelFormat::Rgb565;
};

// Packs and unpacks one pixel value (16-bit formats use the low 16 bits). Unpacking expands
// 5/6-bit channels to the full 0-255 range, so white stays 255.
std::uint32_t Pack(PixelFormat format, Rgb color);
Rgb Unpack(PixelFormat format, std::uint32_t pixel);

// Mixes `src` over `dst`: alpha 0 keeps dst, 255 gives src.
Rgb Mix(Rgb dst, Rgb src, std::uint8_t alpha);

// Clipped drawing into a view. Coordinates may lie partly or fully outside it.
void FillRect(const SurfaceView& view, int x, int y, int width, int height, Rgb color, std::uint8_t alpha);
// Blends an 8-bit coverage mask (0 = transparent, 255 = opaque), scaled by `opacity`.
void DrawMask(const SurfaceView& view, int x, int y, const std::uint8_t* mask, int maskWidth, int maskHeight,
              int maskPitch, Rgb color, std::uint8_t opacity);

}  // namespace ddmod::overlay
