#include "text/style.h"

#include <algorithm>
#include <cmath>

namespace ddmod::text {
namespace {

const overlay::Glyph& Letter(const Capitals& caps, wchar_t ch) {
    static const overlay::Glyph kEmpty;
    const std::size_t i = static_cast<std::size_t>(ch - L'A');
    return i < caps.size() ? caps[i] : kEmpty;
}

int Lum3(Rgb c) { return c.r + c.g + c.b; }

std::uint8_t Scale(std::uint8_t v, float k) { return static_cast<std::uint8_t>(std::clamp(v * k + 0.5f, 0.0f, 255.0f)); }

}  // namespace

Image MakeImage(int width, int height) {
    Image img;
    img.width = width;
    img.height = height;
    img.alpha.assign(static_cast<std::size_t>(width) * height, 0.0f);
    img.color.assign(static_cast<std::size_t>(width) * height, Rgb{});
    return img;
}

Rgb To565(Rgb c) {
    const int r = c.r >> 3, g = c.g >> 2, b = c.b >> 3;
    return {static_cast<std::uint8_t>(r << 3 | r >> 2), static_cast<std::uint8_t>(g << 2 | g >> 4),
            static_cast<std::uint8_t>(b << 3 | b >> 2)};
}

Rgb Shade(Rgb c, Rgb to, float k) {
    return Rgb{static_cast<std::uint8_t>(c.r + (to.r - c.r) * k), static_cast<std::uint8_t>(c.g + (to.g - c.g) * k),
               static_cast<std::uint8_t>(c.b + (to.b - c.b) * k)};
}

CapMetrics MeasureCap(const Capitals& caps) {
    const overlay::Glyph& g = Letter(caps, L'H');
    int top = g.height, bottom = -1;
    for (int y = 0; y < g.height; ++y) {
        for (int x = 0; x < g.width; ++x) {
            if (g.coverage[static_cast<std::size_t>(y) * g.width + x] != 0) {
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }
    if (bottom < top) return {};
    return {g.offsetY + top, bottom - top + 1};
}

std::vector<Rgb> MeasureProfile(const Capitals& caps) {
    constexpr int kSteps = 16;
    const overlay::Glyph& h = Letter(caps, L'H');
    int top = -1;
    for (int y = 0; y < h.height && top < 0; ++y) {
        for (int x = 0; x < h.width; ++x) {
            if (h.coverage[static_cast<std::size_t>(y) * h.width + x] != 0) {
                top = y + h.offsetY;
                break;
            }
        }
    }
    const int height = std::max(1, MeasureCap(caps).height);
    std::vector<double> sum(kSteps * 3, 0.0);
    std::vector<int> count(kSteps, 0);
    for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) {
        const overlay::Glyph& g = Letter(caps, ch);
        if (g.colors.empty()) continue;
        const auto on = [&](int x, int y) {
            return x >= 0 && y >= 0 && x < g.width && y < g.height && g.coverage[static_cast<std::size_t>(y) * g.width + x] != 0;
        };
        for (int y = 0; y < g.height; ++y) {
            const int step = (y + g.offsetY - top) * kSteps / height;
            if (step < 0 || step >= kSteps) continue;
            for (int x = 0; x < g.width; ++x) {
                if (!on(x, y) || !on(x - 1, y) || !on(x + 1, y) || !on(x, y - 1) || !on(x, y + 1)) continue;
                const Rgb c = g.colors[static_cast<std::size_t>(y) * g.width + x];
                sum[step * 3] += c.r;
                sum[step * 3 + 1] += c.g;
                sum[step * 3 + 2] += c.b;
                ++count[step];
            }
        }
    }
    std::vector<Rgb> profile(kSteps);
    for (int s = 0; s < kSteps; ++s) {
        int src = s;  // empty steps take the nearest filled one
        for (int d = 1; count[src] == 0 && d < kSteps; ++d) {
            if (s - d >= 0 && count[s - d] > 0) src = s - d;
            else if (s + d < kSteps && count[s + d] > 0) src = s + d;
        }
        const int n = std::max(1, count[src]);
        profile[s] = {static_cast<std::uint8_t>(sum[src * 3] / n), static_cast<std::uint8_t>(sum[src * 3 + 1] / n),
                      static_cast<std::uint8_t>(sum[src * 3 + 2] / n)};
    }
    return profile;
}

Bevel MeasureBevel(const Capitals& caps, const std::vector<Rgb>& profile) {
    const CapMetrics cap = MeasureCap(caps);
    const int top = cap.top, height = std::max(1, cap.height);
    double sum[4][3] = {}, count[4][3] = {};
    for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) {
        const overlay::Glyph& g = Letter(caps, ch);
        if (g.colors.empty()) continue;
        const auto on = [&](int x, int y) {
            return x >= 0 && y >= 0 && x < g.width && y < g.height && g.coverage[static_cast<std::size_t>(y) * g.width + x] != 0;
        };
        const auto run = [&](int x, int y, int dx, int dy) {
            int d = 0;
            while (on(x + d * dx, y + d * dy)) ++d;
            return d;  // 1 = the pixel itself is the last one in that direction
        };
        for (int y = 0; y < g.height; ++y) {
            const int bin = std::clamp((y + g.offsetY - top) * static_cast<int>(profile.size()) / height, 0,
                                       static_cast<int>(profile.size()) - 1);
            const int ref = std::max(1, Lum3(profile[static_cast<std::size_t>(bin)]));
            for (int x = 0; x < g.width; ++x) {
                if (!on(x, y)) continue;
                const int d[4] = {run(x, y, 0, -1), run(x, y, 0, 1), run(x, y, -1, 0), run(x, y, 1, 0)};
                const double ratio = static_cast<double>(Lum3(g.colors[static_cast<std::size_t>(y) * g.width + x])) / ref;
                for (int s = 0; s < 4; ++s) {
                    const int across = s < 2 ? std::min(d[2], d[3]) : std::min(d[0], d[1]);
                    const int opposite = d[s ^ 1];
                    if (d[s] <= 3 && opposite > d[s] && across > 1) {
                        sum[s][d[s] - 1] += ratio;
                        count[s][d[s] - 1] += 1;
                    }
                }
            }
        }
    }
    Bevel b;
    for (int s = 0; s < 4; ++s) {
        for (int k = 0; k < 3; ++k) {
            if (count[s][k] > 0) b.side[s][k] = static_cast<float>(sum[s][k] / count[s][k]);
        }
    }
    return b;
}

Shine MeasureShine(const Capitals& caps) {
    double sum[2][kShineBins] = {}, count[2][kShineBins] = {};
    double resid = 0.0, residCount = 0.0;
    for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) {
        const overlay::Glyph& g = Letter(caps, ch);
        if (g.colors.empty()) continue;
        const auto on = [&](int x, int y) {
            return x >= 0 && y >= 0 && x < g.width && y < g.height && g.coverage[static_cast<std::size_t>(y) * g.width + x] != 0;
        };
        const auto lum = [&](int x, int y) { return static_cast<double>(Lum3(g.colors[static_cast<std::size_t>(y) * g.width + x])); };
        for (int dir = 0; dir < 2; ++dir) {
            const int outer = dir == 0 ? g.height : g.width, inner = dir == 0 ? g.width : g.height;
            for (int o = 0; o < outer; ++o) {
                for (int i = 0; i < inner;) {
                    const auto at = [&](int k) { return dir == 0 ? on(k, o) : on(o, k); };
                    if (!at(i)) {
                        ++i;
                        continue;
                    }
                    int end = i;
                    while (end < inner && at(end)) ++end;
                    const int len = end - i;
                    if (len >= 4) {
                        double mean = 0.0;
                        for (int k = i; k < end; ++k) mean += dir == 0 ? lum(k, o) : lum(o, k);
                        mean /= len;
                        for (int k = i; k < end; ++k) {
                            const double v = (dir == 0 ? lum(k, o) : lum(o, k)) / std::max(1.0, mean);
                            const int bin = std::min(kShineBins - 1, (k - i) * kShineBins / len);
                            sum[dir][bin] += v;
                            count[dir][bin] += 1;
                        }
                    }
                    i = end;
                }
            }
        }
    }
    Shine s;
    for (int dir = 0; dir < 2; ++dir) {
        for (int b = 0; b < kShineBins; ++b) {
            if (count[dir][b] > 0) s.across[dir][b] = static_cast<float>(sum[dir][b] / count[dir][b]);
        }
    }
    // Texture: what is left of the inner pixels after the run profiles, as a standard deviation.
    for (wchar_t ch = L'A'; ch <= L'Z'; ++ch) {
        const overlay::Glyph& g = Letter(caps, ch);
        if (g.colors.empty()) continue;
        for (int y = 1; y + 1 < g.height; ++y) {
            for (int x = 1; x + 1 < g.width; ++x) {
                const auto c = [&](int dx, int dy) {
                    return g.coverage[static_cast<std::size_t>(y + dy) * g.width + x + dx] != 0;
                };
                if (!c(0, 0) || !c(-1, 0) || !c(1, 0) || !c(0, -1) || !c(0, 1)) continue;
                double around = 0.0;
                for (int k = 0; k < 4; ++k) {
                    const int dx = k == 0 ? -1 : k == 1 ? 1 : 0, dy = k == 2 ? -1 : k == 3 ? 1 : 0;
                    around += Lum3(g.colors[static_cast<std::size_t>(y + dy) * g.width + x + dx]);
                }
                const double v = Lum3(g.colors[static_cast<std::size_t>(y) * g.width + x]) / std::max(1.0, around / 4.0) - 1.0;
                resid += v * v;
                residCount += 1;
            }
        }
    }
    if (residCount > 0) s.texture = static_cast<float>(std::sqrt(resid / residCount));
    return s;
}

