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
std::atomic<LeftButtonObserver> g_leftButtonObserver{nullptr};

// Game-thread state: the key whose messages we are currently swallowing.
std::uint8_t g_swallowVk = 0;

// [Features] BlockMenuKey, read in Install.
bool g_blockMenuKey = false;

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
        if (msg.message == WM_LBUTTONDOWN || msg.message == WM_LBUTTONDBLCLK) {
            if (const LeftButtonObserver observer = g_leftButtonObserver.load()) {
                observer(static_cast<unsigned>(msg.time), msg.pt.x, msg.pt.y);
            }
        }
        if (HandleKey(msg)) {
            msg.message = WM_NULL;
        } else if (g_blockMenuKey) {
            const auto vk = static_cast<std::uint8_t>(msg.wParam);
            if (const UINT plain = PlainMenuKeyMessage(msg.message, vk, GetKeyState(VK_MENU) < 0); plain != msg.message) {
                if (msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP) {
                    log::Debug("Menu key {} {}: passed on as a normal key", vk == VK_F10 ? "F10" : "Alt",
                               msg.message == WM_SYSKEYDOWN ? "down" : "up");
                }
                msg.message = plain;
            }
        }
    }
    return CallNextHookEx(nullptr, code, removal, lParam);
}

std::atomic<HHOOK> g_sentHook{nullptr};

// LogLevel=debug: sent messages that show menu mode (F10, Alt) and screen mode changes (Alt+Tab).
LRESULT CALLBACK CallWndProcHook(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION) {
        const auto& m = *reinterpret_cast<const CWPSTRUCT*>(lParam);
        const auto hwnd = reinterpret_cast<std::uintptr_t>(m.hwnd);
        switch (m.message) {
            case WM_SYSCOMMAND:
                log::Debug("Window message WM_SYSCOMMAND {:04X} (hwnd {:08X})", static_cast<unsigned>(m.wParam & 0xFFF0),
                           hwnd);
                break;
            case WM_ENTERMENULOOP:
                log::Debug("Window message WM_ENTERMENULOOP (hwnd {:08X})", hwnd);
                break;
            case WM_EXITMENULOOP:
                log::Debug("Window message WM_EXITMENULOOP (hwnd {:08X})", hwnd);
                break;
            case WM_ACTIVATEAPP:
                log::Debug("Window message WM_ACTIVATEAPP {} (hwnd {:08X})", m.wParam ? "on" : "off", hwnd);
                break;
            case WM_STYLECHANGED:
                if (m.wParam == static_cast<WPARAM>(GWL_STYLE)) {
                    const auto& s = *reinterpret_cast<const STYLESTRUCT*>(m.lParam);
                    log::Debug("Window message WM_STYLECHANGED {:08X} -> {:08X} (hwnd {:08X})", s.styleOld, s.styleNew,
                               hwnd);
                }
                break;
            default:
                break;
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
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

void SetLeftButtonObserver(LeftButtonObserver observer) { g_leftButtonObserver.store(observer); }

bool Install(HWND window) {
    if (g_hook) return true;
    const DWORD thread = GetWindowThreadProcessId(window, nullptr);
    g_blockMenuKey = config::GetBool("Features", "BlockMenuKey", true);
    // hMod = nullptr: thread-specific hook on a thread of our own process.
    const HHOOK hook = SetWindowsHookExA(WH_GETMESSAGE, &GetMessageHook, nullptr, thread);
    if (!hook) {
        log::Error("Input hook: SetWindowsHookEx(WH_GETMESSAGE) on thread {} failed (error {})", thread,
                   GetLastError());
        return false;
    }
    g_hook = hook;
    log::Info("Input hook installed on thread {} ({} hotkey(s)){}", thread, g_hotkeys.size(),
              g_blockMenuKey ? ", F10/Alt open no window menu" : "");
    if (log::Enabled(log::Level::Debug)) {
        g_sentHook = SetWindowsHookExA(WH_CALLWNDPROC, &CallWndProcHook, nullptr, thread);
        if (!g_sentHook) log::Debug("Window message log: SetWindowsHookEx(WH_CALLWNDPROC) failed ({})", GetLastError());
    }
    return true;
}

void Uninstall() {
    if (const HHOOK sent = g_sentHook.exchange(nullptr)) UnhookWindowsHookEx(sent);
    const HHOOK hook = g_hook.exchange(nullptr);
    if (!hook) return;
    UnhookWindowsHookEx(hook);
    log::Info("Input hook removed");
}

}  // namespace ddmod::input
