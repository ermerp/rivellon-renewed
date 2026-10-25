#include "overlay/overlay.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <format>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <windows.h>

#include "core/config.h"
#include "core/hooks.h"
#include "core/log.h"
#include "core/paths.h"
#include "fonts/fontchain.h"
#include "overlay/backend.h"
#include "overlay/banner.h"
#include "overlay/glyphs.h"
#include "text/ttface.h"

namespace ddmod::overlay {
namespace {

using EndFrameFn = int(__cdecl*)();

// Where the banner's first pen position and its baseline lie (top left, beside the game's 4:3
// picture on a 16:9 screen; the game draws its fonts at fixed pixel sizes, so does the banner).
constexpr int kBannerX = 18;
constexpr int kBannerBaseline = 38;

struct Settings {
    StatusMode status = StatusMode::Banner;
    int bannerSeconds = 5;
    bool gameLook = true;  // [Overlay] BannerLook=game (else plain)
    std::filesystem::path fontFile;  // from the font chain ([Fonts] Chain); empty = none available
    int fontSize = 0;  // 0 = from the frame height
};

struct State {
    Settings settings;
    std::unique_ptr<Backend> backend;
    std::unique_ptr<GlyphCache> glyphs;
    int glyphsForHeight = 0;
    std::wstring status;
    // The banner in the game's look, rendered on first use (empty if that failed).
    bool bannerTried = false;
    BannerImage banner;
    std::vector<std::uint8_t> bannerMask;
    std::vector<Rgb> bannerColors;
    LARGE_INTEGER frequency{};
    LARGE_INTEGER firstFrame{};  // the banner time counts from here
    // FPS, measured over half-second windows.
    LARGE_INTEGER windowStart{};
    int framesInWindow = 0;
    int fps = 0;
};

State g_state;
EndFrameFn g_originalEndFrame = nullptr;
// Written by the hotkey handler, read at the end of each frame.
std::atomic<bool> g_showFps{false};
// ShowNotice: text and the tick count when it disappears.
std::mutex g_noticeMutex;
std::wstring g_notice;
ULONGLONG g_noticeUntil = 0;

std::wstring CurrentNotice() {
    std::lock_guard lock(g_noticeMutex);
    if (g_notice.empty() || GetTickCount64() >= g_noticeUntil) return {};
    return g_notice;
}

std::wstring Widen(const std::string& text) {
    if (text.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), len);
    return wide;
}

Settings ReadSettings() {
    Settings s;
    const std::string status = config::GetString("Overlay", "Status", "banner");
    if (!ParseStatusMode(status, s.status)) {
        log::Warn("[Overlay] Status=\"{}\" is not banner, always or off - using banner", status);
    }
    s.bannerSeconds = std::clamp(config::GetInt("Overlay", "BannerSeconds", 5), 1, 60);
    const std::string look = config::GetString("Overlay", "BannerLook", "game");
    if (!ParseBannerLook(look, s.gameLook)) {
        log::Warn("[Overlay] BannerLook=\"{}\" is not game or plain - using game", look);
        s.gameLook = true;
    }
    const auto& font = fonts::Configured();
    if (font) s.fontFile = font->file;
    s.fontSize = std::clamp(config::GetInt("Overlay", "FontSize", 0), 0, 96);
    return s;
}

void CountFrame(State& st, LARGE_INTEGER now) {
    if (st.framesInWindow++ == 0 && st.windowStart.QuadPart == 0) st.windowStart = now;
    const long long elapsed = now.QuadPart - st.windowStart.QuadPart;
    if (elapsed >= st.frequency.QuadPart / 2) {
        st.fps = static_cast<int>((st.framesInWindow * st.frequency.QuadPart + elapsed / 2) / elapsed);
        st.framesInWindow = 0;
        st.windowStart = now;
    }
}

// The font follows the frame height (a resolution change recreates the cache).
GlyphCache* GlyphsFor(State& st, int frameHeight) {
    if (st.glyphs && st.glyphsForHeight == frameHeight) return st.glyphs->LineHeight() > 0 ? st.glyphs.get() : nullptr;
    // The size is the em (character height without leading), drawn by our own rasterizer.
    const int em = st.settings.fontSize > 0 ? st.settings.fontSize : std::clamp(frameHeight / 60, 12, 48);
    std::unique_ptr<GlyphSource> source;
    if (const int cell = text::CellHeightForEm(st.settings.fontFile, em); cell > 0) {
        source = CreateFaceGlyphSource(std::make_unique<text::TtFace>(st.settings.fontFile, cell, FW_BOLD));
    }
    if (source) {
        log::Info("Overlay font: {} {} px (line height {}), own rasterizer", paths::ToUtf8(st.settings.fontFile.filename()), em,
                  source->LineHeight());
    } else {
        log::Error("Overlay font: no readable font file from the font chain - no status line");
    }
    st.glyphs = std::make_unique<GlyphCache>(std::move(source));
    st.glyphsForHeight = frameHeight;
    return st.glyphs->LineHeight() > 0 ? st.glyphs.get() : nullptr;
}

// The banner in the game's look; false if it cannot be drawn (the plain line is used then).
bool BannerReady(State& st) {
    if (!st.settings.gameLook) return false;
    if (!st.bannerTried) {
        st.bannerTried = true;
        st.banner = RenderBanner(paths::ExePath().parent_path() / L"fonts",
                                 DefaultBanner(Widen(DDMOD_NAME), L"v" + Widen(DDMOD_VERSION), L"active"));
        const text::Image& img = st.banner.image;
        st.bannerMask.resize(img.alpha.size());
        for (std::size_t i = 0; i < img.alpha.size(); ++i) {
            st.bannerMask[i] = static_cast<std::uint8_t>(std::clamp(img.alpha[i], 0.0f, 1.0f) * 255.0f + 0.5f);
        }
        st.bannerColors = img.color;
        if (img.width > 0) {
            log::Info("Overlay: banner in the game's look ({}x{} px)", img.width, img.height);
        } else {
            log::Warn("Overlay: banner in the game's look not available - plain status line");
        }
    }
    return st.banner.image.width > 0;
}

void Draw(State& st, const SurfaceView& view, bool showStatus, bool showFps, const std::wstring& notice) {
    int top = 0;  // the plain panel starts below the banner
    if (showStatus && BannerReady(st)) {
        const text::Image& img = st.banner.image;
        const int y = kBannerBaseline - st.banner.baseline;
        DrawColorMask(view, kBannerX - st.banner.left, y, st.bannerMask.data(), st.bannerColors.data(), img.width,
                      img.height, img.width, 255);
        top = y + img.height;
        showStatus = false;
    }
    const bool showNotice = !notice.empty();
    if (!showStatus && !showFps && !showNotice) return;
    GlyphCache* glyphs = GlyphsFor(st, view.height);
    if (!glyphs) return;
    const std::wstring status = showStatus ? st.status : std::wstring();
    const std::wstring fps = showFps ? std::format(L"{} FPS", st.fps) : std::wstring();
    const int line = glyphs->LineHeight();
    const int pad = std::max(2, line / 4);
    const int margin = pad * 2;
    const int lines = (showStatus ? 1 : 0) + (showFps ? 1 : 0) + (showNotice ? 1 : 0);
    const int width = std::max({MeasureString(*glyphs, status), MeasureString(*glyphs, fps), MeasureString(*glyphs, notice)}) +
                      2 * pad;
    FillRect(view, margin, top + margin, width, lines * line + 2 * pad, Rgb{0, 0, 0}, 140);
    int y = top + margin + pad;
    if (showStatus) {
        DrawString(view, *glyphs, margin + pad, y, status, Rgb{255, 255, 255}, true);
        y += line;
    }
    if (showFps) {
        DrawString(view, *glyphs, margin + pad, y, fps, Rgb{255, 214, 96}, true);
        y += line;
    }
    if (showNotice) DrawString(view, *glyphs, margin + pad, y, notice, Rgb{255, 255, 255}, true);
}

int __cdecl EndFrameDetour() {
    State& st = g_state;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (st.firstFrame.QuadPart == 0) {
        st.firstFrame = now;
        log::Info("Overlay: first frame");
    }
    CountFrame(st, now);
    const double elapsed = static_cast<double>(now.QuadPart - st.firstFrame.QuadPart) / st.frequency.QuadPart;
    const bool showStatus = StatusShown(st.settings.status, elapsed, st.settings.bannerSeconds);
    const bool showFps = g_showFps.load(std::memory_order_relaxed);
    const std::wstring notice = CurrentNotice();
    // Nothing to show: the frame is not even locked.
    SurfaceView view;
    if ((showStatus || showFps || !notice.empty()) && st.backend->BeginFrame(view)) {
        Draw(st, view, showStatus, showFps, notice);
        st.backend->EndFrame();
    }
    return g_originalEndFrame();
}

// The renderer plugin div.exe loaded (slashN.dll / xslashN.dll, docs/re/renderer-d3d6.md).
HMODULE FindRenderer(std::wstring& name) {
    for (const wchar_t* prefix : {L"slash", L"xslash"}) {
        for (int id = 1; id <= 4; ++id) {
            name = std::format(L"{}{}.dll", prefix, id);
            if (const HMODULE module = GetModuleHandleW(name.c_str())) return module;
        }
    }
    name.clear();
    return nullptr;
}

}  // namespace