float ValueNoise(int x, int y, int cell) {
    const auto hash = [](int i, int j) {
        std::uint32_t h = static_cast<std::uint32_t>(i) * 374761393u + static_cast<std::uint32_t>(j) * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return static_cast<float>((h ^ (h >> 16)) & 0xFFFF) / 32767.5f - 1.0f;
    };
    const int cx = x / cell, cy = y / cell;
    const float fx = static_cast<float>(x % cell) / cell, fy = static_cast<float>(y % cell) / cell;
    const float a = hash(cx, cy) * (1 - fx) + hash(cx + 1, cy) * fx;
    const float b = hash(cx, cy + 1) * (1 - fx) + hash(cx + 1, cy + 1) * fx;
    return a * (1 - fy) + b * fy;
}

Image PaintRamp(const Image& mask, Rgb text, Rgb edge) {
    Image img = MakeImage(mask.width, mask.height);
    for (int y = 0; y < mask.height; ++y) {
        for (int x = 0; x < mask.width; ++x) {
            const int v = static_cast<int>(mask.A(x, y) * 64.0f + 0.5f);
            if (v == 0) continue;
            img.A(x, y) = 1.0f;
            img.C(x, y) = To565(Shade(edge, text, v / 64.0f));
        }
    }
    return img;
}

