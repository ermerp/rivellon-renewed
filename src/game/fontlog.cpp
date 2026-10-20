#include "game/fontlog.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

#include "core/hooks.h"
#include "core/log.h"

namespace ddmod::game {
namespace {

using CreateFileAFn = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

CreateFileAFn g_createFileA = nullptr;
std::atomic<bool> g_enabled{false};

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

}  // namespace

void LogGlyph(HDC dc, UINT ch) {
    if (!g_enabled) return;
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

bool InstallFontLog() {
    auto* target = reinterpret_cast<CreateFileAFn>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "CreateFileA"));
    const bool ok = hooks::Create("CreateFileA", target, &CreateFileADetour, &g_createFileA) &&
                    hooks::Enable("CreateFileA");
    g_enabled = true;
    log::Debug("Font log installed (GDI glyphs via the font hook{})", ok ? ", CreateFileA for .fnt" : "");
    return ok;
}

}  // namespace ddmod::game