void ToggleFps() {
    const bool on = !g_showFps.load();
    g_showFps.store(on);
    log::Info("Overlay: FPS display {}", on ? "on" : "off");
}

void ShowNotice(std::wstring text, int seconds) {
    std::lock_guard lock(g_noticeMutex);
    g_notice = std::move(text);
    g_noticeUntil = GetTickCount64() + static_cast<ULONGLONG>(std::max(seconds, 0)) * 1000;
}

bool ParseStatusMode(std::string_view text, StatusMode& mode) {
    const auto is = [text](std::string_view word) {
        return std::ranges::equal(text, word, [](char a, char b) { return (a | 0x20) == b; });
    };
    if (is("banner")) mode = StatusMode::Banner;
    else if (is("always")) mode = StatusMode::Always;
    else if (is("off")) mode = StatusMode::Off;
    else return false;
    return true;
}

bool ParseBannerLook(std::string_view text, bool& gameLook) {
    const auto is = [text](std::string_view word) {
        return std::ranges::equal(text, word, [](char a, char b) { return (a | 0x20) == b; });
    };
    if (is("game")) gameLook = true;
    else if (is("plain")) gameLook = false;
    else return false;
    return true;
}

bool StatusShown(StatusMode mode, double elapsedSeconds, int bannerSeconds) {
    switch (mode) {
        case StatusMode::Always: return true;
        case StatusMode::Banner: return elapsedSeconds < bannerSeconds;
        case StatusMode::Off: break;
    }
    return false;
}

