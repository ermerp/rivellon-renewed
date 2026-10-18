// Entry point of ddmod.asi. Loaded into div.exe by Ultimate ASI Loader (installed as winmm.dll).

#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

#include "core/config.h"
#include "core/log.h"
#include "core/paths.h"

namespace {

std::string Narrow(const std::wstring& text) {
    if (text.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), len, nullptr, nullptr);
    return out;
}

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

void Startup(HMODULE self) {
    using namespace ddmod;
    paths::Init(self);
    config::Load(paths::ModDir() / L"ddmod.ini");

    const auto level = log::ParseLevel(config::GetString("General", "LogLevel", "info"), log::Level::Info);
    log::Open(paths::ModDir() / L"ddmod.log", level);

    log::Info("ddmod {} loaded", DDMOD_VERSION);
    log::Info("Host: {} (file version {})", Narrow(paths::ExePath().wstring()), ExeFileVersion());
    log::Info("Platform: {}", RunningUnderWine() ? "Wine/Proton" : "Windows");
    if (config::Exists()) {
        log::Info("Config: {}", Narrow(config::File().wstring()));
    } else {
        log::Warn("Config not found ({}), using defaults", Narrow(config::File().wstring()));
    }
}

void Shutdown() {
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
