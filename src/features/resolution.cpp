#include "features/resolution.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include <windows.h>

#include "core/config.h"
#include "core/log.h"
#include "game/configfile.h"

namespace ddmod::features {
namespace {

std::optional<Size> g_chosen;
// What the config files said (put back before the game writes config.div).
Size g_fromFiles;

std::int32_t& Field(void* settings, std::size_t offset) {
    return *reinterpret_cast<std::int32_t*>(static_cast<std::uint8_t*>(settings) + offset);
}

void AfterConfigRead(void* settings, const char* file) {
    if (!g_chosen) return;
    const Size read{Field(settings, kWidthField), Field(settings, kHeightField)};
    if (read != *g_chosen) {
        g_fromFiles = read;
        log::Info("Resolution: {}x{} instead of {}x{} after reading {}", g_chosen->width, g_chosen->height,
                  read.width, read.height, file ? file : "?");
    }
    Field(settings, kWidthField) = g_chosen->width;
    Field(settings, kHeightField) = g_chosen->height;
}

void AroundConfigWrite(void* settings, bool before) {
    if (!g_chosen || g_fromFiles.width == 0) return;
    const Size& size = before ? g_fromFiles : *g_chosen;
    Field(settings, kWidthField) = size.width;
    Field(settings, kHeightField) = size.height;
}

// Display modes of the primary monitor (any color depth, each size once) and its current size.
std::vector<Size> MonitorModes(Size& current) {
    std::vector<Size> modes;
    current = {};
    const HMONITOR monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return modes;
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode)) {
        current = {static_cast<int>(mode.dmPelsWidth), static_cast<int>(mode.dmPelsHeight)};
    }
    for (DWORD i = 0; EnumDisplaySettingsW(info.szDevice, i, &mode); ++i) {
        const Size size{static_cast<int>(mode.dmPelsWidth), static_cast<int>(mode.dmPelsHeight)};
        if (std::find(modes.begin(), modes.end(), size) == modes.end()) modes.push_back(size);
    }
    return modes;
}

using GetResolutionsFn = std::uint32_t(__cdecl*)(std::uint32_t* pairs);

}  // namespace

