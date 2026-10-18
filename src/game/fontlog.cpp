#include "game/fontlog.h"

#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <map>
#include <mutex>
#include <string>

#include <windows.h>

#include "core/hooks.h"
#include "core/log.h"

namespace ddmod::game {
namespace {

using GetGlyphOutlineAFn = DWORD(WINAPI*)(HDC, UINT, UINT, LPGLYPHMETRICS, DWORD, LPVOID, const MAT2*);
using CreateFileAFn = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

GetGlyphOutlineAFn g_getGlyphOutlineA = nullptr;
CreateFileAFn g_createFileA = nullptr;

// Characters each GDI font rasterized, in order of first use, logged in batches. The game caches
// glyphs, so every character shows up once per font: enough to tell which texts a font draws.
std::mutex g_mutex;
std::map<HFONT, std::string> g_pending;
constexpr std::size_t kBatch = 40;

std::string AnsiToUtf8(char byte) {
    wchar_t wide = 0;
    if (MultiByteToWideChar(CP_ACP, 0, &byte, 1, &wide, 1) != 1) return "?";
    char out[8] = {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, &wide, 1, out, sizeof(out), nullptr, nullptr);
    return std::string(out, len > 0 ? static_cast<std::size_t>(len) : 0);
}

void Flush(HFONT font, std::string& chars) {
    if (chars.empty()) return;
    log::Debug("GDI glyphs of font {:08X}: \"{}\"", reinterpret_cast<std::uintptr_t>(font), chars);
    chars.clear();
}

DWORD WINAPI GetGlyphOutlineADetour(HDC dc, UINT ch, UINT format, LPGLYPHMETRICS metrics, DWORD size, LPVOID buffer,
                                    const MAT2* matrix) {
    // Only the calls that rasterize (the game first asks for the size with a null buffer).
    if (buffer && format != GGO_METRICS) {
        const auto font = static_cast<HFONT>(GetCurrentObject(dc, OBJ_FONT));
        std::lock_guard lock(g_mutex);
        // Flush other fonts' batches first, so the log keeps the drawing order.
        for (auto& [other, chars] : g_pending) {
            if (other != font) Flush(other, chars);
        }
        std::string& chars = g_pending[font];
        chars += AnsiToUtf8(static_cast<char>(ch & 0xFF));
        if (chars.size() >= kBatch) Flush(font, chars);
    }
    return g_getGlyphOutlineA(dc, ch, format, metrics, size, buffer, matrix);
}

bool EndsWithFnt(LPCSTR name) {
    if (!name) return false;
    const std::size_t len = std::strlen(name);
    return len >= 4 && _stricmp(name + len - 4, ".fnt") == 0;
}

HANDLE WINAPI CreateFileADetour(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                                DWORD disposition, DWORD flags, HANDLE templateFile) {
    if (EndsWithFnt(name)) log::Debug("Bitmap font opened: {}", name);
    return g_createFileA(name, access, share, security, disposition, flags, templateFile);
}

template <typename Fn>
bool HookExport(const wchar_t* module, const char* name, Fn* detour, Fn** original) {
    auto* target = reinterpret_cast<Fn*>(GetProcAddress(GetModuleHandleW(module), name));
    return hooks::Create(name, target, detour, original) && hooks::Enable(name);
}

}  // namespace

bool InstallFontLog() {
    const bool ok = HookExport(L"gdi32.dll", "GetGlyphOutlineA", &GetGlyphOutlineADetour, &g_getGlyphOutlineA) &&
                    HookExport(L"kernel32.dll", "CreateFileA", &CreateFileADetour, &g_createFileA);
    if (ok) log::Debug("Font log installed (GetGlyphOutlineA, CreateFileA for .fnt)");
    return ok;
}

}  // namespace ddmod::game
