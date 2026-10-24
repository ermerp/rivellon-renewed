#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ddmod::features {

// Hero movement by mouse (docs/re/movement.md).
//
// Walk radius: holding the left button makes the hero walk while the cursor is inside a circle
// around the screen center and run outside it. The game sizes the circle at 3/8 of the screen
// height, so it grows with the resolution although the world is drawn 1:1. [Movement] WalkRadius
// sets a fixed radius in pixels instead (the same distance in the world at every resolution);
// original keeps the game's circle. The mod rewrites the radius computation in memory (one
// instruction that loads the value), the comparison stays the game's.
//
// Double click: a click on the ground sends the hero there with pathfinding, walking unless the
// game's always-run field is set. [Movement] DoubleClickRun=1 makes the move started by a double
// click run: the hook on the field's getter (called only by that move) also answers "run" while
// the last press of the left button was the second of a double click.

// The circle check in the move handler: from loading the screen width up to the always-run test.
inline constexpr std::string_view kWalkCirclePattern =
    "A1 ?? ?? ?? ?? 8B 8E 1C 05 00 00 8B 1D ?? ?? ?? ?? 8B BE 20 05 00 00 99 2B C2 D1 F8 2B C8 8B C3 99 2B C2 D1 F8 "
    "2B F8 8D 04 5B 99 83 E2 07 03 C2 8B D7 0F AF D7 8B F9 C1 F8 03 0F AF F9 8B C8 0F AF C8 03 D7 3B D1 7F ?? 83 BE "
    "80 04 00 00 00";
// RVA of the match in the verified 1.0062A build (fallback only, rule 8).
inline constexpr std::uint32_t kWalkCircleRva = 0xA65AE;
// Offset of the radius computation in the match: 3 × height (lea eax,[ebx+ebx*2]) plus the
// rounding of the signed division (cdq / and edx,7 / add eax,edx); a later sar eax,3 divides by 8.
inline constexpr std::size_t kRadiusCodeOffset = 39;
inline constexpr std::array<std::uint8_t, 9> kOriginalRadiusCode = {0x8D, 0x04, 0x5B, 0x99, 0x83,
                                                                    0xE2, 0x07, 0x03, 0xC2};

// Getter of the player's always-run field (+0x480): __thiscall, no arguments, returns the field.
inline constexpr std::string_view kAlwaysRunGetterPattern = "8B 81 80 04 00 00 C3";
inline constexpr std::uint32_t kAlwaysRunGetterRva = 0xA6FA0;

// The party member's run setter (vtable +0x84, __thiscall, one argument: run 1/0), used by the
// held-button move only; hooked at LogLevel=debug to log the walk/run choice.
inline constexpr std::string_view kHeldRunSetterPattern =
    "D9 EE D8 91 2C 02 00 00 DF E0 F6 C4 44 7A 0F D8 99 30 02 00 00 DF E0 F6 C4 44 7B 0D EB 02 DD D8 83 B9 78 02 00 00 00 7F";
inline constexpr std::uint32_t kHeldRunSetterRva = 0x41030;

// Radius limits for [Movement] WalkRadius (pixels).
inline constexpr int kMinWalkRadius = 0;
inline constexpr int kMaxWalkRadius = 4000;
inline constexpr int kDefaultWalkRadius = 225;  // the original circle at 800×600

// [Movement] WalkRadius: a number of pixels (clamped to the limits) or "original" (nullopt).
// Invalid text gives the default and `valid` = false.
std::optional<int> ParseWalkRadius(std::string_view text, bool& valid);
std::string WalkRadiusText(std::optional<int> radius);

// The bytes that replace kOriginalRadiusCode: mov eax, radius × 8 (the game's sar eax,3 then
// leaves the radius), padded with nops; nullopt gives the original bytes.
std::array<std::uint8_t, 9> RadiusCode(std::optional<int> radius);

// The value after `current` in the test cycle of [Keys] CycleWalkRadius (… → original → first).
std::optional<int> NextWalkRadius(std::optional<int> current);

// Double click detection from the left button's presses (time in ms, screen position): a press
// is the second of a double click if it follows the previous one within `maxMs` and inside the
// `maxDx` × `maxDy` box (Windows' double click settings).
class DoubleClickTracker {
public:
    void Configure(unsigned maxMs, int maxDx, int maxDy) {
        maxMs_ = maxMs;
        maxDx_ = maxDx;
        maxDy_ = maxDy;
    }
    // Returns whether this press completes a double click; a third press within the time starts
    // over, so triple clicks do not count twice.
    bool Press(unsigned timeMs, int x, int y);
    bool LastWasDouble() const { return lastWasDouble_; }

private:
    unsigned maxMs_ = 500;
    int maxDx_ = 4;
    int maxDy_ = 4;
    bool havePrevious_ = false;
    unsigned previousTime_ = 0;
    int previousX_ = 0;
    int previousY_ = 0;
    bool lastWasDouble_ = false;
};

// Writes the radius code at `circleCheck` (the match of kWalkCirclePattern) after checking that
// the original bytes are there. Exposed for the unit tests.
bool InstallWalkRadiusAt(std::uintptr_t circleCheck, std::optional<int> radius);
// Changes the radius after InstallWalkRadiusAt (game thread only: the move handler runs there).
bool SetWalkRadius(std::optional<int> radius);

// Hooks the always-run getter at `getter`. Exposed for the unit tests, which feed presses through
// OnLeftButtonDown (the input hook does that in game).
bool InstallDoubleClickRunAt(void* getter);
void OnLeftButtonDown(unsigned timeMs, int x, int y);

// Apply [Movement]; call after hooks::Init and game::IdentifyExe.
bool InstallWalkRadius();
bool InstallDoubleClickRun();
// LogLevel=debug: logs every change of the held-button walk/run choice.
bool InstallRunLog();

// Hotkey [Keys] CycleWalkRadius: next radius of the test cycle, saved into the INI, shown in the
// overlay.
void CycleWalkRadius();

}  // namespace ddmod::features
