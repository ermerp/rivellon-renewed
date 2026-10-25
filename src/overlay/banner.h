#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "fonts/families.h"
#include "text/style.h"

namespace ddmod::overlay {

// The start banner in the game's own look (docs/decisions.md): parts in different families on one
// baseline, each at the size and in the colors of one of the game's bitmap fonts and in its
// family's face and style ("Rivellon Renewed" like the menu's small capitals, the version like the
// load list, "active" like the load dialog's title). The bitmap fonts are read from the game's
// fonts folder only to measure them (src/text); nothing of them is drawn.
struct BannerPart {
    std::string font;  // stem of the game font ("fontbig24")
    fonts::Family family;
    std::wstring text;
    int gap = 0;  // pixels after the previous part
};

struct BannerImage {
    text::Image image;  // empty if a font or face was missing
    int baseline = 0;   // row of the capitals' bottom in `image`
    int left = 0;       // column of the first part's pen start in `image`
};

// The default parts: the name in capitals, the version, "active".
std::vector<BannerPart> DefaultBanner(const std::wstring& name, const std::wstring& version, const std::wstring& state);

// Renders the parts; logs and returns an empty image if a font file cannot be read or a
// family has no face.
BannerImage RenderBanner(const std::filesystem::path& fontsDir, const std::vector<BannerPart>& parts);

}  // namespace ddmod::overlay
