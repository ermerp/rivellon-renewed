#include "scale.h"

#include <algorithm>
#include <cmath>

namespace fontpreview {
namespace {

int Lum(Rgb c) { return std::max({c.r, c.g, c.b}); }

std::uint8_t ToByte(float v) { return static_cast<std::uint8_t>(std::clamp(v, 0.0f, 255.0f) + 0.5f); }

bool Inside(const Image& img, int x, int y) {
    return x >= 0 && y >= 0 && x < img.width && y < img.height && img.A(x, y) >= 0.5f;
}

// Color of the nearest pixel with alpha > 0 within a small radius (for pixels the scalers turn on).
Rgb NearestColor(const Image& img, int x, int y) {
    for (int r = 0; r <= 2; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                const int sx = x + dx, sy = y + dy;
                if (sx >= 0 && sy >= 0 && sx < img.width && sy < img.height && img.A(sx, sy) > 0.0f) {
                    return img.C(sx, sy);
                }
            }
        }
    }
    return {255, 255, 255};
}

}  // namespace

Image RenderLine(ddmod::overlay::GlyphCache& cache, std::wstring_view text, Rgb color, int pad) {
    Image img = MakeImage(std::max(1, ddmod::overlay::MeasureString(cache, text) + 2 * pad),
                          cache.LineHeight() + 2 * pad);
    int maxLum = 1;
    for (const wchar_t ch : text) {
        for (const Rgb c : cache.Get(ch).colors) maxLum = std::max(maxLum, Lum(c));
    }
    int pen = pad;
    for (const wchar_t ch : text) {
        const ddmod::overlay::Glyph& g = cache.Get(ch);
        for (int y = 0; y < g.height; ++y) {
            for (int x = 0; x < g.width; ++x) {
                const int ox = pen + g.offsetX + x, oy = pad + g.offsetY + y;
                if (ox < 0 || oy < 0 || ox >= img.width || oy >= img.height) continue;
                const std::size_t i = static_cast<std::size_t>(y) * g.width + x;
                if (g.coverage[i] == 0) continue;
                if (g.colors.empty()) {
                    img.A(ox, oy) = g.coverage[i] / 255.0f;
                    img.C(ox, oy) = color;
                } else {
                    const Rgb c = g.colors[i];
                    const float a = static_cast<float>(Lum(c)) / maxLum;
                    img.A(ox, oy) = a;
                    const float k = a > 0.0f ? 1.0f / a : 1.0f;
                    img.C(ox, oy) = {ToByte(c.r * k), ToByte(c.g * k), ToByte(c.b * k)};
                }
            }
        }
        pen += g.advance;
    }
    return img;
}

Image ScaleNearest(const Image& src, int scale) {
    Image out = MakeImage(src.width * scale, src.height * scale);
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            out.A(x, y) = src.A(x / scale, y / scale);
            out.C(x, y) = src.C(x / scale, y / scale);
        }
    }
    return out;
}

Image ScaleEpx(const Image& src) {
    Image out = MakeImage(src.width * 2, src.height * 2);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            const bool p = Inside(src, x, y);
            const bool a = Inside(src, x, y - 1), b = Inside(src, x + 1, y);
            const bool c = Inside(src, x - 1, y), d = Inside(src, x, y + 1);
            bool q[4] = {p, p, p, p};  // top-left, top-right, bottom-left, bottom-right
            if (c == a && c != d && a != b) q[0] = a;
            if (a == b && a != c && b != d) q[1] = b;
            if (d == c && d != b && c != a) q[2] = c;
            if (b == d && b != a && d != c) q[3] = d;
            const Rgb color = src.A(x, y) > 0.0f ? src.C(x, y) : NearestColor(src, x, y);
            for (int k = 0; k < 4; ++k) {
                const int ox = 2 * x + k % 2, oy = 2 * y + k / 2;
                out.A(ox, oy) = q[k] ? 1.0f : 0.0f;
                out.C(ox, oy) = color;
            }
        }
    }
    return out;
}

Image ScaleSdf(const Image& src, int scale) {
    // Signed distance (in source pixels) from each pixel center to the edge: negative inside.
    constexpr int kRadius = 4;
    std::vector<float> dist(static_cast<std::size_t>(src.width) * src.height);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            const bool in = Inside(src, x, y);
            float best = kRadius + 0.5f;
            for (int dy = -kRadius; dy <= kRadius; ++dy) {
                for (int dx = -kRadius; dx <= kRadius; ++dx) {
                    if (Inside(src, x + dx, y + dy) != in) {
                        best = std::min(best, std::sqrt(static_cast<float>(dx * dx + dy * dy)));
                    }
                }
            }
            // The edge lies halfway between a pixel center and the nearest opposite one; for pixels
            // right at the edge the font's own gray level places it more precisely.
            float d = (in ? -1.0f : 1.0f) * (best - 0.5f);
            if (best <= 1.0f) d = 0.5f - src.A(x, y);
            dist[static_cast<std::size_t>(y) * src.width + x] = d;
        }
    }
    const auto at = [&](int x, int y) {
        x = std::clamp(x, 0, src.width - 1);
        y = std::clamp(y, 0, src.height - 1);
        return dist[static_cast<std::size_t>(y) * src.width + x];
    };
    Image out = MakeImage(src.width * scale, src.height * scale);
    for (int oy = 0; oy < out.height; ++oy) {
        for (int ox = 0; ox < out.width; ++ox) {
            const float sx = (ox + 0.5f) / scale - 0.5f, sy = (oy + 0.5f) / scale - 0.5f;
            const int x0 = static_cast<int>(std::floor(sx)), y0 = static_cast<int>(std::floor(sy));
            const float fx = sx - x0, fy = sy - y0;
            const float d = (at(x0, y0) * (1 - fx) + at(x0 + 1, y0) * fx) * (1 - fy) +
                            (at(x0, y0 + 1) * (1 - fx) + at(x0 + 1, y0 + 1) * fx) * fy;
            out.A(ox, oy) = std::clamp(0.5f - d * scale, 0.0f, 1.0f);
            const int nx = std::clamp(static_cast<int>(sx + 0.5f), 0, src.width - 1);
            const int ny = std::clamp(static_cast<int>(sy + 0.5f), 0, src.height - 1);
            out.C(ox, oy) = src.A(nx, ny) > 0.0f ? src.C(nx, ny) : NearestColor(src, nx, ny);
        }
    }
    return out;
}

Image Binarize(const Image& src, float threshold) {
    Image out = MakeImage(src.width, src.height);
    for (int y = 0; y < src.height; ++y) {
        for (int x = 0; x < src.width; ++x) {
            out.A(x, y) = 1.0f;
            out.C(x, y) = src.A(x, y) >= threshold ? Rgb{0, 0, 0} : Rgb{255, 255, 255};
        }
    }
    return out;
}

std::vector<std::uint32_t> Composite(const Image& img, Rgb bg) {
    std::vector<std::uint32_t> px(static_cast<std::size_t>(img.width) * img.height);
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            const Rgb c = ddmod::overlay::Mix(bg, img.C(x, y), ToByte(img.A(x, y) * 255.0f));
            px[static_cast<std::size_t>(y) * img.width + x] = ddmod::overlay::Pack(ddmod::overlay::PixelFormat::Xrgb8888, c);
        }
    }
    return px;
}

}  // namespace fontpreview
