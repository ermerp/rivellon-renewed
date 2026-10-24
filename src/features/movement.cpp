#include "features/movement.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <format>

#include <windows.h>

#include "core/config.h"
#include "core/hooks.h"
#include "core/log.h"
#include "core/patch.h"
#include "game/exe.h"
#include "input/input.h"
#include "overlay/overlay.h"

namespace ddmod::features {
namespace {

using AlwaysRunGetterFn = int(__thiscall*)(void* player);
using HeldRunSetterFn = void(__thiscall*)(void* agent, int run);

std::uintptr_t g_radiusCode = 0;  // 0 = walk radius not installed
std::optional<int> g_radius;
AlwaysRunGetterFn g_alwaysRunGetter = nullptr;
DoubleClickTracker g_clicks;
HeldRunSetterFn g_heldRunSetter = nullptr;
int g_lastHeldRun = -1;

// Called every frame while the button is held; only changes are logged.
void __fastcall HeldRunSetterDetour(void* agent, void* /*edx*/, int run) {
    if (run != g_lastHeldRun) {
        g_lastHeldRun = run;
        log::Debug("Held move: {} (agent {:08X})", run ? "run" : "walk", reinterpret_cast<std::uintptr_t>(agent));
    }
    g_heldRunSetter(agent, run);
}

// The test cycle of [Keys] CycleWalkRadius.
constexpr int kCycle[] = {125, 175, 225, 250, 275, 325};

// See FontInitDetour (fontmode.cpp) for the __fastcall stand-in of __thiscall.
int __fastcall AlwaysRunGetterDetour(void* player, void* /*edx*/) {
    const int alwaysRun = g_alwaysRunGetter(player);
    const bool run = alwaysRun != 0 || g_clicks.LastWasDouble();
    log::Debug("Click move: {}{}", run ? "run" : "walk",
               alwaysRun ? " (always run)" : (run ? " (double click)" : ""));
    return alwaysRun != 0 ? alwaysRun : (run ? 1 : 0);
}

std::string Lower(std::string_view text) {
    std::string value(text);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

}  // namespace

std::optional<int> ParseWalkRadius(std::string_view text, bool& valid) {
    valid = true;
    const std::string value = Lower(text);
    if (value == "original") return std::nullopt;
    int radius = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), radius);
    if (error == std::errc() && end == value.data() + value.size() && !value.empty()) {
        return std::clamp(radius, kMinWalkRadius, kMaxWalkRadius);
    }
    valid = false;
    return kDefaultWalkRadius;
}

std::string WalkRadiusText(std::optional<int> radius) { return radius ? std::to_string(*radius) : "original"; }

std::array<std::uint8_t, 9> RadiusCode(std::optional<int> radius) {
    if (!radius) return kOriginalRadiusCode;
    const auto value = static_cast<std::uint32_t>(std::clamp(*radius, kMinWalkRadius, kMaxWalkRadius) * 8);
    std::array<std::uint8_t, 9> code = {0xB8, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90};  // mov eax, imm32; nop × 4
    std::memcpy(&code[1], &value, sizeof(value));
    return code;
}

std::optional<int> NextWalkRadius(std::optional<int> current) {
    if (!current) return kCycle[0];
    for (int value : kCycle) {
        if (value > *current) return value;
    }
    return std::nullopt;
}

bool DoubleClickTracker::Press(unsigned timeMs, int x, int y) {
    const bool isDouble = havePrevious_ && !lastWasDouble_ && timeMs - previousTime_ <= maxMs_ &&
                          std::abs(x - previousX_) <= maxDx_ / 2 && std::abs(y - previousY_) <= maxDy_ / 2;
    havePrevious_ = true;
    previousTime_ = timeMs;
    previousX_ = x;
    previousY_ = y;
    lastWasDouble_ = isDouble;
    return isDouble;
}

bool InstallWalkRadiusAt(std::uintptr_t circleCheck, std::optional<int> radius) {
    const std::uintptr_t code = circleCheck + kRadiusCodeOffset;
    if (std::memcmp(reinterpret_cast<const void*>(code), kOriginalRadiusCode.data(), kOriginalRadiusCode.size()) != 0) {
        log::Warn("Walk radius: unexpected code at {:08X} - original circle", code);
        return false;
    }
    g_radiusCode = code;
    return SetWalkRadius(radius);
}

