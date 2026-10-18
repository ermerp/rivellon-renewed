#include "game/window.h"

#include <format>
#include <string>

#include "core/log.h"

namespace ddmod::game {

namespace {

constexpr DWORD kPollIntervalMs = 50;

std::string ClassNameOf(HWND window) {
    char name[128] = {};
    const int len = GetClassNameA(window, name, static_cast<int>(sizeof(name)));
    return std::string(name, len > 0 ? static_cast<std::size_t>(len) : 0);
}

std::string TitleOf(HWND window) {
    char title[128] = {};
    const int len = GetWindowTextA(window, title, static_cast<int>(sizeof(title)));
    return std::string(title, len > 0 ? static_cast<std::size_t>(len) : 0);
}

// All four renderer plugins (slash1-4.dll) register the window class "Divinity". Matching the
// class keeps message boxes ("not correctly terminated", class #32770), Bink's and the IME
// windows out.
constexpr const char* kGameWindowClass = "Divinity";

bool IsGameWindow(HWND window) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid != GetCurrentProcessId()) return false;
    return GetWindow(window, GW_OWNER) == nullptr && ClassNameOf(window) == kGameWindowClass;
}

BOOL CALLBACK EnumProc(HWND window, LPARAM param) {
    if (!IsGameWindow(window)) return TRUE;
    *reinterpret_cast<HWND*>(param) = window;
    return FALSE;
}

// Name of the module that contains `address` ("?" if none).
std::string ModuleOf(const void* address) {
    HMODULE module = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<LPCSTR>(address), &module)) {
        return "?";
    }
    char path[MAX_PATH] = {};
    GetModuleFileNameA(module, path, MAX_PATH);
    const std::string full(path);
    return full.substr(full.find_last_of('\\') + 1);
}

BOOL CALLBACK TraceProc(HWND window, LPARAM param) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (pid != GetCurrentProcessId()) return TRUE;
    RECT rect{};
    GetWindowRect(window, &rect);
    const auto proc = reinterpret_cast<const void*>(GetWindowLongPtrA(window, GWLP_WNDPROC));
    *reinterpret_cast<std::string*>(param) += std::format(
        "\n  hwnd {:08X} class \"{}\" visible {} rect {},{} {}x{} style {:08X} wndproc {:08X} ({})",
        reinterpret_cast<std::uintptr_t>(window), ClassNameOf(window), IsWindowVisible(window) ? 1 : 0, rect.left,
        rect.top, rect.right - rect.left, rect.bottom - rect.top, static_cast<std::uint32_t>(GetWindowLongA(window, GWL_STYLE)),
        reinterpret_cast<std::uintptr_t>(proc), ModuleOf(proc));
    return TRUE;
}

// Diagnostics (LogLevel=debug): logs all top-level windows of the process with their window
// procedure whenever that list changes, for `durationMs`.
void TraceWindows(DWORD durationMs) {
    const DWORD start = GetTickCount();
    std::string last;
    while (GetTickCount() - start < durationMs) {
        std::string now;
        EnumWindows(&TraceProc, reinterpret_cast<LPARAM>(&now));
        if (now != last) {
            log::Debug("Windows of this process at +{} ms:{}", GetTickCount() - start, now);
            last = std::move(now);
        }
        Sleep(500);
    }
    log::Debug("Window trace finished");
}

DWORD WINAPI WatchThread(LPVOID param) {
    const auto onFound = reinterpret_cast<WindowCallback>(param);
    const DWORD start = GetTickCount();
    HWND window = nullptr;
    while ((window = FindMainWindow()) == nullptr) Sleep(kPollIntervalMs);

    log::Info("Game window found after {} ms: hwnd {:08X}, title \"{}\", thread {}", GetTickCount() - start,
              reinterpret_cast<std::uintptr_t>(window), TitleOf(window), GetWindowThreadProcessId(window, nullptr));
    onFound(window);
    if (log::Enabled(log::Level::Debug)) TraceWindows(60'000);
    // Keep the thread: hooks it installed would be removed when it exits.
    for (;;) Sleep(INFINITE);
}

}  // namespace

HWND FindMainWindow() {
    HWND found = nullptr;
    EnumWindows(&EnumProc, reinterpret_cast<LPARAM>(&found));
    return found;
}

bool WatchForMainWindow(WindowCallback onFound) {
    const HANDLE thread = CreateThread(nullptr, 0, &WatchThread, reinterpret_cast<LPVOID>(onFound), 0, nullptr);
    if (!thread) {
        log::Error("Could not start the window watcher thread (error {})", GetLastError());
        return false;
    }
    CloseHandle(thread);
    return true;
}

}  // namespace ddmod::game
