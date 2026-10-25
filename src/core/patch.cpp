#include "core/patch.h"

#include <cstring>

#include <windows.h>

#include "core/log.h"

namespace ddmod::patch {

bool WriteCode(std::string_view name, std::uintptr_t address, std::span<const std::uint8_t> bytes) {
    void* target = reinterpret_cast<void*>(address);
    DWORD oldProtect = 0;
    if (!VirtualProtect(target, bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        log::Error("Patch {}: VirtualProtect at {:08X} failed (error {})", name, address, GetLastError());
        return false;
    }
    std::memcpy(target, bytes.data(), bytes.size());
    DWORD ignored = 0;
    VirtualProtect(target, bytes.size(), oldProtect, &ignored);
    FlushInstructionCache(GetCurrentProcess(), target, bytes.size());
    return true;
}

}  // namespace ddmod::patch