bool SetWalkRadius(std::optional<int> radius) {
    if (!g_radiusCode) return false;
    if (!patch::WriteCode("WalkRadius", g_radiusCode, RadiusCode(radius))) return false;
    g_radius = radius;
    return true;
}

bool InstallDoubleClickRunAt(void* getter) {
    g_clicks = DoubleClickTracker();
    g_clicks.Configure(GetDoubleClickTime(), GetSystemMetrics(SM_CXDOUBLECLK), GetSystemMetrics(SM_CYDOUBLECLK));
    return hooks::CreateRaw("AlwaysRunGetter", getter, reinterpret_cast<void*>(&AlwaysRunGetterDetour),
                            reinterpret_cast<void**>(&g_alwaysRunGetter)) &&
           hooks::Enable("AlwaysRunGetter");
}

void OnLeftButtonDown(unsigned timeMs, int x, int y) {
    if (g_clicks.Press(timeMs, x, y)) log::Debug("Double click at {},{}", x, y);
}

bool InstallWalkRadius() {
    const std::string text = config::GetString("Movement", "WalkRadius", std::to_string(kDefaultWalkRadius).c_str());
    bool valid = true;
    const std::optional<int> radius = ParseWalkRadius(text, valid);
    if (!valid) log::Warn("[Movement] WalkRadius=\"{}\" is not a number of pixels or original - using {}", text, kDefaultWalkRadius);
    const std::uintptr_t circle = game::Resolve({"WalkCircle", kWalkCirclePattern, 0, kWalkCircleRva});
    if (!circle) {
        log::Warn("Walk radius: circle check not found - original circle");
        return false;
    }
    if (!InstallWalkRadiusAt(circle, radius)) return false;
    if (radius) {
        log::Info("Walk radius: {} px ([Movement] WalkRadius={})", *radius, *radius);
    } else {
        log::Info("Walk radius: original, 3/8 of the screen height ([Movement] WalkRadius=original)");
    }
    return true;
}

bool InstallDoubleClickRun() {
    if (!config::GetBool("Movement", "DoubleClickRun", true)) {
        log::Info("Double click run off ([Movement] DoubleClickRun=0)");
        return false;
    }
    const std::uintptr_t getter = game::Resolve({"AlwaysRunGetter", kAlwaysRunGetterPattern, 0, kAlwaysRunGetterRva});
    if (!getter) {
        log::Warn("Double click run: always-run getter not found - clicks walk as in the original");
        return false;
    }
    if (!InstallDoubleClickRunAt(reinterpret_cast<void*>(getter))) return false;
    input::SetLeftButtonObserver(&OnLeftButtonDown);
    log::Info("Double click run hook installed ([Movement] DoubleClickRun=1, {} ms)", GetDoubleClickTime());
    return true;
}

bool InstallRunLog() {
    const std::uintptr_t setter = game::Resolve({"HeldRunSetter", kHeldRunSetterPattern, 0, kHeldRunSetterRva});
    if (!setter) return false;
    return hooks::CreateRaw("HeldRunSetter", reinterpret_cast<void*>(setter), reinterpret_cast<void*>(&HeldRunSetterDetour),
                            reinterpret_cast<void**>(&g_heldRunSetter)) &&
           hooks::Enable("HeldRunSetter");
}

void CycleWalkRadius() {
    if (!g_radiusCode) {
        log::Info("Walk radius: not installed - hotkey ignored");
        return;
    }
    const std::optional<int> next = NextWalkRadius(g_radius);
    if (!SetWalkRadius(next)) return;
    const std::string text = WalkRadiusText(next);
    const bool saved = config::SetString("Movement", "WalkRadius", text.c_str());
    log::Info("Walk radius: {}{} (hotkey){}", text, next ? " px" : "", saved ? " - saved" : " - INI not writable");
    overlay::ShowNotice(next ? std::format(L"Walk radius {} px", *next) : std::wstring(L"Walk radius original"), 3);
}

}  // namespace ddmod::features
