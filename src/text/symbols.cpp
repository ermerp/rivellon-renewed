#include "text/symbols.h"

#include <vector>

namespace ddmod::text {
namespace {

struct Point {
    float x, y;
};

constexpr int kCurveSteps = 24;
constexpr int kSamples = 8;

// Quadratic Bezier from `a` to `b` through the control point `c`, without its start point.
void AddCurve(std::vector<Point>& poly, Point a, Point c, Point b) {
    for (int i = 1; i <= kCurveSteps; ++i) {
        const float t = static_cast<float>(i) / kCurveSteps, u = 1.0f - t;
        poly.push_back({u * u * a.x + 2 * u * t * c.x + t * t * b.x, u * u * a.y + 2 * u * t * c.y + t * t * b.y});
    }
}

// The outline in the unit box, pointing right. Bowed sides keep their middle 0.12 box units off
// the straight line (twice that for the control point).
std::vector<Point> Outline(ArrowShape shape) {
    const Point top{0, 0}, tip{1, 0.5f}, bottom{0, 1};
    std::vector<Point> poly{top};
    switch (shape) {
        case ArrowShape::Triangle:
        case ArrowShape::Notched:
            poly.push_back(tip);
            poly.push_back(bottom);
            if (shape == ArrowShape::Notched) poly.push_back({0.28f, 0.5f});
            break;
        case ArrowShape::Convex:
        case ArrowShape::Concave: {
            // Normal of the upper side (1, 0.5), pointing away from the arrow's middle.
            const float s = shape == ArrowShape::Convex ? 1.0f : -1.0f, d = 0.24f / 1.118f;
            AddCurve(poly, top, {0.5f + s * 0.5f * d, 0.25f - s * d}, tip);
            AddCurve(poly, tip, {0.5f + s * 0.5f * d, 0.75f + s * d}, bottom);
            break;
        }
    }
    return poly;
}

// Even-odd rule.
bool Inside(const std::vector<Point>& poly, float x, float y) {
    bool in = false;
    for (std::size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const Point a = poly[i], b = poly[j];
        if ((a.y > y) != (b.y > y) && x < a.x + (y - a.y) * (b.x - a.x) / (b.y - a.y)) in = !in;
    }
    return in;
}

}  // namespace

Image ArrowMask(ArrowShape shape, bool right, int width, int height) {
    Image mask = MakeImage(width, height);
    if (width <= 0 || height <= 0) return mask;
    const std::vector<Point> poly = Outline(shape);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int hits = 0;
            for (int s = 0; s < kSamples * kSamples; ++s) {
                const float u = (x + (s % kSamples + 0.5f) / kSamples) / width;
                const float v = (y + (s / kSamples + 0.5f) / kSamples) / height;
                if (Inside(poly, right ? u : 1.0f - u, v)) ++hits;
            }
            mask.A(x, y) = static_cast<float>(hits) / (kSamples * kSamples);
        }
    }
    return mask;
}

}  // namespace ddmod::text
