#include "input/input.h"

#include <atomic>
#include <string>
#include <vector>

#include "core/config.h"
#include "core/log.h"

namespace ddmod::input {

namespace {

struct Hotkey {
    std::string name;
    KeyCombo combo;
    HotkeyHandler handler;
};

// Filled before Install and read only on the window thread afterwards.
std::vector<Hotkey> g_hotkeys;

std::atomic<HWND> g_window{nullptr};
std::atomic<WNDPROC> g_original{nullptr};
void (*g_onDestroyed)() = nullptr;

// Window-thread state: the key whose messages we are currently swallowing.
std::uint8_t g_swallowVk = 0;
UINT g_swallowScan = 0;

UINT ScanCodeOf(LPARAM lParam) { return (static_cast<UINT>(lParam) >> 16) & 0x1FF; }  // incl. extended bit
bool IsRepeat(LPARAM lParam) { return (static_cast<UINT>(lParam) & (1u << 30)) != 0; }

std::uint8_t CurrentModifiers() {
    // GetKeyState reflects the keyboard state at the time of the message being processed.
    std::uint8_t mods = kNone;
    if (GetKeyState(VK_CONTROL) < 0) mods |= kCtrl;
    if (GetKeyState(VK_SHIFT) < 0) mods |= kShift;
    if (GetKeyState(VK_MENU) < 0) mods |= kAlt;
    return mods;
}

// Returns true if the message belongs to one of our hotkeys and must not reach the game.
bool HandleKey(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            const auto vk = static_cast<std::uint8_t>(wParam);
            if (vk == g_swallowVk && IsRepeat(lParam)) return true;
            const std::uint8_t mods = CurrentModifiers();
            for (const auto& hotkey : g_hotkeys) {
                if (!Matches(hotkey.combo, vk, mods)) continue;
                g_swallowVk = vk;
                g_swallowScan = ScanCodeOf(lParam);
                if (IsRepeat(lParam)) return true;  // held down while we were installed
                log::Debug("Hotkey {} ({})", hotkey.name, ToString(hotkey.combo));
                hotkey.handler();
                return true;
            }
            return false;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (g_swallowVk == 0 || static_cast<std::uint8_t>(wParam) != g_swallowVk) return false;
            g_swallowVk = 0;
            return true;
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_DEADCHAR:
        case WM_SYSDEADCHAR:
            // TranslateMessage already turned the swallowed key-down into a character message
            // before the window procedure saw it; drop that character too.
            return g_swallowVk != 0 && ScanCodeOf(lParam) == g_swallowScan;
        case WM_KILLFOCUS:
            g_swallowVk = 0;  // the key-up may never arrive
            return false;
        default:
            return false;
    }
}

LRESULT CALLBACK SubclassProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
    const WNDPROC original = g_original.load();
    if (HandleKey(msg, wParam, lParam)) return 0;

    if (msg == WM_NCDESTROY) {
        const LRESULT result = CallWindowProcA(original, window, msg, wParam, lParam);
        g_window = nullptr;
        g_original = nullptr;
        g_swallowVk = 0;
        log::Info("Game window destroyed, input hook removed");
        if (g_onDestroyed) g_onDestroyed();
        return result;
    }
    return CallWindowProcA(original, window, msg, wParam, lParam);
}

}  // namespace

void RegisterHotkey(const char* iniKey, const char* fallback, HotkeyHandler handler) {
    const std::string text = config::GetString("Keys", iniKey, fallback);
    const auto combo = ParseKeyCombo(text);
    if (!combo) {
        log::Warn("[Keys] {}=\"{}\" is not a valid key combination, hotkey disabled", iniKey, text);
        return;
    }
    if (!combo->Bound()) {
        log::Info("Hotkey {}: not bound", iniKey);
        return;
    }
    for (const auto& other : g_hotkeys) {
        if (other.combo == *combo) {
            log::Warn("Hotkey {}: {} is already used by {}, hotkey disabled", iniKey, ToString(*combo), other.name);
            return;
        }
    }
    g_hotkeys.push_back({iniKey, *combo, handler});
    log::Info("Hotkey {}: {}", iniKey, ToString(*combo));
}

bool Install(HWND window, void (*onDestroyed)()) {
    // The game window is an ANSI window (RegisterClassExA); the A functions keep it that way,
    // so the game keeps receiving ANSI character messages.
    g_onDestroyed = onDestroyed;
    g_original = reinterpret_cast<WNDPROC>(GetWindowLongPtrA(window, GWLP_WNDPROC));
    g_window = window;
    SetLastError(0);
    const auto previous = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&SubclassProc)));
    if (!previous && GetLastError() != 0) {
        log::Error("Input hook: subclassing the game window failed (error {})", GetLastError());
        g_window = nullptr;
        g_original = nullptr;
        return false;
    }
    // Normally identical; differs only if someone changed the procedure in between.
    g_original = previous;
    log::Info("Input hook installed ({} hotkey(s))", g_hotkeys.size());
    return true;
}

void Uninstall() {
    const HWND window = g_window.exchange(nullptr);
    if (!window) return;
    const auto current = reinterpret_cast<WNDPROC>(GetWindowLongPtrA(window, GWLP_WNDPROC));
    if (current != &SubclassProc) {
        log::Warn("Input hook: window procedure was subclassed after us, leaving it in place");
        return;
    }
    SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_original.load()));
    log::Info("Input hook removed");
}

}  // namespace ddmod::input