bool Install() {
    State& st = g_state;
    st.settings = ReadSettings();
    g_showFps.store(config::GetBool("Overlay", "ShowFps", false));
    st.status = Widen(std::format("{} v{} active", DDMOD_NAME, DDMOD_VERSION));
    QueryPerformanceFrequency(&st.frequency);

    std::wstring name;
    const HMODULE renderer = FindRenderer(name);
    if (!renderer) {
        log::Warn("Overlay: no renderer plugin (slash*.dll) loaded - overlay disabled");
        return false;
    }
    if (name != L"slash1.dll") {
        log::Warn("Overlay: renderer {} has no overlay backend yet (only slash1.dll, Direct3D) - overlay disabled",
                  paths::ToUtf8(name));
        return false;
    }
    st.backend = CreateD3d6Backend(renderer);
    if (!st.backend) return false;

    auto* endFrame = reinterpret_cast<EndFrameFn>(GetProcAddress(renderer, "DllSlashedEndFrame"));
    if (!endFrame) {
        log::Error("Overlay: {} has no DllSlashedEndFrame export", paths::ToUtf8(name));
        return false;
    }
    if (!hooks::Create("DllSlashedEndFrame", endFrame, &EndFrameDetour, &g_originalEndFrame)) return false;
    if (!hooks::Enable("DllSlashedEndFrame")) return false;
    log::Info("Overlay: backend {}", st.backend->Name());
    static constexpr const char* kModes[] = {"banner", "always", "off"};
    log::Info("Overlay: status {}{} ({} look), FPS {}", kModes[static_cast<int>(st.settings.status)],
              st.settings.status == StatusMode::Banner ? std::format(" ({} s)", st.settings.bannerSeconds) : "",
              st.settings.gameLook ? "game" : "plain", g_showFps.load() ? "on" : "off");
    return true;
}

}  // namespace ddmod::overlay
