#include "fonts/fonthook.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <string>

#include <windows.h>

#include "core/config.h"
#include "core/hooks.h"
#include "core/log.h"
#include "fonts/fontchain.h"

namespace ddmod::fonts {
namespace {

using CreateFontAFn = HFONT(WINAPI*)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                     LPCSTR);
using CreateFontIndirectAFn = HFONT(WINAPI*)(const LOGFONTA*);

CreateFontAFn g_createFontA = nullptr;
CreateFontIndirectAFn g_createFontIndirectA = nullptr;
bool g_replace = false;
std::uintptr_t g_exeBegin = 0, g_exeEnd = 0;

bool FromGame(const void* caller) {
    const auto at = reinterpret_cast<std::uintptr_t>(caller);
    return at >= g_exeBegin && at < g_exeEnd;
}

// The face to use instead of the game's, as an ANSI string (empty = keep the game's face).
const std::string& ReplacementFace() {
    static const std::string face = [] {
        const auto& chosen = Configured();
        if (!chosen) return std::string();
        char ansi[LF_FACESIZE] = {};
        WideCharToMultiByte(CP_ACP, 0, chosen->face.c_str(), -1, ansi, LF_FACESIZE - 1, nullptr, nullptr);
        return std::string(ansi);
    }();
    return face;
}

int ScaledHeight(int height) {
    static const int percent = std::clamp(config::GetInt("Fonts", "GameFontSize", 100), 50, 300);
    return height * percent / 100;
}

HFONT WINAPI CreateFontIndirectADetour(const LOGFONTA* lf) {
    void* caller = _ReturnAddress();
    if (!lf || !g_replace || !FromGame(caller) || ReplacementFace().empty()) {
        const HFONT font = g_createFontIndirectA(lf);
        if (lf) {
            log::Debug("CreateFontIndirectA from 0x{:08X}: face \"{}\" height {} weight {} charset {} -> font {:08X}",
                       reinterpret_cast<std::uintptr_t>(caller), lf->lfFaceName, lf->lfHeight, lf->lfWeight,
                       lf->lfCharSet, reinterpret_cast<std::uintptr_t>(font));
        }
        return font;
    }
    LOGFONTA changed = *lf;
    strncpy_s(changed.lfFaceName, ReplacementFace().c_str(), _TRUNCATE);
    changed.lfHeight = ScaledHeight(lf->lfHeight);
    const HFONT font = g_createFontIndirectA(&changed);
    log::Debug("CreateFontIndirectA from 0x{:08X}: face \"{}\" height {} weight {} -> \"{}\" height {} -> font {:08X}",
               reinterpret_cast<std::uintptr_t>(caller), lf->lfFaceName, lf->lfHeight, lf->lfWeight,
               changed.lfFaceName, changed.lfHeight, reinterpret_cast<std::uintptr_t>(font));
    return font;
}

HFONT WINAPI CreateFontADetour(int height, int width, int escapement, int orientation, int weight, DWORD italic,
                               DWORD underline, DWORD strikeOut, DWORD charSet, DWORD outPrecision,
                               DWORD clipPrecision, DWORD quality, DWORD pitchAndFamily, LPCSTR face) {
    void* caller = _ReturnAddress();
    const bool replace = g_replace && FromGame(caller) && !ReplacementFace().empty();
    const LPCSTR useFace = replace ? ReplacementFace().c_str() : face;
    const int useHeight = replace ? ScaledHeight(height) : height;
    const HFONT font = g_createFontA(useHeight, width, escapement, orientation, weight, italic, underline, strikeOut,
                                     charSet, outPrecision, clipPrecision, quality, pitchAndFamily, useFace);
    log::Debug("CreateFontA from 0x{:08X}: face \"{}\" height {} weight {} -> \"{}\" height {} -> font {:08X}",
               reinterpret_cast<std::uintptr_t>(caller), face ? face : "", height, weight, useFace ? useFace : "",
               useHeight, reinterpret_cast<std::uintptr_t>(font));
    return font;
}

template <typename Fn>
bool HookGdi(const char* name, Fn* detour, Fn** original) {
    auto* target = reinterpret_cast<Fn*>(GetProcAddress(GetModuleHandleW(L"gdi32.dll"), name));
    return hooks::Create(name, target, detour, original) && hooks::Enable(name);
}

}  // namespace

bool Install(bool replace) {
    g_replace = replace;
    const auto exe = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(exe + reinterpret_cast<const IMAGE_DOS_HEADER*>(exe)->e_lfanew);
    g_exeBegin = reinterpret_cast<std::uintptr_t>(exe);
    g_exeEnd = g_exeBegin + nt->OptionalHeader.SizeOfImage;
    const bool ok = HookGdi("CreateFontIndirectA", &CreateFontIndirectADetour, &g_createFontIndirectA) &&
                    HookGdi("CreateFontA", &CreateFontADetour, &g_createFontA);
    if (ok && replace) log::Info("Font hook installed: game GDI fonts use the font chain ([Fonts], use8bitfont 0 only)");
    if (ok && !replace) log::Info("Font hook installed for logging only (FontHook=0)");
    return ok;
}

}  // namespace ddmod::fonts