Image PaintStyled(const Image& mask, int capTop, int capHeight, const StyleParams& style) {
    const std::vector<Rgb>& profile = style.profile;
    const auto a = [&](int x, int y) {
        return x < 0 || y < 0 || x >= mask.width || y >= mask.height ? 0.0f : mask.A(x, y);
    };
    const auto run = [&](int x, int y, int dx, int dy) {
        int d = 0;
        while (d < 4 && a(x + d * dx, y + d * dy) >= 0.5f) ++d;
        return d;
    };
    Image img = MakeImage(mask.width, mask.height);
    if (profile.empty()) return img;
    for (int y = 0; y < mask.height; ++y) {
        const int bin = std::clamp((y - capTop) * static_cast<int>(profile.size()) / std::max(1, capHeight), 0,
                                   static_cast<int>(profile.size()) - 1);
        for (int x = 0; x < mask.width; ++x) {
            const float own = a(x, y);
            float around = own;
            if (style.outline) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) around = std::max(around, a(x + dx, y + dy));
                }
            }
            if (around <= 0.0f) continue;
            Rgb c = profile[static_cast<std::size_t>(bin)];
            if (style.gloss && own >= 0.5f) {
                if (a(x, y - 1) < 0.5f) c = Shade(c, Rgb{255, 255, 255}, 0.45f);
                else if (a(x, y + 1) < 0.5f) c = Shade(c, Rgb{0, 0, 0}, 0.35f);
            }
            if (style.bevel && own >= 0.5f) {
                const int d[4] = {run(x, y, 0, -1), run(x, y, 0, 1), run(x, y, -1, 0), run(x, y, 1, 0)};
                float k = 1.0f;
                for (int s = 0; s < 4; ++s) {
                    if (d[s] >= 1 && d[s] <= 3) k *= style.bevel->side[s][d[s] - 1];
                }
                c = {Scale(c.r, k), Scale(c.g, k), Scale(c.b, k)};
            }
            if (style.shine && own >= 0.5f) {
                float k = 1.0f;
                for (int dir = 0; dir < 2; ++dir) {
                    const int dx = dir == 0 ? 1 : 0, dy = dir == 0 ? 0 : 1;
                    int lo = 0, hi = 0;
                    while (lo < 60 && a(x - (lo + 1) * dx, y - (lo + 1) * dy) >= 0.5f) ++lo;
                    while (hi < 60 && a(x + (hi + 1) * dx, y + (hi + 1) * dy) >= 0.5f) ++hi;
                    const int len = lo + hi + 1;
                    if (len < 3) continue;
                    const int step = std::min(kShineBins - 1, lo * kShineBins / len);
                    k *= 1.0f + (style.shine->across[dir][step] - 1.0f) * style.shineStrength;
                }
                if (style.texture) k *= 1.0f + style.shine->texture * style.shineStrength * ValueNoise(x + 1000, y + 1000, 2);
                c = {Scale(c.r, k), Scale(c.g, k), Scale(c.b, k)};
            }
            if ((style.bevel || style.shine) && own < 0.5f) c = Shade(style.contour, c, own * 2.0f);
            img.A(x, y) = around;
            img.C(x, y) = To565(style.outline ? Shade(style.contour, c, own / around) : c);
        }
    }
    return img;
}

std::vector<std::uint16_t> ToGamePixels(const Image& img, Rgb background) {
    std::vector<std::uint16_t> pixels(img.alpha.size(), 0);
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        const auto alpha = static_cast<int>(img.alpha[i] * 255.0f + 0.5f);
        if (alpha * 64 < 128) continue;  // below half a step of the game's 65-level coverage
        const Rgb c = overlay::Mix(background, img.color[i], static_cast<std::uint8_t>(std::min(alpha, 255)));
        const auto value = static_cast<std::uint16_t>(((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3));
        pixels[i] = value ? value : 1;
    }
    return pixels;
}

}  // namespace ddmod::text
