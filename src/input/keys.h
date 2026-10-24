#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ddmod::input {

// Modifier bits of a key combination.
enum Modifier : std::uint8_t {
    kNone = 0,
    kCtrl = 1,
    kShift = 2,
    kAlt = 4,
};

// A hotkey: one virtual key plus an exact set of modifiers. vk == 0 means "not bound".
struct KeyCombo {
    std::uint8_t vk = 0;
    std::uint8_t mods = kNone;

    bool Bound() const { return vk != 0; }
    bool operator==(const KeyCombo&) const = default;
};

// Parses "Ctrl+Shift+F10" style text from RivellonRenewed.ini: zero or more modifiers (Ctrl,
// Shift, Alt) followed by exactly one key name, joined with '+', case-insensitive, spaces allowed
// around the parts. Empty text or "None" gives an unbound combo; syntax errors give nullopt.
std::optional<KeyCombo> ParseKeyCombo(std::string_view text);

// Canonical text form ("Ctrl+Shift+F10"), "None" for an unbound combo.
std::string ToString(KeyCombo combo);

// True if a key-down of `vk` with exactly the modifiers `mods` triggers `combo`.
inline bool Matches(KeyCombo combo, std::uint8_t vk, std::uint8_t mods) {
    return combo.Bound() && combo.vk == vk && combo.mods == mods;
}

// [Features] BlockMenuKey: the message a key message becomes so that DefWindowProc opens no window
// menu (docs/re/input.md). F10 without Alt arrives as WM_SYSKEYDOWN/WM_SYSKEYUP and becomes
// WM_KEYDOWN/WM_KEYUP; a lone Alt opens the menu on its WM_SYSKEYUP, which becomes WM_KEYUP.
// Everything else (Alt+Tab, Alt+F4, Alt+Enter, Alt+F10) keeps its message.
inline unsigned PlainMenuKeyMessage(unsigned message, std::uint8_t vk, bool altHeld) {
    constexpr unsigned kKeyDown = 0x0100, kKeyUp = 0x0101, kSysKeyDown = 0x0104, kSysKeyUp = 0x0105;
    constexpr std::uint8_t kVkMenu = 0x12, kVkF10 = 0x79;
    if (vk == kVkF10 && !altHeld) {
        if (message == kSysKeyDown) return kKeyDown;
        if (message == kSysKeyUp) return kKeyUp;
    }
    if (vk == kVkMenu && message == kSysKeyUp) return kKeyUp;
    return message;
}

// [Features] KeyRepeat (docs/re/input.md): the game's text fields act on Backspace, Delete, Left
// and Right when the key comes up, so a press acts on release and Windows' auto-repeats of a held
// key (more key-downs) do nothing. The mod repeats these keys itself: it sends a key-up right
// after the press and then, while the key stays down, one after [Input] RepeatDelay and every
// 1/RepeatRate seconds; the key's own repeats and its real key-up do not reach the game.
inline bool IsTextEditKey(std::uint8_t vk) {
    constexpr std::uint8_t kVkBack = 0x08, kVkLeft = 0x25, kVkRight = 0x27, kVkDelete = 0x2E;
    return vk == kVkBack || vk == kVkLeft || vk == kVkRight || vk == kVkDelete;
}

// lParam of the key-ups the mod sends: the press's scan code and extended flag, previous state
// down, transition up, and a repeat count of 0 - a real key-up always counts 1, so the input hook
// tells its own key-ups apart.
inline std::uint32_t RepeatKeyUpParam(std::uint32_t keyDownParam) {
    return (keyDownParam & 0x01FF0000u) | 0xC0000000u;
}
inline bool IsRepeatKeyUp(std::uint32_t lParam) { return (lParam & 0xFFFFu) == 0; }

// Repeat timing in milliseconds.
struct RepeatTiming {
    unsigned delayMs = 300;
    unsigned intervalMs = 66;
};

// Windows' keyboard settings (Control Panel): delay 0-3 = 250-1000 ms, speed 0-31 = about 2.5 to
// 30 repeats per second.
RepeatTiming WindowsRepeatTiming(unsigned delaySetting, unsigned speedSetting);

// [Input] RepeatDelay (milliseconds, 100-2000) and RepeatRate (repeats per second, 1-50; turned
// into an interval): a number (clamped to the limits) or "windows" (= `windows`'s value). Invalid
// text gives the default and `valid` = false.
unsigned ParseRepeatDelay(std::string_view text, unsigned windowsMs, bool& valid);
unsigned ParseRepeatInterval(std::string_view text, unsigned windowsIntervalMs, bool& valid);
inline constexpr unsigned kDefaultRepeatDelayMs = 300;
inline constexpr unsigned kDefaultRepeatRate = 15;

}  // namespace ddmod::input
