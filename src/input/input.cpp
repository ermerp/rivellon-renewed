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

// [Features] BlockMenuKey and KeyRepeat, [Input] RepeatDelay/RepeatRate, read in Install.
bool g_blockMenuKey = false;
bool g_keyRepeat = false;
RepeatTiming g_repeatTiming;

// Game-thread state of the own key repeat: the text-edit key held down (0 = none), the window
// its key-ups go to and its key-down's lParam.
std::uint8_t g_repeatVk = 0;
HWND g_repeatWindow = nullptr;
std::uint32_t g_repeatParam = 0;
// LogLevel=debug: key-ups sent for the current press.
int g_repeatCount = 0;
// Timer on the game window; the id only has to differ from the game's own timers.
constexpr UINT_PTR kRepeatTimerId = 0x52524B52;  // "RRKR"

bool IsRepeat(LPARAM lParam) { return (static_cast<UINT>(lParam) & (1u << 30)) != 0; }

bool SendRepeatKeyUp() {
    ++g_repeatCount;
    return PostMessageA(g_repeatWindow, WM_KEYUP, g_repeatVk, static_cast<LPARAM>(RepeatKeyUpParam(g_repeatParam))) != 0;
}

void StopRepeat(const char* reason) {
    if (g_repeatVk != 0) {
        log::Debug("Key repeat: {} stopped ({}) after {} key-up(s)", ToString(KeyCombo{g_repeatVk, kNone}), reason,
                   g_repeatCount);
    }
    if (g_repeatWindow) KillTimer(g_repeatWindow, kRepeatTimerId);
    g_repeatVk = 0;
    g_repeatWindow = nullptr;
}

bool GameInForeground() {
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

// Runs on the game thread (DispatchMessage of the timer's WM_TIMER).
void CALLBACK RepeatTimerProc(HWND window, UINT, UINT_PTR, DWORD) {
    if (g_repeatVk == 0 || window != g_repeatWindow) {
        StopRepeat("stale timer");
        return;
    }
    if (GetAsyncKeyState(g_repeatVk) >= 0) {
        StopRepeat("key up");
        return;
    }
    if (!GameInForeground()) {
        StopRepeat("game not in the foreground");
        return;
    }
    SendRepeatKeyUp();
    SetTimer(window, kRepeatTimerId, g_repeatTiming.intervalMs, &RepeatTimerProc);
}

// [Features] KeyRepeat for one message; true if the game must not see it.
bool HandleTextEditKey(const MSG& msg) {
    const auto vk = static_cast<std::uint8_t>(msg.wParam);
    if (msg.message == WM_KEYDOWN) {
        if (!IsTextEditKey(vk)) {
            if (g_repeatVk != 0 && !IsRepeat(msg.lParam)) StopRepeat("another key");  // as in Windows
            return false;
        }
        if (IsRepeat(msg.lParam)) return vk == g_repeatVk || g_repeatVk == 0;  // Windows' repeats: ours instead
        StopRepeat("pressed again");
        g_repeatVk = vk;
        g_repeatWindow = msg.hwnd;
        g_repeatParam = static_cast<std::uint32_t>(msg.lParam);
        g_repeatCount = 0;
        const bool posted = SendRepeatKeyUp();  // acts at once, not on release
        const bool timer = SetTimer(msg.hwnd, kRepeatTimerId, g_repeatTiming.delayMs, &RepeatTimerProc) != 0;
        log::Debug("Key repeat: {} down (hwnd {:08X}), key-up {}, timer {}", ToString(KeyCombo{vk, kNone}),
                   reinterpret_cast<std::uintptr_t>(msg.hwnd), posted ? "posted" : "NOT posted",
                   timer ? "started" : "NOT started");
        return false;  // the key-down itself reaches the game as usual
    }
    if (msg.message == WM_KEYUP && IsTextEditKey(vk)) {
        if (IsRepeatKeyUp(static_cast<std::uint32_t>(msg.lParam))) return false;  // one of ours
        if (vk == g_repeatVk) StopRepeat("released");
        return true;  // the press already acted
    }
    return false;
}

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
        } else if (g_keyRepeat && HandleTextEditKey(msg)) {
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
    g_keyRepeat = config::GetBool("Features", "KeyRepeat", true);
    if (g_keyRepeat) {
        UINT delaySetting = 1, speedSetting = 31;
        SystemParametersInfoW(SPI_GETKEYBOARDDELAY, 0, &delaySetting, 0);
        SystemParametersInfoW(SPI_GETKEYBOARDSPEED, 0, &speedSetting, 0);
        const RepeatTiming windows = WindowsRepeatTiming(delaySetting, speedSetting);
        bool valid = true;
        const std::string delay = config::GetString("Input", "RepeatDelay", "300");
        g_repeatTiming.delayMs = ParseRepeatDelay(delay, windows.delayMs, valid);
        if (!valid) log::Warn("[Input] RepeatDelay=\"{}\" is not a number of milliseconds or windows - using {}", delay, g_repeatTiming.delayMs);
        const std::string rate = config::GetString("Input", "RepeatRate", "15");
        g_repeatTiming.intervalMs = ParseRepeatInterval(rate, windows.intervalMs, valid);
        if (!valid) log::Warn("[Input] RepeatRate=\"{}\" is not a number per second or windows - using {}", rate, kDefaultRepeatRate);
    }
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
    if (g_keyRepeat) {
        log::Info("Key repeat for Backspace, Delete, Left, Right: first repeat after {} ms, then every {} ms ([Input])",
                  g_repeatTiming.delayMs, g_repeatTiming.intervalMs);
    }
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
