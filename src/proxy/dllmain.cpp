// Entry point of ddmod.asi. Loaded into div.exe by Ultimate ASI Loader (installed as winmm.dll).

#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

#include "core/config.h"
#include "core/hooks.h"
#include "core/log.h"
#include "core/paths.h"
#include "core/scanner.h"
#include "game/exe.h"
#include "game/window.h"
#include "input/input.h"
#include "overlay/overlay.h"

namespace {

// "1.0.0.62" from the version resource of the host executable, or "unknown".
std::string ExeFileVersion() {
    const std::wstring exe = ddmod::paths::ExePath().wstring();
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(exe.c_str(), &ignored);
    if (size == 0) return "unknown";
    std::vector<std::uint8_t> data(size);
    if (!GetFileVersionInfoW(exe.c_str(), 0, size, data.data())) return "unknown";
    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoLen = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &infoLen) || !info) return "unknown";
    return std::format("{}.{}.{}.{}", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                       HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
}

bool RunningUnderWine() {
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

void OnTestHotkey() { ddmod::log::Info("Test hotkey pressed"); }

// Deferred start, on the watcher thread once the game window exists. Nothing here may run
// from DllMain (docs/re/input.md): hooks are only installed when the game is up.
void OnGameWindow(HWND window) {
    using namespace ddmod;
    if (hooks::Init()) log::Info("Hook manager ready (MinHook)");
    if (config::GetBool("Features", "InputHook", true)) {
        input::RegisterHotkey("TestHotkey", "Ctrl+Shift+F3", &OnTestHotkey);
        input::Install(window);
    } else {
        log::Info("Input hook disabled ([Features] InputHook=0)");
    }
    if (config::GetBool("Features", "Overlay", true)) {
        overlay::Install();
    } else {
        log::Info("Overlay disabled ([Features] Overlay=0)");
    }
}

void Startup(HMODULE self) {
    using namespace ddmod;
    paths::Init(self);
    config::Load(paths::ModDir() / L"ddmod.ini");

    const auto level = log::ParseLevel(config::GetString("General", "LogLevel", "info"), log::Level::Info);
    log::Open(paths::ModDir() / L"ddmod.log", level);

    log::Info("ddmod {} loaded", DDMOD_VERSION);
    log::Info("Host: {} (file version {})", paths::ToUtf8(paths::ExePath()), ExeFileVersion());
    log::Info("Platform: {}", RunningUnderWine() ? "Wine/Proton" : "Windows");
    if (config::Exists()) {
        log::Info("Config: {}", paths::ToUtf8(config::File()));
    } else {
        log::Warn("Config not found ({}), using defaults", paths::ToUtf8(config::File()));
    }

    game::IdentifyExe(paths::ExePath());

    std::size_t codeBytes = 0;
    int codeSections = 0;
    for (const auto& section : scanner::Sections(GetModuleHandleW(nullptr))) {
        if (!section.executable) continue;
        codeBytes += section.bytes.size();
        ++codeSections;
    }
    log::Info("Scanner: {} code bytes in {} executable section(s) of div.exe", codeBytes, codeSections);

    if (game::WatchForMainWindow(&OnGameWindow)) log::Info("Waiting for the game window");
}

void Shutdown() {
    ddmod::input::Uninstall();
    ddmod::hooks::Shutdown();
    ddmod::log::Info("ddmod unloading");
    ddmod::log::Close();
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(instance);
            Startup(instance);
            break;
        case DLL_PROCESS_DETACH:
            // reserved != nullptr: process is terminating, other threads are already gone.
            if (reserved == nullptr) Shutdown();
            else ddmod::log::Close();
            break;
        default:
            break;
    }
    return TRUE;
}