ResolutionSetting ParseResolution(std::string_view text, bool& valid) {
    std::string value;
    for (char c : text) {
        if (c != ' ' && c != '\t') value += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    valid = true;
    if (value == "auto") return {};
    if (value == "config") return {ResolutionMode::Config, {}};
    const std::size_t x = value.find('x');
    if (x != std::string::npos && x > 0 && x + 1 < value.size() &&
        value.find_first_not_of("0123456789", 0) == x &&
        value.find_first_not_of("0123456789", x + 1) == std::string::npos && x <= 5 && value.size() - x - 1 <= 5) {
        const Size size{std::atoi(value.substr(0, x).c_str()), std::atoi(value.substr(x + 1).c_str())};
        if (size.width >= 640 && size.height >= 400) return {ResolutionMode::Fixed, size};
    }
    valid = false;
    return {};
}

std::optional<Size> ChooseAutoResolution(Size monitor, std::span<const Size> modes, int maxHeight) {
    std::optional<Size> best, bestSameAspect;
    const auto area = [](const Size& s) { return static_cast<long long>(s.width) * s.height; };
    for (const Size& mode : modes) {
        if (mode.width < 640 || mode.height <= 0 || mode.width > monitor.width || mode.height > monitor.height) continue;
        if (maxHeight > 0 && mode.height > maxHeight) continue;
        if (!best || area(mode) > area(*best)) best = mode;
        // Same aspect ratio as the monitor within 1 %: |w/h - mw/mh| <= 1 % of mw/mh.
        const long long lhs = static_cast<long long>(mode.width) * monitor.height;
        const long long rhs = static_cast<long long>(monitor.width) * mode.height;
        if (std::llabs(lhs - rhs) * 100 <= rhs && (!bestSameAspect || area(mode) > area(*bestSameAspect))) {
            bestSameAspect = mode;
        }
    }
    return bestSameAspect ? bestSameAspect : best;
}

bool InstallResolutionAt(void* configReader, void* configWriter, Size size) {
    g_chosen = size;
    g_fromFiles = {};
    game::OnConfigRead(&AfterConfigRead);
    game::OnConfigWrite(&AroundConfigWrite);
    return game::InstallConfigHooksAt(configReader, configWriter);
}

bool InstallResolution() {
    const std::string text = config::GetString("Display", "Resolution", "auto");
    bool valid = true;
    const ResolutionSetting setting = ParseResolution(text, valid);
    if (!valid) log::Warn("[Display] Resolution=\"{}\" is not auto, config or <width>x<height> - using auto", text);
    if (setting.mode == ResolutionMode::Config) {
        log::Info("Resolution as config.div says ([Display] Resolution=config)");
        return false;
    }
    Size monitor;
    const std::vector<Size> modes = MonitorModes(monitor);
    if (modes.empty() || monitor.width == 0) {
        log::Warn("Resolution: display modes of the primary monitor unknown - resolution as config.div says");
        return false;
    }
    if (setting.mode == ResolutionMode::Fixed) {
        if (std::find(modes.begin(), modes.end(), setting.fixed) != modes.end()) {
            g_chosen = setting.fixed;
            log::Info("Resolution: {}x{} ([Display] Resolution={})", g_chosen->width, g_chosen->height, text);
        } else {
            log::Warn("Resolution: the primary monitor offers no {}x{} mode - using auto", setting.fixed.width,
                      setting.fixed.height);
        }
    }
    if (!g_chosen) {
        const int maxHeight = std::max(0, config::GetInt("Display", "AutoMaxHeight", kDefaultAutoMaxHeight));
        g_chosen = ChooseAutoResolution(monitor, modes, maxHeight);
        if (!g_chosen) {
            log::Warn("Resolution: no display mode of the primary monitor ({}x{}) fits - resolution as config.div "
                      "says", monitor.width, monitor.height);
            return false;
        }
        log::Info("Resolution: {}x{} (auto: primary monitor {}x{}, {})", g_chosen->width, g_chosen->height,
                  monitor.width, monitor.height,
                  maxHeight > 0 ? std::format("at most {} lines", maxHeight) : std::string("no height limit"));
    }
    game::OnConfigRead(&AfterConfigRead);
    game::OnConfigWrite(&AroundConfigWrite);
    return true;
}

void CheckRendererModes() {
    HMODULE renderer = nullptr;
    int id = 1;
    for (; id <= 4 && !renderer; ++id) {
        renderer = GetModuleHandleW((L"slash" + std::to_wstring(id) + L".dll").c_str());
    }
    const auto getResolutions =
        renderer ? reinterpret_cast<GetResolutionsFn>(GetProcAddress(renderer, "DllSlashedGetResolutions")) : nullptr;
    if (!getResolutions) return;
    const std::uint32_t count = getResolutions(nullptr);
    std::vector<std::uint32_t> pairs(static_cast<std::size_t>(count) * 2);
    if (count > 0) getResolutions(pairs.data());
    std::string list;
    bool offered = false;
    for (std::uint32_t i = 0; i < count; ++i) {
        list += std::format("{}{}x{}", i ? " " : "", pairs[2 * i], pairs[2 * i + 1]);
        if (g_chosen && static_cast<int>(pairs[2 * i]) == g_chosen->width &&
            static_cast<int>(pairs[2 * i + 1]) == g_chosen->height) {
            offered = true;
        }
    }
    log::Debug("Resolution: slash{}.dll lists {} full-screen modes: {}", id - 1, count, list);
    if (g_chosen && !offered) {
        log::Warn("Resolution: slash{}.dll does not list {}x{} as a full-screen mode (fine when windowed)", id - 1,
                  g_chosen->width, g_chosen->height);
    }
}

}  // namespace ddmod::features
