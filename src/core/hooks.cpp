#include "core/hooks.h"

#include <mutex>
#include <string>
#include <vector>

#include <MinHook.h>

#include "core/log.h"

namespace ddmod::hooks {
namespace {

struct Hook {
    std::string name;
    void* target = nullptr;
    bool enabled = false;
};

std::mutex g_mutex;
bool g_initialized = false;
std::vector<Hook> g_hooks;

Hook* Find(std::string_view name) {
    for (Hook& hook : g_hooks) {
        if (hook.name == name) return &hook;
    }
    return nullptr;
}

bool Check(MH_STATUS status, std::string_view what, std::string_view name) {
    if (status == MH_OK) return true;
    log::Error("Hook '{}': {} failed ({})", name, what, MH_StatusToString(status));
    return false;
}

}  // namespace

bool Init() {
    std::lock_guard lock(g_mutex);
    if (g_initialized) return true;
    if (!Check(MH_Initialize(), "MH_Initialize", "*")) return false;
    g_initialized = true;
    return true;
}

void Shutdown() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return;
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();  // also removes all hooks
    g_hooks.clear();
    g_initialized = false;
}

bool CreateRaw(std::string_view name, void* target, void* detour, void** original) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) {
        log::Error("Hook '{}': hook manager not initialized", name);
        return false;
    }
    if (!target) {
        log::Error("Hook '{}': no target address", name);
        return false;
    }
    if (Find(name)) {
        log::Error("Hook '{}': name already in use", name);
        return false;
    }
    if (!Check(MH_CreateHook(target, detour, original), "create", name)) return false;
    g_hooks.push_back({std::string(name), target, false});
    log::Debug("Hook '{}': created at 0x{:08X}", name, reinterpret_cast<std::uintptr_t>(target));
    return true;
}

bool Enable(std::string_view name) {
    std::lock_guard lock(g_mutex);
    Hook* hook = Find(name);
    if (!hook) {
        log::Error("Hook '{}': unknown", name);
        return false;
    }
    if (!Check(MH_EnableHook(hook->target), "enable", name)) return false;
    hook->enabled = true;
    log::Info("Hook '{}': enabled", name);
    return true;
}

bool Disable(std::string_view name) {
    std::lock_guard lock(g_mutex);
    Hook* hook = Find(name);
    if (!hook) {
        log::Error("Hook '{}': unknown", name);
        return false;
    }
    if (!Check(MH_DisableHook(hook->target), "disable", name)) return false;
    hook->enabled = false;
    log::Info("Hook '{}': disabled", name);
    return true;
}

bool Remove(std::string_view name) {
    std::lock_guard lock(g_mutex);
    for (auto it = g_hooks.begin(); it != g_hooks.end(); ++it) {
        if (it->name != name) continue;
        if (!Check(MH_RemoveHook(it->target), "remove", name)) return false;
        g_hooks.erase(it);
        return true;
    }
    log::Error("Hook '{}': unknown", name);
    return false;
}

bool IsEnabled(std::string_view name) {
    std::lock_guard lock(g_mutex);
    const Hook* hook = Find(name);
    return hook && hook->enabled;
}

bool EnableAll() {
    std::lock_guard lock(g_mutex);
    bool any = false;
    for (const Hook& hook : g_hooks) {
        if (hook.enabled) continue;
        if (!Check(MH_QueueEnableHook(hook.target), "queue enable", hook.name)) return false;
        any = true;
    }
    if (!any) return true;
    if (!Check(MH_ApplyQueued(), "apply queued", "*")) return false;
    for (Hook& hook : g_hooks) {
        if (!hook.enabled) {
            hook.enabled = true;
            log::Info("Hook '{}': enabled", hook.name);
        }
    }
    return true;
}

}  // namespace ddmod::hooks
