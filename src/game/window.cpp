#include "game/window.h"

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

DWORD WINAPI WatchThread(LPVOID param) {
    const auto onFound = reinterpret_cast<WindowCallback>(param);
    const DWORD start = GetTickCount();
    HWND window = nullptr;
    while ((window = FindMainWindow()) == nullptr) Sleep(kPollIntervalMs);

    log::Info("Game window found after {} ms: title \"{}\", thread {}", GetTickCount() - start, TitleOf(window),
              GetWindowThreadProcessId(window, nullptr));
    onFound(window);
    return 0;
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
