#include "game/fontlog.h"

#include <cstdint>
#include <intrin.h>

#include <windows.h>

#include "core/hooks.h"
#include "core/log.h"

namespace ddmod::game {
namespace {

using CreateFontAFn = HFONT(WINAPI*)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                     LPCSTR);
using CreateFontIndirectAFn = HFONT(WINAPI*)(const LOGFONTA*);

CreateFontAFn g_createFontA = nullptr;
CreateFontIndirectAFn g_createFontIndirectA = nullptr;

HFONT WINAPI CreateFontADetour(int height, int width, int escapement, int orientation, int weight, DWORD italic,
                               DWORD underline, DWORD strikeOut, DWORD charSet, DWORD outPrecision,
                               DWORD clipPrecision, DWORD quality, DWORD pitchAndFamily, LPCSTR face) {
    log::Debug("CreateFontA from 0x{:08X}: face \"{}\" height {} weight {} italic {} charset {} quality {}",
               reinterpret_cast<std::uintptr_t>(_ReturnAddress()), face ? face : "", height, weight, italic, charSet,
               quality);
    return g_createFontA(height, width, escapement, orientation, weight, italic, underline, strikeOut, charSet,
                         outPrecision, clipPrecision, quality, pitchAndFamily, face);
}

HFONT WINAPI CreateFontIndirectADetour(const LOGFONTA* lf) {
    if (lf) {
        log::Debug("CreateFontIndirectA from 0x{:08X}: face \"{}\" height {} weight {} italic {} charset {} quality {}",
                   reinterpret_cast<std::uintptr_t>(_ReturnAddress()), lf->lfFaceName, lf->lfHeight, lf->lfWeight,
                   lf->lfItalic, lf->lfCharSet, lf->lfQuality);
    }
    return g_createFontIndirectA(lf);
}

}  // namespace

bool InstallFontLog() {
    const HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    if (!gdi) return false;
    auto* createFontA = reinterpret_cast<CreateFontAFn>(GetProcAddress(gdi, "CreateFontA"));
    auto* createFontIndirectA = reinterpret_cast<CreateFontIndirectAFn>(GetProcAddress(gdi, "CreateFontIndirectA"));
    bool ok = hooks::Create("CreateFontA", createFontA, &CreateFontADetour, &g_createFontA) &&
              hooks::Create("CreateFontIndirectA", createFontIndirectA, &CreateFontIndirectADetour,
                            &g_createFontIndirectA);
    ok = ok && hooks::Enable("CreateFontA") && hooks::Enable("CreateFontIndirectA");
    if (ok) log::Debug("Font log installed (CreateFontA, CreateFontIndirectA)");
    return ok;
}

}  // namespace ddmod::game
