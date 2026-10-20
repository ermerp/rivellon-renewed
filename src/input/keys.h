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

}  // namespace ddmod::input
