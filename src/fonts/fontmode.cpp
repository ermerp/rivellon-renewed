#include "fonts/fontmode.h"

#include "core/hooks.h"
#include "core/log.h"
#include "core/scanner.h"
#include "game/exe.h"

namespace ddmod::fonts {
namespace {

using FontInitFn = int(__thiscall*)(void* self);

FontInitFn g_fontInit = nullptr;
std::uint8_t* const* g_settingsGlobal = nullptr;

// __fastcall with a dummy second parameter has the same register use and stack cleanup as a
// __thiscall function without stack arguments (this in ecx, edx unused).
int __fastcall FontInitDetour(void* self, void* /*edx*/) {
    std::uint8_t* settings = *g_settingsGlobal;
    if (!settings) {
        log::Warn("Font mode: settings object not created yet - use8bitfont left as configured");
        return g_fontInit(self);
    }
    std::uint8_t& use8BitFont = settings[kUse8BitFontField];
    const std::uint8_t configured = use8BitFont;
    use8BitFont = 0;
    log::Info("Font mode: use8bitfont forced to 0 (was {}) while the game creates its fonts", configured);
    const int result = g_fontInit(self);
    use8BitFont = configured;
    return result;
}

}  // namespace

bool InstallFontModeAt(void* fontInit, std::uint8_t* const* settingsGlobal) {
    g_settingsGlobal = settingsGlobal;
    return hooks::CreateRaw("FontInit", fontInit, reinterpret_cast<void*>(&FontInitDetour),
                            reinterpret_cast<void**>(&g_fontInit)) &&
           hooks::Enable("FontInit");
}

bool InstallFontMode() {
    const std::uintptr_t fontInit = game::Resolve({"FontInit", kFontInitPattern, 0, kFontInitRva});
    if (!fontInit) {
        log::Warn("Font mode: font setup not found - texts follow use8bitfont in config.lcl");
        return false;
    }
    const auto settingsGlobal = reinterpret_cast<std::uint8_t* const*>(
        static_cast<std::uintptr_t>(scanner::ReadU32(fontInit + kSettingsGlobalOperand)));
    if (!InstallFontModeAt(reinterpret_cast<void*>(fontInit), settingsGlobal)) return false;
    log::Info("Font mode hook installed: game texts use GDI fonts (settings pointer at 0x{:08X})",
              reinterpret_cast<std::uintptr_t>(settingsGlobal));
    return true;
}

}  // namespace ddmod::fonts
