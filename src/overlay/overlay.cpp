#include "overlay/overlay.h"

#include <algorithm>
#include <format>
#include <memory>
#include <string>

#include <windows.h>

#include "core/config.h"
#include "core/hooks.h"
#include "core/log.h"
#include "core/paths.h"
#include "fonts/fontchain.h"
#include "overlay/backend.h"
#include "overlay/glyphs.h"

namespace ddmod::overlay {
namespace {

using EndFrameFn = int(__cdecl*)();

struct Settings {
    bool showFps = true;
    std::wstring fontFace;  // from the font chain ([Fonts] Chain)
    int fontSize = 0;  // 0 = from the frame height
};

struct State {
    Settings settings;
    std::unique_ptr<Backend> backend;
    std::unique_ptr<GlyphCache> glyphs;
    int glyphsForHeight = 0;
    std::wstring status;
    // FPS, measured over half-second windows.
    LARGE_INTEGER frequency{};
    LARGE_INTEGER windowStart{};
    int framesInWindow = 0;
    int fps = 0;
};

State g_state;
EndFrameFn g_originalEndFrame = nullptr;

std::wstring Widen(const std::string& text) {
    if (text.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), len);
    return wide;
}

Settings ReadSettings() {
    Settings s;
    s.showFps = config::GetBool("Overlay", "ShowFps", true);
    const auto& font = fonts::Configured();
    s.fontFace = font ? font->face : L"Tahoma";  // nothing available: let GDI substitute
    s.fontSize = std::clamp(config::GetInt("Overlay", "FontSize", 0), 0, 96);
    return s;
}

void CountFrame(State& st) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
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
    FontSpec spec;
    spec.face = st.settings.fontFace;
    spec.pixelHeight = st.settings.fontSize > 0 ? st.settings.fontSize : std::clamp(frameHeight / 60, 12, 48);
    st.glyphs = std::make_unique<GlyphCache>(CreateGdiGlyphSource(spec));
    st.glyphsForHeight = frameHeight;
    return st.glyphs->LineHeight() > 0 ? st.glyphs.get() : nullptr;
}

void Draw(State& st, const SurfaceView& view) {
    GlyphCache* glyphs = GlyphsFor(st, view.height);
    if (!glyphs) return;
    const std::wstring fps = st.settings.showFps ? std::format(L"{} FPS", st.fps) : std::wstring();
    const int line = glyphs->LineHeight();
    const int pad = std::max(2, line / 4);
    const int margin = pad * 2;
    const int lines = fps.empty() ? 1 : 2;
    const int width = std::max(MeasureString(*glyphs, st.status), MeasureString(*glyphs, fps)) + 2 * pad;
    FillRect(view, margin, margin, width, lines * line + 2 * pad, Rgb{0, 0, 0}, 140);
    DrawString(view, *glyphs, margin + pad, margin + pad, st.status, Rgb{255, 255, 255}, true);
    if (!fps.empty()) DrawString(view, *glyphs, margin + pad, margin + pad + line, fps, Rgb{255, 214, 96}, true);
}

int __cdecl EndFrameDetour() {
    State& st = g_state;
    CountFrame(st);
    SurfaceView view;
    if (st.backend->BeginFrame(view)) {
        Draw(st, view);
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

bool Install() {
    State& st = g_state;
    st.settings = ReadSettings();
    st.status = Widen(std::format("ddmod {} active", DDMOD_VERSION));
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
    return true;
}

}  // namespace ddmod::overlay
