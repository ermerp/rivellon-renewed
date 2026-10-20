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

// Filled before Install and read only on the game thread afterwards.
std::vector<Hotkey> g_hotkeys;

std::atomic<HHOOK> g_hook{nullptr};

// Game-thread state: the key whose messages we are currently swallowing.
std::uint8_t g_swallowVk = 0;

bool IsRepeat(LPARAM lParam) { return (static_cast<UINT>(lParam) & (1u << 30)) != 0; }

std::uint8_t CurrentModifiers() {
    // GetKeyState reflects the keyboard state up to the message just retrieved.
    std::uint8_t mods = kNone;
    if (GetKeyState(VK_CONTROL) < 0) mods |= kCtrl;
    if (GetKeyState(VK_SHIFT) < 0) mods |= kShift;
    if (GetKeyState(VK_MENU) < 0) mods |= kAlt;
    return mods;
}

// Returns true if the message belongs to one of our hotkeys and must not reach the game.
bool HandleKey(const MSG& msg) {
    switch (msg.message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {  // F10 and Alt combinations arrive as WM_SYSKEYDOWN
            const auto vk = static_cast<std::uint8_t>(msg.wParam);
            if (vk == g_swallowVk && IsRepeat(msg.lParam)) return true;
            const std::uint8_t mods = CurrentModifiers();
            if (!IsRepeat(msg.lParam)) {
                log::Debug("Key down {} (vk {:02X}, hwnd {:08X})", ToString(KeyCombo{vk, mods}), vk,
                           reinterpret_cast<std::uintptr_t>(msg.hwnd));
            }
            for (const auto& hotkey : g_hotkeys) {
                if (!Matches(hotkey.combo, vk, mods)) continue;
                g_swallowVk = vk;
                if (IsRepeat(msg.lParam)) return true;  // held down while we were installed
                log::Debug("Hotkey {} ({})", hotkey.name, ToString(hotkey.combo));
                hotkey.handler();
                return true;
            }
            return false;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (g_swallowVk == 0 || static_cast<std::uint8_t>(msg.wParam) != g_swallowVk) return false;
            g_swallowVk = 0;
            return true;
        case WM_CHAR:
            // Character messages decide what text fields insert; repeats are logged too, they are
            // what a held key produces.
            log::Debug("Char {:02X}{} (hwnd {:08X})", static_cast<unsigned>(msg.wParam),
                       IsRepeat(msg.lParam) ? " repeat" : "", reinterpret_cast<std::uintptr_t>(msg.hwnd));
            return false;
        default:
            return false;
    }
}

LRESULT CALLBACK GetMessageHook(int code, WPARAM removal, LPARAM lParam) {
    // Only act when the message is taken off the queue; a PM_NOREMOVE peek of the same message
    // comes back later with PM_REMOVE.
    if (code == HC_ACTION && removal == PM_REMOVE) {
        auto& msg = *reinterpret_cast<MSG*>(lParam);
        if (HandleKey(msg)) msg.message = WM_NULL;
    }
    return CallNextHookEx(nullptr, code, removal, lParam);
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

bool Install(HWND window) {
    if (g_hook) return true;
    const DWORD thread = GetWindowThreadProcessId(window, nullptr);
    // hMod = nullptr: thread-specific hook on a thread of our own process.
    const HHOOK hook = SetWindowsHookExA(WH_GETMESSAGE, &GetMessageHook, nullptr, thread);
    if (!hook) {
        log::Error("Input hook: SetWindowsHookEx(WH_GETMESSAGE) on thread {} failed (error {})", thread,
                   GetLastError());
        return false;
    }
    g_hook = hook;
    log::Info("Input hook installed on thread {} ({} hotkey(s))", thread, g_hotkeys.size());
    return true;
}

void Uninstall() {
    const HHOOK hook = g_hook.exchange(nullptr);
    if (!hook) return;
    UnhookWindowsHookEx(hook);
    log::Info("Input hook removed");
}

}  // namespace ddmod::input
