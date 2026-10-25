#include "features/fog.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>

#include "core/config.h"
#include "core/log.h"
#include "game/configfile.h"

namespace ddmod::features {
namespace {

bool g_fogOn = true;
void (*g_onWrite)(bool fogOn) = nullptr;

std::uint32_t& NoFog(void* settings) {
    return *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(settings) + kNoFogField);
}

void AfterConfigRead(void* settings, const char* file) {
    const std::uint32_t wanted = g_fogOn ? 0 : 1;
    if ((NoFog(settings) != 0) != (wanted != 0)) {
        log::Info("Fog: {} after reading {} ([Features] Fog={})", g_fogOn ? "on" : "off", file ? file : "?",
                  g_fogOn ? 1 : 0);
    }
    NoFog(settings) = wanted;
}

void AroundConfigWrite(void* settings, bool before) {
    if (before && g_onWrite) g_onWrite(NoFog(settings) == 0);
}

// The picture settings' last state goes into the INI, so the next start begins with it.
void SaveFogSetting(bool fogOn) {
    if (fogOn == g_fogOn) return;
    if (config::SetString("Features", "Fog", fogOn ? "1" : "0")) {
        log::Info("Fog: switched {} in the game's picture settings - [Features] Fog={} saved", fogOn ? "on" : "off",
                  fogOn ? 1 : 0);
    } else {
        log::Warn("Fog: switched {} in the game's picture settings, but {} cannot be written", fogOn ? "on" : "off",
                  config::File().string());
    }
    g_fogOn = fogOn;
}

}  // namespace

std::optional<bool> ParseFog(std::string_view text, bool& valid) {
    std::string value(text);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    valid = true;
    if (value == "1" || value == "on" || value == "true" || value == "yes") return true;
    if (value == "0" || value == "off" || value == "false" || value == "no") return false;
    if (value == "keep") return std::nullopt;
    valid = false;
    return true;
}

bool InstallFogAt(void* configReader, void* configWriter, bool fogOn, void (*onWrite)(bool fogOn)) {
    g_fogOn = fogOn;
    g_onWrite = onWrite;
    game::OnConfigRead(&AfterConfigRead);
    game::OnConfigWrite(&AroundConfigWrite);
    return game::InstallConfigHooksAt(configReader, configWriter);
}

bool InstallFog() {
    const std::string text = config::GetString("Features", "Fog", "1");
    bool valid = true;
    const std::optional<bool> fog = ParseFog(text, valid);
    if (!valid) log::Warn("[Features] Fog=\"{}\" is not 1, 0 or keep - using 1", text);
    if (!fog) {
        log::Info("Fog as config.div says ([Features] Fog=keep)");
        return false;
    }
    g_fogOn = *fog;
    g_onWrite = &SaveFogSetting;
    game::OnConfigRead(&AfterConfigRead);
    game::OnConfigWrite(&AroundConfigWrite);
    log::Info("Fog: {} ([Features] Fog={})", *fog ? "on" : "off", *fog ? 1 : 0);
    return true;
}

}  // namespace ddmod::features
