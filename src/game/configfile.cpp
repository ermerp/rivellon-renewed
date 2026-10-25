#include "game/configfile.h"

#include <algorithm>
#include <vector>

#include "core/hooks.h"
#include "core/log.h"
#include "game/exe.h"

namespace ddmod::game {
namespace {

using ConfigReaderFn = int(__thiscall*)(void* settings, const char* file);
using ConfigWriterFn = int(__thiscall*)(void* settings);

ConfigReaderFn g_configReader = nullptr;
ConfigWriterFn g_configWriter = nullptr;
std::vector<ConfigReadObserver> g_readObservers;
std::vector<ConfigWriteObserver> g_writeObservers;

// See FontInitDetour (fontmode.cpp) for the __fastcall stand-in of __thiscall.
int __fastcall ConfigReaderDetour(void* settings, void* /*edx*/, const char* file) {
    const int result = g_configReader(settings, file);
    for (ConfigReadObserver observer : g_readObservers) observer(settings, file);
    return result;
}

int __fastcall ConfigWriterDetour(void* settings, void* /*edx*/) {
    for (ConfigWriteObserver observer : g_writeObservers) observer(settings, true);
    const int result = g_configWriter(settings);
    for (ConfigWriteObserver observer : g_writeObservers) observer(settings, false);
    return result;
}

template <typename T>
void AddOnce(std::vector<T>& list, T item) {
    if (item && std::find(list.begin(), list.end(), item) == list.end()) list.push_back(item);
}

}  // namespace

void OnConfigRead(ConfigReadObserver observer) { AddOnce(g_readObservers, observer); }
void OnConfigWrite(ConfigWriteObserver observer) { AddOnce(g_writeObservers, observer); }

void ClearConfigObservers() {
    g_readObservers.clear();
    g_writeObservers.clear();
}

bool InstallConfigHooksAt(void* reader, void* writer) {
    if (!hooks::CreateRaw("ConfigReader", reader, reinterpret_cast<void*>(&ConfigReaderDetour),
                          reinterpret_cast<void**>(&g_configReader)) ||
        !hooks::Enable("ConfigReader")) {
        return false;
    }
    if (!writer) return true;
    return hooks::CreateRaw("ConfigWriter", writer, reinterpret_cast<void*>(&ConfigWriterDetour),
                            reinterpret_cast<void**>(&g_configWriter)) &&
           hooks::Enable("ConfigWriter");
}

bool InstallConfigHooks() {
    if (g_readObservers.empty() && g_writeObservers.empty()) return false;
    const std::uintptr_t reader = Resolve({"ConfigReader", kConfigReaderPattern, 0, kConfigReaderRva});
    if (!reader) {
        log::Warn("Config file reader not found - fog and resolution as config.div says");
        return false;
    }
    const std::uintptr_t writer = Resolve({"ConfigWriter", kConfigWriterPattern, 0, kConfigWriterRva});
    if (!writer) {
        log::Warn("config.div writer not found - fog changes in the picture settings are not kept, and the "
                  "resolution the mod chose can end up in config.div");
    }
    return InstallConfigHooksAt(reinterpret_cast<void*>(reader), reinterpret_cast<void*>(writer));
}

}  // namespace ddmod::game
